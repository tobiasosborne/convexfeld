# Audit Report: Ratio Test & Perturbation
**Auditor:** Agent B2
**Date:** 2026-02-16
**Scope:** src/simplex/ratio_test.c, perturbation.c, phase_steps.c
**Specs:** algorithms/harris_ratio_test.md, perturbation.md

## Summary
- **Total violations:** 29
- **Critical:** 7 / **Major:** 14 / **Minor:** 8

## Executive Summary

The ratio test and perturbation implementations contain severe compliance violations across three critical areas:

1. **Bound-Flipping Ratio Test (BFRT) completely missing** - The spec describes a multi-stage algorithm with long-step bound flipping (Stage 3, lines 123-153), but the implementation only has a basic ratio test.

2. **Perturbation strategy completely wrong** - Spec requires implied bound analysis with selective candidate removal; implementation uses random bound perturbation (wrong algorithm entirely).

3. **Missing anti-cycling infrastructure** - No stalling detection, no basis snapshot comparison, no unperturbation/refinement procedures.

The implementations appear to be based on simplified textbook algorithms rather than the sophisticated production-quality algorithms specified in the v2 specs.

---

## Violations

### V-01: Missing Bound-Flipping Ratio Test (BFRT) - Stage 3
- **Severity:** CRITICAL
- **File:** src/simplex/ratio_test.c (entire file)
- **Spec reference:** harris_ratio_test.md, lines 123-153 ("Stage 3: Bound-Flipping Ratio Test")
- **Description:** The spec describes a three-stage ratio test where Stage 3 implements bound flipping to enable "long steps" that traverse multiple breakpoints. The implementation completely lacks Stage 3.
- **Expected:** After Pass 2 identifies a blocking variable, check if it can flip (has finite bounds on both sides), add it to flip set F, adjust theta by (ub_r - lb_r)/|d_r|, recompute next blocking variable, repeat until a non-flippable blocker is found.
- **Actual:** Function returns immediately after Pass 2 with a single leaving variable. No flip set, no long steps, no bound adjustments.

### V-02: Missing entering direction parameter
- **Severity:** CRITICAL
- **File:** src/simplex/ratio_test.c:37-39
- **Spec reference:** harris_ratio_test.md, lines 39 ("Entering direction")
- **Description:** Spec requires an "Entering direction" input (sign indicator: +1 if entering var increases, -1 if decreases) to determine which bound each basic variable approaches.
- **Expected:** `int cxf_ratio_test(..., int enteringDirection, ...)`
- **Actual:** Function signature lacks this parameter. Implementation infers direction from pivot column sign (lines 97-108) but this doesn't match spec's semantics.

### V-03: Missing bound-flip set output
- **Severity:** CRITICAL
- **File:** src/simplex/ratio_test.c:37-39
- **Spec reference:** harris_ratio_test.md, line 50 ("Bound-flip set")
- **Description:** Spec requires output of "set of (variable index, new bound value) pairs" for variables flipped during long step.
- **Expected:** Output parameter like `BoundFlipSet **flips_out` or similar structure
- **Actual:** No such output. Function only returns single leaving row and pivot element.

### V-04: Missing step length (theta) output
- **Severity:** MAJOR
- **File:** src/simplex/ratio_test.c:37-39
- **Spec reference:** harris_ratio_test.md, line 48 ("Step length (theta)")
- **Description:** Spec requires explicit output of the step length theta (distance to bound).
- **Expected:** Output parameter `double *stepLength_out`
- **Actual:** No step length output. Caller cannot determine how far to step.

### V-05: Missing status code output
- **Severity:** MAJOR
- **File:** src/simplex/ratio_test.c:37-39
- **Spec reference:** harris_ratio_test.md, line 51 ("Status")
- **Description:** Spec requires status enumeration output: NORMAL_PIVOT, DEGENERATE_PIVOT, UNBOUNDED, BOUND_FLIP_ONLY.
- **Expected:** Output parameter or return value indicating pivot type
- **Actual:** Function returns only CXF_OK or CXF_UNBOUNDED. No distinction between normal/degenerate pivots, no BOUND_FLIP_ONLY status.

