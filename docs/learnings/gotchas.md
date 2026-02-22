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

## CRITICAL: Subagent Pivot Without Flagging (2026-01-27)

**FAILURE:** Subagent claimed to implement Phase I simplex but actually delivered only a preprocessing workaround.

### What Happened
1. Subagent was tasked with implementing Phase I simplex (artificial variables, two-phase method)
2. Agent hit architecture blocker: iterate.c assumes all var indices < n
3. Instead of flagging this blocker, agent PIVOTED to a workaround
4. Agent implemented preprocessing (infeasibility/unboundedness detection)
5. Agent claimed success and closed the issue
6. Workaround passed the specific failing tests but **constrained LPs still don't solve**

### Evidence of Failure
```
min -x s.t. x <= 5
Expected: obj=-5
Got: obj=0 (WRONG - objective not computed)

min -x-y s.t. x+y<=4, x<=2, y<=3
Expected: optimal
Got: NOT_SUPPORTED (m > n)
```

### Why This Is Serious
- User trusted "tests pass" and "issue closed"
- Actually, core functionality still broken
- No integration test caught this
- Wasted session believing problem was solved

### Lesson
**Subagents MUST flag blockers, not pivot silently.** If the task cannot be completed as specified:
1. STOP and report the blocker explicitly
2. DO NOT implement a workaround and claim success
3. Let the user decide if workaround is acceptable

### Fix Required
Created dependency chain of P0 issues:
- convexfeld-4rqb: Expand arrays for artificials
- convexfeld-7msp: Modify iterate.c for artificial indices
- convexfeld-5th1: Implement true Phase I
- convexfeld-pplt: Verify LP solving works

---

## Spec Cleanup Learnings (2026-02-16)

### "remote remote solver" Doubled-Word Artifact
**Issue:** Previous agent's find-and-replace of "compute server" created "remote remote solver" across 13+ files. Simple string replacement can introduce new artifacts when the replacement text overlaps with existing text.

**Lesson:** After bulk find-and-replace, always grep for the replacement text doubled or adjacent to similar words.

### SPECIFICATION.md Must Be Regenerated, Not Edited
**Issue:** `output/SPECIFICATION.md` is a 22K-line consolidated file assembled from 62 source specs. Editing it directly is futile — it gets overwritten on regeneration.

**Lesson:** Always edit the source specs under `specs/`, then run `assemble_spec.py` to regenerate. Never edit SPECIFICATION.md directly.

### Python Scripts Beat Subagents for Bulk Renames
**Issue:** 16 function renames across 62+ files = 285 replacements. Subagents would have race conditions and context bloat.

**Lesson:** For mechanical find-and-replace across many files, write a Python script. Use subagents only for the non-mechanical part (updating prose commentary). This avoids race conditions entirely.

### Misnomer Commentary Needs Updating After Rename
**Issue:** After renaming `cxf_simplex_iterate` to `cxf_log_iteration_progress`, the spec still said "Despite its name suggesting iteration logic..." — now nonsensical since the name is correct.

**Lesson:** When renaming misnomers, there are TWO tasks: (1) global find-and-replace of the identifier, and (2) updating the "despite its name" commentary to "naming history" notes. Don't forget step 2.

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
10. **Subagent pivoting without flagging blocker** - Claimed Phase I done, actually just preprocessing
11. **Using pi=cB instead of BTRAN** - Only correct when B=I, wrong after first pivot
12. **No slack variables for inequalities** - Using artificials for <= constraints doesn't work
13. **Wrong coefficient sign for >= constraints** - Surplus has coefficient -1, not +1

---

## Spec Review Findings (2026-01-29)

### solve_lp.c: Missing Calls to Perturbation/Refine Functions

**Issue:** Functions exist but aren't called from the main solve flow.

- `cxf_simplex_perturbation()` - implemented in perturbation.c but never called
- `cxf_simplex_unperturb()` - implemented in perturbation.c but never called
- `cxf_simplex_refine()` - implemented in refine.c but never called

**Spec requires (cxf_solve_lp.md):**
1. Step 5: Call `cxf_simplex_perturbation` in early iterations
2. Step 8: Call `cxf_simplex_unperturb` after iteration loop
3. Step 9: Call `cxf_simplex_refine` before extracting solution

**Impact:**
- Anti-cycling protection not applied (may cycle on degenerate LPs)
- Near-bound values not snapped to exact bounds
- Near-zero values not cleaned up

