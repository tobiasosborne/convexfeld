# Audit Report: Revised Simplex Algorithm
**Auditor:** Agent B1
**Date:** 2026-02-16
**Scope:** src/simplex/iterate.c, step.c, context.c, phase_steps.c
**Specs:** algorithms/revised_simplex.md

## Summary
- Total violations: 18
- Critical: 9 / Major: 6 / Minor: 3

## Violations

### V-01: Missing Phase Determination Logic
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (entire file)
- **Spec reference:** Step 0 (Initialization), lines 99-102
- **Description:** No implementation of phase determination during iteration
- **Expected (from spec):** "Determine initial phase: If x_B satisfies l_B <= x_B <= u_B within tolerance, enter Phase II. Otherwise, enter Phase I with a modified objective that penalizes infeasibility."
- **Actual (in code):** No phase checking or phase transition logic in iterate.c. The code assumes phase has been set externally but never validates or updates it during iteration.

### V-02: Missing Phase Transition Logic
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (entire file)
- **Spec reference:** Step 7 (Check Termination), lines 405-413
- **Description:** No Phase I to Phase II transition implemented
- **Expected (from spec):** "Feasibility achieved (Phase I): If all artificial variables have left the basis (or the Phase I objective is zero within tolerance), transition to Phase II by restoring the original objective coefficients and recomputing reduced costs."
- **Actual (in code):** No transition logic exists. The iteration function never checks if Phase I is complete or switches to Phase II.

### V-03: Missing Infeasibility Detection
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (entire file)
- **Spec reference:** Step 7 (Check Termination), lines 280
- **Description:** No infeasibility detection for Phase I
- **Expected (from spec):** "Infeasibility (Phase I): If the Phase I objective is strictly positive at optimality, the original problem has no feasible solution. Return INFEASIBLE."
- **Actual (in code):** Returns ITERATE_OPTIMAL on line 246 without checking if Phase I completed with positive objective (indicating infeasibility).

### V-04: Missing BTRAN for Pricing Weight Update
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (lines 169-248)
- **Spec reference:** Step 5 (BTRAN), lines 190-196; Step 6 (Update Pricing Weights), lines 199-272
- **Description:** No BTRAN call for steepest edge weight maintenance
- **Expected (from spec):** "Compute the leaving row of the basis inverse for use in updating steepest edge weights: rho = e_r^T B^{-1}... The vector rho is needed for the steepest edge weight update in Step 6."
- **Actual (in code):** No BTRAN call after pivot (Step 5 missing entirely). Pricing weight updates are never performed.

### V-05: Missing Steepest Edge Weight Update
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (entire file)
- **Spec reference:** Step 6 (Update Pricing Weights), lines 199-272
- **Description:** Steepest edge weights are never updated after pivots
- **Expected (from spec):** Detailed formulas for exact DSE update (lines 211-243) or Devex update (lines 245-269)
- **Actual (in code):** No weight update implementation. Weights would become stale after first iteration if initialized.

### V-06: Wrong Iteration Structure - Missing Step 5 (BTRAN)
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (lines 169-455)
- **Spec reference:** Complete iteration cycle, lines 78-86; Step 5, lines 190-196
- **Description:** The main iteration loop skips Step 5 (BTRAN) entirely
- **Expected (from spec):** 7-step cycle: "1. Price, 2. FTRAN, 3. Ratio test, 4. Pivot, 5. BTRAN, 6. Update pricing weights, 7. Check termination"
- **Actual (in code):** Implements: 1. Price (lines 169-247), 2. FTRAN (line 262), 3. Ratio test (line 268), 4. Pivot (line 375), 5. [MISSING], 6. [MISSING], 7. Update reduced costs (lines 388-445), 8. Refactor check (lines 449-452). Steps 5 and 6 are completely absent.

### V-07: Wrong Reduced Cost Update Algorithm
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (lines 388-445)
- **Spec reference:** Step 4 (Pivot), line 186; Incremental update in basis operations
- **Description:** Recomputes ALL reduced costs via BTRAN every iteration instead of incremental update
- **Expected (from spec):** "Set the entering variable's reduced cost to zero (it is now basic)." Reduced costs should be updated incrementally using the BTRAN result from Step 5. Full recomputation only at refactorization (Step 9, line 321).
- **Actual (in code):** Lines 388-445 perform full BTRAN + recomputation of all reduced costs every iteration. This is O(n*m) per iteration instead of O(n).

### V-08: Missing Unboundedness Return Code
- **Severity:** CRITICAL
- **File:** src/simplex/iterate.c (lines 270-274)
- **Spec reference:** Step 7 (Check Termination), line 281
- **Description:** Unboundedness detected but not properly returned in all code paths
- **Expected (from spec):** "Unboundedness: If the ratio test finds no blocking variable, the objective is unbounded. Return UNBOUNDED."
- **Actual (in code):** Line 273 returns ITERATE_UNBOUNDED only if not using Bland's rule OR on last candidate. If Bland's rule is active and other candidates remain, continues loop instead of returning UNBOUNDED immediately.

