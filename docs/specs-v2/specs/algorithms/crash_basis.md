# Crash Basis Construction

## Published Reference

- Gould, N.I.M. and Reid, J.K. (1989). "New crash procedures for large systems of linear constraints." *Mathematical Programming*, 45:475-501.
- Bixby, R.E. (1992). "Implementing the simplex method: The initial basis." *ORSA Journal on Computing*, 4(3):267-284.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 5 and 9.
- Maros, I. and Mitra, G. (1996). "Strategies for creating advanced bases for large-scale linear programs." *Annals of Operations Research*, 62:101-118.

The specific variant implemented here is a **row-scanning crash** that evaluates each constraint for feasibility and selects slack variables into the initial basis, with a secondary pass to remove redundant sparse rows. This is a lightweight crash variant in the spirit of Gould and Reid (1989), designed for rapid initial basis construction with early infeasibility detection. It differs from the more elaborate triangular crash procedures (which make multiple column-scoring passes) by focusing on row feasibility as the primary selection criterion.

## Purpose

The crash procedure constructs an initial basis for the simplex algorithm that is better than--or at least as good as--the trivial all-slack (logical) basis. In the trivial basis, every basic variable is a slack variable, and the basis matrix is the identity. While trivially non-singular, this starting point is typically far from optimal and can require many simplex iterations to reach a solution.

A crash procedure accelerates the simplex method by:

1. **Detecting trivial infeasibility** before the main iteration loop begins, avoiding wasted computation.
2. **Classifying constraints** by their bound status, so that the solver enters its first iteration with a partially constructed basis that reflects the problem structure.
3. **Removing sparse redundant rows** from active consideration, reducing column nonzero counts and potentially reducing fill-in during the subsequent LU factorization of the basis matrix.

The crash procedure executes once, during simplex initialization, after the SolverState has been allocated and populated with problem data but before the first LU factorization or simplex iteration.

## Inputs

- **SolverState**: The fully initialized solver working state, containing:
  - Problem dimensions: number of constraints (m).
  - Row-major sparse matrix (CSR): row start pointers, row entry counts, column indices for each row entry.
  - Column nonzero counts: the number of active nonzero entries in each column, updated during the crash.
  - Constraint right-hand side values: the RHS vector b.
  - Constraint sense array: one character per constraint indicating equality ('='), less-than-or-equal, or greater-than-or-equal.
  - Row status array: per-row status indicators, initialized prior to crash. Each row has one of:
    - *Unassigned* (status = 0): the row has not been assigned a basis role.
    - *Candidate for removal* (status > 0): the row has been marked by a prior initialization step as a candidate for removal from the active constraint set (e.g., because it corresponds to a very sparse row or a non-binding inequality).
  - Basis count accumulator: running count of rows added to the basis.
  - Work counter (optional): pointer to a performance accounting accumulator, or null if work tracking is disabled.
  - Problem row index: diagnostic field to record which row caused infeasibility.

- **Environment**: Solver environment containing:
  - Primal feasibility tolerance (epsilon_feas): a small positive value, typically 1e-6, used to determine whether a constraint's RHS value is consistent with the basis assignment.

### Preconditions

1. The SolverState has been fully initialized with valid problem dimensions, sparse matrix data, and bound information.
2. The row status array has been populated: every row has status 0 (unassigned) or a positive value (candidate for removal). No row has a negative status (those values are reserved for post-crash assignment).
3. The constraint sense array, RHS values, and column nonzero counts are consistent with the constraint matrix.
4. The feasibility tolerance is a positive finite value.

## Outputs

- **Modified SolverState**, specifically:
  - Row status array: each processed row is assigned a definitive basis status:
    - *Basic at lower bound* (status = BASIC_LOWER): the row's slack variable enters the basis at its lower bound.
    - *Basic at upper bound* (status = BASIC_UPPER): the row was a removal candidate and its slack variable enters the basis at its upper bound.
    - Rows that were neither unassigned nor candidates retain their original status.
  - Basis count: incremented by the number of rows assigned to the basis.
  - Column nonzero counts: decremented for each column entry in a removed row (reflecting that the row is no longer active).
  - Column index entries: entries in removed rows are marked as inactive (set to a sentinel value of -1).
  - Work counter: if non-null, incremented proportionally to the computational effort expended.
  - Problem row index: set to the index of the offending row if infeasibility is detected.

