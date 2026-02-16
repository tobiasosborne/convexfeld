# cxf_newmodel

**Module:** API Model Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Creates a new optimization model within a Convexfeld environment. The model initially contains no constraints, but may contain variables specified during creation. Variables can also be added later. This is the primary entry point for building optimization problems programmatically.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| env | CxfEnv* | Parent environment providing configuration | Valid environment pointer | Yes |
| modelP | CxfModel** | Output pointer to receive the created model | Valid pointer | Yes |
| Pname | const char* | Model name for identification and logging | Any string or NULL | No |
| numvars | int | Number of variables to create initially | >= 0 | Yes |
| obj | double* | Objective coefficients for initial variables | Array of length numvars or NULL | No |
| lb | double* | Lower bounds for initial variables | Array of length numvars or NULL | No |
| ub | double* | Upper bounds for initial variables | Array of length numvars or NULL | No |
| vtype | char* | Variable types ('C', 'B', 'I', 'S', 'N') | Array of length numvars or NULL | No |
| varnames | const char** | Names for initial variables | Array of numvars strings or NULL | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | Error code: 0 on success, non-zero on failure |
| *modelP | CxfModel* | Pointer to newly created model (set to NULL on error) |

### 2.3 Side Effects

- Allocates memory for model structure and internal data structures
- Increments environment's model tracking count
- Acquires and releases environment lock for thread safety
- May log debug messages if environment has debug logging enabled
- Sets environment's error state on failure

## 3. Contract

### 3.1 Preconditions

- [ ] Environment pointer must be valid and initialized
- [ ] Environment must be active
- [ ] modelP must not be NULL
- [ ] If numvars > 0, any provided arrays (obj, lb, ub, vtype, varnames) must have at least numvars elements
- [ ] numvars must be non-negative

### 3.2 Postconditions

On success:
- [ ] *modelP contains a valid model pointer
- [ ] Model contains exactly numvars variables
- [ ] Model contains zero constraints
- [ ] Variables have the specified objective coefficients (or 0.0 if NULL)
- [ ] Variables have the specified lower bounds (or 0.0 if NULL)
- [ ] Variables have the specified upper bounds (or +infinity if NULL)
- [ ] Variables have the specified types (or continuous if NULL)
- [ ] Solution status is -1 (unsolved)
- [ ] Objective offset is 0.0

On failure:
- [ ] *modelP is set to NULL
- [ ] No memory leaks (all partial allocations freed)
- [ ] Environment error message is set

### 3.3 Invariants

- [ ] Environment structure is not modified (except for model tracking)
- [ ] Environment lock is not held after return
- [ ] Input arrays are not modified
- [ ] Model is in a consistent state (either fully created or NULL)

## 4. Algorithm

### 4.1 Overview

The function creates a new optimization model through a multi-phase initialization process. First, it validates all inputs and acquires the environment lock for thread safety. Second, it allocates the model wrapper structure and the internal matrix data structure. Third, it initializes all variable-related arrays based on the provided parameters or defaults. Fourth, it performs variable type validation and bound adjustment for special variable types (binary variables are clamped to [0,1], semi-continuous and semi-integer variables have special handling). Finally, it sets up variable names either from the provided array or by generating default names in the format "C0", "C1", etc.

The function uses environment-scoped memory allocation, meaning all allocations are tracked by the environment and can be efficiently bulk-freed when the model is destroyed. The internal matrix data structure uses Compressed Sparse Column (CSC) format for the constraint matrix, initialized to empty since no constraints exist yet.

### 4.2 Detailed Steps

1. Validate environment state by checking if it's initialized and active
2. Acquire environment lock to ensure thread-safe model creation
3. Validate that numvars is non-negative
4. Validate that modelP is not NULL
5. Validate any provided arrays (obj, lb, ub) are accessible if numvars > 0
6. Check environment settings for name suppression (may disable name storage)
7. Initialize output pointer *modelP to NULL
8. Allocate model wrapper structure using environment's allocator
9. Set model initialization flag to indicate valid model
10. Allocate matrix data structure for storing variables and constraints
11. Link matrix data structure to model wrapper
12. Initialize matrix metadata: version=1, numConstrs=0, numVars=numvars, numNonzeros=0, solStatus=-1, objOffset=0.0
13. Copy model name if provided (allocate string, copy bytes)
14. If numvars > 0, allocate arrays for: objective coefficients, CSC column starts, CSC column lengths, lower bounds, upper bounds, variable types
15. Initialize each variable's data:
    - Objective coefficient: use provided value or default to 0.0
    - Lower bound: use provided value or default to 0.0
    - Upper bound: use provided value or default to +infinity (1e100)
    - Variable type: use provided type (uppercased) or default to 'C' (continuous)
    - CSC column start and length: set to 0 (no constraints yet)
