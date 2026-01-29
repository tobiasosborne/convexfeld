# Useful Patterns

This file captures reusable patterns discovered during development.

---

## TDD Patterns

### Stub Extraction Pattern
Functions start as stubs in `*_stub.c` files for TDD, then extract to dedicated files.

```
1. Write TDD tests that call function
2. Implement stub in *_stub.c that passes tests
3. Once tests pass, extract to dedicated file
4. Update CMakeLists.txt to add new file
5. Remove extracted function from stub file
6. Stub file shrinks over time, eventually becomes placeholder
```

**Example:**
- `src/pricing/pricing_stub.c` → `src/pricing/context.c` + `src/pricing/init.c` + ...
- `src/basis/basis_stub.c` → `src/basis/basis_state.c` + `src/basis/eta_factors.c` + ...

### Identity Basis Testing
For basis operations, stubs can implement identity basis behavior (trivial case):
- All tests pass with stubs because they test identity basis
- More complex tests fail until full implementation
- This is valid TDD - tests define expected behavior

---

## C99 Patterns

### Structure Definition Pattern
```c
struct CxfEnv {
    uint32_t magic;           /* 0xC0FEFE1D for validation */
    int active;               /* 1 if environment is active */
    char error_buffer[512];   /* Last error message */
    /* ... fields ... */
};
```

### Unity Test Pattern
```c
#include "unity.h"
void setUp(void) {}
void tearDown(void) {}
void test_function_name(void) {
    /* Test code */
    TEST_ASSERT_EQUAL_INT(expected, actual);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_function_name);
    return UNITY_END();
}
```

### NULL-Safe Free Pattern
```c
void cxf_something_free(Something *ptr) {
    if (ptr == NULL) return;
    /* Free internal arrays */
    cxf_free(ptr->array1);
    cxf_free(ptr->array2);
    /* Free the structure itself */
    cxf_free(ptr);
}
```

### Magic Number Validation Pattern
```c
#define CXF_ENV_MAGIC 0xC0FEFE1D

int cxf_checkenv(CxfEnv *env) {
    if (env == NULL) return CXF_ERROR_NULL_ARGUMENT;
    if (env->magic != CXF_ENV_MAGIC) return CXF_ERROR_INVALID_ARGUMENT;
    return CXF_OK;
}
```

---

## Algorithm Patterns

### Arena Allocator (EtaBuffer)
Fast allocation for eta vectors with reset capability.

```c
/* Fast path: bump pointer in active chunk O(1) */
void *cxf_alloc_eta(CxfEnv *env, EtaBuffer *buffer, size_t size) {
    if (buffer->used + size <= buffer->chunk_size) {
        void *ptr = buffer->current + buffer->used;
        buffer->used += size;
        return ptr;
    }
    /* Slow path: allocate new chunk */
    ...
}

/* Reset enables reuse without deallocation */
void cxf_eta_buffer_reset(EtaBuffer *buffer) {
    buffer->current = buffer->first_chunk;
    buffer->used = 0;
}
```

**Benefits:**
- O(1) allocation in common case
- Exponential growth caps at CXF_MAX_CHUNK_SIZE (64KB)
- Reset for reuse without deallocation (good for refactorization cycles)

### Product Form of Inverse

**FTRAN (Forward):** Solve Bx = b
```
For each eta E in chronological order (oldest to newest):
    temp = result[pivot_row]
    result[pivot_row] = temp / pivot_elem
    For each off-diagonal: result[j] -= eta_value * temp
```

**BTRAN (Backward):** Solve y^T B = c^T
```
For each eta E in REVERSE order (newest to oldest):
    temp = dot_product(eta_values, result[eta_indices])
    result[pivot_row] = (result[pivot_row] - temp) / pivot_elem
    # Other positions UNCHANGED
```

**Key difference:**
- FTRAN applies E^(-1): updates pivot AND off-diagonal positions
- BTRAN applies (E^(-1))^T: updates ONLY pivot position

### Reverse Traversal of Singly-Linked List
When you only have `next` pointers but need reverse order:

```c
#define MAX_STACK_ETAS 64
EtaFactors *eta_ptrs[MAX_STACK_ETAS];
EtaFactors **heap_ptrs = NULL;

/* Collect pointers into array */
int count = 0;
for (EtaFactors *e = basis->eta_head; e != NULL; e = e->next) {
    if (count < MAX_STACK_ETAS) {
        eta_ptrs[count++] = e;
    } else {
        /* Fall back to heap allocation */
    }
}

/* Traverse backwards */
for (int i = count - 1; i >= 0; i--) {
    EtaFactors *e = eta_ptrs[i];
    /* Process eta */
}
```

