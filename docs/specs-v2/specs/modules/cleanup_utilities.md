# Module: Cleanup Utilities

## Purpose

The Cleanup Utilities module contains functions responsible for restoring solver and model state after various operations complete. These functions serve three distinct purposes: (1) releasing temporary data structures used to batch model modifications, (2) restoring signal handling state after optimization completes, and (3) performing constraint-based bound tightening during the simplex cleanup phase.

Despite the module's name, its most algorithmically significant member is the bound propagation function (also known as the cleanup helper), which implements Feasibility-Based Bound Tightening (FBBT) -- a standard preprocessing technique described by Savelsbergh (1994) and Brearley, Mitra, and Williams (1975). This function derives tighter variable bounds from constraint activity analysis and detects hidden infeasibilities. The remaining functions handle resource cleanup for coefficient change tracking structures and signal handler restoration.

Two of the four function names in this module -- cxf_propagate_bounds and cxf_propagate_bounds -- refer to the same underlying function. cxf_propagate_bounds is the algorithmically descriptive name; cxf_propagate_bounds reflects its calling context (invoked during the simplex cleanup phase). Both names are documented here, with cxf_propagate_bounds as the primary specification and cxf_propagate_bounds as an alias.

## Functions

### cxf_cleanup_coeff_change

**Purpose:** Free a coefficient change tracking structure and all its internal arrays, clearing the caller's reference to indicate cleanup is complete.

**Signature:**
- Input: `environment` : pointer-to-Environment -- The environment providing memory management services
- Input: `tracker_ref` : pointer-to-pointer-to-CoefficientChangeTracker -- Indirect reference to the tracker structure to be freed
- Output: void

**Preconditions:**
- No strict preconditions; the function handles null inputs gracefully

**Postconditions:**
- If tracker_ref is non-null and points to a non-null tracker, all internal arrays within the tracker are freed, the tracker structure itself is freed, and the caller's pointer (*tracker_ref) is set to null
- If tracker_ref is null or points to a null pointer, no state is modified

**Side Effects:**
- Frees up to three internal arrays (row indices, column indices, coefficient values) belonging to the tracker
- Frees the tracker structure itself
- Sets the caller's tracker pointer to null

**Error Conditions:**
- Null tracker_ref pointer -> silent return, no action
- Null tracker pointer (*tracker_ref is null) -> silent return, no action

**Behavioral Description:**
The function first checks whether the indirect reference is non-null and whether the referenced tracker pointer is non-null. If either check fails, the function returns immediately without action.

When a valid tracker is found, the function frees each of the tracker's three internal arrays -- the row index array, the column index array, and the coefficient value array -- if they are allocated (non-null). After freeing each array, the corresponding pointer within the tracker is set to null. The function then frees the tracker structure itself, and finally sets the caller's tracker pointer to null.

The CoefficientChangeTracker is a small structure that stores pending coefficient modifications as three parallel arrays: row indices, column indices, and new coefficient values. It also maintains a count of pending changes and an allocated capacity. This structure is populated when the user requests coefficient changes (e.g., via the public coefficient modification API) and is consumed when the model update is applied. This cleanup function is called after the changes have been applied to the constraint matrix, or when the model is being destroyed with unapplied changes.

This function participates in the solver's lazy update pattern: coefficient changes are batched in the tracker rather than applied immediately, and this cleanup function releases the batch after it has been processed.

**Thread Safety:** Unsafe. No synchronization is performed. The caller must ensure exclusive access to the tracker structure.

**Dependencies:**
- Memory deallocation service (from the Environment)

---

### cxf_cleanup_optimization

**Purpose:** Restore the default interrupt signal handler after an optimization call completes, clearing the temporary signal handling state that was installed before optimization began.

**Signature:**
- Input: `model` : pointer-to-Model -- The model whose signal handling state should be cleaned up
- Output: void

**Preconditions:**
- No strict preconditions; the function validates the model before proceeding

**Postconditions:**
- If the model is valid and a custom signal handler was installed for this optimization, the default signal handler is restored, the global model reference used by the handler is cleared, and the model's signal-handler-active flag is reset
- If the model is invalid, no handler was installed, or the environment uses a remote deployment mode, no state is modified