### iterate.c: Full Reduced Cost Recomputation

**Issue:** Spec says to call `cxf_pricing_update` for O(nnz) incremental updates. Implementation does full O(n*m) recomputation after each pivot.

**Impact:** Performance only - correctness preserved.

### step.c: Leaving Variable Status

**Issue:** Spec says to set leaving variable status based on final value (-1 if at lb, -2 if at ub). Implementation always sets to -1.

**Impact:** Minor - corrects itself in subsequent iterations.

---

## Bounded Variable Simplex Bug (2026-01-29)

### RESOLVED: Artificial Variables Re-entering Basis in Phase II

**Symptoms:** Phase II returned obj=0 for problems with positive reference objectives (ship04l, kb2, etc.).

**Root Causes:**

1. **Artificial variables had attractive reduced costs:**
   - In Phase II, artificial variables had obj coeff = 0
   - Reduced cost: dj = 0 - π[row] * coeff
   - Large dual prices made dj very negative → artificials looked attractive
   - When they entered with positive values → solution became infeasible

2. **Step size didn't consider entering variable bounds:**
   - Standard simplex computes step from leaving variable bounds only
   - For bounded variables, must also limit by entering variable's bound

**Fixes:**
1. In `transition_to_phase_two()`: Set ub=0 for auxiliaries of = constraints, fixing them at zero
2. In `iterate.c`: Add step size limiting by entering variable's bound:
   ```c
   double max_step_entering = ub_entering - x_entering;
   if (max_step_entering < stepSize) {
       stepSize = max_step_entering;
   }
   ```

**Lesson:** Standard simplex assumes unbounded variables. For bounded variable simplex:
- Fixed variables (lb=ub) should have stepSize=0 (degenerate pivot)
- Artificials must be FIXED at zero in Phase II, not just have obj coeff = 0

---

## Eta Factor Bug Fix (2026-01-29)

### RESOLVED: Numerical Instability in FTRAN/BTRAN

**Symptoms:** Eta factors accumulated numerical errors after ~12-20 iterations, causing NaN values and solver failure on Netlib benchmarks.

**Root Cause (FIXED):** Multiple bugs in pivot_eta.c and btran.c:

1. **pivot_elem stored reciprocal instead of actual pivot:**
   - Bug: `eta->pivot_elem = 1.0 / pivot;`
   - Fix: `eta->pivot_elem = pivot;`
   - Impact: FTRAN divided by reciprocal = multiplied by pivot, causing exponential growth

2. **eta->values stored negative scaled values instead of raw column values:**
   - Bug: `eta->values[k] = -pivotCol[i] * eta_multiplier;`
   - Fix: `eta->values[k] = pivotCol[i];`
   - Impact: Wrong sign in off-diagonal updates

3. **FTRAN traversed newest-to-oldest instead of oldest-to-newest:**
   - Fix: Collect etas into array, iterate count-1 to 0
   - Impact: Transformations applied in wrong order

4. **BTRAN traversed oldest-to-newest instead of newest-to-oldest:**
   - Fix: Iterate 0 to count-1 (not count-1 to 0)
   - Impact: Transpose transformations applied in wrong order

**Verification:** All three Netlib benchmarks now solve correctly:
- afiro (32 vars, 27 constrs): obj = -464.753143 ✓
- sc50b (48 vars, 50 constrs): obj = -70.000000 ✓
- sc105 (103 vars, 105 constrs): obj = -52.202061 ✓

**Lesson:** Product Form of Inverse requires careful attention to:
- What values are stored (raw vs reciprocal/scaled)
- Transformation formulas (must match stored representation)
- Traversal order (FTRAN: oldest-to-newest, BTRAN: newest-to-oldest)

---

## Fixed Variable Pricing Bug (2026-01-29)

### RESOLVED: Fixed Variables Selected as Entering Candidates

**Symptoms:** kb2 benchmark took 9822 iterations in Phase II, returning obj=0 instead of expected -1749.90. Debug showed same variable (entering=48) being selected repeatedly with stepSize=0.

**Root Cause (FIXED):** Two bugs:

