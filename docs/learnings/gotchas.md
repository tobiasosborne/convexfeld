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
- `cxf_simplex_phase_end` implemented in post.c (called pre+post pivot per spec)
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

---

### Per-Pivot Bound Snap Causes Butterfly Effect (2026-02-22)

**Context:** Adding `state->work_x[leaving] = lb` (snap leaving variable to exact
bound) at every pivot in `cxf_apply_pivot`.

**Result:** etamacro gained PASS, BUT scfxm1 regressed from PASS to FAIL (0.13% error).
The tiny per-pivot snap (~1e-15 change) altered the iteration path enough to reach a
different (slightly suboptimal) vertex on scfxm1.

**Fix:** Snap only at refactorization points (in `cxf_recompute_xB`), not at every pivot.
This gives the accuracy benefit without butterfly-effect regressions.

**Lesson:** Modifications to primal values at every iteration have outsized effects
because they compound over hundreds of pivots. Prefer infrequent batch corrections
(at refactorization) over per-iteration adjustments.

---

### Phase I cxf_recompute_objective Must Update work_obj[] (2026-02-22)

**Context:** Added x_B recomputation after refactorization. Phase I objective
recomputation only updated `obj_value`, not `work_obj[]` (w-coefficients).

**Result:** boeing1 regressed from OPTIMAL (176% error) to INFEASIBLE. After
x_B recomputation, the w-coefficients were stale, so `cxf_compute_reduced_costs`
used wrong objective → wrong pricing → Phase I failed.

**Fix:** In `cxf_recompute_objective` for Phase I, reset ALL `work_obj[j] = 0`
then recompute w-coefficients: `work_obj[bv] = -1` if below lb, `+1` if above ub.

**Lesson:** Any function that recomputes Phase I state must update BOTH `obj_value`
AND `work_obj[]`. They are tightly coupled in Phase I but independent in Phase II.

---

### Forced Refactorize + Recompute at Phase I "Optimal" Unblocks Infeasible (2026-02-22)

**Context:** capri declared INFEASIBLE with Phase I obj=12.3 (99.96% reduced but
nonzero). No improving directions found.

**Fix:** Before returning INFEASIBLE, force refactorization + x_B recomputation +
RC recomputation. The fresh factorization reveals improving directions hidden by
accumulated drift. capri now reaches OPTIMAL (10% error).

**Lesson:** "No improving direction" during Phase I may be a numerical artifact.
Always refactorize + recompute before declaring Phase I infeasibility.

---

### RHS-Dependent diag_coeff Causes Regressions (2026-02-22)

**Context:** scsd1 has an all-equality problem where Phase I gets permanently stuck
with one infeasible slack on a row with negative RHS. The diagnostic tool flags
`DIAG_MISMATCH: diag=1.0 expected=-1.0` for this row.

**Attempted fix:** Make diag_coeff RHS-dependent: `(rhs < 0) ? -1.0 : 1.0` for
= and <= constraints.

**Result:** israel regressed from PASS to 9.4% error, stair regressed to INFEASIBLE,
e226 went from correct to -31.5 (was -18.75). Three regressions for one potential fix.

**Why it fails:** Changing diag_coeff alters the entire algebraic structure — column
extraction, BTRAN, LU factorization, reduced costs. The code assumes diag_coeff is
a property of constraint SENSE, not RHS value. Making it RHS-dependent creates
inconsistencies throughout the solver.

**Correct approach for scsd1:** The issue is Phase I degeneracy on all-equality
problems, not the diag_coeff sign. Need forced refactorization during long
degenerate runs to prevent numerical blowup (RCs grow to 1e9, false UNBOUNDED).

**Lesson:** Don't change a fundamental algebraic convention (diag_coeff = f(sense))
to fix one problem's Phase I convergence. The convention is used throughout the
solver and changing it has far-reaching effects.

---

### e226 Reference CSV Was Wrong (2026-02-22)

The reference CSV had -11.63892907 for e226, but this value comes from the
PRESOLVED version (p_e226.mps) which adds 229 upper bound constraints. The
original e226.mps has NO BOUNDS section. The correct optimal for the original
is -18.7519290660 (confirmed by spec oracle and our solver). Fixed the CSV.

**Lesson:** Always verify reference values against spec oracles and the actual
MPS file structure. A reference from a different problem variant is worse than
no reference.