**Side Effects:**
- Restores the operating system's default interrupt signal (SIGINT) handler
- Clears a module-level global model pointer that was used by the custom signal handler to access the model during optimization
- Resets the model's signal-handler-active flag to indicate no custom handler is installed

**Error Conditions:**
- Invalid model (fails structural validation) -> silent return, no action
- Signal handler not active (flag not set on model) -> silent return, no action
- Remote deployment mode (remote solver, cloud, or cluster manager) -> silent return, no action (remote deployments use a different interrupt mechanism)

**Behavioral Description:**
The function first validates the model using the standard structural validation check. If the model is invalid, it returns immediately.

Next, it checks the model's signal-handler-active flag. If this flag is not set, it means no custom signal handler was installed for this optimization (or cleanup has already been performed), so the function returns.

The function then checks the environment's deployment mode. For remote deployment types (remote solver, cloud, cluster manager), signal handling is managed through the remote protocol rather than through local OS signals, so the function returns without modifying signal state.

If all three checks pass -- valid model, handler active, local deployment -- the function performs three cleanup actions in order:

1. Restores the default interrupt signal handler by calling the operating system's signal registration function for the interrupt signal (SIGINT) with the default handler disposition.
2. Clears the module-level global model pointer to null. This pointer is set by the counterpart setup function (invoked before optimization) and allows the custom signal handler to access the model being optimized. Clearing it prevents dangling references after the model might be freed.
3. Resets the model's signal-handler-active flag to zero, indicating that no custom handler is currently installed.

This function is the cleanup counterpart to a setup function that is called before optimization begins. The setup function saves the current signal handler, installs a custom handler that supports graceful termination (allowing the user to press Ctrl-C to request the solver to stop), and sets the global model pointer and active flag. This cleanup function reverses those actions.

**Thread Safety:** Unsafe. Signal handlers are process-global state. This function must not be called concurrently with the setup function or with other signal-modifying operations. In practice, it is called from the optimization dispatch function under the solve lock.

**Dependencies:**
- Model structural validation
- Operating system signal management (POSIX signal() or platform equivalent)

---

### cxf_propagate_bounds

**Purpose:** Perform iterative constraint-based bound tightening using a worklist-driven propagation algorithm, deriving tighter variable bounds from constraint activity analysis and detecting infeasibilities.

**Aliases:** cxf_propagate_bounds (reflects the calling context: invoked during the simplex cleanup phase)

**Signature:**
- Input: `environment` : pointer-to-Environment -- The environment providing memory allocation services
- Input: `solver_state` : pointer-to-SolverState -- The current solver state containing the constraint matrix in both row-major and column-major sparse formats, basis status arrays, and diagnostic output fields
- Input/Output: `lower_bounds` : array-of-double [numVars] -- Working variable lower bounds, tightened in place
- Input/Output: `upper_bounds` : array-of-double [numVars] -- Working variable upper bounds, tightened in place
- Input: `bound_class` : array-of-byte [numVars] -- Per-variable bound classification codes indicating constraint sense context (less-than-or-equal, equality, greater-than-or-equal, or free)
- Input/Output: `lower_activity` : array-of-double [numConstrs] -- Pre-computed lower activity bounds for each constraint, updated incrementally as bounds change
- Input/Output: `upper_activity` : array-of-double [numConstrs] -- Pre-computed upper activity bounds for each constraint, updated incrementally as bounds change
- Input/Output: `positive_unbounded_count` : array-of-int [numConstrs] -- Count of variables with positive coefficient that are unbounded above, per constraint
- Input/Output: `negative_unbounded_count` : array-of-int [numConstrs] -- Count of variables with negative coefficient that are unbounded below, per constraint
- Input: `infinity_threshold` : double -- Magnitude at or above which a bound is treated as unbounded
- Input: `feasibility_tolerance` : double -- Tolerance for infeasibility detection and minimum improvement thresholds
- Output: return value : int -- Status code: success (0), infeasible, or out-of-memory