### V-09: Wrong Objective Value Update Formula
- **Severity:** MAJOR
- **File:** src/simplex/iterate.c (lines 383-384)
- **Spec reference:** Step 4 (Pivot), line 184
- **Description:** Objective update uses reduced cost of entering variable BEFORE pivot instead of proper formula
- **Expected (from spec):** "Update the objective value: z += theta * d_q" where theta is the step size and d_q is the reduced cost at the time of pivot selection (before the reduced cost is set to zero).
- **Actual (in code):** Line 383 reads `double rc_entering = state->work_dj[entering]` AFTER the pivot has been performed (line 375). The step.c implementation doesn't modify reduced costs, but the value should be captured before pivot for correctness.

### V-10: Missing Iteration Limit Check
- **Severity:** MAJOR
- **File:** src/simplex/iterate.c (entire file)
- **Spec reference:** Step 7 (Check Termination), line 282
- **Description:** No iteration limit enforcement
- **Expected (from spec):** "Iteration limit: If the iteration count exceeds the configured limit, return ITERATION_LIMIT."
- **Actual (in code):** Line 454 increments state->iteration but never checks against state->max_iterations. Solver could run indefinitely.

### V-11: Missing Numeric Difficulty Detection
- **Severity:** MAJOR
- **File:** src/simplex/iterate.c (entire file)
- **Spec reference:** Step 7 (Check Termination), line 283
- **Description:** No numeric difficulty handling
- **Expected (from spec):** "Numeric difficulty: If the basis factorization fails or pivot elements are below minimum thresholds, attempt recovery by refactorizing the basis. If recovery fails, return NUMERIC_DIFFICULTY."
- **Actual (in code):** Line 280-283 checks pivot tolerance but just returns CXF_NUMERIC. No recovery attempt via refactorization. Line 451 calls refactor on schedule but doesn't handle refactor failures.

### V-12: Missing Partial Pricing Implementation
- **Severity:** MAJOR
- **File:** src/simplex/iterate.c (lines 190-243)
- **Spec reference:** Step 1 (Pricing), lines 103-123
- **Description:** Pricing always scans all variables instead of using partial pricing with sections
- **Expected (from spec):** "Partial pricing strategy: Rather than examining all non-basic variables at every iteration (full pricing), the variable index set is partitioned into sections. Only one or a few sections are scanned per iteration... Periodic full scans ensure that no variable is neglected indefinitely."
- **Actual (in code):** Lines 190-243: Uses cxf_pricing_candidates (which may implement partial pricing internally, unclear) OR fallback full scan (lines 212-243). No evidence of section-based partial pricing with periodic full scans as specified.

### V-13: Missing Multi-Level Pricing Tolerance
- **Severity:** MAJOR
- **File:** src/simplex/iterate.c (lines 169-247)
- **Spec reference:** Step 1 (Pricing), lines 118-122
- **Description:** No dynamic pricing tolerance adjustment
- **Expected (from spec):** "Multi-level tolerance: The pricing tolerance is adjusted dynamically across three levels: Initial phase (fast): A relatively loose tolerance (e.g., 1e-6)... Standard phase... Aggressive phase: A tight tolerance (e.g., 1e-9) is used for final convergence when near-optimal."
- **Actual (in code):** Always uses env->optimality_tol without adjustment (lines 184, 196, 214, 235).

### V-14: Wrong Bound Flip Implementation
- **Severity:** MAJOR
- **File:** src/simplex/iterate.c; src/simplex/phase_steps.c (step2)
- **Spec reference:** Step 3 (Ratio Test), lines 159-168
- **Description:** Bound flip logic exists but is not integrated into main iteration loop
- **Expected (from spec):** "During a bound flip: 1. The non-basic variable x_q is moved from one bound to the other. 2. Basic variables are updated... 3. The reduced costs of other non-basic variables are updated. 4. No basis exchange occurs; x_q remains non-basic at its new bound. 5. The algorithm returns to Step 1 for a new pricing decision."
- **Actual (in code):** phase_steps.c:cxf_simplex_step2 implements bound flip (lines 59-91) but iterate.c calls cxf_simplex_step, not step2. Bound flips are never triggered. Even if step2 were called, reduced costs aren't updated after flip (violates point 3).

