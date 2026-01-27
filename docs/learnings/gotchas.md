# Gotchas, Failures, and Things to Avoid

This file captures mistakes made and lessons learned the hard way.

---

## Critical Failures

### Wrong Language (C99 vs Rust)
**FAILURE:** Implementation plan written for Rust instead of C99.

- The PRD explicitly states C99
- Agent failed to read the PRD carefully
- Entire implementation plan had wrong file structure, syntax, tooling
- Required full rewrite (3 agent resets)

**Lesson:** ALWAYS verify the target language from PRD before writing ANY implementation details.

---

### Jumped to Implementation Without Creating Issues
**FAILURE:** Agent started writing code instead of creating beads issues.

- The task was ONLY to create beads issues for each step in the plan
- Agent misread the workflow and started implementing M0
- Created ~10 files that had to be deleted
- User had to intervene forcefully

**Lesson:** After a plan is complete, the NEXT STEP is to create beads issues for tracking. DO NOT START IMPLEMENTING until issues exist for every step.

**Correct Workflow:**
1. Plan is written ✓
2. Create beads issues for EVERY step in the plan ← NEXT STEP
3. THEN start implementing (claiming issues one at a time)

---

### Not Reading HANDOFF.md First
**FAILURE:** Agents repeated same mistakes from previous sessions.

**Lesson:** ALWAYS read HANDOFF.md first. It contains:
- What work was completed
- Current state of the project
- What the next agent should do
- Known issues and blockers

---

## C99 Gotchas

### Error Code Names
**Wrong:** `CXF_ERROR_MEMORY`
**Right:** `CXF_ERROR_OUT_OF_MEMORY`

Always check `cxf_types.h` for correct error code names.

---

### POSIX Time Functions
**Problem:** `clock_gettime(CLOCK_MONOTONIC)` requires a feature macro.

```c
/* MUST be at the top of the file, before any includes */
#define _POSIX_C_SOURCE 199309L

#include <time.h>

double cxf_get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
```

---

### NaN/Inf Detection
**Wrong approach:**
```c
/* This fails for DBL_MAX (~1.8e308) */
if (val > 1e308) return 1;  /* BAD */
```

**Right approach:**
```c
#include <math.h>

/* Use isfinite() for NaN/Inf detection */
if (!isfinite(val)) return 1;

/* Or for NaN specifically: val != val is portable */
if (val != val) return 1;  /* NaN only */
```

---

### CMake Static Library Requires Source
**Problem:** CMake STATIC libraries require at least one source file.

**Solution:** Create `src/placeholder.c` as a workaround until real modules added.

---

### Header Guards
**Always include header guards in .h files:**
```c
#ifndef CXF_TYPES_H
#define CXF_TYPES_H

/* ... header content ... */

#endif /* CXF_TYPES_H */
```

---

### Forward Declarations
Required for circular struct references. Put in `cxf_types.h`:
```c
typedef struct CxfEnv CxfEnv;
typedef struct CxfModel CxfModel;
/* etc. */
```

---

### Magic Number Types
Use `uint32_t` for 32-bit magic, `uint64_t` for 64-bit:
```c
#define CXF_ENV_MAGIC 0xC0FEFE1D              /* 32-bit */
#define CXF_CALLBACK_MAGIC2 0xF1E1D5AFE7E57A7EULL  /* 64-bit */
```

---

## Build System Gotchas

### CMake Version
**Requires CMake 3.16+** for modern C99 support and Unity integration.

---

### Unity Test Framework Warnings
Unity's own code has `-Wdouble-promotion` warnings (float→double). This is expected from third-party code and doesn't affect functionality.

---

### Include Order
Standard headers first, then project headers:
```c
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
```

---

## Design Gotchas

### Spec vs Implementation Staging
**Lesson:** Specs describe the full-featured version, but implementation can be staged.

Example: Memory functions spec describes full environment tracking, but that requires threading infrastructure. Implement simple working version first, add features as dependencies are built.

---

### qsort with Global State
C's `qsort` doesn't support user data, so comparison functions need global state:
```c
/* File-static global - NOT thread-safe */
static const double *g_reduced_costs = NULL;

static int compare_by_rc(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (fabs(g_reduced_costs[ib]) > fabs(g_reduced_costs[ia])) ? 1 : -1;
}
```

**Note:** This is a C limitation. For thread safety, would need qsort_r (BSD/glibc extension).

---

### Singly-Linked List Reverse Traversal
When you only have `next` pointers but need reverse order, collect pointers into an array first:
```c
/* Stack allocation for small cases, heap for large */
#define MAX_STACK_ETAS 64
```

---

## Code Review Findings (2026-01-27)

### Critical Bugs Found