16. Add sentinel value to CSC column start array at position numvars
17. Perform variable type validation and bound adjustment:
    - Binary variables: ensure bounds are within [0,1]
    - Semi-continuous/semi-integer: apply special bound handling
18. If variable names are provided, allocate name array and copy each name; if name is NULL, generate default "C{index}"
19. Set output pointer *modelP to the created model
20. Release environment lock
21. Return success code (0)
22. On any error: free all partial allocations, release lock, return error code

### 4.3 Pseudocode

```
function cxf_newmodel(env, modelP, Pname, numvars, obj, lb, ub, vtype, varnames):
    # Validation phase
    if not cxf_checkenv(env):
        return INVALID_ENVIRONMENT

    acquire_lock(env)

    if env.active == 0:
        release_lock(env)
        return ENV_NOT_ACTIVE

    if numvars < 0 or modelP == NULL:
        release_lock(env)
        return INVALID_ARGUMENT

    if not validate_arrays(env, numvars, obj, lb, ub):
        release_lock(env)
        return INVALID_ARGUMENT

    # Check name suppression
    suppress_names ← (env.noNames ≠ 0)
    if suppress_names:
        Pname ← NULL
        varnames ← NULL

    # Allocation phase
    *modelP ← NULL
    model ← allocate_model_wrapper(env)
    if model == NULL:
        release_lock(env)
        return OUT_OF_MEMORY

    model.initialized ← 1
    model.matrix ← allocate_matrix_data(env)
    if model.matrix == NULL:
        free_model(model)
        release_lock(env)
        return OUT_OF_MEMORY

    # Initialize matrix metadata
    mat ← model.matrix
    mat.version ← 1
    mat.numConstrs ← 0
    mat.numVars ← numvars
    mat.numNonzeros ← 0
    mat.solStatus ← -1
    mat.objOffset ← 0.0

    # Copy model name
    if Pname ≠ NULL:
        mat.modelName ← copy_string(Pname)

    # Allocate variable arrays
    if numvars > 0:
        mat.objCoeffs ← allocate(numvars × sizeof(double))
        mat.colStart ← allocate((numvars + 1) × sizeof(int64))
        mat.colLen ← allocate(numvars × sizeof(int32))
        mat.lb ← allocate(numvars × sizeof(double))
        mat.ub ← allocate(numvars × sizeof(double))
        mat.vtype ← allocate(numvars × sizeof(char))
        # (check each allocation, goto error on failure)

    # Initialize variable data
    for i ← 0 to numvars-1:
        mat.objCoeffs[i] ← (obj ≠ NULL) ? obj[i] : 0.0
        mat.lb[i] ← (lb ≠ NULL) ? lb[i] : 0.0
        mat.ub[i] ← (ub ≠ NULL) ? ub[i] : INFINITY

        if vtype ≠ NULL:
            t ← uppercase(vtype[i])
            mat.vtype[i] ← t
        else:
            mat.vtype[i] ← 'C'

        mat.colStart[i] ← 0
        mat.colLen[i] ← 0

    mat.colStart[numvars] ← 0  # sentinel

    # Validate and adjust variable types
    validate_vartypes(model)  # Adjusts binary bounds, etc.

    # Set up variable names
    if varnames ≠ NULL and numvars > 0:
        mat.varnames ← allocate(numvars × sizeof(char*))
        for i ← 0 to numvars-1:
            if varnames[i] ≠ NULL:
                mat.varnames[i] ← copy_string(varnames[i])
            else:
                mat.varnames[i] ← format_string("C%d", i)

    # Success
    *modelP ← model
    release_lock(env)
    return SUCCESS
```

### 4.4 Mathematical Foundation

The function initializes an optimization problem with the form:

```
minimize    c^T x + objOffset
subject to  (no constraints initially)
            lb_i ≤ x_i ≤ ub_i    ∀i ∈ {0, ..., numvars-1}
            x_i ∈ type_i           ∀i ∈ {0, ..., numvars-1}
```

Where:
- c = obj (objective coefficient vector)
- x = decision variables
- lb = lower bound vector
- ub = upper bound vector
- type_i ∈ {ℝ, {0,1}, ℤ, ℝ⁺∪{0}, ℤ⁺∪{0}} (continuous, binary, integer, semi-continuous, semi-integer)

Default values:
- c_i = 0.0 if obj is NULL
- lb_i = 0.0 if lb is NULL
- ub_i = +∞ (represented as 1×10¹⁰⁰) if ub is NULL
- type_i = ℝ (continuous) if vtype is NULL

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) when numvars = 0 (empty model)
- **Average case:** O(n) where n = numvars
- **Worst case:** O(n + L) where n = numvars and L = total length of all variable names

Where:
- n = numvars (number of variables to create)
- L = sum of string lengths in Pname and varnames

