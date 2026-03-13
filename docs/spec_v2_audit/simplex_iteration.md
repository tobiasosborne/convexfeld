# Spec V2 Audit: Simplex Iteration

**Auditor:** Claude Opus 4.6 (read-only audit)
**Date:** 2026-03-13
**Scope:** simplex_iteration.md, revised_simplex.md, harris_ratio_test.md

## Files Reviewed

### Spec Files
- `docs/specs-v2/specs/modules/simplex_iteration.md`
- `docs/specs-v2/specs/algorithms/revised_simplex.md`
- `docs/specs-v2/specs/algorithms/harris_ratio_test.md`
- `docs/specs-v2/specs/reference/numerical_stability.md` (Sections A-D)

### Implementation Files
- `src/simplex/step.c` (cxf_simplex_step, cxf_apply_pivot, pricing_and_ftran, post_pivot_updates)
- `src/simplex/step2.c` (cxf_simplex_step2)
- `src/simplex/step3.c` (cxf_simplex_step3)
- `src/simplex/iterate.c` (cxf_log_iteration_progress)
- `src/simplex/bfrt.c` (cxf_bfrt_extend_step, cxf_bfrt_set_status)
- `src/simplex/pivot_primal.c` (cxf_pivot_primal)
- `src/simplex/pivot_update.c` (cxf_pivot_update, cxf_pivot_check)
- `src/simplex/reduced_costs.c` (cxf_compute_reduced_costs)
- `src/simplex/ratio_test.c` (cxf_ratio_test)
- `src/simplex/post.c` (cxf_simplex_post_iterate, cxf_simplex_phase_end)
- `src/simplex/simplex_internal.h`

---

## Compliant Functions

### cxf_compute_reduced_costs (reduced_costs.c)
- Signature matches spec (revised_simplex.md Step 5/Step 0.5): computes pi = B^{-T} c_B via BTRAN, then d_j = c_j - pi^T A_j for all nonbasic variables.
- Correctly handles structural (CSC dot product) and slack/surplus (diag_coeff) variables.
- Basic variables get d_j = 0 per spec.

### cxf_pivot_update (pivot_update.c)
- Implements cancellation-safe incremental activity bound updates per numerical_stability.md Section B.
- Conservative rounding factors (1 + 1e-12, 1 - 1e-12) correctly widen bounds on cancellation detection.
- Handles finite-to-finite, finite-to-infinite, and infinite-to-finite transitions correctly.
- Tracks unbounded variable counts (posUnbdCount, negUnbdCount).

### cxf_simplex_step2 (step2.c) -- Largely Compliant
- Implements variable-side bound propagation per spec.
- Two-stage infeasibility detection with activity recomputation (Stage 2 confirmation).
- Uses Savelsbergh (1994) implied bound technique.
- Calls cxf_pivot_update for activity bound maintenance and cxf_pricing_mark_dirty for notification.
- Increments flip_count per spec.

### cxf_simplex_step3 (step3.c) -- Largely Compliant
- Implements constraint-side bound propagation per spec.
- Two-stage infeasibility detection with fresh activity recomputation.
- Uses cxf_pricing_constr_candidates_v2 for candidate retrieval.
- Calls tighten_bound -> cxf_pivot_update -> cxf_pricing_mark_dirty chain.
- Increments bounds_propagated per spec.

### cxf_ratio_test (ratio_test.c) -- Largely Compliant
- Harris two-pass structure: Pass 1 finds theta_max with relaxed band, Pass 2 selects largest pivot among strict ratios within theta_max.
- Band = feasibility_tol per harris_ratio_test.md: "epsilon is the feasibility tolerance."
- Bland's rule tie-breaking by smallest variable index in Pass 2.
- Pre-checks bound validity (lb > ub + tol -> INFEASIBLE).
- Delegates to BFRT Stage 3 via cxf_bfrt_extend_step.

