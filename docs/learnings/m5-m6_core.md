# M5-M6: Core Operations & Algorithm Layer Learnings

## M5: Core Operations (Level 3)

### 2026-01-26: M5.1.1 Basis Tests

**Files created:**
- `tests/unit/test_basis.c` (468 LOC) - 29 comprehensive TDD tests
- `src/basis/basis_stub.c` (337 LOC) - Stub implementations for TDD

**Tests written (29 total):**
- BasisState create/free/init (4 tests) - PASS
- EtaFactors create/free (4 tests) - PASS
- cxf_ftran FTRAN tests (4 tests) - PASS (identity basis stub)
- cxf_btran BTRAN tests (4 tests) - PASS (identity basis stub)
- cxf_basis_refactor tests (3 tests) - PASS
- Snapshot/comparison tests (6 tests) - PASS
- Validation/warm start tests (4 tests) - PASS

**TDD pattern difference from test_matrix:**
- Basis stubs implement identity basis behavior (trivial case)
- All tests pass with stubs because they test identity basis
- More complex tests (non-identity basis) will fail until full implementation
- This is still valid TDD - tests define expected behavior

**Key decisions:**
- Tests use explicit function declarations (not headers) for TDD
- Stubs handle identity basis case correctly (x = Ax when A = I)
- Validation tests check for duplicate basic variables
- Snapshot tests verify basis state can be saved/compared/restored

---

### 2026-01-26: M5.1.2 BasisState Structure

**File created:** `src/basis/basis_state.c` (127 LOC)

**Functions implemented:**
- `cxf_basis_create(m, n)` - Allocates BasisState with all arrays
- `cxf_basis_free(basis)` - Frees BasisState and eta linked list
- `cxf_basis_init(basis, m, n)` - Reinitializes existing BasisState

**TDD integration pattern:**
- Stub file (basis_stub.c) originally contained all functions for TDD
- As implementations are completed, extract to dedicated files
- Stub file retains only unimplemented functions (FTRAN, BTRAN, etc.)
- Both files linked in CMakeLists.txt; no duplicate symbols

**Key decisions:**
- Added negative dimension checks (m < 0 || n < 0 return NULL/error)
- Enhanced cxf_basis_init to clear arrays and validate dimension match
- DEFAULT_REFACTOR_FREQ = 100 (pivots between refactorizations)

---

### 2026-01-26: M5.1.3 EtaFactors Structure

**File created:** `src/basis/eta_factors.c` (192 LOC)

**Functions implemented:**
- `cxf_eta_create(type, pivot_row, nnz)` - Allocates eta with sparse arrays
- `cxf_eta_free(eta)` - Frees eta and arrays (NULL-safe)
- `cxf_eta_init(eta, type, pivot_row, nnz)` - Reinitialize existing eta
- `cxf_eta_validate(eta, max_rows)` - Validate eta invariants
- `cxf_eta_set(eta, indices, values)` - Copy data into eta arrays

**Key validations in cxf_eta_validate:**
- nnz >= 0
- type is 1 (refactor) or 2 (pivot)
- pivot_row in range [0, max_rows)
- pivot_elem is finite and non-zero
- All indices in range and values finite

**Error code gotcha:**
- Use `CXF_ERROR_OUT_OF_MEMORY`, not `CXF_ERROR_MEMORY`
- Check cxf_types.h for correct error codes

---

### 2026-01-26: M5.1.4 cxf_ftran

**File created:** `src/basis/ftran.c` (95 LOC)

**Function implemented:**
- `cxf_ftran(basis, column, result)` - Solve Bx = b using eta representation

**Algorithm (Product Form of Inverse):**
1. Copy input column to result (identity basis case)
2. Apply eta vectors in chronological order (oldest to newest via linked list)
3. For each eta E with pivot row r and pivot element p:
   - Save temp = result[r]
   - result[r] = temp / pivot_elem
   - For each off-diagonal entry: result[j] -= eta_value[j] * temp