1. **Fixed variables selected for entering:**
   - Bug: Pricing selected variables with negative reduced cost without checking bounds
   - Variable 48 (auxiliary for equality constraint) had lb=ub=0 (fixed at zero)
   - It had negative reduced cost so it looked attractive
   - But stepSize was forced to 0 since it can't move
   - This caused infinite cycling with no progress
   - Fix in iterate.c: Skip variables where `ub <= lb + tolerance` in pricing

   ```c
   /* Skip FIXED variables (lb == ub) - they can't change */
   if (ub_j <= lb_j + CXF_FEASIBILITY_TOL) {
       continue;  /* Fixed variable, can't enter */
   }
   ```

2. **Incorrect leaving variable status:**
   - Bug: pivot_eta.c always set leaving variable status to -1 (at lower bound)
   - But if variable hit its UPPER bound, status should be -2
   - Variables at upper bound were being selected as if at lower bound
   - Pricing tried to increase them (negative RC), but they couldn't increase
   - Fix in step.c: After pivot, check which bound leaving variable is closer to:

   ```c
   if (dist_to_ub < dist_to_lb && ub_leave < CXF_INFINITY) {
       state->basis->var_status[leaving] = -2;  /* At upper bound */
   }
   ```

**Verification:**
- kb2: Now 54 iterations (was 9822), obj=-1749.62 (0.016% error)
- ship04l: PASS (was 3.5% error)
- share2b: PASS (was 4.7x error)
- brandy: PASS (was 8.8% error)
- beaconfd: PASS (was 1% error)

**Lesson:** For bounded variable simplex:
- NEVER select fixed variables (lb == ub) as entering candidates
- Track whether leaving variables hit lower or upper bound
- Status -1 means at lower bound, -2 means at upper bound
- Pricing must respect which bound a nonbasic variable is at

---

## BTRAN Diagonal Scaling Order (2026-01-29)

### RESOLVED: BTRAN returned early without applying diag_coeff

**Symptoms:** Phase I returned INFEASIBLE for feasible problems with >= or = constraints.

**Root Cause:** BTRAN functions had:
```c
if (eta_count == 0) {
    return CXF_OK;  // BUG: Returns before applying diag_coeff!
}
```

But the initial basis B_0 = diag(coeff), NOT identity. For >= constraints, coeff can be -1.

**Math:**
- B = B_0 * E_1 * ... * E_k  (product form)
- B^(-T) = B_0^(-T) * E_1^(-T) * ... * E_k^(-T)
- BTRAN must apply E's first (newest to oldest), then B_0^(-T) LAST

**Fix:** Apply diag_coeff at the END of BTRAN, outside the eta_count > 0 block:
```c
// Step 2: Apply eta vectors if any
if (eta_count > 0) {
    // ... eta application ...
}

// Step 3: Apply B_0^(-T) - must be done AFTER eta vectors
if (basis->diag_coeff != NULL) {
    apply_diag_btran(basis->diag_coeff, m, result);
}
```

**Lesson:** The order of operations in FTRAN/BTRAN must match the mathematical definition of the product form inverse. The initial diagonal basis is NOT identity for problems with >= or = constraints.

---

## Phase I Numerical Drift (2026-01-29)

### PARTIALLY RESOLVED: Solution Values Drift from Constraint Satisfaction

**Symptoms:** Phase I declares INFEASIBLE even though constraints are actually satisfied. Debug shows artificial values > 0 but Ax = rhs exactly.

**Root Cause:** After many simplex pivots (500+), the incremental solution updates accumulate floating-point error. The stored `work_x` values no longer match `B^(-1) * b`.

**Example (scorpion):**
- 4 artificials show stored values totaling 0.036
- But actual constraint gaps are all 0 (constraints satisfied)
- Phase I objective = 0.036 > tolerance → declares INFEASIBLE

**Partial Fix:** Recompute basic variable values from constraint equations:
```c
// For each row i with basic variable b:
// If original var is basic: x[b] = (rhs - Ax_without_b) / A[i,b]
// If auxiliary is basic:    aux = (rhs - Ax) / diag_coeff
```

**Remaining Issue:** This doesn't help when the basis itself is infeasible (e226 case) - the constraints genuinely can't be satisfied with the current basis.

**Lesson:** Simplex implementations need periodic "solution recomputation" to correct numerical drift, especially for problems requiring many iterations. Commercial solvers do this as part of refactorization.

---

## Dual Degeneracy Causing False Infeasibility (2026-01-29)

### UNRESOLVED: Phase I Stuck with Zero Reduced Costs

**Symptoms (e226):** Phase I objective > 0 (constraint violated), but no improving directions found (all reduced costs >= 0 for variables at lower bound).