### CSC to CSR Conversion (Two-Pass Transpose)
```c
/* Pass 1: Count entries per row */
for (int64_t k = 0; k < nnz; k++) {
    row_ptr[row_idx[k] + 1]++;
}
/* Convert to cumulative offsets */
for (int i = 1; i <= num_rows; i++) {
    row_ptr[i] += row_ptr[i-1];
}

/* Pass 2: Fill CSR using working copy */
int64_t *work = copy of row_ptr;
for (int j = 0; j < num_cols; j++) {
    for (int64_t k = col_ptr[j]; k < col_ptr[j+1]; k++) {
        int row = row_idx[k];
        int64_t dest = work[row]++;
        col_idx[dest] = j;
        row_values[dest] = values[k];
    }
}
```

---

## Pricing Patterns

### Variable Status Codes
```c
#define VAR_BASIC     >= 0  /* Index of basic variable */
#define VAR_AT_LOWER  -1
#define VAR_AT_UPPER  -2
#define VAR_FREE      -3
```

### Reduced Cost Attractiveness
```c
/* At lower bound: attractive if RC < -tolerance (want to increase) */
/* At upper bound: attractive if RC > tolerance (want to decrease) */
/* Free variable: attractive if |RC| > tolerance */

if (status == VAR_AT_LOWER && rc < -tolerance) attractive = 1;
if (status == VAR_AT_UPPER && rc > tolerance) attractive = 1;
if (status == VAR_FREE && fabs(rc) > tolerance) attractive = 1;
```

### Steepest Edge Ratio
```c
double se_ratio = fabs(reduced_cost) / sqrt(weight);
/* Higher ratio = more attractive */
/* Weight safeguard: if weight < 1e-10, use 1.0 */
```

### Partial Pricing Sections
```c
#define NUM_SECTIONS 10
int section = iteration % NUM_SECTIONS;
int section_size = num_vars / NUM_SECTIONS;
int start = section * section_size;
int end = (section == NUM_SECTIONS - 1) ? num_vars : start + section_size;
```

---

## API Patterns

### Dynamic Array Growth Pattern
For resizable arrays in model structures, use doubling strategy with cxf_realloc:

```c
extern void *cxf_realloc(void *ptr, size_t size);

static int cxf_model_grow_vars(CxfModel *model, int needed_capacity) {
    int new_capacity = model->var_capacity;
    while (new_capacity < needed_capacity) {
        new_capacity *= 2;  /* Amortized O(1) insertion */
    }

    /* Reallocate all parallel arrays */
    double *new_obj = (double *)cxf_realloc(model->obj_coeffs,
                                             (size_t)new_capacity * sizeof(double));
    if (new_obj == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }
    model->obj_coeffs = new_obj;
    /* ... repeat for other arrays ... */

    model->var_capacity = new_capacity;
    return CXF_OK;
}
```

**Key points:**
- Double capacity until >= needed_capacity (amortized O(1))
- Check each realloc for NULL before updating model pointer
- Update capacity field after successful reallocation
- Initial capacity: 16 (INITIAL_VAR_CAPACITY)
- Handles parallel arrays: obj_coeffs, lb, ub, vtype, solution

**Test coverage:**
- test_addvar_exceeds_initial_capacity - add 20 vars (> 16)
- test_addvars_batch_exceeds_capacity - add 50 vars at once
- test_addvar_grows_capacity - verify capacity increased

---

## Development Workflow Patterns

### Parallel Subagent Pattern
For independent implementation tasks, launch multiple sonnet subagents in parallel:

```
1. Identify independent tasks (no dependencies between them)
2. Read specs and existing code patterns for context
3. Launch 3-4 subagents simultaneously with clear prompts:
   - Task description and file paths
   - Existing code patterns to follow
   - Specific functions to implement
   - Tests to add
   - Build/test commands
4. Each subagent commits its own changes
5. Verify all tests pass after subagents complete
6. Close issues and update documentation
```

**Example (API implementation session):**
```
Subagent 1: M8.1.10 - Model API (copymodel, updatemodel)
Subagent 2: M8.1.11 - Variable API (dynamic resizing)
Subagent 3: M8.1.12 - Constraint API (validation)
Subagent 4: M8.1.9 - Environment API (verification)
```

**Benefits:**
- 4x throughput for independent tasks
- Each subagent has focused context
- Reduces main agent context usage
- Parallel commits don't conflict if files are independent

