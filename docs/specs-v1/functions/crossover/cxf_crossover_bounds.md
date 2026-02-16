# cxf_crossover_bounds

**Module:** Crossover
**Spec Version:** 1.0

## 1. Purpose

Performs bound-snapping during barrier-to-simplex crossover by identifying variables whose values lie close to their bounds and moving them to exact bound values. This function creates a partial basic feasible solution by classifying variables as nonbasic-at-lower, nonbasic-at-upper, superbasic, or basic, then executing the bound movements. The goal is to reduce the number of superbasic variables from the interior-point solution, creating a better starting point for simplex cleanup iterations while maintaining feasibility and numerical stability.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | void* (CxfEnv*) | Environment pointer containing parameters and constants | Valid non-NULL pointer | Yes |
| state | void* (SolverState*) | Solver state with solution, bounds, basis information | Valid non-NULL pointer | Yes |
| snap_tolerance | double | Distance threshold for snapping variable to bound | 0 < tol <= 0.1, typically 1e-6 to 1e-8 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Status code: 0=success, 1001=out of memory, 1006=invalid index, others=specific errors |

### 2.3 Side Effects

- Modifies primal variable values to exact bound values
- Updates variable basis status array (nonbasic-lower, nonbasic-upper, superbasic, basic)
- Triggers basis representation updates through variable change operations
- Modifies constraint right-hand sides to maintain feasibility
- Updates work counter/timing accumulator
- May trigger basis refactorization
- Stores crossover statistics (counts of snapped variables, superbasic variables)

## 3. Contract

### 3.1 Preconditions

- [ ] Environment pointer must be valid
- [ ] State pointer must be valid with allocated arrays
- [ ] Number of variables (numVars) must be positive and reasonable (<= 1,000,000)
- [ ] Snap tolerance must be positive and not too large (<= 0.1)
- [ ] Primal values array contains current interior-point solution
- [ ] Lower bounds and upper bounds arrays are populated
- [ ] Variable status array is allocated
- [ ] Basis header is initialized
- [ ] Matrix structure (CSC format) is available for constraint updates
- [ ] Variable change function (cxf_pivot_bound) is available

### 3.2 Postconditions

- [ ] Variables within snap_tolerance of lower bound are set to exact lower bound value
- [ ] Variables within snap_tolerance of upper bound are set to exact upper bound value
- [ ] Fixed variables (ub - lb < tolerance) are marked nonbasic-at-lower
- [ ] Free variables (infinite bounds) are marked basic
- [ ] Interior variables (far from both bounds) are marked superbasic
- [ ] Basis status array is consistent with variable positions
- [ ] Work counter reflects number of bound movements performed
- [ ] Crossover statistics are stored in state structure
- [ ] Return value indicates success or specific error condition
- [ ] Constraint feasibility is maintained through RHS adjustments

### 3.3 Invariants

- [ ] Problem dimensions (numVars, numConstrs) remain unchanged
- [ ] Bound arrays are not modified
- [ ] Variable type array remains unchanged
- [ ] Matrix structure (column starts, row indices, coefficients) remains unchanged
- [ ] Variables far from bounds retain their interior values
- [ ] Total number of basic + superbasic variables equals number of constraints (eventually)

## 4. Algorithm

### 4.1 Overview

The bound snapping algorithm operates in two distinct passes over the variable set. This two-pass design ensures consistent state updates and avoids cascading errors from partial modifications.

The first pass performs classification: each variable is examined to determine its relationship to its bounds. Fixed variables (where upper and lower bounds are nearly equal) are immediately marked nonbasic-at-lower. Free variables (with infinite bounds) are marked basic since they cannot be nonbasic. For bounded variables, the algorithm computes the distance to each bound. If the current primal value is within the snap tolerance of a bound, the variable is classified as nonbasic at that bound. If the variable is far from both bounds, it is classified as superbasic, meaning it will need to be either pushed to a bound or added to the basis in subsequent processing.

The second pass performs the actual movements. For each variable classified as nonbasic-at-lower or nonbasic-at-upper in the first pass, the algorithm checks if the current value differs significantly from the target bound. If so, it invokes a variable change function that updates the primal value, adjusts the basis representation, and maintains constraint feasibility by updating right-hand sides according to the change in variable value times the variable's constraint coefficients.

Throughout both passes, counters track the number of variables snapped to lower bounds, upper bounds, and remaining superbasic. These statistics inform subsequent optimization phases about the quality of the warm start.

### 4.2 Detailed Steps

**Validation Phase:**

1. **Validate input dimensions**: Check that numVars > 0 and numVars <= 1,000,000. If invalid, return error code 1006 (INVALID_INDEX).