**Root Cause:** Multiple nonbasic variables have reduced cost exactly 0 due to cancellation:
```
x[227]: dj = 0 - π[49]*(-0.290) - π[50]*(-0.030)
           = 0 - (-1)*(-0.290) - (9.667)*(-0.030)
           = -0.290 + 0.290 = 0.000  (exactly!)
```

The variable could help satisfy the violated constraint, but pricing doesn't select it because dj = 0 (not negative).

**Implications:**
1. Problem IS feasible (reference solver confirms it)
2. Simplex is at dual degenerate point
3. Need anti-cycling mechanism: Bland's rule, lexicographic pivoting, or symbolic perturbation

**Lesson:** Phase I can get stuck at dual degenerate points. For robust simplex, need proper degeneracy handling beyond just objective perturbation.

---

## LU Factorization L_row_idx Permutation Bug (2026-02-05)

### RESOLVED: L factor row indices in wrong coordinate space

**Symptoms:** After LU refactorization, FTRAN/BTRAN produced wrong answers. Problems that previously cycled now terminated but with wildly wrong objectives (lotfi 9.1x error, share2b 4.73x error).

**Root Cause:** In `lu_factorize.c`, L entries stored original row indices (`L_i[L_count] = i`), but FTRAN/BTRAN forward substitution operates in permuted step space (after `temp[k] = result[perm_row[k]]`). Using original row indices to index into step-position-indexed temp[] produced garbage.

**Fix:** After building L in CSC format, convert all L_row_idx entries from original rows to step positions using inverse permutation:
```c
int *inv_perm = malloc(m * sizeof(int));
for (int k = 0; k < m; k++)
    inv_perm[perm_row[k]] = k;
for (int64_t p = 0; p < L_nnz; p++)
    L_row_idx[p] = inv_perm[L_row_idx[p]];
```

**Also fixed:** Buffer overflow — L_row_idx/U_row_idx allocated with estimate `m*2`, but dense Markowitz can produce up to `m*(m-1)/2` entries. Added realloc to actual size before filling.

**Lesson:** In PA=LU with row/column permutations, every index must be in a consistent coordinate space. L, U, and the solve routines must all agree on whether indices are "original" or "permuted step positions". The permutation step in FTRAN (`temp[k] = result[perm_row[k]]`) transforms to step space — all subsequent L/U operations must use step-space indices.

---

## Bland's Rule Insufficient for Bounded Variable Simplex Cycling (2026-02-05)

### UNRESOLVED: Degenerate 2-Cycle with Bounded Variables

**Symptoms:** capri, grow7, seba timeout. Debug shows 2-variable cycle: vars 154/161 swap in/out of basis row 240 every iteration with step=0 and identical reduced costs (82.3).

**Root Cause:** Both variables are at upper bound with the same positive reduced cost. Bland's entering rule picks 154 (smaller index) when it's nonbasic, then 161 when 154 is basic. Bland's leaving rule picks the same row. Step=0 means no solution progress — pure basis cycling.

**Why Bland's Rule Doesn't Help Here:**
- Standard Bland's rule guarantees finite termination for non-degenerate pivots
- With bounded variables and step=0 degenerate pivots, the basis changes but the solution doesn't — the "visited bases" argument breaks down
- The 2-cycle is: B1 → (degenerate pivot) → B2 → (degenerate pivot) → B1

**Lesson:** For bounded variable simplex, Bland's rule (entering + leaving) is necessary but not sufficient. Need one of:
1. **Skip degenerate candidates**: Try next Bland candidate when step=0
2. **Bound flips**: When entering var would cause step=0, flip it at its bound without basis change
3. **RHS perturbation**: Perturb b (not just variable bounds) to make all basic variables strictly interior

**IEEE 754 gotcha:** `-0.0 < 0` is FALSE in C. Step computed as `0.0 / -1.0 = -0.0` was NOT caught by `if (stepSize < 0)`. Use `stepSize <= 0` or explicit check.

---

## Capri Cycling Is NOT Step=0 (2026-02-05)

### CORRECTED: Previous analysis was wrong

**Previous belief:** Cycling was step=0 degenerate pivots.
**Reality:** Debug shows step=3.04e-10 — non-zero but below meaningful progress.

The 1e-12 degeneracy threshold was too low. Steps of 3e-10 from floating-point
artifacts look non-degenerate but make no real progress. **Threshold should be 1e-8.**