### cxf_bfrt_extend_step (bfrt.c) -- Largely Compliant
- Iteratively flips bounded variables, adjusts theta, searches for next blocker.
- Respects flip cap (MAX_BFRT_FLIPS = 10).
- Correctly checks flip eligibility: finite bounds on both sides, gap >= feasTol.
- Disabled under Bland's rule per spec.

---

## VIOLATIONS

### [V1] cxf_simplex_iterate -- Renamed Without Alias
- **Spec says:** Function named `cxf_simplex_iterate` with signature `(Model*, SolverState*) -> void`. Purpose: progress logging and callback notification.
- **Code does:** Function named `cxf_log_iteration_progress` with signature `(CxfModel*, SolverState*) -> void` in `iterate.c`. No function named `cxf_simplex_iterate` exists anywhere in the codebase.
- **File:** `src/simplex/iterate.c:33`
- **Severity:** Medium. The spec explicitly documents that despite the name being misleading, it IS the canonical name. Renaming without a compatibility alias means any caller using the spec name will fail to link.

### [V2] cxf_simplex_iterate -- Missing Thread Count Normalization
- **Spec says:** Step 2: "The elapsed time is normalized by dividing by the thread count (so that parallel solves do not produce proportionally more messages) and rounded to the nearest integer second."
- **Code does:** Simple wall-clock comparison `(now - state->last_log_time) >= LOG_INTERVAL_SEC` with no thread count normalization and no rounding to integer seconds.
- **File:** `src/simplex/iterate.c:43`
- **Severity:** Low. Single-threaded solver currently, but deviates from spec algorithm.

### [V3] cxf_simplex_iterate -- Missing Presolve Message Format
- **Spec says:** Step 3: Message format depends on solve mode -- preprocessing reports phase and elapsed time; presolve reports rows/columns removed and elapsed time.
- **Code does:** Always emits `"Iter %d: phase=%d obj=%.6e"` regardless of solve mode. Does not report presolve statistics or preprocessing phase.
- **File:** `src/simplex/iterate.c:48-49`
- **Severity:** Low. Cosmetic/logging difference.

### [V4] cxf_simplex_post_iterate -- Signature Mismatch
- **Spec says:** Signature is `(Model*, SolverState*, int*) -> int`. First parameter is a Model pointer for model-level settings (stall detection config, thread count, logging config).
- **Code does:** Signature is `(SolverState*, CxfEnv*, int*) -> int`. Takes CxfEnv instead of Model.
- **File:** `src/simplex/post.c:37-38`
- **Severity:** Medium. The spec explicitly distinguishes between monitoring functions (which take Model) and computational functions (which take SolverState + Env). This deviation loses access to model-level stall detection configuration.

### [V5] cxf_simplex_post_iterate -- Stall Logic Inverted
- **Spec says:** "If **either** the column check or the row check fails, the stall flag is set to 1." A check "fails" when `eliminated > threshold` (i.e., progress exceeds expectations -- this seems counterintuitive but the spec is explicit: "the solver has completed a full refactorization interval but has not eliminated or fixed a proportional fraction of the problem dimensions").
- **Code does:** Sets `*outStall = 1` when `cols_eliminated > col_thresh || rows_eliminated > row_thresh`.
- **Analysis:** The spec text is self-contradictory. It says stalling means "insufficient progress" but the mathematical condition `eliminated <= alpha * n + beta` is described as "passing" (adequate progress), and violation of this (i.e., `eliminated > threshold`) means the check "fails." Yet the prose says "stalling means insufficient progress" which would be `eliminated < threshold`. The code uses `>` which matches the formula's "violation" interpretation but contradicts the prose definition of stalling as "insufficient progress." This needs clarification from the spec author.
- **File:** `src/simplex/post.c:65-66`
- **Severity:** High (ambiguous). The code may have the stall condition exactly backwards. If "stall" means "too little progress," the condition should be `cols_eliminated < col_thresh`, not `>`.

