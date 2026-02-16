# Module: Basis Operations

## Purpose

The Basis Operations module manages the Product Form of the Inverse (PFI) representation of the simplex basis during LP solver iterations. It provides functions for recording pivot transformations as eta vectors, fixing variables at bounds with appropriate eta records, capturing solver progress snapshots for cycling detection, and computing progress metrics to trigger anti-cycling measures. Together, these functions maintain the implicit basis inverse that the simplex algorithm uses for FTRAN and BTRAN operations, and they provide the monitoring infrastructure that detects degenerate cycling behavior.

## Functions

### cxf_fix_variables_at_bounds

**Purpose:** Identify and fix variables at their bounds during simplex iterations, creating eta vectors for the PFI representation to reduce the working basis size and improve numerical stability.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver's working state containing the basis, constraint matrix, bounds, and objective data
- Input: `env` : pointer-to-Environment - Environment with solver parameters and memory allocation services
- Output: int - Zero on success, or the out-of-memory error code if eta allocation fails

**Preconditions:**
- The solver state must be fully initialized with valid constraint matrix data in both CSR and CSC formats
- The basis header and variable status arrays must be consistent
- The environment must contain valid tolerance and control parameters

**Postconditions:**
- Variables identified as fixable at their bounds have been recorded in the eta vector chain as variable-fixing eta records (see EtaVector Variant 2 in P1.08)
- The objective value accumulator has been updated to reflect the contributions of fixed variables: for each fixed variable j, objective += reducedCost[j] * fixedValue[j]
- For quadratic objectives, diagonal terms contribute 0.5 * Q[j,j] * fixedValue[j]^2 to the objective, and off-diagonal terms are absorbed into neighboring variables' reduced costs: reducedCost[k] += Q[j,k] * fixedValue[j] for each neighbor k
- Reduced costs of fixed variables have been set to zero
- Constraint activity tracking arrays have been updated to reflect removed variable contributions
- The pricing subsystem has been notified of all variable status changes

**Side Effects:**
- Allocates eta vectors from the memory pool (see P2.01 for PFI memory management)
- Modifies constraint matrix row/column count fields to mark fixed variables as inactive
- Updates pricing state via invalidation and update notifications
- Increments the eta vector count on the solver state
- Modifies the objective value accumulator
- Updates piecewise-linear tracking data if PWL variables are being fixed
- Accumulates work metrics on the performance counter

**Error Conditions:**
- Memory allocation failure during eta vector creation -> returns the out-of-memory error code; partial fixing may have occurred

**Behavioral Description:**
The function processes a list of candidate constraints to identify variables that can be safely fixed at their bounds without causing infeasibility. It operates in several phases:

**Phase 1: Skip condition evaluation.** The function exits immediately without action in three situations: (a) when the solver is in a special initialization mode (e.g., reoptimization setup), (b) when the crossover control parameter is disabled in the environment, or (c) when piecewise-linear constraints are active.

**Phase 2: Constraint-driven candidate identification.** For each candidate constraint, the function scans the constraint's row in the CSR matrix to count eligible variables (those that are basic and unflagged). It computes the constraint's activity range -- the span between the tightest possible activity when all eligible variables are at their most-favorable bounds and the current right-hand side. If this range provides enough slack, some variables can be fixed. Equality constraints receive a two-pass treatment with different sign multipliers to handle both directions.

**Phase 3: Ratio-based selection.** Eligible variables are ranked by a ratio metric that measures the feasibility impact of fixing each variable: the product of the variable's bound range and the absolute coefficient in the constraint. Variables are sorted by this ratio (ascending), so variables with the smallest feasibility impact are fixed first. The function greedily selects variables to fix until fixing the next variable would make the constraint infeasible.