**Key design decisions:**
- Follows linked list from eta_head → next → next (oldest to newest)
- Separate input/output arrays to preserve original column
- Bounds checking on pivot_row and division by zero check on pivot_elem
- Sparse off-diagonal entries stored in indices/values arrays

---

### 2026-01-26: M5.1.5 cxf_btran

**File created:** `src/basis/btran.c` (126 LOC)

**Function implemented:**
- `cxf_btran(basis, row, result)` - Solve y^T B = e_row^T using eta representation

**Algorithm (Product Form of Inverse, REVERSE order):**
1. Initialize result as unit vector e_row
2. Collect eta pointers into array (singly-linked list only has next)
3. Apply eta vectors in REVERSE order (newest to oldest):
   - Compute dot product: temp = sum(eta_val[k] * result[indices[k]])
   - Update pivot: result[pivot_row] = (result[pivot_row] - temp) / pivot_elem
   - Other positions UNCHANGED (key difference from FTRAN)

**Key mathematical insight - BTRAN vs FTRAN:**
- FTRAN applies E^(-1): updates pivot AND off-diagonal positions
- BTRAN applies (E^(-1))^T: updates ONLY pivot position
- BTRAN traverses eta list in REVERSE order (newest to oldest)

**Implementation pattern for reverse traversal:**
- Singly-linked list with only `next` pointers
- Collect pointers into array, then traverse backwards
- Stack allocation for ≤64 etas, heap for larger (MAX_STACK_ETAS = 64)
- This avoids recursion overhead and is cache-friendly

---

### 2026-01-26: M5.1.6 cxf_basis_refactor

**File created:** `src/basis/refactor.c` (208 LOC)

**Functions implemented:**
- `cxf_basis_refactor(basis)` - Clear eta vectors, reset counters (PFI approach)
- `cxf_solver_refactor(ctx, env)` - Full refactorization with solver context
- `cxf_refactor_check(ctx, env)` - Determine if refactorization needed

**Design decision - PFI vs LU:**
- Spec describes full LU factorization with Markowitz ordering
- Current architecture uses Product Form of Inverse (PFI) with eta vectors
- FTRAN/BTRAN already work with eta vectors
- Full LU would require BasisState structure changes (L, U storage)
- Implemented PFI-compatible version for now

**Refactorization triggers (cxf_refactor_check):**
- eta_count >= max_eta_count → Required (return 2)
- eta_memory >= max_eta_memory → Required (return 2)
- iterations since refactor >= refactor_interval → Recommended (return 1)
- FTRAN degradation (avg > 3x baseline) → Recommended (return 1)

**Key insight:**
- For identity basis (all slacks), no eta vectors needed (B = I)
- For structural columns, full implementation needs matrix access via model_ref
- TODO marker left for Markowitz-ordered LU when matrix access available

---

## M6: Algorithm Layer (Level 4)

### 2026-01-26: M6.1.1 Pricing Tests

**Files created:**
- `tests/unit/test_pricing.c` (310 LOC) - 24 comprehensive TDD tests
- `src/pricing/pricing_stub.c` (233 LOC) - Stub implementations for TDD

**Tests written (24 total):**
- PricingContext create/free (4 tests) - PASS
- cxf_pricing_init (4 tests) - PASS
- cxf_pricing_candidates (4 tests) - PASS
- cxf_pricing_steepest (4 tests) - PASS
- cxf_pricing_update (2 tests) - PASS
- cxf_pricing_invalidate (3 tests) - PASS
- cxf_pricing_step2 (2 tests) - PASS
- Statistics tracking (1 test) - PASS

**TDD pattern for pricing:**
- Tests define expected behavior for partial pricing, steepest edge, Dantzig
- Stubs implement working versions to enable test-first development
- Variable status codes: VAR_BASIC (>=0), VAR_AT_LOWER (-1), VAR_AT_UPPER (-2)
- Reduced cost attractiveness:
  - At lower bound: attractive if RC < -tolerance
  - At upper bound: attractive if RC > tolerance
