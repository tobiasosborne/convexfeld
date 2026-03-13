# Spec V2 Audit: Two-Phase Method & Lifecycle

**Auditor:** Claude Opus 4.6 (read-only audit)
**Date:** 2026-03-13
**Scope:** Two-Phase Method (P2.9), Simplex Phases (P3.21), Solve LP Core (P3.25), Simplex Lifecycle (P3.22)

## Files Reviewed

### Spec Files
- `docs/specs-v2/specs/algorithms/two_phase_method.md`
- `docs/specs-v2/specs/modules/simplex_phases.md`
- `docs/specs-v2/specs/modules/solve_lp_core.md`
- `docs/specs-v2/specs/modules/simplex_lifecycle.md`

### Implementation Files
- `src/simplex/phase_one.c` (cxf_setup_phase_one, cxf_transition_to_phase_two)
- `src/simplex/phase_loop.c` (cxf_check_phase_one_end)
- `src/simplex/solve_lp.c` (cxf_solve_lp)
- `src/simplex/setup.c` (cxf_simplex_setup, cxf_simplex_preprocess, cxf_compute_activity_bounds)
- `src/simplex/cleanup.c` (cxf_simplex_postsolve)
- `src/simplex/post.c` (cxf_simplex_post_iterate, cxf_simplex_phase_end)
- `src/simplex/context.c` (cxf_simplex_init, cxf_simplex_final)
- `src/simplex/refine.c` (cxf_simplex_refine)
- `src/simplex/crash.c` (cxf_simplex_crash)
- `src/simplex/perturbation.c` (cxf_simplex_perturbation)

---

## Compliant Functions

### cxf_simplex_crash (P3.21)
- Signature matches spec: `(SolverState*, CxfEnv*) -> int`
- Two-step row scan (unassigned feasibility check, candidate removal) matches spec
- Infeasibility detection on equality constraints with nonzero RHS: compliant
- Column nonzero count decrement for removed rows: compliant
- Basis count update: compliant
- Work counter update: compliant

### cxf_simplex_setup (P3.21)
- Signature matches spec: `(SolverState*, CxfEnv*, int count, int* indices) -> void`
- Selective computation via indices parameter: compliant
- Activity bound formula (min/max of a_ij * lb_j / ub_j): compliant
- Infinite bound handling (count-based tracking): compliant
- Rounding correction when min ~= max: compliant

### cxf_simplex_perturbation (P3.21)
- Signature matches spec: `(SolverState*, CxfEnv*) -> int`
- (Implementation reviewed at function-level only; detailed audit deferred to perturbation scope)

### cxf_simplex_post_iterate (P3.20)
- Stall detection at refactor boundaries: compliant
- Iteration limit check: compliant
- Objective stagnation detection: compliant

### Phase I w-coefficients (two_phase_method.md)
- Below lb: w = -1, above ub: w = +1, feasible: w = 0: compliant (phase_one.c:135-143)
- Nonbasic variables: w = 0: compliant (phase_one.c:124-125 zeroes all, then only sets basics)
- Dynamic update after each pivot: compliant (recompute.c handles this per MEMORY.md)
- Sum-of-infeasibilities formulation: compliant

---

## VIOLATIONS

### [V1] cxf_solve_lp -- Signature mismatch
- **Spec says:** `(model, timing, status_out, mode) -> int` per solve_lp_core.md. Four input parameters plus return code. `timing` is a double-array accumulator, `status_out` receives solve status, `mode` controls iteration limits and tolerance scaling.
- **Code does:** `(CxfModel *model) -> int` -- single parameter, returns status directly through `model->status` and as return value.
- **File:** `src/simplex/solve_lp.c:50`
- **Impact:** Missing timing instrumentation, missing mode parameter (no crossover/primal mode distinction), status communicated differently. The spec's separation of return code (error) from status_out (solve outcome) is collapsed into a single channel.