2. **Validate snap tolerance**: Check that snap_tolerance > 0 and snap_tolerance <= 0.1. If invalid or out of range, reset to default value 1e-6.

3. **Initialize counters**: Set num_snapped_lower = 0, num_snapped_upper = 0, num_superbasic = 0, num_basic = 0.

**Classification Pass (Pass 1):**

4. **Begin variable iteration**: FOR each variable i from 0 to numVars-1:

5. **Retrieve variable data**:
   - x = primalValues[i] (current solution value)
   - lb = lowerBounds[i] (lower bound)
   - ub = upperBounds[i] (upper bound)

6. **Handle fixed variables**: If |ub - lb| < TINY_TOL (approximately 1e-10):
   - Set varStatus[i] = NONBASIC_LOWER
   - Increment num_snapped_lower
   - CONTINUE to next variable

7. **Compute bound distances**:
   - dist_to_lb = x - lb
   - dist_to_ub = ub - x

8. **Identify bound infinity status**:
   - lb_is_infinite = (lb <= -1e20)
   - ub_is_infinite = (ub >= +1e20)

9. **Handle free variables**: If both lb_is_infinite AND ub_is_infinite:
   - Set varStatus[i] = BASIC
   - Increment num_basic
   - CONTINUE to next variable

10. **Snap to lower bound if close**: If NOT lb_is_infinite AND dist_to_lb < snap_tolerance:
    - Set varStatus[i] = NONBASIC_LOWER
    - Increment num_snapped_lower
    - CONTINUE to next variable

11. **Snap to upper bound if close**: ELSE IF NOT ub_is_infinite AND dist_to_ub < snap_tolerance:
    - Set varStatus[i] = NONBASIC_UPPER
    - Increment num_snapped_upper
    - CONTINUE to next variable

12. **Mark interior variables as superbasic**: ELSE (variable is far from all bounds):
    - Set varStatus[i] = SUPERBASIC
    - Increment num_superbasic
    - CONTINUE to next variable

13. **End classification pass**: After processing all variables, proceed to movement pass.

**Movement Pass (Pass 2):**

14. **Begin variable iteration**: FOR each variable i from 0 to numVars-1:

15. **Retrieve variable status and data**:
    - status = varStatus[i]
    - x = primalValues[i]
    - lb = lowerBounds[i]
    - ub = upperBounds[i]

16. **Process nonbasic-lower variables**: If status = NONBASIC_LOWER:
    - If |x - lb| > TINY_TOL (value needs adjustment):
      - errorCode = cxf_pivot_bound(env, state, i, lb, snap_tolerance, fix_mode=0)
      - If errorCode != 0: RETURN errorCode (abort on error)

17. **Process nonbasic-upper variables**: ELSE IF status = NONBASIC_UPPER:
    - If |x - ub| > TINY_TOL (value needs adjustment):
      - errorCode = cxf_pivot_bound(env, state, i, ub, snap_tolerance, fix_mode=0)
      - If errorCode != 0: RETURN errorCode (abort on error)

18. **Skip other statuses**: Variables marked SUPERBASIC or BASIC are not moved in this function.

19. **End movement pass**: After processing all variables, proceed to finalization.

**Finalization Phase:**

20. **Update work counter**: If workCounter pointer is not NULL:
    - Add (num_snapped_lower + num_snapped_upper) * 0.001 to counter (bound snapping cost)
    - Add num_superbasic * 0.0001 to counter (classification cost)

21. **Store crossover statistics in state**:
    - Store num_snapped_lower
    - Store num_snapped_upper
    - Store num_superbasic
    - Store num_basic

22. **Return success**: Return 0.

### 4.3 Pseudocode

