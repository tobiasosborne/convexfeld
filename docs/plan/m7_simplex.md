# Milestone 7: Simplex Engine (Level 5)

**Goal:** Complete Simplex Core, Crossover, Utilities modules
**Parallelizable:** Simplex phases sequential; Crossover and Utilities parallel with late Simplex
**Spec References:**
- `docs/specs/modules/06_simplex_core.md`
- `docs/specs/modules/15_crossover.md`

---

## Module: Simplex Core (21 functions)

**Spec Directory:** `docs/specs/functions/simplex/`, `docs/specs/functions/ratio_test/`, `docs/specs/functions/pivot/`

### Step 7.1.1: Simplex Tests - Basic

**LOC:** ~200
**File:** `tests/unit/test_simplex_basic.c`

Tests for initialization, setup, cleanup.

### Step 7.1.2: Simplex Tests - Iteration

**LOC:** ~200
**File:** `tests/unit/test_simplex_iteration.c`

Tests for iteration loop, phases.

### Step 7.1.3: Simplex Tests - Edge Cases

**LOC:** ~200
**File:** `tests/unit/test_simplex_edge.c`

Tests for degeneracy, unbounded, infeasible.

### Step 7.1.4: cxf_solve_lp

**LOC:** ~150
**File:** `src/simplex/solve_lp.c`
**Spec:** `docs/specs/functions/simplex/cxf_solve_lp.md`

Main entry point replacing stub.

### Step 7.1.5: cxf_simplex_init, cxf_simplex_final

**LOC:** ~120
**File:** `src/simplex/lifecycle.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_init.md`
- `docs/specs/functions/simplex/cxf_simplex_final.md`

### Step 7.1.6: cxf_simplex_setup, cxf_simplex_preprocess

**LOC:** ~150
**File:** `src/simplex/setup.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_setup.md`
- `docs/specs/functions/simplex/cxf_simplex_preprocess.md`

### Step 7.1.7: cxf_simplex_crash

**LOC:** ~120
**File:** `src/simplex/crash.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_crash.md`

Initial basis heuristic.

### Step 7.1.8: cxf_simplex_iterate

**LOC:** ~150
**File:** `src/simplex/iterate.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_iterate.md`

Main iteration loop.

### Step 7.1.9: cxf_simplex_step

**LOC:** ~150
**File:** `src/simplex/step.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_step.md`

Single iteration.

### Step 7.1.10: cxf_simplex_step2, cxf_simplex_step3

**LOC:** ~120
**File:** `src/simplex/phase_steps.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_step2.md`
- `docs/specs/functions/simplex/cxf_simplex_step3.md`

### Step 7.1.11: cxf_simplex_post_iterate, cxf_simplex_phase_end

**LOC:** ~100
**File:** `src/simplex/post.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_post_iterate.md`
- `docs/specs/functions/simplex/cxf_simplex_phase_end.md`

### Step 7.1.12: cxf_simplex_refine

**LOC:** ~120
**File:** `src/simplex/refine.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_refine.md`

Iterative refinement.

### Step 7.1.13: cxf_simplex_perturbation, cxf_simplex_unperturb

**LOC:** ~120
**File:** `src/simplex/perturbation.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_perturbation.md`
- `docs/specs/functions/simplex/cxf_simplex_unperturb.md`

Anti-cycling.

### Step 7.1.14: cxf_simplex_cleanup

**LOC:** ~80
**File:** `src/simplex/cleanup.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_cleanup.md`

### Step 7.1.15: cxf_pivot_primal

**LOC:** ~150
**File:** `src/simplex/pivot_primal.c`
**Spec:** `docs/specs/functions/pivot/cxf_pivot_primal.md`

### Step 7.1.16: cxf_pivot_bound, cxf_pivot_special

**LOC:** ~120
**File:** `src/simplex/pivot_special.c`
**Specs:**
- `docs/specs/functions/ratio_test/cxf_pivot_bound.md`
- `docs/specs/functions/ratio_test/cxf_pivot_special.md`

### Step 7.1.17: cxf_pivot_with_eta

**LOC:** ~100
**File:** `src/simplex/eta_pivot.c`
**Spec:** `docs/specs/functions/basis/cxf_pivot_with_eta.md`

### Step 7.1.18: cxf_ratio_test

**LOC:** ~150
**File:** `src/simplex/ratio_test.c`
**Spec:** `docs/specs/functions/ratio_test/cxf_ratio_test.md`

Harris two-pass ratio test.

### Step 7.1.19: cxf_quadratic_adjust

**LOC:** ~80
**File:** `src/simplex/quadratic.c`
**Spec:** `docs/specs/functions/simplex/cxf_quadratic_adjust.md`

---

## Module: Crossover (2 functions)

**Spec Directory:** `docs/specs/functions/crossover/`

### Step 7.2.1: Crossover Tests

**LOC:** ~100
**File:** `tests/unit/test_crossover.c`

### Step 7.2.2: cxf_crossover

**LOC:** ~150
**File:** `src/crossover/crossover.c`
**Spec:** `docs/specs/functions/crossover/cxf_crossover.md`

### Step 7.2.3: cxf_crossover_bounds

**LOC:** ~80
**File:** `src/crossover/bounds.c`
**Spec:** `docs/specs/functions/crossover/cxf_crossover_bounds.md`

---

## Module: Utilities (10 functions)

**Spec Directory:** `docs/specs/functions/utilities/`, `docs/specs/functions/pivot/`

### Step 7.3.1: Utilities Tests

**LOC:** ~150
**File:** `tests/unit/test_utilities.c`

### Step 7.3.2: cxf_fix_variable

**LOC:** ~80
**File:** `src/utilities/fix_var.c`
**Spec:** `docs/specs/functions/pivot/cxf_fix_variable.md`

### Step 7.3.3: Math Wrappers

**LOC:** ~80
**File:** `src/utilities/math.c`
**Specs:**
- `docs/specs/functions/utilities/cxf_floor_ceil_wrapper.md`
- `docs/specs/functions/utilities/cxf_log10_wrapper.md`

### Step 7.3.4: Constraint Helpers

**LOC:** ~100
**File:** `src/utilities/constraints.c`
**Specs:**
- `docs/specs/functions/utilities/cxf_count_genconstr_types.md`
- `docs/specs/functions/utilities/cxf_get_genconstr_name.md`
- `docs/specs/functions/utilities/cxf_get_qconstr_data.md`

### Step 7.3.5: Multi-Objective Check

**LOC:** ~60
**File:** `src/utilities/multi_obj.c`
**Spec:** `docs/specs/functions/utilities/cxf_is_multi_obj.md`

### Step 7.3.6: Misc Utility

**LOC:** ~60
**File:** `src/utilities/misc.c`
**Spec:** `docs/specs/functions/utilities/cxf_misc_utility.md`