### [V2] cxf_solve_lp -- Missing tolerance calibration (Phase 2)
- **Spec says:** Phase 2 of cxf_solve_lp computes a work estimate from nonzero count, variable count, and constraint count, then calibrates tolerances based on problem size and solve mode.
- **Code does:** No tolerance calibration based on problem size. Uses env tolerances directly. The env parameters are saved/restored (lines 71-74, 376-379) but never scaled.
- **File:** `src/simplex/solve_lp.c:84-86`
- **Impact:** Large problems may suffer from premature termination or excessive iterations due to un-scaled tolerances.

### [V3] cxf_solve_lp -- Missing method selection (Phase 3)
- **Spec says:** Phase 3 selects simplex variant (primal/dual/auto) based on priority chain: user override, simplex mode, concurrent indicator, barrier status.
- **Code does:** No method selection logic. Always runs primal simplex. No dual simplex path.
- **File:** `src/simplex/solve_lp.c:50-382`
- **Impact:** Dual simplex (the spec default for LP) is not available. No concurrent or auto selection.

### [V4] cxf_solve_lp -- Missing crossover (Phase 5)
- **Spec says:** Phase 5 performs barrier-to-simplex crossover when a barrier solution is present.
- **Code does:** No crossover logic. No barrier solution check.
- **File:** `src/simplex/solve_lp.c`
- **Impact:** Barrier solutions cannot be cleaned up to vertex solutions. Expected -- barrier not yet implemented.

### [V5] cxf_solve_lp -- Missing PWL processing (Phase 7)
- **Spec says:** Phase 7 processes piecewise-linear constraints within the iteration loop.
- **Code does:** No PWL processing.
- **File:** `src/simplex/solve_lp.c`
- **Impact:** PWL constraints unsupported. Expected -- advanced feature.

### [V6] cxf_solve_lp -- Iteration loop ordering deviates from spec
- **Spec says:** Inner loop order is: (1) basis_snapshot, (2) iterate, (3) iterate_variant, (4) perturbation, (5) step, (6) step2, (7) step3, (8) phase_end, (9) basis_diff.
- **Code does:** Order is: (2) log_progress, (3) phase_end(pre-pivot), (4) perturbation, (5) step, (6) step2, (7) step3, (8) phase_end(post-pivot), (9) basis_diff, (10) post_iterate. The basis_snapshot is taken at round start only, not per-iteration.
- **File:** `src/simplex/solve_lp.c:157-301`
- **Impact:** Minor. The code calls phase_end twice (pre-pivot and post-pivot), which is consistent with the simplex_phases.md "called at two points" note. But basis_snapshot placement differs.

### [V7] cxf_solve_lp -- Missing cxf_simplex_cleanup call
- **Spec says:** simplex_lifecycle.md specifies the post-solve order as: refine -> final -> cleanup. cxf_simplex_cleanup performs implied-bound tightening and frees temporary arrays.
- **Code does:** Calls `cxf_simplex_postsolve` (in cleanup.c) and `cxf_simplex_final` but never calls `cxf_simplex_cleanup`. The postsolve function is a simpler version that restores preprocessed variables and clamps basic variables.
- **File:** `src/simplex/solve_lp.c:310-373`
- **Impact:** No implied-bound propagation post-solve. No constraint conversion to equality. Reduced solution quality for downstream operations.

### [V8] cxf_simplex_init -- Signature mismatch
- **Spec says:** `(model, altModel, initMode, timing, outState) -> int`. Accepts alternative model for warm-start, init mode for reoptimization, and timing accumulator.
- **Code does:** `(CxfModel *model, SolverState **stateP) -> int`. Missing altModel, initMode, and timing parameters.
- **File:** `src/simplex/context.c:35`
- **Impact:** No warm-start/reoptimization support. No timing instrumentation.

### [V9] cxf_simplex_init -- Missing solve mode selection (Phase 3)
- **Spec says:** Phase 3 selects simplex variant based on environment parameters, QP presence, general constraints, and pricing strategy.
- **Code does:** Hardcodes `ctx->solve_mode = 0` (primal simplex).
- **File:** `src/simplex/context.c:61`
- **Impact:** No dual simplex, no auto mode selection.