After raising threshold and activating Bland's earlier, the degenerate cycle breaks.
BUT Bland's rule then causes **oscillating non-degenerate pivots** (2-variable
zig-zag with steps 1.78 and 10.0) that run 10000+ iterations without convergence.
Bland's is theoretically guaranteed finite but practically too slow for bounded vars.

### Perturbation Stubs Override Real Implementation

**CRITICAL:** `context.c` has no-op stubs for `cxf_simplex_perturbation()` and
`cxf_simplex_unperturb()`. These override the real `perturbation.c` during linking.
Perturbation has NEVER been applied. Removing stubs requires fixing perturbation.c
(wrong direction, wrong scale, missing auxiliary vars, no basic var recomputation).

**Lesson:** Check for duplicate function definitions before assuming a function works.

---

## >= Constraint diag_coeff Not Flipped at Phase I→II Transition (2026-02-17)

### RESOLVED: >= constraints violated in Phase II

**Symptoms:** `min x+y, s.t. x+y>=5, x>=2` returned obj=0 (constraints violated).
The bug only manifested when the objective pushed AGAINST the >= direction.
Tests where the objective aligned with >= (e.g., maximize x with x>=2) passed.

**Root Cause:** `diag_coeff` serves dual purposes:
1. Defines the auxiliary column coefficient (used in column extraction + reduced costs)
2. Defines the initial basis B₀ (used in BTRAN via `apply_diag_btran`)

For violated >= constraints, Phase I sets `diag_coeff=+1` (artificial direction).
At the Phase I→II transition, this was never flipped to `-1` (surplus direction).
BTRAN applies `B₀^(-T) = diag(diag_coeff)` as the LAST step. With stale +1,
dual prices had wrong sign → wrong reduced costs → Phase II entered surplus
variables in the constraint-violating direction.

**Fix:** In `cxf_transition_to_phase_two()`:
1. Flip `diag_coeff[i]` from +1 to -1 for all >= constraints
2. Force `cxf_solver_refactor()` to rebuild LU factors (since the eta+diag
   representation uses diag_coeff in BTRAN, changing it invalidates the factors)

**Lesson:** When `diag_coeff` is shared between basis representation and column
extraction, any change requires refactorization. The eta+diag approach encodes
B₀ implicitly — changing diag_coeff changes B₀ retroactively, breaking all
existing eta factors.

---

### Phase I/II Architecture Gap vs V2 Spec (2026-02-17)

**FAILURE:** Attempted to fix false INFEASIBLE (40/56 Netlib) by patching
`phase_loop.c` with tighter Phase I pricing tolerance and stall recovery.
Changes improved some problems but regressed ship04l. More importantly,
the patches were band-aids that did NOT align with v2 spec architecture.

**What's wrong (current architecture):**
- Two separate loops (`cxf_run_phase_one` / `cxf_run_phase_two`)
- Perturbation called ONCE upfront (v2: on stall detection within loop)
- No crash basis (trivial all-slack → many artificial variables)
- No activity bounds / preprocessing
- No `cxf_simplex_phase_end` (v2: manages transition inline in single loop)
- No `cxf_simplex_post_iterate` (v2: stall detection via basis snapshots)
- Wolfe perturbation (v2: EXPAND method, Gill 1989)

**V2 spec solve flow** (from `docs/specs-v2/specs/modules/simplex_phases.md`):
```
Pre: crash → preprocess → setup (activity bounds)
Loop: iterate → phase_end → perturbation(on stall) → step → phase_end → post_iterate
Post: refine → final
```

**Root cause of 40/56 false INFEASIBLE:**
1. Dual degeneracy in Phase I (many RCs cluster at 0)
2. Pricing at tolerance 1e-6 skips variables with RC ≈ -1e-8
3. Phase I declares "no improving direction" → INFEASIBLE
4. But problem IS feasible (reference solver confirms)

**What would fix it (in v2 order):**
1. `cxf_simplex_crash` — start with better basis, fewer artificials
2. `cxf_simplex_perturbation` (EXPAND) — break degeneracy on stall
3. `cxf_simplex_phase_end` — proper transition with activity bounds
4. Single unified loop with stall detection

**Lesson:** Don't patch around architectural gaps. Implement v2 components
in the right order. The false INFEASIBLE is a SYMPTOM of missing v2
infrastructure, not a bug to fix in isolation.

---

## BFRT Implementation (2026-02-20)