---

### cxf_simplex_refine Destroys Optimal Solution (2026-02-22)

**Context:** recipe solves correctly (obj=-266.616) but the post-solve
`cxf_simplex_refine` changes it to -268.636 (wrong).

**Root cause:** Three destructive operations in refine.c:
1. **Pass 1:** RC-based status reassignment moved nonbasic vars to different
   bounds without updating basic vars → violated Ax=b
2. **Pass 2:** Recovery pivots for basic vars near ub changed the basis →
   moved to suboptimal vertex
3. **Pass 3:** Partial objective recomputation (j<n only) inconsistent with
   the full recomputation in the final accuracy pass

**Fix:** Refine Pass 1 now only snaps to current status bound. Pass 2 (recovery
pivots) and Pass 3 (partial obj recompute) disabled. Final accuracy pass
(refactorize + recompute_xB + recompute_obj) handles everything correctly.

**Result:** Recipe error 0.76% → 0.02%.

**Lesson:** Post-solve operations that modify the basis or variable positions
MUST maintain constraint consistency. Moving nonbasic vars without adjusting
basic vars violates Ax=b. Use the diagnostic tool (diagnose.c) to compare
iteration-level behavior with the actual solver.

---

### Column-Only Scaling Doesn't Work With Implicit Slack Representation (2026-02-22)

**Context:** Implemented Ruiz equilibration (column-only, no row scaling) to
reduce coefficient range. Scaling was correct algorithmically but caused
massive regressions (israel: -8.97e5 → -6.57e8, etamacro: 198394, etc.).

**Root cause:** The solver's slack/surplus variables use diag_coeff (±1) as
their column coefficients in the constraint matrix. These are implicit — not
in the CSC matrix, but used in column extraction, FTRAN diagonal basis, and
BTRAN. Column scaling changes the CSC values (structural columns) but NOT
diag_coeff, creating an inconsistency: the scaled constraint system has
scaled structural columns but unscaled slack columns.

**Why it breaks:** When a structural variable enters the basis and a slack
leaves, the FTRAN/BTRAN operates on a mixed-scale matrix. The basis B has
some columns from A_scaled and some from diag (unscaled). The LU
factorization of this mixed-scale basis is inaccurate.

**What would fix it:** Full scaling requires either:
1. Generalize diag_coeff to non-±1 values (scale by D_r), updating all
   places that assume ±1 (FTRAN diagonal fallback, BTRAN apply_diag_btran,
   column extraction, Phase I w-coefficient computation)
2. OR: scale the model BEFORE building the solver state, so CSC and
   diag_coeff are both in the scaled domain from the start

**Lesson:** Matrix scaling in a simplex solver is not a simple pre/post
transformation. The implicit slack representation tightly couples the
constraint matrix to the basis operations. Scaling must be applied
consistently to ALL matrix elements, including implicit ones.

---

### Full Row+Column Scaling Is Mathematically Correct But Solver-Limited (2026-02-22)

**Context:** Implemented full row+column Ruiz equilibration per matrix_finalization.md
Strategy 3. Fixed FTRAN/BTRAN fallback paths (`*=` → `/=` for non-±1 diag_coeff).
Set `diag_coeff[i] = D_r[i] * (±1)` so slack columns scale consistently.

**Result:** Scaling is mathematically correct (objective invariant, bounds transform
correctly, FTRAN/BTRAN division handles non-±1 diag). BUT enabling scaling
regresses every problem tested:
- blend: PASS → UNBOUNDED (scaling changes iteration path to degenerate basis)
- stair: PASS → INFEASIBLE (same)
- boeing1: 18% → 41% error (worse)
- grow7: 12.5% → 15% error (worse)

**Root cause:** The solver is not robust enough to handle the different iteration
path that scaling creates. Scaling changes which variables get selected by pricing,
which pivots are degenerate, and which bases are visited. Problems that were
barely passing on the boundary of convergence get pushed over the edge.

**Key finding:** Row norms MUST include the implicit slack coefficient (diag_coeff)
to prevent over-scaling rows where the slack dominates. Without this, rows with
small structural coefficients but a unit slack get over-scaled.

**What was shipped:**
1. FTRAN/BTRAN `/=` fix (backward-compatible, correct for non-±1 diag_coeff)
2. Full scaling infrastructure (SolverState fields, scaling.c, Phase I/II integration)
3. Scaling DISABLED (threshold=1e30) pending solver robustness improvements