### [V10] cxf_simplex_init -- Missing performance scaling factor
- **Spec says:** Phase 4 computes a performance scaling factor as `sqrt(log(nnz))` for problems above a minimum size threshold.
- **Code does:** No performance scaling factor computed.
- **File:** `src/simplex/context.c:35-284`

### [V11] cxf_simplex_init -- Missing special variable processing (Phase 7)
- **Spec says:** Phase 7 processes quadratic terms, semi-continuous variables, general constraints, quadratic constraints, SOS constraints, PWL constraints, and ranged constraints.
- **Code does:** None of these are processed. No variable flags array.
- **File:** `src/simplex/context.c`
- **Impact:** Advanced problem types unsupported. Expected for current scope.

### [V12] cxf_simplex_init -- Pool capacity formula mismatch
- **Spec says:** poolCapacity = max(MIN_POOL_SIZE=10000, maxDimension, nnz/10), etaPoolEntries = poolCapacity + nnz, etaPoolBytes = etaPoolEntries * 32.
- **Code does:** Eta pool created with `CXF_MIN_CHUNK_SIZE` (not following the spec formula). Array sizes are direct (n+m) rather than the tiered sizing in the spec.
- **File:** `src/simplex/context.c:268-271`

### [V13] cxf_simplex_final -- Signature and behavior mismatch
- **Spec says:** `cxf_simplex_final(state, env, workOut) -> int`. Performs dual-feasibility-based variable fixing (Phase 1: target value determination, Phase 2: equality constraint verification, Phase 3: activity propagation, Phase 4: constraint feasibility check, Phase 5: apply fixings).
- **Code does:** `cxf_simplex_final(SolverState*) -> void`. Only frees memory. No variable fixing at all. Comment says "CS fix moved to solve_lp.c".
- **File:** `src/simplex/context.c:296`
- **Impact:** The dual-feasibility variable fixing logic specified for cxf_simplex_final is partially implemented in solve_lp.c (lines 349-363, the CS fix loop) but without the spec's constraint verification (Phases 2-4). The CS fix in solve_lp.c only snaps nonbasic variables to bounds based on RC sign -- it does not verify equality constraints, propagate activities, or check constraint feasibility before fixing.

### [V14] cxf_simplex_preprocess -- Missing eta record creation
- **Spec says:** Per-candidate processing creates a variable-fixing eta record (Variant 2) for each fixed variable.
- **Code does:** Directly modifies bounds and x values without creating eta records. No call to `cxf_alloc_eta`.
- **File:** `src/simplex/setup.c:218-221`
- **Impact:** Fixed variables cannot be recovered for warm-start or crossover. Loss of audit trail.

### [V15] cxf_simplex_preprocess -- Missing candidate sorting
- **Spec says:** Candidates are sorted by bound width (tightest first) using `cxf_sort_indices`.
- **Code does:** Processes candidates in natural order (j = 0 to n-1).
- **File:** `src/simplex/setup.c:202`
- **Impact:** Suboptimal fixing order may cause more activity perturbation than necessary.

### [V16] cxf_simplex_preprocess -- Missing objective adjustment
- **Spec says:** Step 4.5: "The objective value is adjusted by the product of the variable's objective coefficient and its fixing value."
- **Code does:** No objective adjustment when fixing variables.
- **File:** `src/simplex/setup.c:202-223`
- **Impact:** Objective value may be incorrect after preprocessing until recomputation.

### [V17] cxf_simplex_preprocess -- Signature deviation
- **Spec says:** `(state, env) -> int`
- **Code does:** `(state, env, flags) -> int` with extra `flags` parameter.
- **File:** `src/simplex/setup.c:186`
- **Impact:** Minor -- extra parameter for control flow.

### [V18] cxf_simplex_phase_end -- Missing pricing candidate retrieval
- **Spec says:** "Phase 1: Candidate constraint processing. The function retrieves constraint candidates from the pricing subsystem (cxf_pricing_get_constr_candidates, P3.18)."
- **Code does:** Iterates over ALL variables/constraints directly rather than using pricing candidate retrieval.
- **File:** `src/simplex/post.c:134-145, 151-201`
- **Impact:** Less efficient -- scans full problem instead of pricing candidates. May process variables/constraints the pricing system considers irrelevant.