**Phase 4: Variable fixing.** For each selected variable, the function:
1. Determines the fixing value (lower or upper bound, based on the coefficient sign direction that best preserves feasibility).
2. Creates an eta record. If the solver is in simplified eta tracking mode (the eta pool is active), a compact record is created storing only the variable index and fixed value. Otherwise, a full record is created that additionally stores the variable's column data from the CSC matrix (filtered to active constraint rows), the previous reduced cost, and the bound status. The full record supports later basis reconstruction during crossover or warm-start scenarios. See P2.01 (Product Form of the Inverse) Step 5 for the variable-fixing eta record format.
3. If the variable has quadratic objective contributions, the function may delegate to cxf_basis_warm to record Q-matrix terms before creating the fixing record.
4. Updates the objective function: adds the fixed variable's reduced cost times its fixed value, plus any quadratic diagonal contribution (0.5 * Q[j,j] * value^2). For off-diagonal Q terms, the function linearizes them into neighboring variables' reduced costs and removes the Q entries from the neighbor's adjacency lists.
5. Notifies the pricing subsystem of the variable change.
6. Updates constraint activity tracking by scanning the fixed variable's column and adjusting per-row activity bounds, counts, and dual steepest-edge weights.
7. Marks the variable as inactive in the constraint matrix by setting its column count to an invalid sentinel.

**Thread Safety:** Not thread-safe. Must be called within a single-threaded simplex iteration context.

**Dependencies:**
- P1.04 (SolverState) - reads and modifies working arrays, basis tracking, and iteration counters
- P1.08 (EtaVector) - creates variable-fixing eta records (Variant 2)
- P2.01 (Product Form of the Inverse) - eta chain management and memory pool allocation
- cxf_basis_warm (this module) - called to record quadratic contributions when fixing variables with Q-matrix terms
- Pricing subsystem (P3.13) - notified of variable status changes via invalidation and update calls

---

### cxf_progress_snapshot

**Purpose:** Capture a lightweight snapshot of the solver's iteration counters and progress metrics, establishing a baseline for subsequent cycling detection via cxf_basis_diff.

**Signature:**
- Input: `state` : pointer-to-SolverState - The solver's working state containing iteration counters
- Input: `snapshot` : pointer-to-array-of-int - Output buffer for the snapshot data; must have capacity for SNAPSHOT_SIZE integer values
- Output: void

**Preconditions:**
- The solver state must be initialized with valid counter values
- The snapshot buffer must be pre-allocated with sufficient capacity (SNAPSHOT_SIZE integers, as defined in P1.02 BasisState)

**Postconditions:**
- The snapshot buffer contains a copy of SNAPSHOT_SIZE integer counter values from the solver state, representing the solver's progress at the time of the call
- No solver state has been modified

**Side Effects:**
- None. This is a pure read-and-copy operation with no memory allocation and no modification of solver state.

**Error Conditions:**
- None. The function performs unconditional copies with no failure modes.

**Behavioral Description:**
The function copies a fixed set of integer counter values from the solver state into the provided snapshot buffer. The snapshot captures:

1. **Problem dimensions:** The current number of variables and constraints (these may differ from the original problem dimensions due to presolve reductions).
2. **Status flags:** The current solution status and bounds-processing state.
3. **Presolve statistics:** Counts of removed rows and columns.
4. **Iteration counters:** A block of progress counters that track various aspects of solver work, including iteration counts, pivot counts, basis change counts, pricing operations, candidate evaluations, feasibility checks, and several additional work metrics. The exact interpretation of some counters is [UNDETERMINED], but their aggregate behavior (measuring solver activity over time) is what matters for the cycling detection algorithm in cxf_basis_diff.

The snapshot is extremely lightweight: it consists of exactly SNAPSHOT_SIZE integer copies with no loops over problem data, no memory allocation, and O(1) time complexity. It is designed to be called frequently (e.g., before each batch of simplex iterations) without measurable overhead.

**Naming history:** Formerly `cxf_basis_snapshot`; renamed to clarify that this function captures only scalar counters (not the variable status array, objective value, or any basis matrix data). The companion function cxf_basis_diff computes a weighted difference score from these counters.

**Thread Safety:** Not thread-safe. The counter values are read without synchronization; must be called from the same thread performing simplex iterations.

