# Module: Simplex Iteration

## Purpose

The Simplex Iteration module contains the core functions that execute within the main LP solver iteration loop. It provides the primary simplex pivot operation (pricing, ratio test, basis exchange, bound flipping), bidirectional bound propagation for additional variable and constraint tightening, progress logging with callback notification, and post-iteration monitoring for stalling, objective stagnation, and termination conditions. Together, these five functions implement the inner loop of the revised simplex method described in P2.1 (Revised Simplex Method) and are called by the LP solve driver (cxf_solve_lp, P3.25).

## Functions

### cxf_simplex_iterate

**Purpose:** Report presolve and iteration progress to the user log and invoke the external monitoring callback. Despite its name, this function does not perform simplex iterations — it is a progress logging and callback notification function.

**Signature:**
- Input: `model` : pointer-to-Model - The model containing logging configuration, solve mode, and thread count
- Input: `state` : pointer-to-SolverState - The solver state containing timing data, presolve statistics, and callback parameters
- Output: void

**Preconditions:**
- The model must have valid logging configuration fields
- The solver state must have valid timing and statistics counters

**Postconditions:**
- If logging is enabled and sufficient time has elapsed since the last report, a progress message has been written to the log
- The external monitoring callback has been invoked regardless of whether a log message was printed
- No solver state has been modified (other than the last-report timestamp for throttling)

**Side Effects:**
- Writes to the log output stream (if enabled and time threshold met)
- Invokes the external logging callback unconditionally (enables GUI monitoring even when console logging is disabled)
- Updates the last-reported-time marker for throttling

**Error Conditions:**
- None. This function cannot fail and returns void.

**Behavioral Description:**
This function provides progress reporting during the LP solve. It is called once per iteration batch from the main solve driver.

**Step 1: Logging gate.** If logging is disabled on the model, the function skips directly to the callback invocation (Step 4).