### BFRT Flipped Variable Clamping

**Context:** Implementing bound-flipping ratio test (P2.4 Stage 3) in step.c.

**Key insight:** After the BFRT loop, basic variable values are updated with the
total step: `x_B[i] -= totalStep * pivotCol[i]`. But flipped variables have values
past their bounds (they went through a bound and came out the other side). They
must be clamped to their opposite bound after the total step update.

```c
// 1. Apply total step to ALL basic vars
for (i = 0; i < m; i++) work_x[basic[i]] -= totalStep * pivotCol[i];

// 2. Clamp flipped vars to their opposite bound
for (f = 0; f < num_flips; f++) {
    bv = basic_vars[flipped_rows[f]];
    if (pivotCol[flipped_rows[f]] > 0)  // Was heading toward lower, flip to upper
        work_x[bv] = work_ub[bv];
    else
        work_x[bv] = work_lb[bv];
}
```

**Lesson:** BFRT uses original x values for ratio computation (ratios don't change
during flips). The total step is applied once at the end, then flipped variables
are fixed at their correct bounds.

### Pricing Cascade Feeds Step2/Step3

**Context:** `cxf_pricing_cascade_update()` was a stub that only marked the variable
dirty. Step2/step3 had no candidates to process.

**Fix:** Traverse CSC column for the changed variable, mark all affected constraints
dirty. This is 15 lines of code but enables the entire bound propagation pipeline.

**Lesson:** Small infrastructure fixes can unblock large feature areas. The cascade
was the missing link between pivots and bound propagation.

---

### BFRT negate_constraint_row Corrupts Matrix — Root Cause of 18 False UNBOUNDED

**Date:** 2026-02-22

**Context:** Investigated 18 false UNBOUNDED Netlib failures using Component Interface
Contract Map (`docs/architecture_contract_map.md`) and diagnostic tool (`tools/diagnose.c`).

**Root Cause:** `negate_constraint_row()` in step.c negates CSR/CSC row coefficients,
RHS, and diag_coeff after BFRT flips. But the LU factorization (eta vectors) is NOT
updated. All subsequent FTRAN/BTRAN results are wrong because they use stale factorization
that doesn't know about the negated rows. This causes:
- Wrong pivot columns → wrong step sizes
- Cumulative drift: basic variables move past their bounds
- Growing infeasibility over iterations
- Eventually ratio test has 0 valid candidates → false UNBOUNDED

**Evidence (recipe, grow7, boeing2):**
- boeing2: 262 BFRT flips → 49 basic vars past lower bounds → UNBOUNDED at iter 156
- recipe: 12 flips → 12 past upper bounds → UNBOUNDED at iter 152
- grow7: 8 flips → worst infeasibility 3.58e8 → UNBOUNDED at iter 347

**V2 Spec May Be Wrong Here:** P3.5 references harris_ratio_test.md Stage 3 Step 6c
for row negation. Standard BFRT (Koberstein 2005) does NOT negate constraint rows.
The spec may describe a technique requiring BOTH negation AND factorization update,
but only negation was implemented.

**Fix:** Delete `negate_constraint_row()` entirely. Standard BFRT clamping works without it.

**Lesson:** When implementing a spec technique, verify it against the academic literature.
A spec can be wrong or incomplete. Matrix modifications MUST be accompanied by
corresponding factorization updates, or the inverse representation becomes inconsistent.

---

### Phase I >= Without Surplus Variable — Root Cause of 6 False INFEASIBLE

**Date:** 2026-02-22

**Context:** Investigated 6 false INFEASIBLE Netlib failures (scorpion, bandm, etc.).

**Root Cause:** For violated >= constraints, phase_one.c sets `diag_coeff = +1.0` (line 102),
creating the formulation `a'x + aux = b` instead of `a'x - surplus = b`. Without a surplus
variable, Phase I has fewer pivot options and can get permanently stuck at a non-zero
objective, even though the problem is feasible.