**Dependencies:**
- P1.05 (BasisState) - references the progressSnapshot field definition and SNAPSHOT_SIZE constant
- P1.04 (SolverState) - reads iteration counters from the solver state's counter region

---

### cxf_basis_diff

**Purpose:** Compute a weighted, normalized progress score by comparing the current solver state against a previously captured snapshot. A low score indicates potential cycling or stalling, which triggers anti-cycling measures such as perturbation.

**Signature:**
- Input: `state` : pointer-to-SolverState - The current solver state with updated iteration counters
- Input: `snapshot` : pointer-to-array-of-int - A previously captured snapshot from cxf_progress_snapshot
- Output: double - A non-negative progress score; higher values indicate more progress since the snapshot

**Preconditions:**
- The solver state must be the same state from which the snapshot was captured (same solve instance)
- The snapshot must have been populated by a prior call to cxf_progress_snapshot

**Postconditions:**
- Returns a non-negative double representing the weighted, normalized amount of solver progress since the snapshot
- No solver state has been modified

**Side Effects:**
- None. This is a pure computation with no state modification.

**Error Conditions:**
- None. All divisions are protected by minimum-of-one denominators to prevent division by zero.

**Behavioral Description:**
The function computes a composite progress score by measuring changes across multiple categories of solver statistics, normalizing each by problem size, and combining them with category-specific weights. The formula produces a score that is comparable across different problem sizes and balances structural changes (which represent genuine progress) against routine iteration counter increments (which are expected and less meaningful).

The scoring formula combines six terms. Each term computes a delta (current value minus snapshot value) for one or more counters, applies a category-specific weight, and divides by a normalization denominator. Two normalization denominators are used: a *column denominator* (the number of variables minus the number of already-removed columns at snapshot time, floored at one) and a *row denominator* (the sum of active constraint count, matrix status change count, and bounds-processing change count at snapshot time, floored at one). A third normalizer, the total number of nonzeros in the constraint matrix, is used for the structural term.

1. **Structural change term:** Measures the delta in the variable-fixing counter (the counter that tracks how many variables have been permanently fixed at bounds or removed from the working problem). This is normalized by the total constraint matrix nonzero count and receives a heavy weight, because each variable removal represents definitive structural progress that shrinks the problem.

2. **Column reduction term:** Measures the net increase in removed columns, adjusted by subtracting any increase in the total variable count (to account for variables added during cuts or reformulation). This is normalized by the column denominator. It receives unit weight.

3. **Iteration counter term:** Aggregates the deltas in four counters that track routine simplex work: basis membership changes (entering/leaving swaps), pricing operations performed, bound-type conversions (e.g., continuous-to-binary fixings), and an additional activity counter for candidate evaluations. The sum is normalized by the column denominator and receives a light weight, because these counters are expected to increase with every batch of iterations; only the absence of change is informative.

4. **Row statistics term:** Aggregates the deltas in five row-related counters: removed rows (constraints eliminated), matrix status transitions (e.g., feasibility state changes), bounds-processing work, and two additional per-constraint activity counters. The sum is normalized by the row denominator. It receives unit weight and captures the breadth of constraint-level progress.

5. **Conversion term:** Measures the delta in the inequality-to-equality conversion counter (tracking constraints that have been tightened from inequalities to equalities during bound propagation or presolve). This is normalized by the row denominator and receives a moderate weight, because such conversions represent structural progress at the constraint level.

6. **Work counter term:** Measures the delta in a specific per-iteration work metric (tracking cumulative computational effort such as feasibility checks or dual operations). This is normalized by the row denominator with its own dedicated weight.

All deltas are clamped to zero (negative progress is treated as no progress), and all normalization denominators have a floor of one to prevent division by zero. The final score is the sum of all six terms and is guaranteed to be non-negative.

**Usage context:** The calling function (the main LP solve driver) captures a snapshot before a batch of iterations and computes the diff afterward. The diff score is compared against a threshold scaled by the number of iterations elapsed. If the score falls below the threshold (indicating insufficient progress), the solver applies perturbation to break potential degeneracy cycling (see Maros, 2003, Section 9.7 on anti-cycling strategies).