### [V6] cxf_simplex_post_iterate -- Missing Time Limit Check
- **Spec says:** Check 2: "The function calls a centralized status-checking routine that evaluates time limits, iteration limits, memory limits, and user abort signals."
- **Code does:** Only checks iteration limit (`state->iteration >= state->max_iterations`). Does not check time limits, memory limits, or user abort signals.
- **File:** `src/simplex/post.c:89-90`
- **Severity:** Medium. Missing time limit is a functional gap -- solver cannot honor time limits.

### [V7] cxf_simplex_post_iterate -- Missing User Interrupt Check
- **Spec says:** Check 4: "The function checks for user-initiated interrupts (such as Ctrl+C in console mode or cancel requests from GUI interfaces)."
- **Code does:** Has only a placeholder comment `/* Check 4: User interrupt (placeholder) */` with no implementation.
- **File:** `src/simplex/post.c:92`
- **Severity:** Medium. No graceful termination on user interrupt from post_iterate (though iterate.c callback does handle this).

### [V8] cxf_simplex_step -- Superbasic Variable Handling Incomplete
- **Spec says:** Phase 3.4 (Free variable handling): "If the variable is free (superbasic, between bounds), the function checks for unboundedness based on the reduced cost direction. If bounded, it creates a pivot eta record and updates the basis state directly."
- **Code does:** Pricing in `pricing_and_ftran` filters superbasic variables with `state->work_dj[entering] > 0.0` for direction but then routes them through the standard ratio test path. No special-case handling for free variables that creates a pivot eta directly without ratio test.
- **File:** `src/simplex/step.c:350-352`
- **Severity:** Medium. The spec describes a distinct code path for free variables; the implementation treats them identically to bounded variables in the ratio test.

### [V9] cxf_simplex_step -- Missing Dual Simplex Path
- **Spec says:** Phase 3.3: "If the solver is in dual simplex mode and the variable has special constraint flags (SOS, indicators), the function delegates to cxf_pivot_special (P3.19) for special-case handling."
- **Code does:** No dual simplex mode check. No call to cxf_pivot_special anywhere in the step function.
- **File:** `src/simplex/step.c` (entire file)
- **Severity:** Low. The solver appears to implement only primal simplex currently, so this is a missing feature rather than incorrect behavior.

### [V10] cxf_simplex_step -- Pricing Tolerance Levels Wrong
- **Spec says:** Phase 1 (Tolerance selection): "initial pricing phase: loose tolerance... fallback phase: very tight tolerance... standard pricing phase: intermediate tolerance."
- **Code does:** Level 0 = `optimality_tol * 10` (loose), Level 1 = `optimality_tol` (standard), Level 2 = `optimality_tol * 0.1` (tight). This maps initial=loose, standard=intermediate, aggressive=tight.
- **Spec says (revised_simplex.md Step 1):** "Initial phase (fast): ~1e-6", "Standard phase: ~1e-10", "Aggressive phase: ~1e-9".
- **Code does:** With default optimality_tol = 1e-6: Level 0 = 1e-5, Level 1 = 1e-6, Level 2 = 1e-7. The spec's "standard phase" at 1e-10 is far tighter than the code's Level 1 at 1e-6.
- **File:** `src/simplex/step.c:270-273`
- **Severity:** Medium. The tolerance levels differ significantly from the spec's suggested values. The spec's "fallback" (very tight) is not the tightest level in the code; instead the code goes loose->standard->tight which is the spec's "initial->standard->aggressive" mapping, not "initial->fallback->standard."