**Step 2: Time throttling.** The function retrieves the elapsed solve time (wall clock or CPU time, depending on the solver's timing mode). The elapsed time is normalized by dividing by the thread count (so that parallel solves do not produce proportionally more messages) and rounded to the nearest integer second. If the rounded normalized time has not changed since the last report, no message is printed. This ensures at most one log message per second of sequential-equivalent time.

**Step 3: Format and emit message.** The message format depends on the current solve mode:
- During general constraint preprocessing: reports the preprocessing phase and elapsed time.
- During standard presolve: reports the number of rows and columns removed and the elapsed time. The message prefix varies depending on whether this is the initial presolve or a subsequent presolve phase.

**Step 4: Callback invocation.** The external logging callback is always invoked, regardless of whether a message was printed. This ensures that external monitoring systems (GUI progress bars, distributed computing managers) receive regular heartbeat notifications even when console output is suppressed.

**Naming note:** Despite its name suggesting iteration logic, this function is purely a logging and notification utility. The actual simplex iteration logic resides in cxf_simplex_step, cxf_simplex_step2, and cxf_simplex_step3 (all in this module).

**Thread Safety:** Not thread-safe. Must be called from the main solve thread.

**Dependencies:**
- P1.03 (SolverState) - reads timing data, presolve statistics, and callback parameters
- P3.10 (Logging) - log output functions for message formatting and flushing
- P3.13 (Callbacks) - logging callback invocation

---

### cxf_simplex_step

**Purpose:** Execute one step of the revised simplex method: retrieve pricing candidates, perform the Harris two-pass ratio test with bound-flipping, execute the pivot (basis exchange or bound flip), create the pivot eta record, and update the basis state.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with basis, bounds, constraint matrix, eta chain, and pricing state
- Input: `env` : pointer-to-Environment - Environment containing algorithm parameters, tolerances, and solve mode configuration
- Output: int - Zero on success, the infeasibility code if a bound violation is detected, the unbounded code if no blocking variable exists, or the out-of-memory code if eta allocation fails

**Preconditions:**
- The solver state must be fully initialized with valid basis, constraint matrix (both CSR and CSC), bounds, reduced costs, and pricing state
- The pricing subsystem must be initialized and ready to supply candidates
- The environment must contain valid tolerance parameters and solve mode configuration

**Postconditions:**
- On a successful standard pivot: the basis has been updated (entering variable is now basic, leaving variable is now non-basic at the appropriate bound), a pivot eta record has been prepended to the eta chain, the objective value has been updated, reduced costs of affected variables have been updated, and the pricing subsystem has been notified of the basis change
- On a successful bound flip: the entering variable has been flipped to its opposite bound without a basis exchange, basic variable values have been updated, constraint data has been adjusted for matrix consistency, and the pricing subsystem has been notified
- On infeasibility: the diagnostic variable index has been stored in the solver state
- On unbounded detection: the diagnostic variable index has been stored
- On no candidates (optimal): returns success with no state change

**Side Effects:**
- Allocates pivot eta vectors from the memory pool
- Modifies the basis header and variable status arrays
- Updates the objective value, reduced costs, and basic variable values
- Notifies the pricing subsystem of all variable and constraint changes
- For bound flips: negates constraint row coefficients and adjusts auxiliary data to maintain algebraic consistency
- Updates the work counter with per-operation cost estimates
- Increments the eta vector counts (total and row-type)

**Error Conditions:**
- Lower bound exceeds upper bound (plus tolerance) for a candidate variable -> returns the infeasibility code with diagnostic variable index
- No blocking variable found in the ratio test (problem is unbounded) -> returns the unbounded code with diagnostic variable index
- Memory allocation failure for eta vector -> returns the out-of-memory code
- Helper function errors are propagated immediately

**Behavioral Description:**
This is the primary simplex iteration engine, implementing the revised simplex method with Harris two-pass ratio test and bound-flipping ratio test (BFRT). It corresponds to Steps 1-4 of the simplex iteration cycle described in P2.1 (Revised Simplex Method).

**Phase 1: Tolerance selection.** The function selects the pricing tolerance based on the current pricing strategy phase. The tolerance tier determines the numerical strictness of candidate evaluation:
- In the initial pricing phase: a loose tolerance enables fast progress with more candidate acceptance.
- In a fallback phase: a very tight tolerance prevents false candidates that would cause numerical difficulties.
- In the standard pricing phase: an intermediate tolerance balances speed and accuracy.

This multi-tier approach is standard in production simplex implementations (Maros, 2003, Section 7.5).

**Phase 2: Pricing candidate retrieval.** The function calls cxf_pricing_candidates (P3.17) to obtain the set of non-basic variables that could improve the objective. If the pricing system returns zero candidates, the current basis is optimal (Phase II) or feasible (Phase I), and the function returns success.

**Phase 3: Per-candidate processing.** For each candidate variable returned by pricing, the function evaluates it for pivot eligibility:

1. **Infeasibility detection.** If the variable's lower bound exceeds its upper bound by more than the tolerance, the problem is infeasible at the current state. The function stores the diagnostic variable index and returns the infeasibility code.

2. **Tight bound handling.** If the variable's bound range is at or below the pricing tolerance, the variable is processed by cxf_pivot_primal (P3.19) for safe elimination of near-fixed variables.

3. **Dual simplex path.** If the solver is in dual simplex mode and the variable has special constraint flags (SOS, indicators), the function delegates to cxf_pivot_special (P3.19) for special-case handling.

4. **Free variable handling.** If the variable is free (superbasic, between bounds), the function checks for unboundedness based on the reduced cost direction. If bounded, it creates a pivot eta record and updates the basis state directly.

5. **Standard pivot path (basic status).** This is the main path for most iterations:
   a. **Column scan and ratio computation.** The function calls cxf_pivot_check (P3.19) to compute the initial step length bounds.
   b. **Harris two-pass ratio test.** Pass 1 computes the relaxed minimum ratio (theta_max) with tolerance band. Pass 2 selects the leaving variable with the largest absolute pivot element among tied candidates. This implements the algorithm described in P2.4 (Harris Ratio Test), Stages 1-2.
   c. **Bound-flipping ratio test (BFRT).** If the leaving variable has finite bounds on both sides, it may be flipped to its opposite bound instead of becoming the leaving variable. The function adjusts the step length, records the flip, and continues scanning for the next blocking variable. Multiple bound flips may occur in a single step. Each flip involves negating the constraint row coefficients and swapping auxiliary data arrays to maintain matrix algebraic consistency. This implements P2.4, Stage 3.
   d. **Pivot execution.** The function calls cxf_pivot_with_eta (P3.16) to create the pivot eta record and updates the basis header, variable status arrays, objective value, and basic variable values.
   e. **Pricing notification.** After the pivot, the function notifies the pricing subsystem of the basis change via cascading updates (cxf_pricing_cascade_update, P3.18).

6. **Additional candidate processing.** After the standard pivot path, variables with remaining special status may receive additional processing via cxf_pivot_special (P3.19).

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P2.1 (Revised Simplex Method) - implements Steps 1-4 of the simplex iteration cycle
- P2.4 (Harris Ratio Test and BFRT) - implements the two-pass ratio test with bound flipping
- P3.16 (Basis Operations) - cxf_pivot_with_eta creates the pivot eta record
- P3.17 (Pricing Core) - cxf_pricing_candidates retrieves pricing candidates
- P3.18 (Pricing Support) - cxf_pricing_cascade_update, cxf_pricing_mark_dirty for post-pivot notification
- P3.19 (Pivot Operations) - cxf_pivot_check for step bounds, cxf_pivot_primal for tight bounds, cxf_pivot_special for special cases, cxf_pivot_bound for bound flips
- P1.03 (SolverState) - reads and modifies all iteration state fields
- P1.04 (EtaVector) - creates Variant 1 (PIVOT) eta records
- P2.01 (Product Form of the Inverse) - eta chain management

---

### cxf_simplex_step2

**Purpose:** Process the variable-side bound-flipping queue, determining which deferred variables should flip to opposite bounds and creating bound-change eta records to track those changes. This is the variable-perspective half of the bidirectional bound propagation system.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with constraint matrix, bounds, and pricing state
- Input: `env` : pointer-to-Environment - Environment containing the primal feasibility tolerance
- Output: int - Zero on success, the infeasibility code if confirmed infeasibility is detected, or the out-of-memory code if eta allocation fails

**Preconditions:**
- cxf_simplex_step must have been called first in the current iteration (populating the step2 candidate queue)
- The solver state must have valid CSR matrix data, bounds, and pricing state

**Postconditions:**
- All eligible variables in the step2 queue have been processed: their bounds have been tightened, pricing state has been updated, and bound-change eta records have been created
- Variables identified as both-bound-tight (fixed) have been processed via cxf_pivot_bound (P3.19)
- The flip counter on the solver state has been incremented by the number of processed candidates
- Activity bounds have been incrementally updated for all affected constraints

**Side Effects:**
- Allocates bound-change eta vectors from the memory pool
- Modifies variable bounds (tightening based on flip analysis)
- Calls cxf_pivot_update (P3.19) to incrementally update constraint activity bounds
- Marks affected variables as dirty in the pricing subsystem via cxf_pricing_mark_dirty (P3.18)
- Calls cxf_pivot_bound (P3.19) when both bounds are tightened (variable becomes fixed)
- Updates the flip counter and work counters on the solver state
- Invalidates processed entries in the CSR matrix (lazy deletion)

**Error Conditions:**
- Confirmed infeasibility (two-stage detection: ratio check plus dual bounds confirmation) -> returns the infeasibility code with diagnostic indices stored in the solver state
- Memory allocation failure for eta vector -> returns the out-of-memory code
- Errors from cxf_pivot_bound propagated immediately

**Behavioral Description:**
This function processes the secondary pricing queue populated during cxf_simplex_step. While the primary step handles the main simplex pivot, step2 processes additional variable candidates for bound-flipping that were deferred from the main pivot.

**Step 1: Candidate retrieval.** The function obtains the step2 candidate list from the pricing subsystem. Only variables with a specific pricing status (indicating they were deferred from step for further processing) are eligible.

**Step 2: Per-candidate evaluation.** For each eligible candidate:

1. **Pivot element search.** The function scans the candidate variable's constraint row in the CSR matrix to find the first valid pivot element (a variable with non-negative status and sufficient column count).

2. **Pivot threshold check.** If the absolute value of the pivot coefficient is below a tight numerical threshold, the candidate is skipped to avoid numerical instability.

3. **Ratio computation.** The primal value divided by the pivot coefficient produces the ratio used for flip-type classification.

4. **Flip type classification.** Based on the constraint sense (equality versus inequality) and the sign of the pivot coefficient, the function assigns a flip type:
   - No flip needed (ratio within bounds)
   - Flip to upper bound
   - Flip to lower bound
   - Both bounds tightened (variable effectively fixed)
   - Infeasible (ratio exceeds bounds in both directions)

   For equality constraints, both bound directions are checked independently, with the flip type combining the results.

5. **Bound clamping.** The computed ratio is clamped to the variable's current bounds, with tolerance corrections applied to prevent over-tightening.

6. **Infeasibility handling.** Infeasibility detection uses a two-stage procedure: the initial ratio check is confirmed against dual activity bounds before returning the infeasibility code. If confirmation fails, the candidate entry is restored and processing continues. This prevents false infeasibility alarms caused by numerical noise.

7. **Bound-change eta record creation.** A lightweight bound-change eta record is created for each processed candidate (when eta tracking is active). This record stores the variable index, constraint index, flip classification, pivot coefficient, and ratio value.

8. **Bound update and notification.** New bounds are written to the variable's working bound arrays, cxf_pivot_update (P3.19) is called to incrementally update constraint activity bounds, and the pricing subsystem is notified via dirty-marking. If the flip type indicates both bounds are tightened (variable fixed), cxf_pivot_bound (P3.19) is called to fully fix the variable.

**Step 3: Counter update.** The solver state's flip counter is incremented by the total number of candidates processed.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P3.17 (Pricing Core) - step2 candidate retrieval from the pricing subsystem
- P3.18 (Pricing Support) - cxf_pricing_mark_dirty for post-flip notification
- P3.19 (Pivot Operations) - cxf_pivot_update for activity bound maintenance, cxf_pivot_bound for both-bound-tight variables
- P1.03 (SolverState) - reads CSR matrix, bounds; modifies bounds, pricing state, eta chain
- P1.04 (EtaVector) - creates bound-change eta records (a lightweight variant distinct from pivot and variable-fixing records)
- P2.01 (Product Form of the Inverse) - eta chain management and memory pool allocation

---

### cxf_simplex_step3

**Purpose:** Process the constraint-side bound propagation queue, computing implied variable bounds from constraint activities and tightening bounds accordingly. This is the constraint-perspective half of the bidirectional bound propagation system, complementary to cxf_simplex_step2.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver state with constraint matrix, activity bounds, and pricing state
- Input: `env` : pointer-to-Environment - Environment containing the primal feasibility tolerance
- Output: int - Zero on success, the infeasibility code if confirmed infeasibility is detected, or the out-of-memory code if eta allocation fails

**Preconditions:**
- cxf_simplex_step must have been called first in the current iteration
- The solver state must have valid CSR matrix data, activity bounds, constraint sense data, and pricing state
- This function is called for LP problems only (skipped for quadratic programs)

**Postconditions:**
- All eligible constraints in the constraint candidate queue have been processed: implied bounds have been computed and variable bounds have been tightened where the implication is stronger
- Variables whose bounds are tightened from both sides (effectively fixed) have been processed via cxf_pivot_bound (P3.19)
- The propagation counter on the solver state has been incremented
- Activity bounds have been incrementally updated for all affected constraints

**Side Effects:**
- Allocates bound-change eta vectors from the memory pool (same lightweight format as step2)
- Modifies variable bounds (tightening based on implied bound computation)
- Calls cxf_pivot_update (P3.19) to incrementally update constraint activity bounds
- Marks affected variables as dirty in the pricing subsystem
- Calls cxf_pivot_bound (P3.19) when implied bounds fix a variable
- Updates the propagation counter and work counters
- Invalidates processed entries in the CSR matrix (lazy deletion)

**Error Conditions:**
- Confirmed infeasibility (constraint activity bounds inconsistent with constraint sense) -> returns the infeasibility code with diagnostic variable and constraint indices stored
- Memory allocation failure for eta vector -> returns the out-of-memory code
- Errors from cxf_pivot_bound propagated immediately

**Behavioral Description:**
This function implements constraint-based bound propagation, the constraint-perspective counterpart to step2. While step2 asks "how should variable bounds change based on the pricing queue?", step3 asks "what do constraint activity bounds imply for individual variable bounds?"

This technique is a standard LP presolve reduction also applicable during simplex iterations: given a constraint sum(a_j * x_j) <= b with known activity bounds, the implied upper bound on variable x_k is (b - minActivity_excluding_k) / a_k. When this implied bound is tighter than the current bound, the variable's bound can be tightened (Savelsbergh, 1994; Achterberg et al., 2020).

**Step 1: Candidate retrieval.** The function obtains the constraint candidate list from the pricing subsystem via cxf_pricing_get_constr_candidates (P3.18). Only constraints with a specific pricing status (indicating they need bound propagation evaluation) are eligible.

**Step 2: Per-constraint evaluation.** For each eligible constraint:

1. **Pivot element search.** The function scans the constraint's row in the CSR matrix to find the first variable with non-negative status (an active, unfixed variable in the constraint).

2. **Pivot threshold check.** If the absolute value of the coefficient is below a numerical threshold, the constraint is skipped.

3. **Implied value computation.** The constraint's current activity divided by the pivot coefficient yields the implied value for the variable: how far the variable must be from zero for the constraint to be satisfied.

4. **Violation classification.** Based on the constraint sense (equality, less-than, greater-than) and the sign of the pivot coefficient, the function classifies whether the implied value indicates:
   - No tightening needed
   - Lower bound tightening
   - Upper bound tightening
   - Both bounds tightened (variable fixed by the constraint)
   - Infeasibility (implied value contradicts existing bounds)

5. **Bound clamping.** Implied values are clamped to existing bounds with tolerance corrections.

6. **Infeasibility handling.** As in step2, a two-stage procedure confirms infeasibility before reporting: the initial implication is verified against the constraint's minimum and maximum activity bounds. Unconfirmed infeasibilities are treated as false alarms (the constraint entry is restored and processing continues).

7. **Bound-change eta record creation.** A lightweight bound-change eta record is created for each processed constraint (same format as step2). The record stores the variable index, constraint index, violation flags, coefficient, and implied value. Piecewise-linear variables receive additional flags.

8. **Bound update and notification.** New bounds are applied, activity bounds are updated via cxf_pivot_update (P3.19), and the pricing subsystem is notified. If both bounds are tightened, cxf_pivot_bound (P3.19) fixes the variable.

**Step 3: Counter update.** The solver state's propagation counter is incremented.

**Conditional execution:** This function is called only for LP problems. For quadratic programs (QP), constraint-based bound propagation is skipped because the quadratic objective terms introduce nonlinear dependencies that invalidate the linear activity-bound inference.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P3.18 (Pricing Support) - cxf_pricing_get_constr_candidates for constraint candidate retrieval, cxf_pricing_mark_dirty for notification
- P3.19 (Pivot Operations) - cxf_pivot_update for activity bound maintenance, cxf_pivot_bound for fixing variables
- P1.03 (SolverState) - reads CSR matrix, activity bounds, constraint sense; modifies variable bounds, pricing state, eta chain
- P1.04 (EtaVector) - creates bound-change eta records (same lightweight variant as step2)
- P2.01 (Product Form of the Inverse) - eta chain management and memory pool allocation

---

### cxf_simplex_post_iterate

**Purpose:** Perform post-iteration monitoring to detect stalling (insufficient elimination progress), check termination conditions (time limits, iteration limits, user abort), detect objective stagnation, and handle user interrupts.

**Signature:**
- Input: `model` : pointer-to-Model - The model with stall detection settings and solve configuration
- Input: `state` : pointer-to-SolverState - The solver state with progress counters, objective tracking, and timing data
- Input: `outStall` : pointer-to-int - Output flag: set to 1 if stalling is detected, 0 otherwise; may be null to skip stall detection
- Output: int - Zero to continue iterating, or a non-zero termination code if the solve should stop

**Preconditions:**
- The model and solver state must be valid
- This function should be called after the step/step2/step3 sequence in each iteration

**Postconditions:**
- If stall detection is active and `outStall` is non-null: the stall flag reflects whether the solver is making sufficient progress
- If a termination condition is met: the function returns a non-zero code indicating the reason for termination
- If objective stagnation is detected: the solver state's iteration mode flag is set to signal stagnation to the outer loop (which may trigger strategy switching or perturbation)
- The user interrupt status has been checked

**Side Effects:**
- Sets the stall flag at the `outStall` pointer (if non-null)
- May set the iteration mode flag on the solver state to signal objective stagnation
- No memory allocation
- No modification of the basis, constraint matrix, or eta chain

**Error Conditions:**
- Time limit exceeded -> returns termination code (propagated from the solve status check)
- Iteration limit exceeded -> returns termination code
- User abort -> returns termination code
- Other termination conditions from the solve status check are propagated

**Behavioral Description:**
This function implements three independent monitoring checks that run after each batch of simplex iterations.

**Check 1: Stall detection.** This check is conditional on four prerequisites: (a) the stall output pointer is non-null, (b) stall detection is enabled on the model, (c) the solver is not in a special mode that suppresses stall detection, and (d) the basis needs refactorization (indicating enough work has been done to evaluate progress).

When all prerequisites are met, the function evaluates two progress metrics that compare the actual elimination work done against dimension-scaled thresholds. The thresholds follow the standard simplex practice of scaling progress expectations proportionally to problem size (Maros, 2003, Section 10.2), ensuring that stall detection adapts to both small and large instances.

**Column progress formula.** Let `cols_eliminated` be the number of variables fixed or eliminated during the current refactorization interval, and let `n` be the total number of variables. The column progress check passes if:

> cols_eliminated <= alpha * n + beta

where alpha is a fractional multiplier (typically in the range 0.25 to 0.75, with 0.5 being a standard default) and beta is a small additive offset (typically 1 to 5, providing a grace allowance for small problems). If this inequality holds, column elimination is considered to be making adequate progress. If it is violated (i.e., the number of eliminations exceeds the threshold), the column check fails.

**Row progress formula.** Let `rows_eliminated` be the total row reduction progress, aggregated as the sum of the primary row elimination count and all auxiliary progress counters (constraints tightened, bounds propagated, variables fixed by propagation). Let `M_eff` be the effective row dimension, computed as the number of constraints plus all auxiliary dimension counters (additional row-like work items such as deferred constraints and propagation targets). The row progress check passes if:

> rows_eliminated <= beta + alpha * M_eff

using the same alpha and beta parameters as the column check. If violated, the row check fails.

**Stall determination.** If **either** the column check or the row check fails, the stall flag is set to 1. The logic is that stalling means *insufficient* progress: the solver has completed a full refactorization interval (typically 50 to 200 iterations, depending on basis dimension) but has not eliminated or fixed a proportional fraction of the problem dimensions. When stall is detected, the outer iteration loop may respond by switching simplex phases (e.g., from primal to dual), activating bound perturbation (see P2.6), or forcing a strategy change.

**Design rationale.** This threshold-based stall detection is a practical heuristic common in production simplex implementations. The linear dependence on problem dimension ensures that the threshold scales naturally: for a 1000-variable problem with alpha = 0.5, approximately 500 column eliminations per refactorization interval are expected before declaring progress, while a 10-variable problem requires only about 5. The additive offset beta prevents false stall detection on very small problems where a single elimination might be the only progress possible. This approach is simpler than the EXPAND procedure's tolerance-expansion mechanism (Gill, Murray, Saunders, and Wright, 1989) and serves a complementary purpose: EXPAND prevents cycling by gradually relaxing feasibility bounds, while this check detects when the solver has exhausted its elimination opportunities and needs a strategy change.

**Published context.** Maros (2003, Sections 10.2--10.3) discusses monitoring iteration progress and deciding when to switch strategies based on insufficient reduction. The general principle of scaling stall thresholds by problem dimension is standard: see also Koberstein (2005, Section 4.3) on monitoring dual simplex progress and triggering refactorization or strategy changes after dimension-proportional iteration counts.

**Check 2: Solve status verification.** The function calls a centralized status-checking routine that evaluates time limits, iteration limits, memory limits, and user abort signals. If any termination condition is met, the function returns the corresponding termination code immediately. This is the primary termination gateway for the simplex iteration loop.

**Check 3: Objective stagnation detection.** The function compares the current objective value to the objective value recorded at the end of the previous refactorization interval to determine whether the solver is making meaningful progress toward optimality.

Let `z_current` be the current objective value and `z_previous` be the objective value at the end of the previous interval. The objective improvement is:

> delta_z = z_current - z_previous

The stagnation check uses the solver's optimality tolerance (epsilon_opt, typically 1e-6; see P5.2 Section 2):

> If delta_z < epsilon_opt (and delta_z is not exactly equal to epsilon_opt), then objective stagnation is declared.

Note: for minimization problems, the objective decreases, so `z_current - z_previous` is typically negative when making progress. The comparison `delta_z < epsilon_opt` therefore detects the case where the objective has either not improved or has worsened. The strict inequality with the exact-equality exception handles the edge case where the improvement is precisely at the tolerance boundary.

When stagnation is detected, the iteration mode flag is set to a sentinel value signaling stagnation. This signal is read by the outer iteration loop, which may respond by:
- Switching simplex phases (e.g., Phase I to Phase II or vice versa)
- Activating bound perturbation to break degeneracy (see P2.6, Perturbation)
- Triggering a basis refactorization to clear accumulated numerical drift and recompute reduced costs
- Switching from primal to dual simplex or the reverse

This single-interval comparison (rather than tracking progress over multiple intervals) provides a simple but effective early-warning mechanism. Because the check runs at refactorization boundaries -- which represent O(m/4) to O(m) iterations of work -- the objective has had substantial opportunity to improve, and failure to do so is a strong signal that the current strategy is ineffective. The use of the optimality tolerance as the threshold is standard: if the objective cannot improve by more than epsilon_opt over a full refactorization cycle, the solver is effectively stagnant relative to its own precision requirements (Maros, 2003, Section 10.3).

**Check 4: User interrupt handling.** The function checks for user-initiated interrupts (such as Ctrl+C in console mode or cancel requests from GUI interfaces). This provides graceful termination with a valid solver state, rather than an abrupt process exit.

**Thread Safety:** Not thread-safe. Must be called from the main solve thread.

**Dependencies:**
- P1.03 (SolverState) - reads progress counters, objective value, iteration mode; may modify iteration mode flag
- P3.13 (Callbacks) - user interrupt check may invoke callback infrastructure

---

## Module-Level Behavioral Notes

### Iteration Loop Calling Sequence

The five functions in this module are called in a specific order within the main LP solve driver (cxf_solve_lp, P3.25). The typical per-iteration sequence is:

1. **cxf_basis_snapshot** (P3.16) — capture progress baseline
2. **cxf_simplex_iterate** (this module) — progress logging and callback
3. **cxf_simplex_phase_end** (P3.21) — check for phase transition
4. **cxf_simplex_perturbation** (P3.21) — anti-cycling perturbation if needed
5. **cxf_simplex_step** (this module) — primary simplex pivot
6. **cxf_simplex_step2** (this module) — variable-side bound flipping
7. **cxf_simplex_step3** (this module) — constraint-side bound propagation (LP only)
8. **cxf_simplex_phase_end** (P3.21) — alternative phase ending check
9. **cxf_basis_diff** (P3.16) — cycling detection via progress comparison
10. **cxf_simplex_post_iterate** (this module) — stall, stagnation, and termination checks

This sequence repeats until termination. The post_iterate function's return code determines whether the loop continues.

### Naming Clarifications

Several function names in this module are historically misleading:

- **cxf_simplex_iterate** does not perform simplex iterations. It reports presolve progress and invokes the logging callback. A more descriptive name would be "progress_report" or "log_iteration_progress."

- **cxf_simplex_step2** and **cxf_simplex_step3** are not sequential steps of a single operation. They are complementary bound propagation passes that operate on different candidate queues (variable-side and constraint-side, respectively). They can be thought of as "variable_bound_propagation" and "constraint_bound_propagation."

### Bidirectional Bound Propagation (step2 + step3)

cxf_simplex_step2 and cxf_simplex_step3 form a complementary pair implementing bidirectional bound propagation:

| Aspect | cxf_simplex_step2 | cxf_simplex_step3 |
|--------|-------------------|-------------------|
| Perspective | Variable-centric | Constraint-centric |
| Question answered | "How should variable bounds change given pricing decisions?" | "What do constraint activities imply for variable bounds?" |
| Candidate source | Variable pricing queue (step2 candidates) | Constraint pricing queue (constraint candidates) |
| Status filter | Variables with specific deferred status | Constraints with propagation status |
| LP/QP applicability | Both LP and QP | LP only |

Both functions share the same structural pattern: retrieve candidates from the pricing queue, evaluate each candidate against bounds, classify the result, create a lightweight bound-change eta record, update bounds, and notify pricing. This parallel structure reflects the duality between variable-bound and constraint-activity perspectives in LP bound tightening.

The constraint-side propagation (step3) is the standard implied-bound technique from LP presolve (Savelsbergh, 1994), applied iteratively during the simplex solve rather than only as a preprocessing step.

### Eta Vector Types Created by This Module

| Function | Eta Type | Description |
|----------|----------|-------------|
| cxf_simplex_step | PIVOT (Variant 1) | Standard simplex pivot record (created via cxf_pivot_with_eta, P3.16) |
| cxf_simplex_step2 | BOUND_CHANGE | Lightweight bound-change record for variable-side flips |
| cxf_simplex_step3 | BOUND_CHANGE | Lightweight bound-change record for constraint-side propagation |

The bound-change eta records created by step2 and step3 are a lightweight variant distinct from the full pivot eta records (Variant 1) and the variable-fixing records (Variant 2) created by cxf_pivot_bound (P3.19). They store only the variable index, constraint index, classification flags, pivot coefficient, and ratio/implied value. For piecewise-linear variables, an additional flag is included in the classification to enable downstream PWL processing.

All eta records are allocated from the SolverState's memory pool via bump allocation and are freed in bulk during basis refactorization.

### Two-Stage Infeasibility Detection

Both cxf_simplex_step2 and cxf_simplex_step3 implement a two-stage infeasibility detection procedure:

1. **Stage 1 (preliminary):** The ratio or implied value is compared against the variable's bounds plus tolerance. If the comparison indicates infeasibility, stage 2 is invoked.
2. **Stage 2 (confirmation):** The preliminary finding is cross-checked against dual bounds or constraint activity bounds. Only if both stages agree is the infeasibility code returned. If confirmation fails, the candidate entry is restored and processing continues.

This conservative approach prevents false infeasibility reports caused by accumulated numerical noise. A single-stage check would be faster but would produce false alarms that unnecessarily terminate the solve.

### Parameter Structure Distinction

The five functions in this module use two different parameter patterns:

- **cxf_simplex_iterate** and **cxf_simplex_post_iterate** accept a model pointer and a solver state pointer. They require model-level information (logging configuration, stall detection settings, thread count) that is not available in the solver state alone.

- **cxf_simplex_step**, **cxf_simplex_step2**, and **cxf_simplex_step3** accept a solver state pointer and an environment pointer. They require solver-level tolerances and algorithm parameters but do not need model-level logging or configuration data.

This distinction reflects the separation of concerns between monitoring/logging functions (which need model context) and computational functions (which operate purely on solver state).

### Return Code Conventions

| Code | Meaning | Functions |
|------|---------|-----------|
| Success (zero) | Iteration completed or no candidates (optimal) | All five functions |
| Infeasibility code | Bound violation or implied bound contradiction confirmed | step, step2, step3 |
| Unbounded code | No blocking variable in the ratio test | step only |
| Out-of-memory code | Eta vector allocation failed | step, step2, step3 |
| Termination code | Time limit, iteration limit, or user abort | post_iterate only |

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_simplex_iterate | Not thread-safe | Writes to log, invokes callback |
| cxf_simplex_step | Not thread-safe | Modifies basis, eta chain, pricing, objective, constraint matrix |
| cxf_simplex_step2 | Not thread-safe | Modifies bounds, pricing, eta chain, activity bounds |
| cxf_simplex_step3 | Not thread-safe | Modifies bounds, pricing, eta chain, activity bounds |
| cxf_simplex_post_iterate | Not thread-safe | Reads/writes progress flags, checks termination status |

All functions operate within a single-threaded simplex solve. Thread safety for concurrent solves is achieved at the model level by creating independent solver instances.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.03, P1.04, P2.01, P2.1, P2.4 (algorithm specs) and P3.16-P3.19 (module specs)
[x] Naming misnomers documented (cxf_simplex_iterate is logging, not iteration)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Achterberg, T., Bixby, R.E., Gu, Z., Rothberg, E., and Weninger, D. (2020). "Presolve Reductions in Mixed Integer Programming." *INFORMS Journal on Computing*, 32(2):473-506.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341-374.
- Gill, P.E., Murray, W., Saunders, M.A., and Wright, M.H. (1989). "A practical anti-cycling procedure for linearly constrained optimization." *Mathematical Programming*, 45(1-3):437-474.
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Koberstein, A. (2005). *The dual simplex method, techniques for a fast and stable implementation.* PhD Thesis, University of Paderborn.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. International Series in Operations Research and Management Science, Vol. 61.
- Savelsbergh, M.W.P. (1994). "Preprocessing and Probing Techniques for Mixed Integer Programming Problems." *ORSA Journal on Computing*, 6(4):445-454.