```
CROSSOVER_BOUNDS(env, state, snap_tolerance):
    // Validation
    numVars <- state.numVars
    IF numVars <= 0 OR numVars > 1000000:
        RETURN INVALID_INDEX_ERROR

    IF snap_tolerance <= 0 OR snap_tolerance > 0.1:
        snap_tolerance <- 1e-6  // Default

    // Initialize counters
    num_snapped_lower <- 0
    num_snapped_upper <- 0
    num_superbasic <- 0
    num_basic <- 0

    // Pass 1: Classification
    FOR i <- 0 TO numVars-1:
        x <- primalValues[i]
        lb <- lowerBounds[i]
        ub <- upperBounds[i]

        // Fixed variable
        IF |ub - lb| < TINY_TOL:
            varStatus[i] <- NONBASIC_LOWER
            num_snapped_lower <- num_snapped_lower + 1
            CONTINUE

        // Compute distances
        dist_lb <- x - lb
        dist_ub <- ub - x

        // Check infinity
        lb_infinite <- (lb <= -1e20)
        ub_infinite <- (ub >= 1e20)

        // Free variable must be basic
        IF lb_infinite AND ub_infinite:
            varStatus[i] <- BASIC
            num_basic <- num_basic + 1
            CONTINUE

        // Snap to lower bound
        IF NOT lb_infinite AND dist_lb < snap_tolerance:
            varStatus[i] <- NONBASIC_LOWER
            num_snapped_lower <- num_snapped_lower + 1
            CONTINUE

        // Snap to upper bound
        IF NOT ub_infinite AND dist_ub < snap_tolerance:
            varStatus[i] <- NONBASIC_UPPER
            num_snapped_upper <- num_snapped_upper + 1
            CONTINUE

        // Interior point - superbasic
        varStatus[i] <- SUPERBASIC
        num_superbasic <- num_superbasic + 1

    // Pass 2: Movement
    FOR i <- 0 TO numVars-1:
        status <- varStatus[i]
        x <- primalValues[i]
        lb <- lowerBounds[i]
        ub <- upperBounds[i]

        IF status = NONBASIC_LOWER:
            IF |x - lb| > TINY_TOL:
                errorCode <- PIVOT_BOUND(env, state, i, lb, snap_tolerance, 0)
                IF errorCode != 0:
                    RETURN errorCode

        ELSE IF status = NONBASIC_UPPER:
            IF |x - ub| > TINY_TOL:
                errorCode <- PIVOT_BOUND(env, state, i, ub, snap_tolerance, 0)
                IF errorCode != 0:
                    RETURN errorCode

    // Finalization
    IF workCounter IS NOT NULL:
        workCounter <- workCounter + (num_snapped_lower + num_snapped_upper) * 0.001
        workCounter <- workCounter + num_superbasic * 0.0001

    STORE_STATISTICS(num_snapped_lower, num_snapped_upper, num_superbasic, num_basic)

    RETURN SUCCESS
```

### 4.4 Mathematical Foundation

**Distance-Based Classification**

For each variable x_i with bounds [lb_i, ub_i], define the distance to bounds:
```
d_lower(i) = x_i - lb_i
d_upper(i) = ub_i - x_i
```

The snapping rule with tolerance tau is:
```
           { NONBASIC_LOWER    if d_lower(i) < tau
status_i = { NONBASIC_UPPER    if d_upper(i) < tau
           { SUPERBASIC        if d_lower(i) >= tau AND d_upper(i) >= tau
```

**Complementary Slackness Preservation**

In the optimal interior-point solution, complementary slackness conditions are approximately satisfied:
```
(x_i - lb_i) * pi_i^L ~ 0
(ub_i - x_i) * pi_i^U ~ 0
```

where pi_i^L and pi_i^U are dual multipliers for lower and upper bound constraints.

Variables near bounds (small d_lower or d_upper) correspond to large dual multipliers, indicating the bound is active at optimality. Snapping these variables to exact bound values converts approximate complementarity to exact complementarity, creating a vertex solution.

**Basis Structure Requirements**

A basic feasible solution for a problem with m constraints and n variables requires:
- Exactly m basic variables (in the basis)
- n - m nonbasic variables (at bounds)
- Zero superbasic variables (for simplex methods)

After bound snapping:
```
n = num_nonbasic_lower + num_nonbasic_upper + num_superbasic + num_basic
m ~ num_superbasic + num_basic (after basis construction)
```

The crossover process reduces num_superbasic by pushing variables to bounds, approaching the target of m basic + (n-m) nonbasic variables.

**Feasibility Maintenance**

When variable x_i changes from value x_old to x_new, constraint feasibility requires RHS update:
```
For each constraint j with coefficient a_ji:
    rhs'_j = rhs_j - a_ji * (x_new - x_old)
```

This ensures Ax = b remains satisfied after the variable movement.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n)
  - All variables are far from bounds (no snapping needed)
  - Two O(n) passes: classification and (trivial) movement
  - No basis updates or matrix operations

- **Average case:** O(n + k*m)
  - k variables are snapped (typically k << n)
  - Classification pass: O(n)
  - Movement pass: O(n) iteration + O(k*m) for k bound pivots
  - Each pivot updates O(m) constraints

- **Worst case:** O(n*m^2)
  - All n variables need snapping
  - Each snap triggers basis update: O(m) work
  - Some snaps trigger refactorization: O(m^2) work
  - If refactorization is frequent: O(n*m^2)