### [V11] cxf_simplex_step -- Missing cxf_pricing_candidates Call
- **Spec says:** Phase 2: "The function calls cxf_pricing_candidates (P3.17) to obtain the set of non-basic variables."
- **Code does:** Calls `cxf_pricing_candidates_v2` (different function name), and has three separate fallback paths (Bland's rule, pricing system, manual scan). When the V2 pricing system returns 0 candidates, it falls back to a full scan with most-negative-reduced-cost selection.
- **File:** `src/simplex/step.c:289`
- **Severity:** Low. The function name differs (v2 suffix), but this appears to be an intentional API evolution. The fallback-to-full-scan is a reasonable implementation choice.

### [V12] cxf_simplex_step -- BFRT Flip Application Inconsistent With Spec
- **Spec says (harris_ratio_test.md Stage 3, step 6):** "For each variable in the flip set F: (a) Set the variable's value to its new bound. (b) Update the constraint activities... (c) Negate the relevant row coefficients in the constraint matrix... (d) Update the variable's status."
- **Code does:** In `step.c:613-640`, for flipped basic variables, the code sets their x values based on pivot column sign but does NOT: negate row coefficients in the constraint matrix, update constraint activities incrementally, or update flipped variable statuses explicitly. The flipped variables remain basic.
- **File:** `src/simplex/step.c:613-640`
- **Severity:** High. The spec requires row coefficient negation and activity updates for each flip. The code only sets x values to bounds for flipped rows. This means the constraint matrix is inconsistent after flips -- the algebraic meaning of the rows has changed but the matrix has not been updated.

### [V13] cxf_simplex_step -- DSE Weight Update Uses Simplified Formula
- **Spec says (revised_simplex.md Step 6):** Full DSE: `gamma_j' = gamma_j - 2*(tau_j/alpha_{q,r})*(sigma_j - tau_j) + (tau_j/alpha_{q,r})^2 * (gamma_q - 2*alpha_{q,r} + 1)` with sigma_j requiring an additional BTRAN solve. The spec notes the "one-BTRAN approximation" sets sigma_j = tau_j, eliminating the cross-term: `gamma_j' = gamma_j + (tau_j/alpha_{q,r})^2 * (gamma_q - 2*alpha_{q,r} + 1)`.
- **Code does:** Uses the simplified one-BTRAN formula: `nw = pricing->weights[j] + r2 * dse_factor` where `dse_factor = gamma_q - 2.0 * pivotElement + 1.0`.
- **File:** `src/simplex/step.c:215-216`
- **Severity:** Low. The spec explicitly documents this as an acceptable approximation: "Periodic exact recomputation of all weights from scratch (e.g., at each basis refactorization) corrects this drift." The code does recompute weights at refactorization. This is a valid implementation choice, not a true violation.

### [V14] cxf_simplex_step -- Devex Reference Framework delta_j Always 1
- **Spec says (revised_simplex.md Step 6):** Devex: `gamma_j' = max(epsilon_devex * gamma_j, (tau_j/alpha_{q,r})^2 + delta_j)` where "delta_j = 1 if variable j is in the current reference framework R, and delta_j = 0 otherwise."
- **Code does:** `dj = (pricing->ref_framework && pricing->ref_framework[j]) ? 1.0 : 0.0;` then `dv = r2 + dj;` and `dc = 0.99 * pricing->weights[j];` then `pricing->weights[j] = (dv > dc) ? dv : dc;`
- **Analysis:** The code correctly implements the spec formula. The ref_framework array tracks membership. Compliant.
- **Severity:** None (false alarm on initial inspection; marking as compliant).

### [V15] cxf_simplex_step -- Objective Update Sign for AT_UPPER
- **Spec says (revised_simplex.md Step 4.4):** "Update the objective value: z += theta * d_q."
- **Code does (Phase II):** `state->obj_value += entering_sign * d_entering * stepSize` where `entering_sign = -1` for AT_UPPER variables.
- **Analysis:** When entering from upper bound, the variable decreases, so the step direction is negative. The spec's theta is always non-negative and d_q carries the sign. The code's `entering_sign * d_entering * stepSize` adjusts for direction. For AT_UPPER, d_q > 0 (else it wouldn't be a candidate), entering_sign = -1, so objective update is `(-1) * d_q * theta` which is negative -- correct for a variable decreasing from upper bound with positive reduced cost. Compliant.

### [V16] cxf_simplex_step2/step3 -- Missing Bound-Change Eta Records
- **Spec says:** Both step2 and step3 create "lightweight bound-change eta records" for each processed candidate. The spec table says step2 and step3 create BOUND_CHANGE eta type records storing "variable index, constraint index, flip classification, pivot coefficient, and ratio value."
- **Code does:** Neither step2.c nor step3.c creates any eta records. The tighten_bound helper updates bounds and calls cxf_pivot_update and cxf_pricing_mark_dirty, but no eta record allocation occurs.
- **File:** `src/simplex/step2.c` (entire file), `src/simplex/step3.c` (entire file)
- **Severity:** Medium. Missing eta records means bound changes from propagation cannot be tracked/undone by the basis reconstruction system. For pure LP solving this may be acceptable (the solver appears to not use bound-change etas for anything currently), but it deviates from the spec's data flow.

### [V17] cxf_simplex_step2 -- Not Using Pricing Queue for Candidates
- **Spec says:** "Step 1: Candidate retrieval. The function obtains the step2 candidate list from the pricing subsystem. Only variables with a specific pricing status (indicating they were deferred from step for further processing) are eligible."
- **Code does:** Iterates over ALL variables `j = 0..n-1` and checks `pricing->var_dirty[j]`. The dirty flag is a general-purpose dirty marker, not a specific "deferred from step" status.
- **File:** `src/simplex/step2.c:80-81`
- **Severity:** Low. The code uses dirty flags as a proxy for "needs processing" which is functionally similar, but the spec implies a specific deferred-status queue populated by cxf_simplex_step.

### [V18] cxf_simplex_step2/step3 -- Missing cxf_pivot_bound Call
- **Spec says:** "Variables whose bounds are tightened from both sides (effectively fixed) have been processed via cxf_pivot_bound (P3.19)."
- **Code does:** When lb > ub + tol (bounds crossed), step2 either returns CXF_INFEASIBLE or restores safe bounds. There is no code path that calls cxf_pivot_bound when both bounds are tightened to effectively fix a variable. In step3, same: no cxf_pivot_bound call.
- **File:** `src/simplex/step2.c`, `src/simplex/step3.c`
- **Severity:** Medium. Variables that become effectively fixed through propagation are not properly eliminated, potentially leading to degenerate pivots on near-fixed variables.

### [V19] cxf_ratio_test -- Pass 2 Uses Strict Ratio Incorrectly
- **Spec says (harris_ratio_test.md Stage 2, Pass 2):** "Among all candidates i satisfying: `slack_i / |d_i| <= theta_max`, select the candidate r that maximizes |d_r|."
- **Code does:** Pass 2 calls `row_ratio` with `band=0.0` (strict ratio), then checks `if (ratio > minRatio) continue;`. The `minRatio` from Pass 1 was computed with `band = feasTol`. So Pass 2 checks `strict_ratio <= relaxed_theta_max`. This is correct per spec.
- **However:** The `row_ratio` function with band=0 computes `(x - lb) / sd` which is `slack / (s * d_i)`, not `slack / |d_i|`. The sign handling through `s * d_i` means the ratio can be computed differently than the spec's `slack_i / |d_i|` formula.
- **Analysis:** Actually the ratio function handles both bound directions (lower and upper) and takes the minimum. The signed version `s * d_i` correctly determines which bound is approached. The result is equivalent to the spec's unsigned formulation. Compliant on deeper inspection.

### [V20] cxf_ratio_test -- row_ratio Handles Variables Past Bounds
- **Spec says (harris_ratio_test.md Stage 1):** Ratios computed from `(x_i - lb_i)` or `(ub_i - x_i)`. "theta_i >= 0 for a feasible basis."
- **Code does:** In `row_ratio`, when `sd > 0`, it checks `if (ub < inf && x > ub + feasTol)` and computes `(x - ub + band) / sd`. Similarly when `sd < 0`, checks `if (lb > -inf && x < lb - feasTol)` and computes `(x - lb - band) / sd`. These extra branches handle variables that are past their bounds (infeasible basic variables).
- **Severity:** Low. This is a reasonable extension not explicitly forbidden by the spec, but the spec assumes a feasible basis where these branches would not trigger. In Phase I, basic variables may be past bounds, making this necessary.

### [V21] cxf_pivot_primal -- Uses model_ref->matrix Instead of Working Arrays
- **Spec says (simplex_iteration.md Phase 3.2):** Tight-bound variables are "processed by cxf_pivot_primal (P3.19) for safe elimination of near-fixed variables."
- **Code does:** In Step 5, cxf_pivot_primal updates `matrix->rhs` (the original model's RHS) rather than `state->work_rhs` (the working copy). The spec's precondition is "Working copies of bounds and RHS have been created so that the algorithm may modify them without affecting the original model data."
- **File:** `src/simplex/pivot_primal.c:185-211`
- **Severity:** High. Modifying the original model matrix's RHS instead of the working copy violates a fundamental spec invariant. This could corrupt the model data, making the original problem unrecoverable.

### [V22] cxf_pivot_primal -- Missing Eta Vector Creation
- **Spec says:** pivot_primal should handle safe elimination. The file's own TODO (line 213-230) lists missing: eta vector creation, PWL handling, QP handling, pricing state update.
- **Code does:** Acknowledged as deferred. No eta vector is created for the elimination.
- **File:** `src/simplex/pivot_primal.c:213-230`
- **Severity:** Medium. The basis representation is not updated when a variable is eliminated via pivot_primal, so subsequent FTRAN/BTRAN operations use a stale basis. This may work if the variable was nearly fixed (elimination has minimal effect), but is technically incorrect.

### [V23] cxf_pivot_primal -- Infeasibility Threshold Wrong
- **Spec says (numerical_stability.md Section C):** "When a variable's lower bound equals its upper bound within the bound equality tolerance (approximately 1e-10), the variable is treated as fixed."
- **Code does:** Returns CXF_INFEASIBLE when `fabs(boundRange) < 2.0 * tolerance` where tolerance is the pricing tolerance (which can be as large as 1e-5). This is far looser than the spec's ~1e-10 bound equality tolerance.
- **File:** `src/simplex/pivot_primal.c:101`
- **Severity:** Medium. With pricing_tol = 1e-5 at level 0, any variable with bound range < 2e-5 would be declared infeasible rather than fixed, potentially causing false infeasibility reports.

### [V24] cxf_pivot_check -- Does Not Account for Entering Direction
- **Spec says (harris_ratio_test.md Stage 1):** "let s be +1 if the entering variable increases, -1 if it decreases. Then basic variable i is driven toward its lower bound when s * d_i > 0, and toward its upper bound when s * d_i < 0."
- **Code does:** `if (pivotCol[i] > 0)` checks sign of d_i directly, not s * d_i. No entering direction parameter.
- **File:** `src/simplex/pivot_update.c:160-161`
- **Severity:** Low. cxf_pivot_check appears to be a simplified version not used in the main ratio test path (cxf_ratio_test handles direction correctly). It may be dead code or used only for initialization.

### [V25] cxf_simplex_step -- MAX_CANDIDATES = 10 Limits Pricing
- **Spec says (revised_simplex.md Step 1):** Partial pricing scans sections; the best candidate across recently scanned sections is selected.
- **Code does:** Hard caps at `MAX_CANDIDATES = 10` candidates. If the V2 pricing system returns more than 10, excess candidates are silently dropped.
- **File:** `src/simplex/step.c:27,301`
- **Severity:** Low. This is a practical limit, but it means the pricing system's candidate ranking may be subverted -- the 11th-best candidate from v2 pricing could be globally better than candidates 1-10 after filtering.

### [V26] cxf_simplex_step -- BFRT MAX_BFRT_FLIPS = 10 Hard Limit
- **Spec says (harris_ratio_test.md Stage 3):** No explicit limit on number of flips. "Multiple consecutive bound flips may occur before a standard pivot is performed."
- **Code does:** Hard-coded `MAX_BFRT_FLIPS = 10` in both step.c and bfrt.c. After 10 flips, the loop stops regardless of whether more flips could improve the step.
- **File:** `src/simplex/step.c:26`, `src/simplex/bfrt.c:69`
- **Severity:** Low. Practical limit for performance/memory, but could reduce effectiveness of BFRT on problems with many bounded variables. The spec says "multiple" which is vague enough to permit a cap.

### [V27] cxf_simplex_step -- Phase 1 Recomputes ALL Reduced Costs
- **Spec says (revised_simplex.md Step 4.6):** "Set the entering variable's reduced cost to zero (it is now basic)." The spec describes incremental RC update via the BTRAN result.
- **Code does:** In Phase I, `post_pivot_updates` calls `cxf_compute_reduced_costs(state)` which recomputes ALL reduced costs from scratch, rather than incremental update.
- **File:** `src/simplex/step.c:516-517`
- **Severity:** Low. Full recomputation in Phase I is actually BETTER than incremental (avoids drift), and the spec's Phase I uses a dynamic objective that changes after each pivot (work_obj recomputed in Phase 6), making incremental update impossible anyway. This is a correct implementation choice.

### [V28] cxf_simplex_step3 -- Implied Bound Formula Different From Spec
- **Spec says (revised_simplex.md Edge Cases):** "If a_ik > 0: x_k <= (b_i - MinActivity_{without k}) / a_ik"
- **Spec says (simplex_iteration.md Step3):** "The constraint's current activity divided by the pivot coefficient yields the implied value."
- **Code does:** For `<=` with `a > 0`: `impl = lb - min_act / a`. This is `lb - L_act / a`, not `lb + (rhs - L_act) / a`. The code does NOT subtract the RHS -- it computes `lb - min_act/a` instead of `(rhs - min_act) / a + lb` or similar.
- **File:** `src/simplex/step3.c:161-162`
- **Severity:** High. The implied bound formula appears to be wrong. The standard formula from Savelsbergh (1994) is `x_k <= (b_i - L_i + a_ik * lb_k) / a_ik = lb_k + (b_i - L_i) / a_ik`, where L_i is the minimum activity INCLUDING variable k's contribution. The code computes `lb - min_act / a` which would be correct only if min_act already represents `rhs - minActivity_without_k` (i.e., the residual activity). If min_act is the raw minimum activity, the formula is missing the RHS term.

**Note:** step2.c uses the SAME formula structure (`lb + (rhs_i - min_act) / a` in step2.c:164 vs `lb - min_act / a` in step3.c:161). Step2 includes the RHS; step3 does not. This inconsistency strongly suggests step3's formula is wrong.

### [V29] cxf_simplex_step -- Cycling Detection Threshold Not Spec-Aligned
- **Spec says (revised_simplex.md Step 8):** "Periodically, the algorithm takes a snapshot of the current basis and compares it to a later basis. If the basis has not changed significantly (measured by the fraction of basic variables that differ) after a configurable number of iterations, cycling is suspected."
- **Code does:** Uses `degenerate_count > 50` consecutive degenerate steps (stepSize < 1e-8) to trigger Bland's rule. No basis snapshot comparison.
- **File:** `src/simplex/step.c:593-600`
- **Severity:** Medium. The spec describes basis-snapshot-based cycling detection leading to perturbation. The code uses a simpler degenerate-step counter leading to Bland's rule. While Bland's rule prevents cycling, the spec explicitly states perturbation is preferred over Bland's rule: "Perturbation is preferred over Bland's rule (Bland, 1977) because Bland's rule, while guaranteeing finite termination, often leads to poor pivot choices and substantially increased iteration counts."

---

## Missing Functions (in spec but not implemented)

### cxf_simplex_iterate (by name)
The spec's `cxf_simplex_iterate` function is implemented as `cxf_log_iteration_progress` in iterate.c. No function with the spec name exists.

### cxf_pivot_special
Referenced in spec Phase 3.3 and 3.6 for dual simplex and special cases. Not called from any simplex iteration code.

### cxf_pricing_candidates (P3.17)
The spec references this; the code uses `cxf_pricing_candidates_v2` instead.

### cxf_pricing_cascade_update (P3.18)
The spec's Phase 5e says "the function notifies the pricing subsystem of the basis change via cascading updates (cxf_pricing_cascade_update, P3.18)." The code uses `cxf_pricing_update_var` and `cxf_pricing_update_constr` instead.

---

## Extra Functions (in code but not in spec)

### cxf_apply_pivot (step.c:73)
Low-level pivot helper not described in the spec. The spec says pivot is handled by `cxf_pivot_with_eta` (P3.16). The code uses `cxf_apply_pivot` as an intermediate wrapper that updates primal values before calling `cxf_pivot_with_eta`.

### pricing_and_ftran (step.c:246)
Static helper combining pricing + FTRAN + ratio test. Not in spec but is an internal decomposition of cxf_simplex_step.

### post_pivot_updates (step.c:470)
Static helper for post-pivot objective/RC/weight/pricing/refactor updates. Not in spec but is an internal decomposition.

### update_reduced_costs (step.c:150)
Incremental RC update via BTRAN. Not a separate spec function -- it implements part of revised_simplex.md Step 5 inline.

### update_rc_and_weights (step.c:177)
Fused RC + weight update. Performance optimization not in spec.

### compute_tau (step.c:123)
Common kernel for tau_j = rho^T a_j computation. Factored out for code reuse.

### cxf_simplex_phase_end (post.c:112)
Referenced in spec as P3.21 (simplex_phases module), not part of simplex_iteration module.

### cxf_pivot_check (pivot_update.c:141)
Step-length computation helper. The spec references cxf_pivot_check in P3.19 but the implementation here seems simplified (no entering direction).

---

## Notes

### Structural Decomposition
The spec describes `cxf_simplex_step` as a single monolithic function. The implementation decomposes it into `pricing_and_ftran`, the main `cxf_simplex_step` orchestrator, and `post_pivot_updates`. This is good engineering but means the spec's phase numbering (Phase 1-9 mentioned in comments) is spread across three functions.

### Return Code Convention
The spec defines return codes: success=0, infeasibility, unbounded, OOM. The code uses custom `ITERATE_*` codes (ITERATE_CONTINUE=0, ITERATE_OPTIMAL=1, ITERATE_INFEASIBLE=2, ITERATE_UNBOUNDED=3) which differ from the standard `CXF_*` error codes. `ITERATE_CONTINUE` maps to success, but `ITERATE_OPTIMAL` is 1 (not 0), creating a potential confusion with the spec's "Zero on success... no candidates (optimal): returns success with no state change."

### Tolerance Hardcoding
Several tolerances are hardcoded as macros (CXF_PIVOT_TOL=1e-9, CXF_FEASIBILITY_TOL=1e-6) in cxf_types.h rather than being read from the Environment or SolverState. The spec describes these as configurable parameters. The CxfEnv does have `feasibility_tol` and `optimality_tol` fields, but many places use the hardcoded macro instead.

### Phase I Objective Handling
The Phase I objective is recomputed from scratch after every pivot (step.c:482-496), including full work_obj[] reconstruction. This is correct per the project's Phase I architecture (MEMORY.md) but is more expensive than the spec's incremental update approach. It is also a defensive choice that prevents drift.

### step2.c and step3.c Duplicate tighten_bound
Both files contain identical `tighten_bound` static functions (step2.c:30-59, step3.c:33-62). This violates DRY but is not a spec compliance issue.