### V-15: Missing Perturbation Implementation
- **Severity:** MAJOR
- **File:** src/simplex/context.c (lines 268-293)
- **Spec reference:** Step 8 (Anti-Cycling via Perturbation), lines 286-303
- **Description:** Perturbation functions are stubs
- **Expected (from spec):** "When cycling is detected, the algorithm applies perturbation to the variable bounds: For each basic variable x_{beta_i}, perturb its bounds: l'_{beta_i} = l_{beta_i} - epsilon_i, u'_{beta_i} = u_{beta_i} + epsilon_i"
- **Actual (in code):** context.c lines 274-279 (cxf_simplex_perturbation) and 288-293 (cxf_simplex_unperturb) are empty stubs that just return CXF_OK.

### V-16: Wrong Degeneracy Handling - Forced Step Size
- **Severity:** MINOR
- **File:** src/simplex/iterate.c (lines 356-370)
- **Spec reference:** Step 8 (Anti-Cycling via Perturbation), lines 286-303
- **Description:** Forces artificial non-zero step size instead of proper perturbation
- **Expected (from spec):** Perturbation-based anti-cycling: perturb bounds of basic variables to break degeneracy structurally
- **Actual (in code):** Lines 364-367 artificially inflate stepSize = scale * CXF_FEASIBILITY_TOL / fabs(pivotElement) instead of perturbing bounds. This is a different anti-cycling approach not mentioned in the spec.

### V-17: Missing Weight Initialization
- **Severity:** MINOR
- **File:** src/simplex/iterate.c; src/simplex/context.c
- **Spec reference:** Step 0 (Initialization), line 98
- **Description:** No steepest edge weight initialization
- **Expected (from spec):** "Initialize steepest edge weights: gamma_j = ||B^{-1} a_j||^2 for each non-basic variable j (or set gamma_j = 1 for initial Devex approximation)."
- **Actual (in code):** No weight initialization in context.c:cxf_simplex_init or anywhere else.

### V-18: Missing Dual Simplex Support in Main Loop
- **Severity:** MINOR
- **File:** src/simplex/iterate.c (entire file)
- **Spec reference:** Key Design Choices, lines 343-344
- **Description:** Main iteration loop only implements primal simplex
- **Expected (from spec):** "The solver supports both primal and dual simplex variants. The dual simplex maintains dual feasibility (all reduced costs satisfy optimality conditions) and drives toward primal feasibility..."
- **Actual (in code):** iterate.c only implements primal simplex (pricing based on reduced costs, ratio test for leaving variable). phase_steps.c has cxf_simplex_step3 for dual pivot but it's never called from iterate.c. No dual pricing or dual ratio test implementation.

## Spec Sections Not Implemented

### From Step 0 (Initialization):
- Line 93: "Factorize B_0 = LU" - No LU factorization in iterate.c (may be in separate module)
- Line 94: "Compute basic variable values: x_B = B^{-1} b" - Not in iterate.c
- Line 95: "Assign non-basic variables to their bounds" - Not in iterate.c
- Line 96: "Compute the objective value: z = c^T x" - Not in iterate.c
- Line 97: "Compute reduced costs: d = c_N - N^T y" - Not in iterate.c
- Lines 98-99: "Initialize steepest edge weights" - Not implemented anywhere
- Lines 99-102: "Determine initial phase" - Not implemented

Note: Lines 93-97 may be implemented in separate initialization module (setup.c), but they are part of the revised simplex algorithm specification.

### From Step 5 (BTRAN):
- Lines 190-196: Entire BTRAN step missing from iteration loop

### From Step 6 (Update Pricing Weights):
- Lines 199-243: Exact DSE weight update - Not implemented
- Lines 245-269: Devex weight update - Not implemented
- Lines 237-242: Weight of entering variable calculation - Not implemented
- Lines 243-244: Numerical safeguard (weight clamping) - Not implemented

### From Step 8 (Anti-Cycling via Perturbation):
- Lines 292-299: Perturbation application - Stub only
- Line 300: Cleanup iterations after unperturbing - Not implemented