Where:
- n = Number of variables (numVars)
- m = Number of constraints (numConstrs)
- k = Number of variables actually snapped (typically small fraction of n)

### 5.2 Space Complexity

- **Auxiliary space:** O(1)
  - Fixed number of counters (4 integers)
  - Local variables for loop iteration
  - No dynamic allocation

- **Total space:** O(n + m + nnz)
  - Variable arrays: O(n) for primal values, bounds, status
  - Constraint arrays: O(m) for RHS, dual values, basis
  - Matrix storage: O(nnz) for CSC format (already allocated)

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Invalid numVars | 1006 (INVALID_INDEX) | Number of variables is <= 0 or > 1,000,000 |
| Memory allocation failure in pivot | 1001 (OUT_OF_MEMORY) | cxf_pivot_bound could not allocate work arrays |
| Basis refactorization failure | Various | Singular matrix or numerical instability in LU factorization |
| Invalid variable index | 1006 (INVALID_INDEX) | Variable index passed to pivot function out of range |

### 6.2 Error Behavior

The function performs input validation before any state modification. If numVars is invalid, the function returns immediately without touching any arrays.

If snap_tolerance is invalid, the function corrects it to a default value (1e-6) and continues. This is a defensive measure rather than an error.

During the classification pass (Pass 1), no errors can occur because it only reads data and writes to the status array.

During the movement pass (Pass 2), errors can occur from the cxf_pivot_bound calls. If an error occurs:
- The function immediately returns the error code
- Variables processed before the error have been moved to bounds
- Variables not yet processed remain at their interior-point values
- The state is partially converted, but remains valid (feasible if pivot maintained feasibility)

The calling code (typically cxf_crossover main function) must handle partial conversion and decide whether to:
- Abort optimization and report the error
- Continue with simplex from the partially-converted state
- Retry crossover with different tolerance

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No variables | numVars = 0 | Returns INVALID_INDEX immediately |
| All variables fixed | forall i: ub[i] = lb[i] | All marked NONBASIC_LOWER, no movements needed |
| All variables free | forall i: lb[i] = -inf, ub[i] = +inf | All marked BASIC, no movements needed |
| All at exact bounds | forall i: x[i] = lb[i] or x[i] = ub[i] | Classification correct, movement pass is no-op (|x - bound| < TINY_TOL) |
| All interior | forall i: dist_lb > tol AND dist_ub > tol | All marked SUPERBASIC, no movements, left for subsequent processing |
| Very tight tolerance | snap_tolerance = 1e-12 | Fewer variables snapped, more superbasic variables remain |
| Very loose tolerance | snap_tolerance = 0.01 | More variables snapped, fewer superbasic, risk of degrading objective |
| Mixed infinite bounds | Some lb = -inf, some ub = +inf | Handled per-variable: free variables -> BASIC, semi-infinite -> check feasible bound |
| Exactly at bound | x[i] = lb[i], dist = 0 | Marked NONBASIC_LOWER, movement sees |x - lb| = 0 < TINY_TOL, skips pivot |
| Just outside tolerance | x = lb + 1.01*tol | Marked SUPERBASIC, not snapped |
| Tolerance validation | snap_tolerance = -1 or 10 | Reset to default 1e-6, function proceeds normally |

## 8. Thread Safety

**Thread-safe:** No

The function modifies shared state structures:
- Primal values array (during pivot operations)
- Variable status array (classification results)
- Constraint RHS array (feasibility maintenance)
- Basis header and representation (through pivot function)
- Work counter (accumulator update)
- Statistics storage in state (crossover counters)

**Synchronization required:** The calling code must ensure exclusive access to the solver state during bound snapping. In typical LP solver architecture, crossover occurs in a single-threaded phase after barrier convergence, so no explicit locking is required. If this function were called from multiple threads (e.g., parallel crossover experiments), the entire solver state would require mutex protection.

The function makes no assumptions about thread-local storage and does not use any global variables, so it could be made thread-safe by protecting the state parameter with a lock.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_pivot_bound | Simplex | Moves variable to specified bound value, updates basis and RHS to maintain feasibility |
| fabs | Math | Computes absolute value for distance and tolerance comparisons |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_crossover | Crossover | Main crossover function may call this as a first step before quadratic optimization |
| cxf_barrier_cleanup | Barrier | Called during barrier-to-simplex transition to prepare basis |
| cxf_crossover_driver | Crossover | Orchestrates multiple crossover strategies, may include bound snapping |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_crossover | Main crossover function that handles quadratic objectives and calls this for bound handling |
| cxf_basis_crash | Alternative basis construction method using crash heuristics rather than crossover |
| cxf_simplex_push_to_bounds | Simplex-phase function that may perform similar bound pushing during presolve |
| cxf_presolve_tighten_bounds | Presolve function that fixes variables at bounds but in problem reduction context |
| cxf_pivot_bound | Low-level function that executes individual bound movements |