**What's needed to enable scaling:**
- Anti-degeneracy: EXPAND perturbation must handle scaled problems
- Ratio test: needs fallback when no blockers found (instead of UNBOUNDED)
- Phase I: needs more robust convergence on scaled problems
- Refactorization: may need more frequent refactoring after scaling

**Lesson:** Scaling is a SYSTEMS problem, not an algorithm problem. The scaling
math is simple (D_r * A * D_c). The hard part is that every downstream component
(pricing, ratio test, Phase I, perturbation, refine) must be robust enough to
handle the changed numerical landscape. Don't enable scaling until the solver
passes at least 30/35 Netlib without it.

---

### Inactive Constraint Slack Must Use RHS (2026-02-25)

**Context:** `cxf_simplex_phase_end` constraint cleanup computed slack as
`-max_activity`. Activity bounds don't include RHS (they're just `sum a_j x_j`
range). This made `slack = -max(a^T x)`, which is almost always negative for
non-trivial problems, so the constraint cleanup was a no-op.

**Fix:** Use `rhs - max_activity` for `<=` constraints, `min_activity - rhs`
for `>=` constraints. Skip `=` constraints (always active). Also removed
`cols_eliminated++` from the cleanup path — constraint identification is not
column elimination and was corrupting stall detection thresholds.

**Also:** Added `cxf_compute_activity_bounds(state, 0, NULL)` to
`cxf_transition_to_phase_two`. Phase I perturbation and bound propagation
leave activity bounds stale; fresh computation at transition ensures the
first Phase II phase_end call operates on accurate data.

