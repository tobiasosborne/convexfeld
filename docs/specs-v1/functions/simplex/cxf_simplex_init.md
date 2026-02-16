# cxf_simplex_init

**Module:** Simplex Core
**Spec Version:** 1.0

## 1. Purpose

Allocates and initializes the SolverState structure that holds all runtime state for the simplex solver. This function creates the working environment for simplex iterations by copying problem data from the model into working arrays, processing variable types, and setting up quadratic program extensions if needed. It is the first function called in the LP solve sequence and establishes all data structures that subsequent functions will use.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Pointer to model containing problem definition | Non-null, valid model | Yes |
| warmStart | void* | Warm start basis information (can be NULL for cold start) | NULL or valid basis | No |
| mode | int | Solve mode selection (primal/dual/barrier) | 0, 1, or 2 | Yes |
| timing | double* | Timing accumulator array for profiling | Non-null array | Yes |
| stateOut | SolverState** | Output pointer to receive allocated state | Non-null | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0=success, 1001=out of memory |
| *stateOut | SolverState* | Pointer to allocated and initialized solver state |

### 2.3 Side Effects

- Allocates SolverState structure (approximately 1192 bytes)
- Allocates multiple per-variable and per-constraint arrays
- Copies problem data from model to working arrays
- Modifies variable types (S->C, N->I for semi-continuous handling)

## 3. Contract

### 3.1 Preconditions

- [ ] Model pointer must be valid with initialized matrix data
- [ ] stateOut pointer must be writable

### 3.2 Postconditions

- [ ] On success: *stateOut points to fully initialized SolverState
- [ ] On success: All working arrays are allocated and initialized
- [ ] On failure: No memory is leaked (all partial allocations freed)
- [ ] On failure: *stateOut is unchanged

### 3.3 Invariants

- [ ] Original model data is not modified (data is copied)
- [ ] Matrix structure pointers remain valid after call

## 4. Algorithm

### 4.1 Overview

The function performs extensive memory allocation and data copying to create a self-contained solver state. It first allocates the main state structure, then iteratively allocates arrays for scaling, bounds, objective, constraints, variable types, and quadratic terms. Each allocation is checked, and on any failure, all previously allocated memory is freed before returning an error.

Variable types receive special processing: semi-continuous (S) and semi-integer (N) variables are converted to continuous (C) and integer (I) respectively, with a flag marking their semi-continuous nature. This allows the solver core to handle semi-continuous variables through bounds logic.

### 4.2 Detailed Steps

1. **Allocate main SolverState structure** (approximately 1192 bytes). Zero-initialize the entire structure.

2. **Store model pointer** for later access to environment and other model data.

3. **Extract problem dimensions** from model:
   - Read numConstrs, numVars, numNonzeros from matrix

4. **Store dimensions** in state.

5. **Allocate scaling arrays**:
   - Row scaling: numConstrs doubles, initialize to 1.0
   - Column scaling: numVars doubles, initialize to 1.0

6. **Allocate bound arrays**:
   - Original lower bounds: numVars doubles, copy from matrix
   - Original upper bounds: numVars doubles, copy from matrix
   - Working lower bounds: numVars doubles, copy from matrix
   - Working upper bounds: numVars doubles, copy from matrix

7. **Allocate objective and RHS arrays**:
   - Objective coefficients: numVars doubles, copy from matrix
   - RHS values: numConstrs doubles, copy from matrix
   - Constraint senses: numConstrs chars, copy from matrix

8. **Allocate variable type and flags**:
   - Variable types: numVars chars, copy from matrix
   - Variable flags: numVars uint32s, initialize to 0

9. **Process semi-continuous variables**:
   - For each variable type 'S': change to 'C', set flag 0x04
   - For each variable type 'N': change to 'I', set flag 0x04
   - Count semi-continuous variables

10. **Check for quadratic terms**:
    - If matrix has Q matrix pointer, set hasQuadratic=1
    - Read numQuadratic from Q matrix structure

11. **Allocate quadratic arrays** (if hasQuadratic):
    - Q row pointers, column starts, column ends

12. **Allocate auxiliary buffers**:
    - Auxiliary buffer for temporary storage
    - Timing state structure