## 11. Design Notes

### 11.1 Design Rationale

**Two-pass design:** The separation of classification (Pass 1) and movement (Pass 2) ensures that all variables are evaluated with the original solution values. If classification and movement were interleaved, early movements could affect basis status computation for later variables, leading to inconsistent or suboptimal decisions.

**Status enumeration:** Using explicit status codes (NONBASIC_LOWER, NONBASIC_UPPER, SUPERBASIC, BASIC) rather than boolean flags makes the algorithm clearer and allows for future extension (e.g., adding FIXED status).

**Tolerance handling:** The function accepts snap_tolerance as a parameter rather than using a fixed constant, allowing the calling code to control aggressiveness. Tighter tolerance preserves objective value better but leaves more superbasic variables. Looser tolerance creates a sparser basis but may degrade objective.

**Error recovery:** The function returns errors immediately rather than collecting them and reporting at the end. This fail-fast approach prevents cascading errors and makes debugging easier.

**Fixed variable handling:** Fixed variables (ub ~ lb) are immediately classified as NONBASIC_LOWER without checking the current primal value. This is safe because fixed variables must be at their common bound value for feasibility, and the nonbasic classification is correct regardless.

### 11.2 Performance Considerations

**Pass overhead:** The two-pass design requires iterating through all variables twice. For very large problems (millions of variables), this overhead is acceptable because:
- Both passes are O(n) with very small constants
- The memory access patterns are sequential (good cache locality)
- The alternative (single pass with rechecks) would be more complex and error-prone

**Pivot batching:** Currently, each variable snap triggers an individual cxf_pivot_bound call. For problems where many variables snap, batching these updates could amortize basis refactorization cost. However, this would require more complex state management.

**Early termination:** The function does not support early termination if a certain number of variables have been snapped. This could be added for iterative crossover strategies where partial conversion is sufficient.

**Work counter granularity:** The work counter update uses weights (0.001 for snapping, 0.0001 for classification) to reflect that bound movements are more expensive than status classification. These weights should be calibrated based on actual profiling data.

### 11.3 Future Considerations

**Potential enhancements:**
- Add parameter to control maximum number of variables to snap (partial crossover)
- Support asymmetric tolerances (different tau for lower vs upper bounds)
- Use dual multipliers from barrier solution to guide snapping priority
- Implement variable reordering to snap high-impact variables first
- Add statistics on objective degradation from snapping
- Support incremental crossover for reoptimization scenarios

**Known limitations:**
- No special handling for variables with SOS constraints (caller must manage)
- Does not consider constraint activity when snapping (might create unnecessary basic variables)
- Snap tolerance is uniform across all variables (could use per-variable scaling)
- No rollback mechanism if snapping degrades objective too much

**Integration opportunities:**
- Could be called iteratively with decreasing tolerance for gradual crossover
- Could use barrier trajectory information to predict which variables to snap
- Could be integrated with presolve to permanently fix variables at bounds

## 12. References

- Andersen, E.D., & Andersen, K.D. (1995). "Presolving in Linear Programming." *Mathematical Programming*, 71(2), 221-245.
  - Section 4.3 discusses variable fixing near bounds

- Bixby, R.E., & Martin, A. (2000). "Parallelizing the Dual Simplex Method." *INFORMS Journal on Computing*, 12(1), 45-56.
  - Discusses basis construction from interior-point solutions

- Fourer, R., & Mehrotra, S. (1993). "Solving Symmetric Indefinite Systems in an Interior-Point Method for Linear Programming." *Mathematical Programming*, 62(1), 15-39.
  - Crossover strategies for converting IPM to simplex solutions

- Mehrotra, S., & Ye, Y. (1993). "Finding an Interior Point in the Optimal Face of Linear Programs." *Mathematical Programming*, 62(1), 497-515.
  - Theory of identifying optimal face from interior-point solutions

- Vanderbei, R.J. (2014). *Linear Programming: Foundations and Extensions*, 4th edition. Springer.
  - Chapter 19: Interior-Point Methods
  - Section 19.5: Crossover to vertex solution

## 13. Validation Checklist

Before finalizing this spec, verify:

- [X] No code copied from implementation
- [X] Algorithm description is implementation-agnostic
- [X] All parameters documented
- [X] All error conditions listed
- [X] Complexity analysis complete
- [X] Edge cases identified
- [X] A competent developer could implement from this spec alone

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