**Lesson:** When functions compute derived quantities (like slack), verify
the formula accounts for ALL inputs. The activity bounds were relative to
the origin, not relative to the RHS. A formula like `slack = -max_a` only
works if RHS is already embedded in the activity bounds (which it wasn't).

---

### col_nz_count Initialization Order Bug (2026-02-26)

**Context:** `col_nz_count` (column nonzero counts for crash candidate removal)
was populated from `ctx->csc_col_ptr` at context.c:172, but the CSC copy that
creates `ctx->csc_col_ptr` happened at context.c:193 — AFTER the population.
Result: `col_nz_count[j]` was always 0 for all structural variables.

**Impact:** Any code reading `col_nz_count` got zeros. The crash candidate
removal path (which decrements these counts) would have produced negative
counts. Phase I Step 3 (structural swap) was dead code because it checked
`col_nz_count[j] != 1` — always true when everything is 0.

**Fix:** Move `col_nz_count` population after the CSC copy block.

**Lesson:** When initialization has ordering dependencies (array A populated
from array B), verify B exists before reading it. `calloc` returns zeros,
masking the bug — the code "worked" with all-zero counts.

---

### Phase I Step 3 Structural Swap is Non-Spec (2026-02-26)

**Context:** phase_one.c had a Step 3 that searched for singleton structural
variables to swap into the basis for infeasible slack rows. This code was
dead (col_nz_count always 0) and was removed.

**V2 spec (crash_basis.md line 176):** "Unlike more aggressive crash procedures
that attempt to insert structural (non-slack) variables into the basis, this
variant constructs a basis entirely from slack variables."

**When activated** (by fixing col_nz_count), Step 3 caused regressions on
test_geq_constraints and test_mixed_senses. The value computation
`x_j = rhs / a_ij` is only correct for singletons but the basis change
invalidates the LU factorization state that hasn't been computed yet.

**Lesson:** Dead code that was never tested is a liability. When fixing a bug
that activates dead code, verify the dead code against the spec BEFORE
assuming it's correct. Removing non-spec dead code is better than debugging it.

### Dense Phase Single-Elim-Array Bug (Sparse LU, 2026-02-27)

**FAILURE:** Dense phase of sparse LU used one `elim[]` array for both rows
AND columns. Setting `elim[piv_row]=1` also blocked column `piv_row`. This
only manifests when piv_row != piv_col (non-diagonal pivots), so identity-like
bases passed but real problems failed silently.

**Symptom:** test_constraint_satisfaction mixed_senses: obj=6.0 instead of -15.0.
No memory errors (valgrind clean). Diagnosis took significant effort because the
factorization roundtrip tests all passed (they only tested small matrices where
piv_row == piv_col).

**Fix:** Separate `d_relim[]` and `d_celim[]` arrays.

**Lesson:** In Gaussian elimination, row elimination and column elimination are
INDEPENDENT operations. Never conflate them in a single array. The old dense
code (before the sparse rewrite) correctly used separate `row_elim`/`col_elim`.

### Sparse Elimination Count Maintenance (2026-02-27)

**FAILURE:** `sparse_eliminate` decremented `row_count`/`total_nnz` on
cancellation without checking if the old value was already dead (below
MIN_PIVOT). Also didn't increment counts when a dead entry was "revived"
by fill-in from a different elimination step.

**Fix:** Check `fabs(old_val) >= MIN_PIVOT` before updating counts, matching
the dense code's `if (fabs(old_val) < MIN_PIVOT && fabs(new_val) >= MIN_PIVOT)`
pattern.

**Lesson:** When converting dense algorithms to sparse, every count update
must handle the four transitions: live→live, live→dead, dead→live, dead→dead.
The dense code handles all four implicitly; the sparse code must handle them
explicitly.

---

### ITERATE_CONTINUE == CXF_OK == 0

**FAILURE:** `pricing_and_ftran` returned `ITERATE_CONTINUE` (0) from Phase I
UNBOUNDED recovery, but `ITERATE_CONTINUE == CXF_OK == 0`. The caller
(`cxf_simplex_step`) checked `if (rc != CXF_OK) return rc;` — this passed,
and the code proceeded to pivot with `leavingRow = -1` (never set by ratio
test which returned UNBOUNDED). Result: `CXF_ERROR_INVALID_ARGUMENT` crash.

**Fix:** After `pricing_and_ftran`, validate `leavingRow >= 0` before pivoting.

**Lesson:** When a helper function has both "success with output" and "handled
internally, skip this iteration" return paths, they MUST use distinct return
codes. `ITERATE_CONTINUE = 0 = CXF_OK` is a design flaw — always validate
output parameters after calls that can return CXF_OK from multiple paths.

---

### FTRAN errors should trigger refactorization, not propagate

**FAILURE:** FTRAN encountered a degraded eta (non-finite pivot element after
87 degenerate Phase I pivots). Original code: `if (rc != CXF_OK) return rc;`
propagated the error immediately. But the existing NaN/Inf recovery path (which
refactorizes and retries) was only 20 lines below — the FTRAN error returned
*before reaching it*.

**Fix:** Fold FTRAN error into `need_refactor` flag, reusing the existing
recovery infrastructure.

**Lesson:** When adding error recovery, make sure ALL error paths feed into it.
The "early return on error" pattern can bypass recovery code that handles the
same class of problems.

---

### NEVER deviate from Spec V2 to preserve Netlib pass rates

**FAILURE:** An agent implemented step length handling that deviated from the
V2 spec (numerical_stability.md Section C). The spec requires:
1. Step length clamping before application
2. Post-pivot bound projection for overshot basic variables

The agent skipped both, reasoning that `cxf_recompute_xB` after refactorization
would recover accuracy, and that per-pivot bound snapping "causes butterfly
regressions." The agent cited a project learning (MEMORY.md) to justify the
deviation. Three spec deviations were defended as "deliberate engineering
choices." All three were wrong.

**The actual fix:**
1. Clamp stepSize to 1e15 BEFORE applying the pivot
2. Project basic variables to bounds AFTER the pivot (Phase II only)
3. Move Phase I UNBOUNDED handling to solve_lp.c (where phase context belongs)

**Lesson:** The spec is the law. NEVER rationalize a deviation. If compliance
causes Netlib regressions, the regressions are acceptable — file them as issues
and fix them with MORE spec-compliant work, not by bending the spec. Previous
learnings about "butterfly regressions" may reflect bugs in the OLD non-compliant
code, not fundamental limitations of the spec's approach.

---

### EXPAND eps_base Must Be ~100x Feasibility Tolerance
**FAILURE:** Set eps_base = feas_tol (1e-6), matching spec text "1e-6 to 1e-8".
scfxm1 regressed from 0.09s to TIMEOUT because perturbation was too small to
produce nonzero step lengths in the ratio test.

**Root cause:** The spec's recommended "1e-6 to 1e-8" assumes feas_tol in the
1e-8 to 1e-10 range (standard in production solvers). For our feas_tol = 1e-6,
eps_base must be proportionally larger. The effective scaling is ~100*feas_tol.

**Fix:** `eps_base = 100 * feas_tol`, clamped to [1e-8, 1e-4]. At feas_tol=1e-8,
this gives 1e-6 (within spec range). At feas_tol=1e-6, this gives 1e-4 (effective).

**Lesson:** Spec absolute values may assume a specific tolerance regime. When our
tolerances differ, scale proportionally rather than using the spec's absolute range.

---

### EXPAND Activation: degenerate_count Resets, Use Iteration Fallback
**FAILURE:** EXPAND threshold `degenerate_count > 100` was unreachable for
bore3d (486/500 pivots degenerate) because the counter resets on ANY non-degenerate
pivot. 14 scattered non-degenerate pivots kept resetting the counter.

**Root cause:** `degenerate_count` tracks CONSECUTIVE degenerate pivots.
For severe degeneracy with occasional non-degenerate pivots interspersed,
the counter never reaches high thresholds even though degeneracy is extreme.

**Fix:** Keep primary threshold (degenerate_count > 100) for problems with pure
degenerate streaks. Add fallback: `iteration > 3*m && degenerate_count > 0` for
Phase I stalling where non-degenerate pivots intersperse.

**Lesson:** Resetting counters are poor proxies for cumulative behavior.
Use complementary conditions: one for streaks, one for duration.

---

### EXPAND Threshold Sensitivity (Session 6)
**FAILURE:** Lowering EXPAND threshold from `degenerate_count > 100` to `> 50`
and removing the one-shot `perturb_expand_active` guard caused 10+ Netlib
regressions (sc50b, sc105, share2b, adlittle, blend, lotfi, beaconfd, ship04l,
scagr7, scorpion). EXPAND was firing too aggressively, distorting bounds in
Phase II before the solver had a chance to converge naturally.

**Root cause:** The proactive perturbation at `iteration <= 2` sets
`perturb_count > 0`. With no degenerate_count guard, EXPAND fires on the
very next perturbation call, widening bounds before any degeneracy occurs.

**Fix:** Restored conservative thresholds (100 consecutive / iteration > 3*m
fallback). Kept Phase II enablement (spec-compliant) but with one-shot guard.

**Lesson:** EXPAND threshold changes must be tested against ALL passing Netlib
instances, not just the failing ones. Aggressive EXPAND is worse than no EXPAND.

---

### Source Comparison Reveals Spec V2 Was Wrong About Algorithms (2026-03-19)

**FAILURE:** V2 spec compliance sprint fixed code to match specs that THEMSELVES
misidentified the algorithms. step2/step3 described FBBT; binary does BFRT
post-processing/constraint elimination. Ratio test described Harris two-pass;
binary uses steepest-edge weighted single-pass.

**15 surgical P0 fixes applied from source comparison review:**
Most impactful: pricing polarity fix (end_level.c kept basic vars instead of
nonbasic — every pricing did a full scan), outer loop 5→100, Bland's removal,
simplex_final filter inversion, fabricated EXPAND removal.

**Workflow that worked:** 2 proposer subagents → review & implement → reviewer
subagent. 4 independent reviewers confirmed all 15 fixes correct. Reviewer
caught one premature termination bug in the outer-loop convergence check.

**Lesson:** When a spec compliance sprint doesn't improve results, question
whether the SPEC is correct, not just the code. Cleanroom specs can misidentify
algorithms. A source comparison against decompiled binaries is the definitive
authority.

---

### Markowitz Tie-Breaking: Absolute vs. Relative (Session 6)
**FINDING:** The original Markowitz tie-breaking used absolute magnitude
(`av > best_abs`), comparing raw element values across columns of different
scales. A 0.01 pivot in a column with max 0.01 (ratio 1.0, perfectly stable)
lost to 100.0 in a column with max 100000 (ratio 0.001, barely acceptable).

**Fix:** Changed to relative stability: `av/col_max[j] > best_rel`. This is
scale-independent and matches Suhl & Suhl (1990) recommendations.

**Impact:** No regressions on any Netlib instance. The change is orthogonal
to cycling behavior — it improves factorization quality but doesn't address
the anti-cycling mechanism.