**Thread Safety:** Not thread-safe. Must be called from the simplex iteration thread.

**Dependencies:**
- P1.04 (SolverState) - reads current counter values and the nonzero count for normalization
- cxf_progress_snapshot (this module) - produces the snapshot array that this function compares against
- cxf_simplex_perturbation (P3.14) - the anti-cycling action triggered when this function reports low progress

---

### cxf_basis_warm

**Purpose:** Create a quadratic warm-start eta vector that records a variable's Q-matrix (quadratic objective) contributions in the PFI representation. This enables correct reduced-cost maintenance when variables with quadratic objective terms are fixed during simplex iterations.

**Signature:**
- Input: `env` : pointer-to-Environment - Environment (passed for API consistency; not directly used)
- Input: `state` : pointer-to-SolverState - Solver state with Q-matrix data and eta chain
- Input: `varIndex` : int - Index of the variable whose quadratic contributions are being recorded
- Input: `varValue` : double - The value at which the variable is being fixed
- Output: int - Zero on success, or the out-of-memory error code if eta allocation fails

**Preconditions:**
- The solver state must have valid quadratic objective data (diagonal Q array and off-diagonal Q adjacency structure)
- The variable index must be valid (within the range of problem variables)
- The variable must have at least one nonzero Q-matrix contribution (diagonal or off-diagonal); if it has none, the function returns success immediately without creating an eta vector

**Postconditions:**
- If the variable has Q-matrix contributions: a warm-start eta vector (EtaVector Variant 3, type = WARM_START) has been allocated from the memory pool and prepended to the eta chain
- The eta vector stores the variable index, its bound status, and all associated Q-matrix entries (diagonal and off-diagonal)
- The work counter has been updated to reflect processing effort

**Side Effects:**
- Allocates a warm-start eta vector from the memory pool
- Prepends the new eta vector to the head of the eta chain
- Accumulates work on the performance counter proportional to the number of Q entries processed

**Error Conditions:**
- Memory allocation failure from the eta pool -> returns the out-of-memory error code
- No Q-matrix contributions for the variable -> returns success immediately (no eta created)

**Behavioral Description:**
This function is part of the PFI update mechanism for problems with quadratic objectives. When a variable with nonzero Q-matrix entries is being fixed at a bound (typically called from cxf_fix_variables_at_bounds), the solver must record the quadratic contributions so that reduced costs of neighboring variables can be correctly maintained during warm-start restoration or crossover.

The function proceeds as follows:

1. **Check for Q contributions.** The function examines the diagonal Q value (Q[varIndex, varIndex]) and the off-diagonal Q entry count for the variable. If the diagonal is zero and there are no off-diagonal entries (or the off-diagonal array does not exist), the function returns immediately -- there is nothing to record.

2. **Compute entry count and allocation size.** The total entry count is the sum of (1 if the diagonal is nonzero, else 0) plus the number of off-diagonal Q entries. The eta vector allocation includes a fixed header followed by inline storage for index and value arrays. The index array is padded to an 8-byte boundary to ensure proper alignment of the subsequent value array (see P1.08 EtaVector sparse storage format).

3. **Allocate from the memory pool.** Memory is obtained from the SolverState's arena allocator using bump allocation (see P2.01, Memory Management section). If allocation fails, the function returns the out-of-memory error code.

4. **Initialize the eta vector.** The type tag is set to WARM_START (Variant 3 as defined in P1.08). Internal pointers are set to reference the inline data areas. The new eta vector is linked at the head of the eta chain.

5. **Determine bound status.** The function compares the variable's fixing value against its current lower and upper bounds to assign a status: at lower bound, at upper bound, or superbasic (between bounds).

6. **Copy Q-matrix entries.** If the diagonal Q value is nonzero, it is stored as the first entry (index = varIndex, value = Q[varIndex, varIndex]). Then, off-diagonal entries are copied from the Q-matrix adjacency structure, which stores the quadratic objective in a compressed sparse row format: for each variable, a start index and count identify the off-diagonal neighbors and their coefficients.