- **Return status**: either success (feasible initial basis constructed) or infeasibility detected (a constraint's RHS violates the feasibility tolerance).

### Postconditions

1. On success, every unassigned row has been assigned BASIC_LOWER status, and every eligible candidate row has been assigned BASIC_UPPER status. The basis count reflects the total number of assignments.
2. On infeasibility, the problem row index identifies the constraint that failed the feasibility check. The basis count reflects partial progress (rows assigned before the infeasible row was encountered).
3. Column nonzero counts are consistent with the active (non-removed) entries in the constraint matrix.

## Algorithm Description

### Overview

The crash procedure makes a single pass through all constraints. For each constraint, it performs one of two actions based on the row's pre-assigned status:

- **Unassigned rows** (status = 0): the row is checked for feasibility. If feasible, the corresponding slack variable is assigned to the basis at its lower bound. If infeasible, the procedure terminates immediately with a diagnostic report.

- **Candidate rows** (status > 0): if the row is a non-equality constraint with a sufficiently large RHS, the row is removed from active consideration. All column entries in that row are marked as inactive, and the corresponding column nonzero counts are decremented. The slack variable enters the basis at its upper bound.

This approach is a simplified crash in the taxonomy of Maros (2003, Section 5.1): rather than performing multiple passes with column scoring to build a triangular basis of structural variables, it accepts the slack basis for all feasible rows and focuses on quickly identifying infeasible constraints and removing sparse non-binding rows. The benefit is speed: the procedure runs in O(nnz_removed) time, where nnz_removed is the total number of nonzeros in removed rows, plus O(m) for the row scan itself.

The pre-assignment of positive status values to candidate rows is performed by a separate initialization step (typically during simplex setup), which may use sparsity-based heuristics or bound analysis to identify rows that are unlikely to be active at the optimum. The crash procedure itself does not compute these candidate scores; it only acts on them.

### Detailed Steps

**Input**: SolverState S, feasibility tolerance epsilon_feas

**Output**: Return SUCCESS or INFEASIBLE

```
PROCEDURE CrashBasis(S, epsilon_feas):

    LET m := S.numConstrs
    LET basicCount := 0

    FOR i := 0 TO m - 1 DO

        IF S.rowStatus[i] = 0 THEN
            // --- Unassigned row: feasibility check ---

            LET rhs := S.constraintRHS[i]
            LET sense := S.constraintSense[i]

            IF sense = '=' THEN
                // Equality constraint: |rhs| must be small
                IF |rhs| >= epsilon_feas THEN
                    S.problemRowIndex := i
                    S.numBasic := S.numBasic + basicCount
                    RETURN INFEASIBLE
                END IF
            ELSE
                // Inequality constraint: rhs must not be too negative
                IF rhs < -epsilon_feas THEN
                    S.problemRowIndex := i
                    S.numBasic := S.numBasic + basicCount
                    RETURN INFEASIBLE
                END IF
            END IF

            // Row is feasible: assign slack to basis at lower bound
            S.rowStatus[i] := BASIC_LOWER
            basicCount := basicCount + 1

        ELSE IF S.rowStatus[i] > 0 THEN
            // --- Candidate row: conditional removal ---

            IF S.constraintSense[i] != '=' AND
               S.constraintRHS[i] >= epsilon_tiny THEN

                // Remove all column entries in this row
                LET start := S.rowStart[i]
                LET count := S.rowColCount[i]

                FOR k := start TO start + count - 1 DO
                    LET col := S.rowColIndices[k]
                    IF col >= 0 THEN
                        S.colNonzeroCounts[col] := S.colNonzeroCounts[col] - 1
                        S.rowColIndices[k] := -1    // mark as inactive
                    END IF
                END FOR

                // Track computational work
                IF S.workCounter != NULL THEN
                    S.workCounter := S.workCounter + count * WORK_SCALE_COLUMN
                END IF

                // Assign slack to basis at upper bound
                S.rowStatus[i] := BASIC_UPPER
                basicCount := basicCount + 1

            END IF
            // Else: candidate row does not meet removal criteria; skip

        END IF
        // Else: row has other status (already processed); skip

    END FOR

    // Account for per-row overhead in work counter
    IF S.workCounter != NULL THEN
        S.workCounter := S.workCounter + m * WORK_SCALE_ROW
    END IF

    S.numBasic := S.numBasic + basicCount
    RETURN SUCCESS
```

### Key Design Choices

- **Single-pass row scan**: The crash procedure uses a single linear scan rather than the multi-pass column-scoring approach described by Gould and Reid (1989) or Bixby (1992). This is appropriate when the pre-assignment step has already identified candidate rows using structural analysis. The single-pass design ensures O(m + nnz_removed) worst-case time.

- **Feasibility-first strategy**: Unassigned rows are checked for feasibility before being accepted into the basis. This provides early detection of problems that would otherwise require a full Phase I of the simplex method to identify. For equality constraints, feasibility requires the RHS to be near zero (within tolerance). For inequality constraints, feasibility requires the RHS to be non-negative (within tolerance), consistent with the convention that slack variables have non-negative lower bounds.

- **Candidate removal with column count maintenance**: When a candidate row is removed, all its column entries are explicitly invalidated and the column nonzero counts are decremented. This maintains the invariant that column counts reflect the number of active entries, which is critical for subsequent pricing and factorization steps. The invalidation uses a sentinel value (-1) to distinguish removed entries from valid column indices.

- **Slack-based basis**: Unlike more aggressive crash procedures that attempt to insert structural (non-slack) variables into the basis, this variant constructs a basis entirely from slack variables. The quality improvement over a trivial all-slack basis comes from two sources: (a) infeasible rows are detected and reported immediately, and (b) removal-candidate rows are handled differently (basic at upper bound vs. lower bound), which can place the initial solution closer to the optimal vertex.

- **Separate pre-classification**: The determination of which rows are candidates for removal (positive status values) is not part of the crash procedure itself. This separation of concerns allows different initialization strategies (e.g., sparsity-based scoring, bound analysis, or user-supplied warm-start information) to be used without modifying the crash algorithm.

## Numerical Considerations

### Tolerances

Two tolerance parameters govern the crash procedure:

1. **Primal feasibility tolerance** (epsilon_feas, typically 1e-6): Used to test whether unassigned rows are feasible. For equality constraints, the test is |b_i| < epsilon_feas. For inequalities, the test is b_i >= -epsilon_feas. This tolerance must be consistent with the feasibility tolerance used in subsequent simplex iterations. Using a tighter tolerance here than in the simplex iterations would cause false infeasibility reports; using a looser tolerance could allow infeasible rows to enter the basis.

2. **Tiny RHS threshold** (epsilon_tiny, a very small positive value, typically much smaller than epsilon_feas): Used as a minimum RHS magnitude for candidate row removal. Rows with RHS values very close to zero are not removed even if they are candidates, because their near-zero RHS suggests they may be active at the optimum.

### Stability Concerns

- The crash procedure does not perform any floating-point arithmetic beyond comparisons, so it does not introduce rounding errors. All arithmetic (absolute value, negation, comparison) operates on the original problem data without modification.
- Column nonzero counts are maintained as exact integers, so no numerical drift occurs in the structural bookkeeping.
- The feasibility tolerance should be consistent across the entire solver to avoid contradictory feasibility assessments between the crash and the simplex iterations.

### Degeneracy

- If many constraints have RHS values near zero, the crash procedure will accept them all as feasible (since |b_i| < epsilon_feas), but the resulting basis may be highly degenerate. Degenerate starting bases can lead to cycling in the simplex method, which must be handled by anti-cycling mechanisms (e.g., perturbation or Bland's rule) in the iteration loop.
- The crash procedure does not attempt to avoid degeneracy in the initial basis; degeneracy management is deferred to the simplex iteration phase.

## Termination

The crash procedure always terminates:

- The outer loop iterates exactly m times (once per constraint).
- The inner loop for candidate removal iterates at most once per row entry, bounded by the row's nonzero count.
- The total work across all inner loop iterations is bounded by the total number of nonzeros in candidate rows, which is at most the total number of nonzeros in the constraint matrix.

**Early termination**: If an infeasible row is detected, the procedure terminates immediately, returning an infeasibility indicator. The basis count is updated to reflect partial progress, and the problem row index identifies the offending constraint.

**No iteration limit**: The procedure is not iterative in the sense that the simplex method is; it runs once to completion (or early exit). No convergence criterion is needed.

## Complexity

### Time Complexity

- **Best case**: O(m), when no rows are candidates for removal (all rows are unassigned and feasible). The outer loop performs m comparisons and m status assignments.
- **Worst case**: O(m + nnz), where nnz is the total number of nonzeros in the constraint matrix. This occurs when every row is a candidate for removal, requiring all entries to be marked as inactive.
- **Typical case**: O(m + nnz_candidates), where nnz_candidates is the total number of nonzeros in the candidate rows. In practice, only a fraction of rows are candidates, so the inner loop work is a fraction of the total nonzero count.

### Space Complexity

- O(1) auxiliary space: the procedure uses only a constant number of local variables. All modifications are made in-place on the SolverState arrays.
- The SolverState arrays (row status, column indices, column counts, RHS values, sense array) must already be allocated; the crash procedure does not allocate any memory.

## Edge Cases

### Empty Problem (m = 0)

If the problem has zero constraints, the outer loop does not execute. The procedure returns success with zero basic variables added. This is correct: a problem with no constraints has a trivial feasible basis (the empty basis).

### All Equality Constraints

If every constraint is an equality, no candidate rows can be removed (since removal is restricted to non-equality constraints). The crash reduces to a pure feasibility check: each row is tested for |b_i| < epsilon_feas, and the slack variable enters at the lower bound.

### All Candidate Rows

If every row has positive status (all are candidates), the feasibility-check branch is never executed. Each row is tested for removal eligibility (non-equality, RHS >= epsilon_tiny), and eligible rows are removed. Ineligible candidate rows retain their positive status and are not assigned to the basis by this procedure.

### RHS Exactly at Tolerance Boundary

When |b_i| is exactly equal to epsilon_feas (within floating-point representation), the behavior depends on the comparison convention:
- For equalities: |b_i| >= epsilon_feas triggers infeasibility. A row with |b_i| = epsilon_feas is declared infeasible.
- For inequalities: b_i < -epsilon_feas triggers infeasibility. A row with b_i = -epsilon_feas is declared feasible.

This asymmetry is a consequence of the strict-less-than comparison for inequalities and the greater-than-or-equal comparison for equalities. In practice, exact equality is vanishingly rare due to floating-point representation, but the convention is well-defined.

### Mixed Pre-Assignment

If some rows are unassigned (status = 0) and others are candidates (status > 0), the procedure handles both in a single pass. The two cases are independent: unassigned rows are always checked for feasibility and assigned to the basis at the lower bound; candidate rows are checked for removal eligibility independently.

### Work Counter Disabled

If the work counter pointer is null, all work-tracking logic is skipped. The algorithm behavior is identical in every other respect.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All algorithms cite published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```

## References

- Bixby, R.E. (1992). "Implementing the simplex method: The initial basis." *ORSA Journal on Computing*, 4(3):267-284.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Gould, N.I.M. and Reid, J.K. (1989). "New crash procedures for large systems of linear constraints." *Mathematical Programming*, 45:475-501.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Maros, I. and Mitra, G. (1996). "Strategies for creating advanced bases for large-scale linear programs." *Annals of Operations Research*, 62:101-118.