- Steepest edge ratio: |RC| / sqrt(weight)

**Key design decisions:**
- PricingContext owns candidate_counts, candidate_arrays, cached_counts arrays
- Multi-level partial pricing support (max_levels typically 3-5)
- Cache invalidation via flags (INVALID_CANDIDATES, INVALID_WEIGHTS, etc.)
- Weights safeguard: zero/negative weights replaced with 1.0

---

### 2026-01-26: M6.1.2 PricingContext Structure

**File created:** `src/pricing/context.c` (118 LOC)

**Functions implemented:**
- `cxf_pricing_create(num_vars, max_levels)` - Allocates PricingContext with level arrays
- `cxf_pricing_free(ctx)` - Frees context and all arrays (NULL-safe)
- `cxf_pricing_init(ctx, num_vars, strategy)` - Reinitialize for new solve

**Extraction pattern:**
- Original lifecycle functions lived in `pricing_stub.c` for TDD
- Extracted to `context.c` as proper implementation
- Stub file now only contains algorithm functions (candidates, steepest, update, etc.)
- Both files linked - no duplicate symbols

**Key fields in PricingContext:**
- current_level, max_levels - level management
- candidate_counts, candidate_arrays, cached_counts - per-level arrays
- last_pivot_iteration, total_candidates_scanned, level_escalations - statistics

---

### 2026-01-26: M6.1.3 cxf_pricing_init

**File created:** `src/pricing/init.c` (179 LOC)

**Function implemented:**
- `cxf_pricing_init(ctx, num_vars, strategy)` - Initialize pricing context for new solve

**Algorithm:**
1. Auto-select strategy if requested (strategy=0, threshold n<1000 for small)
2. Store num_vars and strategy in context
3. Allocate candidate arrays per level:
   - Level 0: full (all variables)
   - Level 1+: sqrt(n) for partial pricing, minimum MIN_CANDIDATES=100
4. For steepest edge/Devex: allocate weights array, initialize to 1.0
5. Reset statistics and invalidate all caches

**PricingContext structure extended with:**
- `num_vars` - problem size
- `strategy` - pricing strategy (0=auto, 1=partial, 2=SE, 3=Devex)
- `candidate_sizes` - allocated size per level
- `weights` - steepest edge weights array

**Key implementation pattern:**
- Helper function `compute_level_size()` for determining per-level allocation
- Proper cleanup on allocation failure (free partial allocations)
- Reinit case: free existing arrays before reallocating
- Forward declaration needed when function called before definition

---

### 2026-01-26: M6.1.4 cxf_pricing_candidates

**File created:** `src/pricing/candidates.c` (172 LOC)

**Function implemented:**
- `cxf_pricing_candidates(...)` - Select candidate entering variables

**Algorithm:**
1. Determine scan range (full or section for partial pricing)
2. Scan nonbasic variables for attractive reduced costs:
   - At lower bound: attractive if RC < -tolerance
   - At upper bound: attractive if RC > tolerance
   - Free variable: attractive if |RC| > tolerance
3. When array full, replace least attractive candidate
4. Sort candidates by |reduced_cost| descending (most attractive first)
5. Update statistics (total_candidates_scanned)

**Key design decisions:**
- Partial pricing uses 10 sections by default, cycling via last_pivot_iteration
- qsort with file-static global for reduced_costs (C limitation, not thread-safe)
- Replace-least-attractive strategy keeps best candidates when array full
- Free variable handling (status -3) included per spec

---

### 2026-01-26: M6.1.5 cxf_pricing_steepest

**File created:** `src/pricing/steepest.c` (143 LOC)