### 5.2 Space Complexity

- **Auxiliary space:** O(n) for variable arrays
- **Total space:** O(n) where n = numvars

The model structure requires:
- numvars × sizeof(double) for each of: objCoeffs, lb, ub (3 arrays)
- numvars × sizeof(int64) for colStart
- numvars × sizeof(int32) for colLen
- numvars × sizeof(char) for vtype
- numvars × sizeof(char*) for varnames (if provided)
- Plus string storage for model name and variable names

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Environment not initialized | 1003 | cxf_checkenv fails |
| Environment not active | 1009 | env.active == 0 |
| Negative numvars | 1003 | numvars < 0 |
| NULL modelP | 1002 | Required output pointer is NULL |
| Invalid array | 1003 | Array validation fails |
| Allocation failure | 1001 | Any memory allocation fails |

### 6.2 Error Behavior

On any error, the function:
1. Frees all partially allocated memory (model wrapper, matrix data, arrays)
2. Sets *modelP to NULL
3. Releases environment lock if held
4. Sets environment error message (retrievable via cxf_geterrormsg)
5. Returns non-zero error code

The function guarantees no memory leaks and leaves the environment in a consistent state. The environment can be used to create other models after a failed attempt.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty model | numvars = 0 | Creates valid model with no variables or constraints |
| NULL arrays | All array parameters NULL | Uses default values (0.0 for bounds, continuous for type) |
| NULL model name | Pname = NULL | Model has no name (mat.modelName = NULL) |
| Name suppression | env.noNames = 1 | Ignores Pname and varnames, sets all to NULL |
| Mixed NULL varnames | varnames[i] = NULL for some i | Generates default name "Ci" for those variables |
| Lowercase vtype | vtype[i] = 'b' | Converted to uppercase 'B' |
| Binary out of bounds | vtype='B', lb=-1, ub=2 | Bounds clamped to [0,1] |
| Negative infinity bound | lb[i] = -1e100 | Accepted as valid lower bound |

## 8. Thread Safety

**Thread-safe:** Yes (conditionally)

The function is thread-safe for concurrent calls with the same environment, as it acquires the environment lock during creation. However:
- Multiple threads can safely create models from the same environment
- Each created model should then be used by only one thread
- The environment lock prevents concurrent modifications to environment state

**Synchronization required:** Environment lock (acquired internally)

The function handles all necessary locking internally. Callers do not need to acquire locks.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_checkenv | Environment | Validates environment is initialized |
| cxf_env_acquire_lock | Threading | Acquires environment lock for thread safety |
| cxf_env_release_lock | Threading | Releases environment lock |
| cxf_validate_array | Validation | Validates input arrays are accessible |
| cxf_model_alloc | Memory | Allocates model wrapper structure |
| cxf_matrix_alloc | Memory | Allocates matrix data structure |
| cxf_malloc | Memory | Environment-scoped memory allocation |
| cxf_validate_vartypes | Validation | Validates and adjusts variable types/bounds |
| cxf_model_free | Memory | Frees partial model on error |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| User code | N/A | Primary API for creating models |
| cxf_readmodel | Model I/O | Creates model then populates from file |
| cxf_fixedmodel | Analysis | Creates fixed model from solution |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_freemodel | Inverse operation - destroys model and frees memory |
| cxf_copymodel | Creates duplicate of existing model |
| cxf_addvar | Adds variables to existing model (requires update) |
| cxf_addvars | Adds multiple variables to existing model |
| cxf_updatemodel | Applies pending modifications (not needed for new models) |

## 11. Design Notes

### 11.1 Design Rationale

The function allows specifying initial variables during creation for efficiency - creating a model with 10,000 variables is faster when done in one call versus 10,000 individual cxf_addvar calls. The lazy update mechanism is not needed here since the model starts with no constraints.

Variable type conversion to uppercase provides user-friendly API where 'b' and 'B' both work for binary variables.

Environment-scoped allocation allows efficient bulk deallocation when the model is freed, without tracking each individual allocation.

### 11.2 Performance Considerations

- String length calculations
- Array initialization
- Memory copy operations

When building models programmatically, consider:
- Batching variable creation in cxf_newmodel rather than incremental cxf_addvar calls
- Suppressing names (env.noNames = 1) to save memory for very large models
- Passing NULL for unused arrays (obj, lb, ub) if defaults are acceptable

### 11.3 Future Considerations


The CSC format is initialized empty but must be resized when constraints are added. The column start array has a sentinel at position numvars to simplify iteration.

## 12. References

- Convexfeld Optimizer Reference Manual: Model Creation
- "Linear Programming and Extensions" - Dantzig (1963) for LP problem formulation
- IEEE 754 standard for CXF_INFINITY representation (1×10¹⁰⁰)

## 13. Validation Checklist

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