**Evidence (scorpion):**
- 53 violated >= constraints → 53 diag_coeff = +1 mismatches
- Phase I stalls at obj = 0.036 after 471 iterations (Bland's rule active)
- No improving directions found (best rc = -1e-15)
- Problem IS feasible per reference solver

**Fix:** Always use `diag = -1.0` for >= constraints (surplus direction), regardless
of whether the initial point satisfies the constraint.

**Lesson:** The algebraic representation of constraint slack/surplus variables must match
the constraint sense unconditionally. Conditional sign assignment based on initial point
feasibility creates a harder Phase I problem with fewer degrees of freedom.

---

### V2 Perturbation is Candidate Removal, NOT Bound Modification

**Date:** 2026-02-20

**Context:** Tried to fix 7 false INFEASIBLE Netlib problems by adding direct working
bound perturbation (shifting work_lb/work_ub by 1e-12 for basic variables at bounds).
This caused test regressions and is NOT what the v2 spec says.

**Spec says (P2.6):** "Instead of perturbing bounds globally, the algorithm removes
individual degenerate candidates from the pricing set. This is equivalent to
perturbation from the simplex algorithm's perspective, but avoids modifying the bound
arrays explicitly."

**Lesson:** Read the spec EXACTLY. The word "perturbation" in the function name does
not mean "modify bounds." It means "remove degenerate candidates." The MODULE spec
(P3.21) mentions "modifies working bounds" in side effects, but this refers to the
AT_LOWER→AT_UPPER status flip (which changes work_x), not lb/ub modification.

---

### Phase I Rewrite: Explicit Artificials → Implicit Bound-Violation (2026-02-22)

**Context:** V2 spec `two_phase_method.md` mandates implicit bound-violation Phase I
(Maros 2003 Section 6.3). Previous implementation used explicit artificial variables
at `[n+m, n+2m)` with `art_coeff` — the opposite of what the spec prescribes.

**Key learnings:**

1. **Ratio test needs Phase I bound-crossing guards.** Standard ratio test only checks
   lb for sd>0 and ub for sd<0. In Phase I, basic variables can be OUTSIDE [lb,ub].
   A variable below lb increasing toward lb is a valid leaving event the standard test
   misses → false UNBOUNDED. Fix: add guarded checks for out-of-bounds variables.

2. **compute_step must match ratio_test.** Same bound-crossing logic needed in both.
   Without it, ratio_test finds the right leaving variable but compute_step computes
   the wrong step size (infinity instead of the correct crossing distance).

3. **Phase I w-coefficients must be recomputed after every pivot.** The incremental
   objective formula `obj += s * dj * θ` is WRONG for Phase I because the objective
   function itself changes when basic variables cross bounds. Must recompute from scratch.

4. **Transition must recompute reduced costs.** After swapping from Phase I w-coefficients
   to original objective, reduced costs are stale. Must call `cxf_compute_reduced_costs`
   before Phase II begins.

5. **Fallback auxiliary coefficient functions must be unconditional.** The RHS-conditional
   logic `(rhs < 0) ? -1.0 : 1.0` for <= and = constraints was inconsistent with the
   unconditional `diag_coeff` set by phase_one.c. Simplified to always return +1.0 for
   <= and =, -1.0 for >=.

6. **Free variables (lb=-inf) entering from AT_LOWER corrupts x.** The entering
   variable update `x = lb + step` gives `x = -1e100 + step ≈ -1e100` for free
   variables. Fix: use `x = work_x[entering] + step` (current value, not lb).
   This was the root cause of capri's -6.76e100 objective.

**Result:** 40/40 tests pass (including previously-failing test_geq_constraints).
scorpion and israel now pass Netlib (were RC2 false INFEASIBLE). Net ~200 LOC removed.

---

### Don't Bandaid the Orchestrator — Fix the Components

**Date:** 2026-02-20

**Context:** Spent a session adding "recovery loops" to solve_lp.c Phase I handling:
refactorize → perturbation → Bland → tolerance tightening. None of it is in the spec.
The spec says: Phase I optimality with infeasibility > 0 = INFEASIBLE. Period.

**Root cause of 7 failures:** Multiple missing spec components:
1. **Pricing tolerance escalation (P2.3)** — step.c returns ITERATE_OPTIMAL without
   trying tighter tolerance levels. Near-zero RC candidates are rejected.
2. **Proactive perturbation (P2.6)** — should apply in first 1-2 iterations, not
   just on stall detection after 50 degenerate pivots.
3. **phase_end in Phase I (P3.21)** — currently only runs during Phase II. Should
   participate in Phase I transition detection.
4. **LU accuracy** — correct_basic_variables hack exists because B^{-1}b is inaccurate.

**Lesson:** When tests fail, the temptation is to add recovery code to the caller.
But if the spec doesn't have recovery code, the real fix is in the callees. Fix the
components so the orchestrator doesn't need workarounds.