**Functions implemented:**
- `cxf_pricing_steepest(...)` - Select entering variable using SE criterion
- `cxf_pricing_compute_weight(column, num_rows)` - Compute SE weight (squared norm)

**Algorithm:**
- SE ratio = |reduced_cost| / sqrt(weight)
- Variable attractiveness based on status:
  - At lower bound (status -1): attractive if RC < -tolerance
  - At upper bound (status -2): attractive if RC > tolerance
  - Free variable (status -3): attractive if |RC| > tolerance
- Weight safeguard: weights < 1e-10 replaced with 1.0
- Statistics tracking: total_candidates_scanned updated

---

### 2026-01-26: M6.1.6-M6.1.7 Pricing Update/Phase

**Files created:**
- `src/pricing/update.c` (117 LOC) - cxf_pricing_update, cxf_pricing_invalidate
- `src/pricing/phase.c` (81 LOC) - cxf_pricing_step2 (full scan fallback)

**Key learning:**
- SE weight handling deferred until SolverContext integration
- Invalidation flags defined locally in update.c (not in header yet)

---

### 2026-01-26: M5.1.7 Basis Snapshots

**File created:** `src/basis/snapshot.c` (159 LOC)

**Functions implemented:**
- `cxf_basis_snapshot_create(basis, snapshot)` - Deep copy of basis state
- `cxf_basis_snapshot_diff(s1, s2)` - Count element-wise differences
- `cxf_basis_snapshot_equal(s1, s2)` - Returns 1 if identical (diff == 0)
- `cxf_basis_snapshot_free(snapshot)` - Free arrays and clear valid flag

**Structure added to cxf_basis.h:**
```c
typedef struct {
    int numVars, numConstrs;
    int* basisHeader;
    int* varStatus;
    int valid, iteration;
    void *L, *U;        // Reserved for future factor copies
    int* pivotPerm;
} BasisSnapshot;
```

**Tests added to test_basis.c (12 new, 41 total):**
- test_snapshot_create_copies_data
- test_snapshot_create_null_args
- test_snapshot_create_empty_basis
- test_snapshot_diff_identical (returns 0)
- test_snapshot_diff_one_header_change
- test_snapshot_diff_var_status_change
- test_snapshot_diff_dimension_mismatch (returns -1)
- test_snapshot_diff_null_args
- test_snapshot_equal_true/false
- test_snapshot_free_null_safe
- test_snapshot_free_clears_valid

**Key design decisions:**
- BasisState extended with `n` (numVars) and `iteration` fields
- Dimension mismatch in diff returns -1 (not comparable)
- NULL args in diff return -1 (error indicator)
- Free sets valid=0 to prevent use-after-free bugs

---

### 2026-01-26: M5.1.8 Basis Validation/Warm Start

**File created:** `src/basis/warm.c` (115 LOC, 244 lines with comments)

**Functions implemented:**
- `cxf_basis_validate(basis)` - Validate basis for consistency
- `cxf_basis_validate_ex(basis, flags)` - Extended validation with selective checks
- `cxf_basis_warm(basis, basic_vars, m)` - Warm start from basic variable array
- `cxf_basis_warm_snapshot(basis, snapshot)` - Warm start from BasisSnapshot

**Validation flags defined:**
```c
#define CXF_CHECK_COUNT       0x01  // Verify m basic variables
#define CXF_CHECK_BOUNDS      0x02  // Check indices in [0, n)
#define CXF_CHECK_DUPLICATES  0x04  // Check no duplicate basic vars
#define CXF_CHECK_CONSISTENCY 0x10  // Check varStatus matches basisHeader
#define CXF_CHECK_ALL         0xFF  // Run all checks
```

**Tests added to test_basis.c (15 new, 56 total):**
- Validation tests (9): valid_basis, duplicate_vars, null_arg, empty_basis, out_of_bounds, negative_index, check_count, check_all, no_flags
- Warm start tests (6): loads_basis, clears_eta_list, null_args, size_mismatch, resets_pivot_count, warm_snapshot variants