**model->matrix Never Allocated**
```c
/* BUG in cxf_newmodel() - matrix field declared but never allocated */
model->matrix = NULL;  /* Will crash when accessed */

/* FIX: Add allocation */
model->matrix = cxf_sparse_create(0, 0);
```

**O(n²) Duplicate Detection**
```c
/* BUG: O(n²) nested loop for duplicate detection */
for (int i = 0; i < m; i++) {
    for (int j = i + 1; j < m; j++) {  /* O(m²)! */
        if (basic_vars[i] == basic_vars[j]) return ERROR;
    }
}

/* FIX: Use seen array for O(n) */
int *seen = calloc(n, sizeof(int));
for (int i = 0; i < m; i++) {
    if (seen[basic_vars[i]]) return ERROR;
    seen[basic_vars[i]] = 1;
}
```

**Thread-Safety Violation in qsort**
```c
/* BUG: Global variable for qsort comparison - not thread-safe */
static const double *g_reduced_costs = NULL;  /* Race condition! */

/* FIX: Use custom sort or qsort_r */
```

**Stub Returns Wrong Answers**
```c
/* BUG: Stub claims OPTIMAL but ignores constraints entirely */
model->status = CXF_STATUS_OPTIMAL;  /* WRONG - haven't solved anything */
return CXF_OK;

/* FIX: Return error until implemented */
return CXF_ERROR_NOT_IMPLEMENTED;
```

---

### Code Quality Issues

**Comment Bloat (Linus Review)**
- 40% of comments are noise explaining what malloc/calloc does
- Delete comments that state the obvious
- Keep comments that explain WHY, not WHAT

**No-Op Functions "For Future Extensibility"**
```c
/* BAD: Don't write functions that do nothing */
int cxf_finalize_row_data(SparseMatrix *mat) {
    if (mat == NULL) return CXF_ERROR_NULL_ARGUMENT;
    /* Currently a no-op */
    return CXF_OK;
}
/* Delete until actually needed */
```

**Theatrical NULL-ing Before Free**
```c
/* POINTLESS: Setting fields to NULL before freeing structure */
ctx->field1 = NULL;
ctx->field2 = NULL;
free(ctx);  /* Memory is gone - NULLing helps nothing */
```

**Magic Numbers Without Documentation**
```c
/* BAD: Where did 16 come from? */
#define INSERTION_THRESHOLD 16

/* GOOD: Document the source */
#define INSERTION_THRESHOLD 16  /* Benchmark: insertion beats qsort below this */
```

---

## Spec Compliance Audit Findings (2026-01-27)

### Function Name Collisions
**Problem:** `cxf_pivot_check` in spec vs implementation were completely different functions.
- Spec: Constraint propagation for bound computation
- Impl: Simple pivot element validation (NaN/magnitude)

**Fix:** Renamed implementation to `cxf_validate_pivot_element` to free the name.

**Lesson:** Before implementing a spec function, check if a function with that name already exists doing something else.

---

### Callback Signature Evolution
**Problem:** Spec had 4-parameter callback, impl had 2.
- Spec: `callback(model, cbdata, where, usrdata)`
- Impl: `callback(model, usrdata)`

**Fix:** Updated to 4-param signature with WHERE constants.

**Lesson:** When specs and impl diverge, decide which is correct and update the other. Don't leave them inconsistent.

---

### Stubs That Accept But Don't Store
**Problem:** `cxf_addconstr` validated constraints but didn't store them.
- Function returns CXF_OK (success)
- Constraint data is validated
- But nothing is stored in the matrix
- Tests pass because they check return value, not actual storage

**Impact:** LP solver can't solve constrained problems - THE blocker for real LP solving.

**Lesson:** A stub that validates but doesn't store is worse than one that returns NOT_IMPLEMENTED. Users think it works.

---

### Parallel Subagent Coordination
**Success:** Spawning 6 parallel subagents to work on different modules.

**Key:** Tell each agent:
1. Which files they CAN modify (isolated set)
2. NOT to run git commit
3. Handle commits centrally after all complete

This avoids git race conditions while maximizing parallelism.

---

## Things That Didn't Work

1. **Writing implementation plan in Rust when spec says C99** - Major failure
2. **Not reading HANDOFF.md first** - Repeated same mistakes
3. **Not consulting PRD for language requirement** - Root cause of Rust error
4. **Simple `val > 1e308` check for infinity** - Fails for DBL_MAX
5. **Starting implementation without creating tracking issues** - Lost work context
6. **Returning OPTIMAL status from stub** - Wrong answers worse than errors
7. **Nested loops for duplicate detection** - O(n²) when O(n) is trivial
8. **Global state for qsort comparisons** - Thread-safety violation
9. **Stubs that validate but don't store** - Users think it works when it doesn't