**Gotchas:**
- Ensure tasks modify different files (avoid merge conflicts)
- Provide enough context in prompts (subagents don't share state)
- Verify combined build passes after all complete

### Code Review Checklist
Before major milestones, perform full-scale code review:

```
1. Duplicate code detection
   - grep for similar function signatures
   - Look for copy-pasted validation patterns
   - Identify candidates for extraction

2. Consistency check
   - Error code usage consistent across modules
   - NULL handling follows same pattern
   - Magic number validation in all public APIs

3. File size compliance
   - Find files > 200 LOC
   - Create refactor issues for violations

4. Test coverage gaps
   - Functions without corresponding tests
   - Error paths not exercised

5. Documentation sync
   - Function signatures match headers
   - Specs match implementation
```

---

## Simplex Patterns

### Unconstrained LP Special Case
Handle LPs with no constraints directly without iteration:

```c
if (model->num_constrs == 0) {
    /* Check infeasibility (lb > ub) first */
    for (int j = 0; j < n; j++) {
        if (model->lb[j] > model->ub[j] + tol) return CXF_INFEASIBLE;
    }

    /* Set each variable to optimal bound */
    for (int j = 0; j < n; j++) {
        if (c[j] < 0) {        /* Want max x */
            if (ub[j] >= INF) return CXF_UNBOUNDED;
            x[j] = ub[j];
        } else if (c[j] > 0) { /* Want min x */
            if (lb[j] <= -INF) return CXF_UNBOUNDED;
            x[j] = lb[j];
        } else {               /* c=0, any value works */
            x[j] = 0.0;        /* Or any feasible value */
        }
    }
}
```

### Harris Two-Pass Ratio Test
Improves numerical stability over simple ratio test:

```c
/* Pass 1: Find minimum ratio with RELAXED tolerance */
double relaxed_tol = 10.0 * feasibility_tol;
double min_ratio = INF;
for (row in basic_rows) {
    if (|pivot_col[row]| > relaxed_tol) {
        ratio = compute_ratio(row);
        min_ratio = min(min_ratio, ratio);
    }
}

/* Pass 2: Among near-minimum ratios, pick LARGEST pivot */
double best_pivot = 0;
for (row in basic_rows) {
    if (|pivot_col[row]| > relaxed_tol) {
        ratio = compute_ratio(row);
        if (ratio <= min_ratio + tol && |pivot_col[row]| > |best_pivot|) {
            best_pivot = pivot_col[row];
            leaving_row = row;
        }
    }
}
```

### Return Value Convention
- `cxf_solve_lp`: Returns CXF_OK (0) on success, sets model->status to CXF_OPTIMAL
- Callers check return for errors, check model->status for optimization outcome
- Non-zero return indicates error/special status (UNBOUNDED, INFEASIBLE, etc.)

---

## Performance Patterns

### Row-Major Access for CSC Matrix
The matrix is stored in CSC (column-major), but row access is O(nnz). Build CSR format once for O(nnz_row) row access:

```c
/* Build row-major format if not present (one-time O(nnz) cost) */
if (mat->row_ptr == NULL) {
    if (cxf_prepare_row_data(mat) == CXF_OK) {
        cxf_build_row_major(mat);
    }
}

/* Fast row access using CSR */
if (mat->row_ptr != NULL) {
    int64_t start = mat->row_ptr[row];
    int64_t end = mat->row_ptr[row + 1];
    for (int64_t k = start; k < end; k++) {
        int col = mat->col_idx[k];
        double val = mat->row_values[k];
        /* Process coefficient */
    }
}
```

### Limit O(n²) Preprocessing
For algorithms that compare all pairs (e.g., parallel constraint detection), limit to small problem sizes:

```c
#define MAX_PARALLEL_CHECK_ROWS 100

/* Only do O(m²) check for small problems */
if (m <= MAX_PARALLEL_CHECK_ROWS) {
    for (int i = 0; i < m; i++) {
        for (int j = i + 1; j < m; j++) {
            /* O(m²) comparison */
        }
    }
}
```

### Profiling with Callgrind
```bash
# Build with debug symbols
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_FLAGS="-O2 -g -fno-omit-frame-pointer"

# Profile
valgrind --tool=callgrind --callgrind-out-file=callgrind.out ./bench_netlib --filter name

# Analyze
callgrind_annotate --auto=yes callgrind.out | head -60
```

### Hash Table for String Lookups (djb2)
When you have O(n) linear search with strcmp, replace with O(1) hash table:

```c
#define HASH_SIZE 2048  /* Power of 2 for fast modulo */

typedef struct HashEntry {
    char name[MAX_NAME];
    int index;
    struct HashEntry *next;  /* Separate chaining */
} HashEntry;

/* djb2 hash - simple and fast */
static unsigned int hash_string(const char *str) {
    unsigned int hash = 5381;
    int c;
    while ((c = (unsigned char)*str++)) {
        hash = ((hash << 5) + hash) + c;  /* hash * 33 + c */
    }
    return hash & (HASH_SIZE - 1);  /* Fast modulo */
}

/* O(1) lookup */
static int hash_find(HashEntry **table, const char *name) {
    unsigned int bucket = hash_string(name);
    HashEntry *e = table[bucket];
    while (e) {
        if (strcmp(e->name, name) == 0) return e->index;
        e = e->next;
    }
    return -1;
}
```

**Results on MPS parser:**
- Before: mps_find_col 27%, strcmp 22%, mps_find_row 6% = 43% total
- After: hash_table_find 0.31%, strcmp 0.12% = 0.43% total
- **33% total runtime reduction**