**Preconditions:**
- The solver state contains valid row-major and column-major sparse matrix representations of the constraint matrix
- The activity arrays (lower_activity, upper_activity) and unbounded count arrays (positive_unbounded_count, negative_unbounded_count) are initialized consistently with the current variable bounds by the caller
- lower_bounds[j] <= upper_bounds[j] for all variables j
- feasibility_tolerance is positive
- infinity_threshold is positive and represents the solver's convention for treating a bound as effectively infinite

**Postconditions:**
- If status is success: lower_bounds[j] >= original lower_bounds[j] and upper_bounds[j] <= original upper_bounds[j] for all j (bounds are tightened monotonically); activity arrays and unbounded counts are consistent with the tightened bounds
- If status is infeasible: the solver state's diagnostic fields record the index of the constraint or variable that caused the infeasibility detection
- If status is out-of-memory: no bounds or activity arrays are modified
- All internally allocated working memory is freed before return regardless of status

**Side Effects:**
- Modifies the lower_bounds and upper_bounds arrays in place (tightening)
- Modifies the activity and unbounded count arrays in place
- On infeasibility detection, writes the violating constraint index and/or variable index to diagnostic fields on the solver state
- Accumulates computational work to the solver state's work counter (if the work counter is non-null), for performance profiling
- Allocates and frees temporary working arrays (circular queue and membership flags)

**Error Conditions:**
- Out of memory during allocation of working arrays -> returns OUT_OF_MEMORY error code without modifying any input arrays
- Infeasibility detected (bounds cross) -> returns INFEASIBLE status code with diagnostic indices set on the solver state
- Empty problem (zero variables) -> returns success immediately with no modifications

**Behavioral Description:**
This function implements the standard Feasibility-Based Bound Tightening (FBBT) algorithm as described by Savelsbergh (1994) and further analyzed by Belotti et al. (2010). The algorithm iteratively derives tighter variable bounds by analyzing what values each variable can take within each constraint, given the bounds on all other variables.

The algorithm proceeds in the following phases:

1. **Handle empty problem.** If the number of variables is zero, the function updates the work counter (if present) and returns success immediately.

2. **Allocate working arrays.** Two integer arrays of size numVars are allocated: a circular queue for storing indices of constraints to be processed, and a boolean membership array that prevents duplicate queue entries. If either allocation fails, previously allocated memory is freed and the function returns with an out-of-memory status.

3. **Initialize the worklist.** The function iterates over all variables and marks each as initially seen in the membership array. Variables that are active in the basis (as indicated by the basis header having a non-negative entry) are enqueued into the circular queue. This seeds the worklist with all active constraints for the initial pass.