13. **Initialize control fields**:
    - phase = 0
    - solveMode = mode
    - maxIterations = 100000
    - tolerance = 1e-6
    - Various flags initialized to 0
    - objValue = 0.0, scaleFactor = 1.0

14. **Return state** via stateOut pointer.

### 4.3 Pseudocode

```
INIT_STATE(model, mode):
    state := ALLOCATE(1192 bytes)
    ZERO_FILL(state)

    state.model := model
    state.dims := COPY_DIMENSIONS(model.matrix)

    state.rowScale := ALLOCATE_INIT_ONES(numConstrs)
    state.colScale := ALLOCATE_INIT_ONES(numVars)

    state.lb_orig := COPY(matrix.lb, numVars)
    state.ub_orig := COPY(matrix.ub, numVars)
    state.lb_work := COPY(matrix.lb, numVars)
    state.ub_work := COPY(matrix.ub, numVars)

    state.obj := COPY(matrix.obj, numVars)
    state.rhs := COPY(matrix.rhs, numConstrs)
    state.sense := COPY(matrix.sense, numConstrs)

    state.vtype := COPY(matrix.vtype, numVars)
    state.flags := ALLOCATE_ZEROS(numVars)

    FOR each variable i:
        IF vtype[i] = 'S':
            vtype[i] := 'C'
            flags[i] |= 0x04
        ELSE IF vtype[i] = 'N':
            vtype[i] := 'I'
            flags[i] |= 0x04

    IF matrix.Q exists:
        state.hasQuadratic := 1
        ALLOCATE_Q_ARRAYS(state)

    ALLOCATE_BUFFERS(state)
    INIT_CONTROL_FIELDS(state, mode)

    RETURN state
```

## 5. Complexity

### 5.1 Time Complexity

- O(n + m) for allocation and copying

Where n = number of variables, m = number of constraints.

### 5.2 Space Complexity

- **Total space:** O(n + m) for all allocated arrays

Detailed memory breakdown:
- SolverState: ~1192 bytes
- Per-variable: ~56 bytes per variable
- Per-constraint: ~24 bytes per constraint

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| State allocation fails | 1001 | Cannot allocate SolverState |
| Array allocation fails | 1001 | Cannot allocate any working array |

### 6.2 Error Behavior

On any allocation failure:
- All previously allocated arrays are freed in reverse order
- SolverState is freed if allocated
- Returns error code 1001 (OUT_OF_MEMORY)
- No memory is leaked

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| No variables | numVars=0 | Allocates zero-size arrays, succeeds |
| No constraints | numConstrs=0 | Allocates zero-size arrays, succeeds |
| No matrix | matrix=NULL | Uses fallback dimensions, empty arrays |
| All semi-continuous | All 'S' or 'N' types | All converted, high semiCount |
| No quadratic | Q matrix NULL | hasQuadratic=0, no Q arrays allocated |
| Cold start | warmStart=NULL | Ignored, no special handling needed |

## 8. Thread Safety

**Thread-safe:** Yes

The function only allocates new memory and reads from the model. Multiple calls on different models can proceed concurrently.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate memory blocks |
| cxf_free | Memory | Free memory on error cleanup |
| memset | C stdlib | Zero-initialize structures |
| memcpy | C stdlib | Copy array data |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_solve_lp | Simplex | First step in LP solve sequence |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_simplex_final | Deallocates everything this function allocates |
| cxf_simplex_setup | Further initialization after crash and preprocess |

## 11. Design Notes

### 11.1 Design Rationale

**Copy vs reference:** Working arrays are copies of model data rather than references. This allows:
- Preprocessing to modify bounds without affecting model
- Scaling to be applied to working data
- Thread safety (model can be read by other threads)

**Semi-continuous conversion:** Converting S/N to C/I with flags allows the simplex core to use standard bounds logic while still tracking which variables need special handling at solution time.

**Separate original and working bounds:** Original bounds are preserved for solution recovery after preprocessing and perturbation modify working bounds.

### 11.2 Performance Considerations

- Memory is allocated in many small blocks
- Arrays are copied using memcpy for efficiency
- Initialization values set in bulk loops

## 12. References

- Maros, I. (2003). "Computational Techniques of the Simplex Method" - Data structure design

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