### [V19] cxf_simplex_phase_end -- Fixed-size modified array
- **Spec says:** Recompute activity bounds for all modified constraints.
- **Code does:** Uses `int modified[256]` fixed-size array. If more than 256 constraints are modified, later ones are silently dropped.
- **File:** `src/simplex/post.c:126-127`
- **Impact:** On problems with >256 constraints, activity bounds may not be recomputed for all modified constraints, leading to stale data.

### [V20] cxf_simplex_phase_end -- Incomplete Phase I->II transition
- **Spec says:** phase_end participates in detecting Phase I->II transition conditions (primal feasibility achieved + no dual infeasibility among free variables).
- **Code does:** Comment at line 244: "Phase I -> Phase II transition is handled by cxf_check_phase_one_end in the orchestrator (solve_lp.c), not here." The function only does the free-variable dual feasibility check, which is compliant, but the spec assigns additional transition responsibilities to phase_end.
- **File:** `src/simplex/post.c:244-246`
- **Impact:** The transition logic is split differently than the spec envisions. Functionally equivalent but architecturally different.

### [V21] cxf_simplex_refine -- Missing Pass 2 (basic variable recovery)
- **Spec says:** "Pass 2: Basic variable recovery. The function scans basic variables to identify those that have drifted near their upper bounds ... delegates to cxf_pivot_primal."
- **Code does:** Pass 2 is explicitly disabled with comment "recovery pivots change the basis and can move the solution to a suboptimal vertex."
- **File:** `src/simplex/refine.c:98-101`
- **Impact:** Basic variables near upper bounds are not recovered via cxf_pivot_primal. The comment justifies this as intentional to preserve optimality.

### [V22] cxf_simplex_refine -- Missing unboundedness check
- **Spec says:** "If the target bound is infinite, the variable cannot be fixed and the problem is unbounded in that direction. The function returns the unbounded code."
- **Code does:** No unboundedness check. Variables with infinite bounds are simply skipped (the snap logic only fires for AT_LOWER with finite lb, or AT_UPPER with finite ub).
- **File:** `src/simplex/refine.c:66-71`
- **Impact:** Unbounded detection at refinement time is missing. Should be caught earlier.

### [V23] cxf_transition_to_phase_two -- Missing pricing state reset via P3.21 spec
- **Spec says:** Transition Step 3: "pricing subsystem's candidate sets and tolerance levels are reinitialized." Transition Step 6: "optimality tolerance used for Phase II termination may differ from feasibility tolerance."
- **Code does:** Calls `cxf_pricing_invalidate` and `cxf_pricing_set_level(0)` which is a reasonable implementation of the reset. However, no tolerance adjustment (Step 6) -- uses same tolerances throughout.
- **File:** `src/simplex/phase_one.c:223-226`
- **Impact:** Minor. The pricing reset is functionally present. Tolerance adjustment is an optional optimization.

### [V24] cxf_check_phase_one_end -- Tolerance mutation
- **Spec says:** two_phase_method.md line 142: "solver may attempt additional iterations with tighter tolerances."
- **Code does:** Temporarily mutates `env->optimality_tol *= 0.01` to search for improving directions, then restores it.
- **File:** `src/simplex/phase_loop.c:109-114`
- **Impact:** The spec says "attempt additional iterations" but the code only re-checks for improving directions with tighter tolerances without actually iterating. If found, it returns 1 (continue Phase I) and the outer loop will iterate. The env mutation is safe (restored immediately) but architecturally questionable.

### [V25] compute_phase1_objective -- No tolerance in violation check
- **Spec says:** two_phase_method.md Phase I Determination Step 1: violations are measured as x < lb - epsilon_feas or x > ub + epsilon_feas.
- **Code does:** `compute_phase1_objective` in phase_loop.c:32-39 counts violations without tolerance: `if (x < lb) ... else if (x > ub)`.
- **File:** `src/simplex/phase_loop.c:32-39`
- **Impact:** Near-feasible variables contribute to the Phase I objective when they should be considered feasible. This could cause false non-zero Phase I objectives and delay transition to Phase II. Note: `cxf_setup_phase_one` (phase_one.c:135-139) correctly uses CXF_FEASIBILITY_TOL for the initial w-coefficient assignment, so this inconsistency only affects the fresh recomputation in phase_loop.c.