7. **Update work counter.** The performance tracking counter is incremented proportionally to the number of Q entries processed, scaled by a configurable factor.

**Thread Safety:** Not thread-safe. Called within single-threaded simplex context.

**Dependencies:**
- P1.04 (SolverState) - reads Q-matrix data, bounds, and eta management fields
- P1.08 (EtaVector) - creates Variant 3 (WARM_START) eta vector
- P2.01 (Product Form of the Inverse) - memory pool allocation and eta chain management
- cxf_fix_variables_at_bounds (this module) - primary caller during variable fixing with quadratic objectives

---

### cxf_pivot_with_eta

**Purpose:** Record a standard simplex pivot operation by creating an eta vector that captures the elementary basis transformation. This is the core PFI update mechanism: each pivot appends one eta vector to the chain, maintaining the implicit basis inverse for FTRAN and BTRAN operations.

**Signature:**
- Input: `env` : pointer-to-Environment - Environment for memory allocation services
- Input: `state` : pointer-to-SolverState - Solver state with constraint matrix and eta chain
- Input: `direction` : int - Pivot direction: zero indicates the entering variable moves from its lower bound; nonzero indicates it moves from its upper bound
- Input: `leavingRow` : int - Constraint row index where the leaving variable was basic
- Input: `enteringVar` : int - Column index of the variable entering the basis
- Input: `leavingVar` : int - Identifier for the variable leaving the basis (may encode additional status information in its upper bits)
- Input: `pivotElement` : double - The pivot coefficient: the value of the constraint matrix entry at the intersection of the leaving row and entering variable's column, after basis transformation. Must be nonzero.
- Input: `etaRowLen` : int - Expected number of nonzero entries in the eta row data (from the CSR matrix view)
- Input: `etaColLen` : int - Expected number of nonzero entries in the eta column data (from the CSC matrix view); zero if column data is not needed (primal simplex mode)
- Output: int - Zero on success, or the out-of-memory error code if eta allocation fails

**Preconditions:**
- The pivot element must be nonzero (a zero pivot is algebraically invalid and would cause division by zero)
- The entering variable and leaving row must be valid indices within the problem dimensions
- The constraint matrix must contain the entering variable's row and column data
- The eta row and column length estimates must be non-negative

**Postconditions:**
- A new pivot eta vector (EtaVector Variant 1, type = PIVOT) has been allocated from the memory pool and prepended to the eta chain
- The eta vector stores the leaving row, entering variable, leaving variable, pivot element, the entering variable's reduced cost, the pivot direction, and the sparse eta row data
- If column data was requested (etaColLen > 0), the eta vector also stores the sparse column representation for dual simplex support
- The total eta count and row eta count have each been incremented by one
- The work counter has been updated based on the number of matrix entries processed

**Side Effects:**
- Allocates a pivot eta vector from the memory pool
- Prepends the new eta vector to the head of the eta chain
- Increments both the total eta count and the row-type eta count on the solver state
- Accumulates work on the performance counter proportional to the number of CSR and CSC entries processed

**Error Conditions:**
- Memory allocation failure from the eta pool -> returns the out-of-memory error code; the eta counts are not incremented in this case [UNDETERMINED: whether counts are incremented before or after allocation success check]

**Behavioral Description:**
This function implements Step 2 (Eta Vector Creation) of the PFI algorithm described in P2.01 (Product Form of the Inverse). Each simplex pivot replaces one column of the basis matrix. Rather than recomputing the entire basis inverse, this function records the elementary transformation as a sparse eta vector.

**Allocation and setup:**
The function computes the total allocation size: a fixed header plus space for the eta row data (index array padded to 8-byte boundary, plus value array), and optionally space for column data if dual simplex support is needed (etaColLen > 0). Memory is obtained from the SolverState's arena allocator. The type tag is set to PIVOT (Variant 1 as defined in P1.08). Internal pointers are configured to reference the inline data areas following the header. The new eta vector is linked at the head of the chain.