4. **Main propagation loop.** The function processes constraints from the circular queue until the queue is empty or a maximum number of full passes (epochs) is reached. The maximum pass count is a small constant (on the order of ten), reflecting the empirical observation from Belotti et al. (2010) that FBBT typically converges within a few passes on practical problems. Each pass through the queue constitutes one epoch, tracked by recording the queue position at which each epoch ends.

   For each constraint dequeued:

   a. **Constraint-level infeasibility check.** Before deriving variable bounds, the function checks whether the constraint is already violated by the current activity bounds. For constraints with an upper-bound sense (less-than-or-equal or equality): if all terms contributing to the upper activity are finite (the negative unbounded count is zero) and the upper activity exceeds the feasibility tolerance, the constraint cannot be satisfied and the function returns infeasible. A symmetric check is performed for constraints with a lower-bound sense (greater-than-or-equal or equality) using the lower activity and positive unbounded count.

   b. **Implied bound derivation.** For each variable appearing in the constraint (traversed via the column-major sparse matrix), the function computes candidate tighter bounds using the activity-based derivation described by Savelsbergh (1994). The key idea: for a constraint with coefficient a_j for variable x_j, if all other variables' contributions to the constraint's activity are finitely bounded, then x_j is bounded by the slack between the constraint's right-hand side and the aggregate activity of the other variables, divided by a_j. The sign of a_j determines whether this yields an upper or lower bound on x_j. Coefficients with absolute value below a tiny threshold (to avoid division by near-zero) are skipped. For equality constraints, both upper-bounded and lower-bounded derivations apply, and the tighter of the two resulting bounds is taken.

   A special case arises when exactly one variable contributing to the activity is unbounded (the unbounded count is one) and that variable is the one being examined. In this case, the finite activity sum of all other variables enables derivation of an implied bound for the unbounded variable.

   c. **Bound tightening and infeasibility detection.** If a candidate lower bound exceeds the current lower bound by more than a minimum improvement threshold (computed as a small fraction of the feasibility tolerance), the function checks whether the new bound would cross the current upper bound. If so, the function returns infeasible with the variable's index recorded in the solver state's diagnostic field. Otherwise, the new lower bound is accepted. The symmetric check applies for upper bound tightening.

   d. **Activity update propagation.** When a variable's bound is tightened, every constraint containing that variable must have its activity bounds updated. The function traverses the constraints containing the affected variable using the row-major sparse matrix. For each affected constraint, the activity contribution is updated incrementally:

   - If the variable was previously finitely bounded, the activity change is computed as the coefficient times the difference between the new and old bounds, and added to the appropriate activity (lower or upper, depending on the coefficient sign).

   - If the variable was previously unbounded, the unbounded count for the affected constraint is decremented, and the newly-finite contribution is added to the activity sum.

   Each affected constraint is enqueued (if not already present) for re-examination.

   Activity updates employ a compensated summation heuristic inspired by Kahan (1965) to detect floating-point cancellation errors. After computing a sum S + delta, the algorithm checks whether the operation is reversible (i.e., (S + delta) - delta recovers S, or (S + delta) - S recovers delta). If not, a small multiplicative correction factor is applied. The direction of the correction is conservative: upper activities are rounded away from zero in the direction that keeps them as upper bounds, and lower activities are rounded in the direction that keeps them as lower bounds. This preserves the mathematical validity of subsequent implied bound derivations at the cost of slightly weaker tightening.

   e. **Epoch tracking.** The circular queue uses modular arithmetic for wraparound. When the read pointer reaches the end of the allocated capacity, it wraps to zero and the epoch counter increments. When the epoch counter exceeds the maximum pass count, the loop terminates. The loop also terminates when the read pointer reaches the current write position (queue empty).

5. **Cleanup.** The function frees the circular queue and membership arrays and returns the status code (success or infeasible).

**Complexity:**
- Best case: O(n) -- single pass with no bound changes, where n is the number of variables
- Typical case: O(K * nnz) -- K full passes (typically 2-3) through the constraint matrix, where nnz is the total number of nonzero coefficients
- Worst case: O(K_max * nnz) -- all maximum passes executed, bounded by the iteration cap

**Thread Safety:** Unsafe. This function operates on solver state that is not thread-safe. It must be called within a single-threaded context during the simplex cleanup phase.

**Dependencies:**
- Memory allocation and deallocation services (from the Environment)
- SolverState: row-major and column-major sparse matrix representations, basis header, variable status array, work counter, diagnostic output fields
- Layer 2 algorithm specification: Bound Propagation (Feasibility-Based Bound Tightening)

**References:**
- Savelsbergh, M.W.P. (1994). "Preprocessing and probing techniques for mixed integer programming problems." *ORSA Journal on Computing*, 6(4):445-454.
- Brearley, A.L., Mitra, G., and Williams, H.P. (1975). "Analysis of mathematical programming problems prior to applying the simplex algorithm." *Mathematical Programming*, 8(1):54-83.
- Belotti, P., Cafieri, S., Lee, J., and Liberti, L. (2010). "Feasibility-based bounds tightening via fixed points." In *Combinatorial Optimization and Applications (COCOA 2010)*, LNCS vol. 6508, pp. 65-76, Springer.
- Kahan, W. (1965). "Pracniques: Further remarks on reducing truncation errors." *Communications of the ACM*, 8(1):40.

---

### cxf_propagate_bounds

**Purpose:** Alias for cxf_propagate_bounds. See cxf_propagate_bounds for the complete behavioral specification.