### [V26] cxf_setup_phase_one -- Uses global constant instead of env tolerance
- **Spec says:** Feasibility tolerance from the environment parameter set.
- **Code does:** Uses `CXF_FEASIBILITY_TOL` (a compile-time constant) instead of `env->feasibility_tol`.
- **File:** `src/simplex/phase_one.c:135, 139`
- **Impact:** If the environment tolerance is changed at runtime, Phase I setup ignores it. However, cxf_setup_phase_one does not receive an env parameter, so it cannot access it.

### [V27] cxf_setup_phase_one -- Missing env parameter
- **Spec says:** The Phase I determination uses "epsilon_feas" from the tolerance parameter set.
- **Code does:** Function signature is `cxf_setup_phase_one(SolverState *state)` -- no env parameter.
- **File:** `src/simplex/phase_one.c:35`
- **Impact:** Cannot access runtime tolerance configuration.

### [V28] cxf_setup_phase_one -- Slack computation uses O(n*m) linear scan
- **Spec says:** x_B = B_0^{-1} b (Phase I Determination, Overview).
- **Code does:** For each constraint row, scans all n columns via CSC to find entries in that row. This is O(n*nnz) total instead of a proper sparse matrix-vector multiply.
- **File:** `src/simplex/phase_one.c:74-86`
- **Impact:** Performance issue on large problems. Correct but slow.

### [V29] cxf_solve_lp -- Missing cxf_simplex_iterate call
- **Spec says:** Phase 6 inner loop step 2: "cxf_simplex_iterate (P3.20): Execute progress logging and bookkeeping."
- **Code does:** Calls `cxf_log_iteration_progress` instead of `cxf_simplex_iterate`.
- **File:** `src/simplex/solve_lp.c:172`
- **Impact:** If cxf_simplex_iterate does more than logging (the spec says "progress logging and bookkeeping"), some bookkeeping may be missing.

### [V30] cxf_solve_lp -- Proactive perturbation on early iterations
- **Spec says:** Phase 6 step 4: "On early iterations only, apply anti-cycling perturbation if the EXPAND procedure determines that the solver is stalling."
- **Code does:** Applies perturbation unconditionally in the first 2 iterations of round 0 (`round == 0 && state->iteration <= 2`), without checking for stalling.
- **File:** `src/simplex/solve_lp.c:184-185`
- **Impact:** Proactive perturbation may introduce unnecessary bound widening. The spec says perturbation should be reactive to stalling detection, not proactive.

### [V31] cxf_solve_lp -- Activity bounds computed BEFORE preprocess
- **Spec says:** simplex_phases.md Module-Level Notes: order is (3) preprocess, (4) setup (activity bounds).
- **Code does:** Calls `cxf_simplex_setup` (activity bounds) at line 148 BEFORE `cxf_simplex_preprocess` at line 151.
- **File:** `src/simplex/solve_lp.c:148-151`
- **Impact:** Activity bounds are computed before variable fixing, then preprocess recomputes them if any variables were fixed (setup.c:226-228). Functionally correct but wasteful -- the first computation is thrown away.

---

## Missing Functions (in spec but not implemented)

### cxf_solver_dispatch (P3.25)
- **Spec says:** Algorithm routing function that determines simplex/barrier/concurrent/PDHG based on model structure.
- **Status:** Not implemented. cxf_solve_lp is called directly.
- **Impact:** No algorithm selection, no barrier/concurrent support, no presolve-solve-uncrush cycle.

### cxf_simplex_cleanup (P3.22)
- **Spec says:** Post-solve constraint-based implied bound tightening and variable fixing, then frees temporary arrays. Nine temporary arrays, 10-phase algorithm.
- **Status:** Not implemented. `cxf_simplex_postsolve` (in cleanup.c) is a simplified substitute that restores preprocessed variables and clamps basic variables.
- **Impact:** No implied-bound propagation post-solve, no constraint-to-equality conversion.