**Eta row extraction (Phase 1):**
The function scans the entering variable's row in the CSR matrix representation. For each nonzero entry in the row where both the column index is valid (non-negative) and the variable at that column has a non-negative basis status (indicating it is basic or eligible):
- The column index is stored in the eta row index array.
- The eta coefficient is computed as: -coefficient / pivotElement. The negation converts the constraint matrix entry into the elementary transformation that undoes the pivot. This follows the standard PFI formula: for entry (pivotRow, j), the eta value is -A[pivotRow, j] / A[pivotRow, enteringVar] (see Maros, 2003, Chapter 5).

**Eta column extraction (Phase 2, conditional):**
If etaColLen is greater than zero (needed for dual simplex operations), the function additionally scans the leaving row's column in the CSC matrix representation. For each nonzero entry where the row is valid, is not the entering variable's own row, and has a non-negative constraint status:
- The row index and raw coefficient value (not scaled by the pivot element) are stored in the eta column data arrays.

**Work tracking:**
After each extraction phase, the performance counter is incremented proportionally to the number of matrix entries scanned, using phase-specific scaling factors.

**Thread Safety:** Not thread-safe. Called within single-threaded simplex iteration context.

**Dependencies:**
- P1.04 (SolverState) - reads CSR and CSC matrix data, basis status arrays, reduced costs, and eta management fields
- P1.08 (EtaVector) - creates Variant 1 (PIVOT) eta vector with row data and optional column data
- P2.01 (Product Form of the Inverse) - defines the PFI update algorithm; this function implements Step 2
- Memory pool (bump allocator) - allocates from the SolverState's arena allocator

---

## Module-Level Behavioral Notes

### Naming Clarifications

**Naming history:** Formerly `cxf_basis_refactor`; renamed to `cxf_fix_variables_at_bounds` to clarify that this function identifies and fixes variables at bounds to reduce the working basis size, rather than performing LU refactorization. The actual benefit is a smaller effective basis, which speeds up subsequent FTRAN/BTRAN operations. True LU refactorization of the basis matrix is a separate operation (see P2.01, Step 6: Refactorization).

### Relationships Between Functions

The five functions in this module interact as follows:

1. **cxf_fix_variables_at_bounds** is the primary variable-fixing function. During its fixing phase, it may call **cxf_basis_warm** to record quadratic objective contributions for variables that have Q-matrix entries. Both functions create eta vectors that are prepended to the same eta chain managed by the SolverState.