**Key design decisions:**
- Simplified validation vs full spec (uses BasisState, not SolverState)
- Out-of-bounds check: 0 <= var < n (not n + m as in full solver)
- cxf_basis_validate_ex with flags=0 returns OK immediately (no checks)
- Warm start clears eta list and resets pivots_since_refactor
- BasisSnapshot warm start copies both basisHeader and varStatus

**Spec adaptation notes:**
- Full spec has cxf_basis_validate(state, env, flags) with SolverState
- Implemented simpler version compatible with current BasisState structure
- Future work: integrate with full SolverState when simplex implemented

---

## M7: Simplex Engine (Level 5)

### 2026-01-26: M7.1.1 Simplex Tests - Basic (TDD)

**File created:** `tests/unit/test_simplex_basic.c` (273 total lines, 165 LOC)

**TDD tests written (17 total):**
- **Init tests (6):** test_simplex_init_creates_state, null_model_fails, null_stateout_fails, empty_model, primal_mode, dual_mode
- **Cleanup tests (2):** test_simplex_final_frees_state, null_safe
- **Setup tests (3):** test_simplex_setup_basic, null_state_fails, null_env_fails
- **Query tests (6):** get_status_initial, get_status_null, get_iteration_initial, get_iteration_null, get_phase_after_setup, get_phase_null

**Expected interface defined:**
```c
int cxf_simplex_init(CxfModel *model, void *warmStart, int mode,
                     double *timing, SolverContext **stateOut);
int cxf_simplex_final(SolverContext *state);
int cxf_simplex_setup(SolverContext *state, CxfEnv *env);
int cxf_simplex_get_status(SolverContext *state);
int cxf_simplex_get_iteration(SolverContext *state);
int cxf_simplex_get_phase(SolverContext *state);
```

**Key learnings:**
- Test file compiles but linker errors expected (TDD before implementation)
- Mode parameter: 0=auto, 1=primal, 2=dual
- Status codes to implement: CXF_STATUS_UNSET, CXF_STATUS_LOADED, etc.
- Phase: 1=Phase I (finding feasibility), 2=Phase II (optimizing)

---

### 2026-01-26: M5.2.1 Callbacks Tests (TDD)

**File created:** `tests/unit/test_callbacks.c` (237 total lines, ~139 LOC)

**TDD tests written (17 total):**
- **cxf_init_callback_struct (3):** zeroes_memory, null_pointer_returns_error, null_env_succeeds
- **cxf_set_terminate (3):** sets_flag, null_env_safe, idempotent
- **cxf_check_terminate (3):** returns_zero_when_clear, returns_one_when_set, null_env_returns_zero
- **cxf_callback_terminate (2):** sets_env_flag, null_model_safe
- **cxf_reset_callback_state (1):** null_env_safe
- **cxf_pre_optimize_callback (2):** null_env_returns_success, no_callback_returns_success
- **cxf_post_optimize_callback (2):** null_env_returns_success, no_callback_returns_success

**Expected interface defined (from specs):**
```c
int cxf_init_callback_struct(CxfEnv *env, void *callbackSubStruct);
void cxf_set_terminate(CxfEnv *env);
int cxf_check_terminate(CxfEnv *env);
void cxf_callback_terminate(CxfModel *model);
void cxf_reset_callback_state(CxfEnv *env);
int cxf_pre_optimize_callback(CxfModel *model);
int cxf_post_optimize_callback(CxfModel *model);
```

**Key learnings:**
- Tests compile but linker errors expected (TDD - functions not implemented)
- Follows test_logging.c and test_basis.c patterns
- Uses CxfEnv and CxfModel structures from existing headers
- cxf_init_callback_struct zeroes 48-byte substructure per spec
- Termination functions are idempotent (safe to call multiple times)
- NULL environment handling returns gracefully (0 or no-op per spec)