**Naming history:** Formerly `cxf_cleanup_helper`; renamed to `cxf_propagate_bounds` to better reflect its actual behavior of performing iterative constraint-based bound tightening.

**Signature:** Identical to cxf_propagate_bounds.

**Behavioral Description:** Identical to cxf_propagate_bounds. Both names refer to the same function.

---

## Module-Level Behavioral Notes

### Relationship Between the Four Functions

The four functions in this module serve three distinct purposes:

| Function | Category | Complexity | Called From |
|----------|----------|------------|-------------|
| cxf_cleanup_coeff_change | Resource cleanup | Simple (leaf function) | Model destruction, model update, pending buffer cleanup |
| cxf_cleanup_optimization | Signal restoration | Simple (leaf function) | Optimization dispatch (after optimization completes) |
| cxf_propagate_bounds / cxf_propagate_bounds | Bound tightening algorithm | Complex (iterative, allocates working memory) | Simplex cleanup phase |

### Naming Convention

Two of the four listed function names -- cxf_propagate_bounds and cxf_propagate_bounds -- are aliases for the same underlying function. The name cxf_propagate_bounds was assigned during early analysis based on its call site (the simplex cleanup function). The name cxf_propagate_bounds was assigned during deeper analysis when the function's algorithm was understood. Both names appear in the function map for traceability. In this specification, cxf_propagate_bounds is the primary name and cxf_propagate_bounds is documented as an alias.

### Common Patterns

**Null-safety:** Both cxf_cleanup_coeff_change and cxf_cleanup_optimization perform early validation of their inputs (null checks, model validation) and return silently on invalid input. This defensive pattern is standard for cleanup functions, which may be called in error recovery paths where not all structures were successfully initialized.

**Cleanup ordering:** These functions are designed to be called in a specific order relative to the operations they clean up:
- cxf_cleanup_coeff_change is called after coefficient changes have been applied to the matrix or when the model is being destroyed.
- cxf_cleanup_optimization is called after the optimization call returns, before the solve lock is released.
- cxf_propagate_bounds is called during the simplex cleanup phase, after the main simplex iterations have completed but before final solution extraction.

### Bound Propagation in Context

cxf_propagate_bounds operates in a phase where the simplex solver has completed its main iterations. The caller (the simplex cleanup function) is responsible for:
1. Initializing the activity arrays (lower_activity, upper_activity) and unbounded count arrays (positive_unbounded_count, negative_unbounded_count) from the current variable bounds and constraint matrix before calling this function.
2. Interpreting the return status: if infeasible, the solver reports the problem as infeasible; if successful, the tightened bounds may detect variables that can be fixed.

The bound propagation algorithm references the Layer 2 algorithm specification on Feasibility-Based Bound Tightening, which provides the full mathematical derivation, convergence theory, and numerical considerations.

### Signal Handling Architecture

cxf_cleanup_optimization is part of a setup/cleanup pair:
- **Before optimization:** A setup function installs a custom interrupt signal handler that enables graceful termination. It stores a reference to the model being optimized in a module-level global variable, sets the model's signal-handler-active flag, and saves the previous signal handler.
- **After optimization:** cxf_cleanup_optimization reverses these actions: restores the default signal handler, clears the global model reference, and resets the active flag.

This pair ensures that during optimization the user can request graceful termination (e.g., via Ctrl-C), while outside of optimization the default signal behavior is in effect. Remote deployment modes (remote solver, cloud, cluster manager) bypass this mechanism because they use protocol-level termination signaling rather than local OS signals.

### Lazy Update Pattern

cxf_cleanup_coeff_change participates in the solver's lazy update pattern, where model modifications are batched in a pending buffer rather than applied immediately. The CoefficientChangeTracker structure stores batched coefficient changes as three parallel arrays (row indices, column indices, new values). This cleanup function is the destructor for that structure, called when the batch has been applied or when the model is being freed with unapplied changes.

---

## IP Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] All algorithms reference published work (Savelsbergh 1994, Belotti et al. 2010, Kahan 1965)
[x] Constants described by algorithmic role, not numeric value
[x] Passes the Clean Room Test: could be written without seeing the binary
```