2. **cxf_pivot_with_eta** is called independently from the simplex iteration loop (not from this module's other functions). It records standard pivot operations, while cxf_fix_variables_at_bounds records variable-fixing operations. Both contribute to the same eta chain, and both types of records must be processed during FTRAN and BTRAN.

3. **cxf_progress_snapshot** and **cxf_basis_diff** form a matched pair for cycling detection. The snapshot captures a baseline, and the diff measures progress against that baseline. They are called from the main LP solve driver (cxf_solve_lp), not from other functions in this module.

### Eta Vector Types Created by This Module

| Function | Eta Type | Variant (P1.08) | Purpose |
|----------|----------|------------------|---------|
| cxf_fix_variables_at_bounds | VARIABLE_FIX | Variant 2 (compact or full) | Records variable fixed at bound |
| cxf_basis_warm | WARM_START | Variant 3 | Records quadratic objective contributions |
| cxf_pivot_with_eta | PIVOT | Variant 1 (with optional column data) | Records simplex pivot transformation |

### Memory Pool Usage

All eta vectors created by this module are allocated from the SolverState's arena (bump) allocator. Individual eta vectors are never freed. The entire pool is released in bulk during basis refactorization (clearing the chain) or simplex cleanup. This matches the create-once, read-many, free-all-at-once lifecycle of eta vectors described in P2.01.

### Interaction with the Pricing Subsystem

cxf_fix_variables_at_bounds is the only function in this module that interacts with the pricing subsystem. When a variable is fixed, the function invalidates the pricing cache entry for that variable (so the pricer does not consider it for future pivot selection) and sends an update notification so the pricing state reflects the reduced problem size. cxf_pivot_with_eta does not interact with pricing directly; pricing updates after a pivot are handled by the calling simplex step function.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_fix_variables_at_bounds | Not thread-safe | Modifies solver state, eta chain, pricing state, and constraint matrix |
| cxf_progress_snapshot | Not thread-safe | Reads solver counters without synchronization |
| cxf_basis_diff | Not thread-safe | Reads solver counters without synchronization |
| cxf_basis_warm | Not thread-safe | Modifies eta chain and allocates from memory pool |
| cxf_pivot_with_eta | Not thread-safe | Modifies eta chain, eta counts, and allocates from memory pool |

All functions in this module operate within a single-threaded simplex solve. Thread safety for concurrent solves is achieved at the model level by creating independent solver instances, each with its own SolverState and eta chain.

---

## Dependencies and Out-of-Scope Components

### Sparse LU Factorization (Out of Scope)

Sparse LU factorization of the basis matrix is **outside the scope of this specification**. The PFI/eta vector system described in this module handles incremental basis updates: each simplex pivot appends one eta vector to the chain, and FTRAN/BTRAN apply the accumulated transformations. However, as the eta chain grows, both computational cost (applying all transformations) and numerical error accumulate. Periodic refactorization -- recomputing B = L * U from the current basis columns -- is essential to restore efficiency and numerical accuracy.

This refactorization step requires a sparse LU decomposition with partial pivoting and sparsity-preserving column ordering. The algorithmic foundations are well established in the literature:

- **Bartels and Golub (1969)** introduced LU decomposition as a replacement for the explicit product form in the simplex method, showing that periodic refactorization controls numerical drift while maintaining efficiency.
- **Forrest and Tomlin (1972)** developed practical sparse update techniques (the "Forrest-Tomlin update") that maintain triangular factors between refactorizations, reducing fill-in compared to the classical product form.
- **Suhl and Suhl (1990)** addressed the computational challenges of sparse LU factorization for large-scale LP bases, including threshold pivoting strategies and dynamic Markowitz ordering for fill-in minimization.

A reimplementation may use any correct sparse LU factorization. Suitable existing open-source implementations include:

- **LUSOL** (Gill, Murray, Saunders, and Wright) -- a well-tested sparse LU package designed specifically for LP basis factorization, with Bartels-Golub and Forrest-Tomlin update support.
- **SuiteSparse** (Davis) -- a comprehensive sparse matrix library including UMFPACK for unsymmetric LU factorization with automatic pivot ordering.
- **Maros (2003), Chapter 5** provides a self-contained algorithmic description of sparse LU factorization for LP solvers, including Markowitz ordering, threshold pivoting, and practical implementation guidance suitable for a from-scratch implementation.

The interface between this module and the LU factorization component is defined by the refactorization lifecycle described in P1.05 (BasisState): when `etaTotalCount` reaches the `refactorizationThreshold` or numerical instability is detected, the entire eta chain is discarded, a fresh LU factorization is computed from the current basis columns, and the eta counters are reset to zero.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1/2 references
[x] Explicit cross-references to P1.05 (BasisState), P1.04 (SolverState), P1.08 (EtaVector), P2.01 (PFI algorithm)
[x] [UNDETERMINED] used for unknowns (eta count increment timing)
[x] Passes the Clean Room Test: could be written without seeing the binary
```

## References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Dantzig, G.B. and Orchard-Hays, W. (1954). "The Product Form for the Inverse in the Simplex Method." *Mathematical Tables and Other Aids to Computation*, 8(46):64-67.
- Bartels, R.H. and Golub, G.H. (1969). "The Simplex Method of Linear Programming Using LU Decomposition." *Communications of the ACM*, 12(5):266-268.
- Forrest, J.J.H. and Tomlin, J.A. (1972). "Updated Triangular Factors of the Basis to Maintain Sparsity in the Product Form Simplex Method." *Mathematical Programming*, 2(1):263-278.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