### V-06: Wrong ratio computation formula (missing entering direction)
- **Severity:** MAJOR
- **File:** src/simplex/ratio_test.c:92-111
- **Spec reference:** harris_ratio_test.md, lines 74-86
- **Description:** Spec's ratio formula depends on entering direction `s` (±1) and uses `s * d_i` to determine blocking direction. Implementation uses sign of `d_i` alone.
- **Expected:**
  ```c
  if (s * d_i > 0) theta_i = (x_i - lb_i) / (s * d_i)
  if (s * d_i < 0) theta_i = (ub_i - x_i) / (-s * d_i)
  ```
- **Actual:**
  ```c
  if (d_i > relaxedTol) ratio = (x_i - lb) / d_i
  else if (d_i < -relaxedTol) ratio = (x_i - ub) / d_i
  ```
  (Lines 97-108: infers direction from coefficient sign, doesn't use entering direction parameter)

### V-07: Wrong Pass 1 formula (missing epsilon in numerator)
- **Severity:** MAJOR
- **File:** src/simplex/ratio_test.c:61-118
- **Spec reference:** harris_ratio_test.md, lines 94-105
- **Description:** Spec defines relaxed minimum as `theta_max = min { (slack_i + epsilon) / |d_i| }` where epsilon is the feasibility tolerance. Implementation computes `ratio = slack_i / |d_i|` without adding epsilon.
- **Expected:** `ratio = (slack + feasTol) / fabs(d_i)`
- **Actual:** Lines 102, 108: `ratio = (x_i - lb) / d_i` or `ratio = (x_i - ub) / d_i` (no `+ feasTol` in numerator)

### V-08: Wrong Pass 2 threshold formula
- **Severity:** MAJOR
- **File:** src/simplex/ratio_test.c:130
- **Spec reference:** harris_ratio_test.md, lines 109-112
- **Description:** Spec says "Among all candidates i satisfying slack_i / |d_i| <= theta_max". Implementation uses `ratio <= threshold` where `threshold = minRatio + feasTol`.
- **Expected:** Candidate eligible if `ratio <= theta_max` (where theta_max already includes epsilon)
- **Actual:** Line 130: `threshold = minRatio + feasTol` then line 175: `if (ratio <= threshold)`. This adds epsilon twice (once in pass 1 via threshold, once here).

### V-09: Missing bound-flip eligibility checks
- **Severity:** CRITICAL
- **File:** src/simplex/ratio_test.c (entire file)
- **Spec reference:** harris_ratio_test.md, lines 129-132
- **Description:** Spec requires checking if blocking variable can flip: "finite bounds on both sides, bound range not negligibly small". No such check exists.
- **Expected:**
  ```c
  if (lb_r > -infinity && ub_r < infinity && (ub_r - lb_r) > threshold) {
      // Variable can flip
  }
  ```
- **Actual:** No code checks whether leaving variable has two finite bounds.

### V-10: Missing bound-flip application logic
- **Severity:** CRITICAL
- **File:** src/simplex/ratio_test.c (entire file)
- **Spec reference:** harris_ratio_test.md, lines 146-151
- **Description:** Spec requires applying bound flips: "Set variable's value to new bound, update constraint activities, negate row coefficients, update status".
- **Expected:** For each flipped variable: update value, adjust RHS, negate matrix row, change status
- **Actual:** No bound flip application code exists

### V-11: Missing Phase I / Phase II ratio test distinction
- **Severity:** MAJOR
- **File:** src/simplex/ratio_test.c (entire file)
- **Spec reference:** harris_ratio_test.md, line 8 (Koberstein thesis reference implies Phase I uses different ratio test)
- **Description:** Modern simplex solvers use different ratio test variants for Phase I (infeasibility minimization) vs Phase II (optimality). Implementation has single ratio test function.
- **Expected:** Separate functions or mode parameter distinguishing Phase I vs Phase II ratio tests
- **Actual:** Single `cxf_ratio_test` function used for all phases

### V-12: Incomplete Bland's rule implementation
- **Severity:** MINOR
- **File:** src/simplex/ratio_test.c:176-181
- **Spec reference:** harris_ratio_test.md, lines 197-198
- **Description:** Bland's rule requires smallest *variable index* selection for both entering and leaving variables (full anti-cycling guarantee). Implementation only applies it to leaving variable selection.
- **Expected:** Bland's rule applied consistently across pricing and ratio test
- **Actual:** Lines 176-181: Only leaving variable uses smallest index. Entering variable selection (pricing) doesn't coordinate with this.

### V-13: Missing pivot tolerance adaptiveness
- **Severity:** MINOR
- **File:** src/simplex/ratio_test.c (uses fixed relaxedTol)
- **Spec reference:** harris_ratio_test.md, lines 178-182
- **Description:** Spec recommends adaptive pivot tolerance (loose in early phase, tight near optimality).
- **Expected:** Pivot tolerance adjusted based on solver phase or iteration count
- **Actual:** Line 59: `relaxedTol = 10.0 * feasTol` (fixed multiplier, never adapted)

### V-14: Wrong perturbation algorithm (bound perturbation instead of implied bound analysis)
- **Severity:** CRITICAL
- **File:** src/simplex/perturbation.c:71-158
- **Spec reference:** perturbation.md, lines 64-74, 100-161
- **Description:** Spec describes "implied bound analysis" that examines constraint structure and removes degenerate variables from pricing (EXPAND method, Gill et al. 1989). Implementation uses random bound perturbation (Wolfe 1963 method, explicitly deprecated).
- **Expected:**
  - Phase 2: Retrieve pricing candidates
  - Phase 4: For each candidate, compute implied bounds from constraint row, check gap, remove degenerate candidates
  - No explicit bound modification
- **Actual:** Lines 107-152: Directly modifies `work_lb[j]` and `work_ub[j]` with random perturbations. No candidate retrieval, no pricing set manipulation.

### V-15: Missing stalling detection
- **Severity:** CRITICAL
- **File:** src/simplex/perturbation.c (entire file)
- **Spec reference:** perturbation.md, lines 78-98
- **Description:** Spec requires "basis snapshot comparison protocol" to detect stalling before invoking perturbation. Implementation has no stalling detection.
- **Expected:**
  - Capture basis snapshot before iteration batch
  - Compute weighted difference score D
  - Compare D to threshold
  - Invoke perturbation only when stalling detected
- **Actual:** No snapshot infrastructure. Function can be called anytime (no precondition checks).

### V-16: Missing candidate retrieval phase (Phase 2)
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c (entire file)
- **Spec reference:** perturbation.md, lines 100-104
- **Description:** Spec requires "Query the pricing subsystem for current list of pricing candidates" before perturbation analysis.
- **Expected:** Call to pricing system to get candidate list, then process only those candidates
- **Actual:** Lines 107-152: Processes all variables (0 to n-1), not just pricing candidates

### V-17: Missing bound restoration phase (Phase 3)
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c (entire file)
- **Spec reference:** perturbation.md, lines 106-112
- **Description:** Spec requires optional bound restoration step (copy saved bounds to working bounds before perturbation analysis) in verbose mode.
- **Expected:**
  ```c
  if (verbose_mode) {
      memcpy(work_lb, saved_lb, n * sizeof(double));
      memcpy(work_ub, saved_ub, n * sizeof(double));
  }
  ```
- **Actual:** No such phase. Function has no saved/working bound distinction.

### V-18: Wrong candidate processing (Phase 4)
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c:107-152
- **Spec reference:** perturbation.md, lines 114-161
- **Description:** Spec requires case analysis based on variable status (AT_LOWER, BASIC, etc.) with different handling for each. Implementation ignores variable status.
- **Expected:**
  - Case A: Non-basic at lower bound → check reduced cost, mark removed
  - Case B: Basic variable → compute implied bounds from constraint row
- **Actual:** Lines 107-152: Processes all variables identically (apply random perturbation)

### V-19: Missing implied bound computation
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c (entire file)
- **Spec reference:** perturbation.md, lines 126-141
- **Description:** Spec requires computing implied bounds for basic variables from constraint structure: `x_j = (b_r - sum a_{ri} * x_i) / a_{rj}`.
- **Expected:**
  - Get constraint row r for basic variable j
  - For each coefficient, determine contribution to implied lower/upper bounds
  - Track unbounded contributions
- **Actual:** No constraint matrix access, no implied bound computation

### V-20: Missing degenerate variable removal mechanism
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c (entire file)
- **Spec reference:** perturbation.md, lines 156-161
- **Description:** Spec requires removing degenerate variables from pricing candidate set: "Remove all entries of this variable's column from pricing candidate set".
- **Expected:** Function modifies pricing data structure to mark variables as non-candidates
- **Actual:** Function only modifies bounds. No interaction with pricing system.

### V-21: Wrong perturbation scale formula
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c:98-125
- **Spec reference:** perturbation.md, lines 143-149
- **Description:** Spec defines perturbation magnitude as `clamp(gap, min_bound_range, max_perturbation)` where gap is implied bound gap. Implementation uses objective coefficient scaling.
- **Expected:**
  ```c
  gap = implied_lower - implied_upper;
  perturbation_magnitude = clamp(gap, 1e-10, 1e-6);
  ```
- **Actual:** Lines 113-125:
  ```c
  scale = base_scale / abs_obj;  // scales by objective, not implied bounds
  eps_lb = rand1 * scale;
  ```

### V-22: Wrong perturbation application (shrinks vs expands feasible region)
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c:138-144
- **Spec reference:** perturbation.md, lines 74, 150-151 (implied: expand working tolerance)
- **Description:** Spec's EXPAND method increases feasibility tolerance (expands feasible region). Implementation shrinks feasible region by increasing lb and decreasing ub.
- **Expected:** Method that grows working tolerance monotonically or removes candidates from pricing
- **Actual:** Lines 138-144: `lb[j] += eps_lb; ub[j] -= eps_ub` (shrinks box constraints)

### V-23: Missing unperturbation infrastructure
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c:170-199
- **Spec reference:** perturbation.md, lines 262-288
- **Description:** Spec requires comprehensive unperturbation: bound restoration + solution refinement (nonbasic cleanup, basic variable recovery, eta vector creation). Implementation only restores bounds.
- **Expected:**
  - Restore bounds (memcpy)
  - Fix nonbasic variables at appropriate bounds
  - Pivot infeasible basic variables to feasibility
  - Create eta vectors for basis updates
- **Actual:** Lines 181-193: Only `memcpy` to restore bounds. No solution refinement.

### V-24: Global perturbation flag is wrong design
- **Severity:** MINOR
- **File:** src/simplex/perturbation.c:26
- **Spec reference:** perturbation.md (implied: per-problem state)
- **Description:** Global static flag `g_perturbation_applied` prevents concurrent solves and survives across different problem instances.
- **Expected:** Perturbation flag stored in SolverContext (per-problem)
- **Actual:** Line 26: `static int g_perturbation_applied = 0;` (global, not thread-safe)

### V-25: Missing constraint sense handling
- **Severity:** MAJOR
- **File:** src/simplex/perturbation.c (entire file)
- **Spec reference:** perturbation.md, lines 118-119, 152-154
- **Description:** Spec requires checking constraint sense (equality vs inequality) and handling differently for implied bound analysis and infeasibility detection.
- **Expected:** Access to `state->constraint_sense[r]` to determine if constraint is equality, <=, or >=
- **Actual:** No constraint sense array accessed. Implementation doesn't distinguish constraint types.

### V-26: Missing Phase I bound-flip logic in phase_steps.c
- **Severity:** MINOR
- **File:** src/simplex/phase_steps.c:38-107
- **Spec reference:** harris_ratio_test.md, lines 123-153
- **Description:** `cxf_simplex_step2` implements primitive bound flip check (lines 59-91) but doesn't integrate with BFRT from ratio test (which should have already determined flip set).
- **Expected:** Function receives flip set from ratio test and applies it
- **Actual:** Lines 59-91: Implements ad-hoc bound flip logic that duplicates (incorrectly) what ratio test should have done

### V-27: Missing dual step size computation
- **Severity:** MINOR
- **File:** src/simplex/phase_steps.c:38
- **Spec reference:** harris_ratio_test.md, line 127, perturbation.md (dual pricing weight updates)
- **Description:** `cxf_simplex_step2` accepts `dualStepSize` parameter but doesn't explain how caller should compute it.
- **Expected:** Documentation or spec reference for dual step size formula
- **Actual:** Line 40: Parameter documented as "Dual step length for dual solution update" (no formula)

### V-28: Inconsistent pivot tolerance values
- **Severity:** MINOR
- **File:** src/simplex/ratio_test.c:73, phase_steps.c:148
- **Spec reference:** harris_ratio_test.md, lines 174-182
- **Description:** Spec recommends pivot tolerance in range 1e-10 to 1e-7. ratio_test.c uses `relaxedTol` (10 * feasTol, typically 1e-5 to 1e-8). phase_steps.c uses `CXF_PIVOT_TOL` constant.
- **Expected:** Consistent tolerance values across ratio test and pivot execution
- **Actual:**
  - ratio_test.c line 73: `fabs(d_i) <= relaxedTol` (1e-5 order)
  - phase_steps.c line 148: `fabs(pivot) < CXF_PIVOT_TOL` (unknown value, likely different)

### V-29: Missing work counter updates
- **Severity:** MINOR
- **File:** src/simplex/perturbation.c (entire file)
- **Spec reference:** perturbation.md, lines 163-167
- **Description:** Spec requires updating work counter to reflect computational effort in perturbation procedure.
- **Expected:** `state->work_counter += candidates_processed;` or similar
- **Actual:** No work counter field accessed or updated

---

## Spec Sections Not Implemented

### Ratio Test (harris_ratio_test.md)

1. **Stage 3: Bound-Flipping Ratio Test (lines 123-153)** - Entirely missing
2. **Entering direction input parameter (line 39)** - Missing
3. **Bound-flip set output (line 50)** - Missing
4. **Step length output (line 48)** - Missing
5. **Status enumeration output (line 51)** - Missing
6. **Bound-flip ordering and traversal (lines 145-153)** - Missing
7. **Matrix consistency after flip (line 166)** - Missing
8. **Adaptive pivot tolerance (lines 178-182)** - Missing
9. **Infeasibility detection in ratio test (lines 207-210)** - Missing

### Perturbation (perturbation.md)

1. **Phase 1: Stalling detection (lines 78-98)** - Entirely missing
   - Basis snapshot capture
   - Weighted difference score computation
   - Threshold comparison
   - Proactive perturbation in early iterations

2. **Phase 2: Candidate retrieval (lines 100-104)** - Missing

3. **Phase 3: Bound restoration in verbose mode (lines 106-112)** - Missing

4. **Phase 4: Candidate processing (lines 114-161)** - Wrong algorithm
   - Case A: Non-basic variable handling (lines 116-122)
   - Case B: Basic variable implied bound analysis (lines 125-161)
     - Implied bound computation (lines 126-141)
     - Gap calculation and clamping (lines 143-149)
     - Infeasibility checks for equality/inequality constraints (lines 152-154)
     - Degenerate variable removal (lines 156-161)

5. **Phase 5: Counter update (lines 163-167)** - Partially missing (no work counter)

6. **Unperturbation and Solution Refinement (lines 262-288)** - Mostly missing
   - Nonbasic variable cleanup (lines 276-277)
   - Basic variable recovery (lines 279-280)
   - Eta vector creation (line 281)
   - Convergence analysis (lines 283-288)

---

## Code Sections Not In Spec

### ratio_test.c

1. **Lines 47-48**: Unused parameter suppressions for `enteringVar` and `columnNZ`
   - Comment says "for future sparse impl" but spec already describes sparse handling
   - Should either be used now or removed

2. **Lines 80-85**: Artificial variable handling
   - Skips variables with indices >= `num_vars + num_constrs`
   - Spec doesn't mention artificial variables in ratio test (handled elsewhere)
   - Comment mentions "artificials" but spec ratio test operates on transformed problem

3. **Lines 133-134, 176-181**: Bland's rule implementation
   - Spec mentions Bland's rule as fallback (line 220) but doesn't specify it as primary tie-breaking
   - Harris' largest-pivot tie-breaking (line 117) is specified as primary method
   - Implementation mixes both based on `state->use_bland` flag (not in spec inputs)

### perturbation.c

1. **Lines 16-23**: Hardcoded perturbation scale constants
   - `PERTURB_BASE_SCALE 1e-6`, `PERTURB_MAX_SCALE 1e-3`, `MIN_OBJ_COEFF 1e-8`
   - Spec uses different constants: `min_bound_range ~1e-10`, `max_perturbation ~1e-6` (lines 189-190)
   - Scale factors don't match spec's magnitude recommendations

2. **Lines 36-48**: Pseudo-random number generator
   - Spec requires deterministic perturbations but doesn't prescribe PRNG algorithm
   - Implementation uses multiplicative hash (golden ratio prime, etc.)
   - Could be acceptable if it matches reference behavior, but spec says implied bound analysis, not random bounds

3. **Lines 113-125**: Objective coefficient scaling
   - Scales perturbation by `1 / abs(obj[j])` when obj is large
   - Spec doesn't mention objective coefficients in perturbation magnitude (perturbation.md lines 143-149)
   - Appears to be hallucinated heuristic not in spec

4. **Lines 147-151**: Bound crossing midpoint adjustment
   - If perturbed bounds cross, uses midpoint: `mid = (orig_lb + orig_ub) * 0.5`
   - Spec doesn't describe this recovery mechanism
   - May be reasonable defensive code, but not specified

### phase_steps.c

1. **Lines 59-91**: Ad-hoc bound flip implementation
   - Checks `range < CXF_INFINITY` and `flipStep < stepSize`
   - Duplicates logic that should be in ratio test (BFRT Stage 3)
   - Spec separates concerns: ratio test returns flip set, pivot step applies it

2. **Lines 84-86**: Objective value update in bound flip
   - `state->obj_value += state->work_dj[entering] * flipStep`
   - Spec doesn't specify objective update in bound flip (should be in higher-level iteration logic)
   - May be correct but location is questionable

---

## Recommendations

### Immediate Actions (Critical Fixes)

1. **Implement BFRT Stage 3** in ratio_test.c:
   - Add flip set data structure and output parameter
   - Add bound-flip eligibility checks after Pass 2
   - Implement iterative flip loop with theta adjustment
   - Add status output (NORMAL_PIVOT vs BOUND_FLIP_ONLY)

2. **Rewrite perturbation.c to use implied bound analysis**:
   - Remove random bound perturbation code (lines 107-152)
   - Implement Phase 2: candidate retrieval from pricing system
   - Implement Phase 4 Case B: implied bound computation from constraint rows
   - Implement degenerate variable removal (modify pricing candidate set)

3. **Add stalling detection** (new module or in solver main loop):
   - Implement basis snapshot capture and comparison
   - Add weighted difference score computation
   - Invoke perturbation only when stalling detected

### Major Enhancements

4. **Add entering direction parameter** to ratio test
5. **Add step length and status outputs** to ratio test
6. **Implement solution refinement** in unperturbation (nonbasic cleanup, basic variable recovery)
7. **Add constraint sense handling** to perturbation (equality vs inequality)

### Minor Improvements

8. **Make pivot tolerance adaptive** (early/middle/late phases)
9. **Move perturbation flag to SolverContext** (remove global state)
10. **Add work counter updates** throughout perturbation
11. **Coordinate Bland's rule** across pricing and ratio test (if used as primary anti-cycling)
12. **Refactor phase_steps.c** to remove duplicated bound-flip logic (should be in ratio test)

### Documentation Gaps

13. Add references to Gill et al. (1989) EXPAND procedure in perturbation.c
14. Document dual step size computation for phase_steps.c::cxf_simplex_step2
15. Add complexity analysis comments (spec provides O(nnz_q) bounds)

---

## Conclusion

The implementations are **fundamentally non-compliant** with v2 specifications. The ratio test is missing its most important feature (BFRT long steps), and the perturbation algorithm implements the *wrong method* entirely (random bound perturbation instead of implied bound analysis). These are not minor deviations but architectural mismatches that prevent the implementations from achieving the performance and robustness goals stated in the specs.

**Estimated rework effort:**
- Ratio test BFRT: ~200-300 LOC, 2-3 days
- Perturbation rewrite: ~300-400 LOC, 3-5 days (requires pricing system integration)
- Stalling detection: ~100-150 LOC, 1-2 days
- Total: ~600-850 LOC, 6-10 days

**Risk:** High. BFRT and implied bound perturbation are production-quality optimizations used in commercial solvers (CPLEX, Gurobi). Without them, ConvexFeld will be significantly slower on degenerate problems and problems with many bounded variables.