---

## Extra Functions (in code but not in spec)

### cxf_setup_phase_one (phase_one.c)
- Not a named function in any spec module. The spec describes Phase I setup as part of the cxf_solve_lp flow (Phase I Determination in two_phase_method.md). The implementation extracts this into a separate function.
- **Assessment:** Reasonable decomposition. Not a violation.

### cxf_transition_to_phase_two (phase_one.c)
- Not a named function in any spec module. The spec describes transition as coordinated between phase_end (P3.21) and solve_lp (P3.25).
- **Assessment:** Reasonable decomposition. Not a violation.

### cxf_check_phase_one_end (phase_loop.c)
- Not a named function in any spec module. Implements Phase I termination logic described in two_phase_method.md.
- **Assessment:** Reasonable decomposition. Not a violation.

### compute_phase1_objective (phase_loop.c, static)
- Internal helper. Not in spec.
- **Assessment:** Reasonable. But see V25 re tolerance mismatch.

### has_improving_direction (phase_loop.c, static)
- Internal helper for infeasibility determination.
- **Assessment:** Implements "Phase I optimality with positive objective" from two_phase_method.md. Reasonable.

### cxf_simplex_postsolve (cleanup.c)
- Not in spec. Simplified substitute for cxf_simplex_cleanup.
- **Assessment:** Partial implementation of spec functionality.

### cxf_compute_activity_bounds (setup.c)
- Internal helper that cxf_simplex_setup delegates to. The spec describes this as the behavior of cxf_simplex_setup itself.
- **Assessment:** Reasonable decomposition.

### cxf_simplex_get_status, cxf_simplex_get_iteration, cxf_simplex_get_phase, cxf_simplex_get_objval, cxf_simplex_set_iteration_limit, cxf_simplex_get_iteration_limit (context.c)
- Accessor functions not in the spec.
- **Assessment:** Utility functions. Not violations.

---

## Notes

### Architectural Differences

1. **Phase I/II transition ownership.** The spec places transition responsibility jointly on cxf_simplex_phase_end (P3.21) and cxf_solve_lp (P3.25). The implementation splits this into cxf_check_phase_one_end (orchestration in solve_lp.c) and cxf_transition_to_phase_two (state transformation in phase_one.c). cxf_simplex_phase_end only handles constraint cleanup and free-variable dual feasibility checks. This is a reasonable decomposition but differs from the spec's assignment of responsibilities.

2. **cxf_simplex_final is a destructor, not a fixer.** The spec's cxf_simplex_final is a complex dual-feasibility variable fixing function. The implementation's cxf_simplex_final is a simple memory deallocator. The CS fix logic is partially in solve_lp.c but without the spec's constraint verification.

3. **No dual simplex.** The implementation is primal simplex only. The spec defaults to dual simplex for LP.

### Severity Assessment

| Severity | Count | Examples |
|----------|-------|---------|
| **High** (behavioral deviation) | 5 | V1, V7, V13, V25, V30 |
| **Medium** (missing functionality) | 9 | V2, V3, V8, V9, V14, V15, V16, V18, V21 |
| **Low** (minor/expected) | 17 | V4, V5, V6, V10, V11, V12, V17, V19, V20, V22, V23, V24, V26, V27, V28, V29, V31 |

### Priority Fixes

1. **V25 (High):** `compute_phase1_objective` missing feasibility tolerance -- easy fix, could cause Phase I to not terminate correctly on near-feasible problems.
2. **V13 (High):** cxf_simplex_final should do variable fixing per spec, not just deallocation. The CS fix in solve_lp.c is a partial workaround missing constraint verification.
3. **V30 (High):** Proactive perturbation in early iterations may destabilize the solver on well-behaved problems.
4. **V7 (High):** Missing cxf_simplex_cleanup means no post-solve bound propagation.
5. **V14 (Medium):** Missing eta records in preprocessing prevents future warm-start.