### From Step 9 (Periodic Basis Refactorization):
- Lines 317-322: Full refactorization procedure details - Partial (line 451 calls cxf_solver_refactor but doesn't check return or recompute values)
- Line 320: "Recompute basic variable values x_B = B^{-1} b to correct accumulated roundoff" - Not in iterate.c
- Line 321: "Recompute reduced costs d_N to correct accumulated drift" - Not in iterate.c

### From Key Design Choices:
- Lines 343-344: Dual simplex variant - step3 exists but never called
- Lines 159-168: Bound flipping - step2 exists but never called
- Lines 118-122: Multi-level pricing tolerance - Not implemented
- Lines 116-117: Partial pricing with sections - Not clearly implemented

## Code Sections Not In Spec (potentially hallucinated)

### Full Reduced Cost Recomputation Every Iteration
- **Location:** iterate.c lines 388-445
- **Description:** Performs BTRAN + full reduced cost recomputation every iteration
- **Issue:** Spec says reduced costs should be updated incrementally (Step 4, line 186 "Set the entering variable's reduced cost to zero"). Full recomputation only at refactorization (Step 9, line 321). This implementation does the opposite - full recomputation every iteration but NOT at refactorization.

### Bland's Rule Candidate Skipping
- **Location:** iterate.c lines 254-320, 324-351
- **Description:** Complex logic to skip degenerate pivots when using Bland's rule
- **Issue:** Spec specifies perturbation-based anti-cycling (Step 8, lines 286-303), not Bland's rule. While Bland's rule is mentioned once as an alternative (line 301 "preferred over Bland's rule"), the spec explicitly chooses perturbation. This implementation uses Bland's rule as the primary anti-cycling mechanism.

### Virtual Perturbation via Forced Step Size
- **Location:** iterate.c lines 364-367
- **Description:** `stepSize = scale * CXF_FEASIBILITY_TOL / fabs(pivotElement)` when degenerate
- **Issue:** Not in spec. This is a different anti-cycling heuristic than the specified bound perturbation approach.

### Auxiliary Variable Coefficient Fallback Heuristic
- **Location:** iterate.c lines 55-71, get_auxiliary_coeff_fallback
- **Description:** Infers artificial variable coefficients from RHS sign and constraint sense
- **Issue:** Spec doesn't specify this heuristic. Artificial variables in Phase I should have known coefficients (+1 in the Phase I objective, and ±1 in constraints based on slack/surplus/artificial type).

### Degenerate Count Threshold for Bland's Rule
- **Location:** iterate.c lines 357-360
- **Description:** Switches to Bland's rule after 50 consecutive degenerate pivots
- **Issue:** Spec specifies perturbation-based anti-cycling with snapshot-based cycle detection (Step 8, line 292 "compares it to a later basis"). No mention of a degenerate-count threshold for switching to Bland's rule.

### Step Size Limiting by Entering Variable Bounds
- **Location:** iterate.c lines 298-311, 340-350
- **Description:** Reduces stepSize if entering variable would exceed its opposite bound
- **Issue:** This is correct logic for bounded variables, but it's intermingled with the bound flip logic in a way that prevents bound flips. Spec says bound flips should occur when "theta = u_q - l_q or l_q - u_q" (line 159), but this code limits stepSize instead of triggering a flip.

## Recommendations

### Immediate Critical Fixes:
1. **Implement Phase I/II logic:** Add phase determination, transition, and infeasibility detection (V-01, V-02, V-03)
2. **Add BTRAN step:** Insert Step 5 after pivot (V-04, V-06)
3. **Implement steepest edge weight update:** Add DSE or Devex weight maintenance (V-05)
4. **Fix reduced cost updates:** Switch from full recomputation to incremental update (V-07)
5. **Fix unboundedness handling:** Return UNBOUNDED immediately when detected (V-08)

### Major Improvements:
6. **Add iteration limit check:** Enforce max_iterations (V-10)
7. **Improve numeric handling:** Add recovery refactorization on numeric difficulty (V-11)
8. **Implement partial pricing:** Add section-based pricing with periodic full scans (V-12)
9. **Add multi-level tolerance:** Dynamic pricing tolerance adjustment (V-13)
10. **Integrate bound flipping:** Call step2 from iterate loop and update reduced costs after flips (V-14)
11. **Implement perturbation:** Replace Bland's rule with proper bound perturbation (V-15)

### Code Quality:
12. **Remove hallucinated logic:** Remove forced step size anti-cycling (V-16) and degenerate-count Bland's switching (iterate.c:357-360)
13. **Simplify reduced cost updates:** Remove lines 388-445 full recomputation; add incremental update after pivot
14. **Add dual simplex:** Integrate step3 into main loop for dual mode support (V-18)

### Architecture Notes:
- The implementation assumes Phase I setup happens elsewhere (correct - likely in setup.c)
- The implementation correctly handles artificial variables (indices n to n+m-1) in pricing and column extraction
- The iteration structure is fundamentally sound (price → FTRAN → ratio → pivot) but missing critical steps (BTRAN, weight update)
- The basis update via eta vectors (step.c) appears correct but wasn't fully audited (that's a basis module concern)

### Performance Impact:
The most severe performance violation is V-07 (full reduced cost recomputation). Spec says this should be O(nnz(N)) per iteration (incremental update) but implementation is O(m + n*nnz(N)) per iteration (full recomputation). On a problem with n=10000, m=5000, and average column density of 10 nonzeros, the difference is:
- Spec: ~100K operations per iteration
- Code: ~100K + 10000*10 = ~200K operations per iteration

This roughly doubles the iteration cost and will be severe on large problems.
