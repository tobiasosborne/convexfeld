# cxf_solve_lp

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Main entry point for solving linear programs using the simplex method. Orchestrates the complete solve sequence including initialization, crash basis construction, preprocessing, setup, iteration loop, and cleanup. Handles both Phase I (finding feasibility) and Phase II (optimizing objective) as needed. Returns the optimal solution or appropriate status code.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model containing LP problem | Non-null, valid model | Yes |
| warmStart | void* | Optional warm start basis | NULL or valid basis | No |
| mode | int | Solve mode (0=auto, 1=primal, 2=dual) | 0, 1, or 2 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Status: 0=optimal, 2=infeasible, 3=unbounded, 10=iteration limit |

### 2.3 Side Effects

- Allocates and frees solver state
- Modifies model solution arrays
- Updates model attributes (ObjVal, Status, etc.)
- Updates timing statistics

## 3. Contract

### 3.1 Preconditions

- [ ] Model has valid problem data
- [ ] Bounds, objective, constraints are set
- [ ] Matrix is in CSC format

### 3.2 Postconditions

- [ ] If optimal: solution arrays contain optimal primal/dual
- [ ] If infeasible: ray of infeasibility available
- [ ] If unbounded: ray of unboundedness available
- [ ] Model status attribute set appropriately
- [ ] All temporary memory freed

### 3.3 Invariants

- [ ] Model problem data unchanged
- [ ] Environment unchanged

## 4. Algorithm

### 4.1 Overview

The function follows the standard simplex algorithm framework:
1. Initialize solver state from model
2. Construct initial basis (crash or warm start)
3. Preprocess (bound tightening, scaling)
4. Setup iteration structures
5. Main iteration loop (Phase I then Phase II)
6. Post-process and extract solution
7. Cleanup and return

### 4.2 Detailed Steps

1. **Initialize solver state**:
   - Call cxf_simplex_init to create SolverState
   - Copy problem data to working arrays
   - Set up tolerances from environment

2. **Construct initial basis**:
   - If warm start provided: call cxf_basis_warm
   - Else: call cxf_simplex_crash for crash basis

3. **Preprocess**:
   - Call cxf_simplex_preprocess
   - Tighten bounds, detect fixed variables
   - Compute scaling factors
   - If infeasible detected: goto cleanup with status 2

4. **Setup**:
   - Call cxf_simplex_setup
   - Allocate reduced costs, dual values
   - Initialize pricing structures
   - Determine phase (1 if infeasible start, else 2)

5. **Apply perturbation** (early iterations):
   - Call cxf_simplex_perturbation to prevent cycling

6. **Main iteration loop**:
   ```
   while not done:
       status = cxf_simplex_iterate(state, env)
       if status == OPTIMAL or status == INFEASIBLE:
           done = true
       if iteration > limit:
           status = ITERATION_LIMIT
           done = true
   ```

7. **Phase transition** (if needed):
   - If Phase I optimal with zero infeasibility:
     - Switch to Phase II
     - Reset reduced costs to original objective
     - Continue iteration loop

8. **Remove perturbation**:
   - Call cxf_simplex_unperturb
   - Restore original bounds

9. **Refine solution**:
   - Call cxf_simplex_refine
   - Clean up near-zero values

10. **Extract solution**:
    - Call cxf_extract_solution
    - Copy to model arrays

11. **Cleanup**:
    - Call cxf_simplex_final
    - Free all temporary memory

12. **Return status**

### 4.3 Pseudocode

```
SOLVE_LP(model, warmStart, mode):
    // Initialize
    state := cxf_simplex_init(model, warmStart, mode)
    IF state == NULL:
        RETURN OUT_OF_MEMORY

    // Crash or warm start
    IF warmStart != NULL:
        err := cxf_basis_warm(state, warmStart, model.env)
    ELSE:
        err := cxf_simplex_crash(state, model.env)
    IF err != 0:
        CLEANUP(state)
        RETURN err

    // Preprocess
    err := cxf_simplex_preprocess(state, model.env, 0)
    IF err == INFEASIBLE:
        CLEANUP(state)
        RETURN INFEASIBLE
    IF err != 0:
        CLEANUP(state)
        RETURN err

    // Setup
    err := cxf_simplex_setup(state, model.env)
    IF err != 0:
        CLEANUP(state)
        RETURN err

    // Perturbation (early iterations)
    IF state.iteration < 2:
        cxf_simplex_perturbation(state, model.env)

    // Main loop
    iterLimit := GET_PARAM(model.env, "IterationLimit")
    WHILE state.status == IN_PROGRESS:
        err := cxf_simplex_iterate(state, model.env)

        IF err != 0:
            state.status := err
            BREAK

        state.iteration++
        IF state.iteration > iterLimit:
            state.status := ITERATION_LIMIT
            BREAK

    // Phase transition
    IF state.phase == 1 AND state.status == OPTIMAL:
        IF state.objValue < state.tolerance:
            // Phase I found feasible
            state.phase := 2
            RESET_OBJECTIVE(state)
            state.status := IN_PROGRESS
            GOTO main_loop

    // Cleanup perturbation
    cxf_simplex_unperturb(state, model.env)

    // Refine
    cxf_simplex_refine(state, model.env)

    // Extract
    cxf_extract_solution(state, model)

    // Final cleanup
    cxf_simplex_final(state)

    RETURN state.status
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(m^2) for easy problems
- **Average case:** O(m^2 * n) for typical LP
- **Worst case:** O(2^n) theoretically, but rare in practice

Where m = constraints, n = variables.

### 5.2 Space Complexity

- O(n + m) for working arrays
- O(m^2) worst case for basis factors

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Out of memory | 1001 | Cannot allocate solver state |
| Infeasible | 2 | No feasible solution exists |
| Unbounded | 3 | Objective unbounded |
| Iteration limit | 10 | Exceeded max iterations |
| Numerical error | 12 | Pivot element too small |

### 6.2 Error Behavior

On any error:
- Cleanup all allocated memory
- Set model status attribute
- Return error code

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty problem | n=0, m=0 | Return optimal immediately |
| Trivial bounds | All fixed | Verify feasibility, return |
| Dense problem | High fill-in | Slower but correct |
| Highly degenerate | Many zero pivots | Perturbation prevents cycling |

## 8. Thread Safety

**Thread-safe:** Yes (uses separate state per call)

**Note:** Model should not be modified during solve

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_simplex_init | Simplex | Initialize state |
| cxf_simplex_crash | Simplex | Initial basis |
| cxf_simplex_preprocess | Simplex | Preprocessing |
| cxf_simplex_setup | Simplex | Setup iteration |
| cxf_simplex_iterate | Simplex | Main iteration |
| cxf_simplex_final | Simplex | Cleanup |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_optimize | API | Main optimization entry |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_barrier_solve | Alternative: interior point |
| cxf_mip_solve | Uses this for LP relaxations |

## 11. Design Notes

### 11.1 Design Rationale

**Two-phase method:** Phase I finds feasibility, Phase II optimizes. Clean separation of concerns.

**Early perturbation:** Applying perturbation proactively is more reliable than detecting cycles.

**Modular structure:** Each step is a separate function for testability and maintainability.

## 12. References

- Dantzig, G.B. (1963). "Linear Programming and Extensions"
- Chvatal, V. (1983). "Linear Programming"
- Maros, I. (2003). "Computational Techniques of the Simplex Method"

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Convexfeld LP Solver Specification*
*Based on published optimization literature*
