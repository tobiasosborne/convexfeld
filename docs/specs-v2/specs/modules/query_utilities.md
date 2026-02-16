# Module: Query Utilities

## Purpose

The Query Utilities module provides a collection of miscellaneous query and mutation functions that serve the solver's internal subsystems but do not belong to a single algorithmic domain. These functions fall into three categories: (1) data retrieval functions that look up constraint metadata by index or type, (2) a state inspection function that checks whether optimization history is available, and (3) a variable fixing function that records a variable-fixing operation within the simplex presolve/bound-propagation framework.

The module spans different layers of the solver -- from simple constant-table lookups (general constraint name retrieval) through cached sparse data extraction (quadratic constraint data) to solver-state mutation (eta vector creation for variable fixing). Despite this diversity, all five functions share the characteristic of being self-contained utilities invoked by higher-level solver routines rather than being part of a cohesive algorithmic pipeline.

## Functions

### cxf_get_genconstr_name

**Purpose:** Map a general constraint type index to a human-readable name string.

**Signature:**
- Input: `type_index` : unsigned int -- The zero-based index of a general constraint type
- Output: pointer-to-constant-string -- The name of the constraint type, or NULL if the index is out of range

**Preconditions:**
- None; the function handles all input values gracefully

**Postconditions:**
- If the type index is within the valid range, returns a pointer to a static, immutable string naming that constraint type
- If the type index is out of range, returns NULL

**Side Effects:**
- None. This is a pure function with no state modification.

**Error Conditions:**
- Out-of-range type index: returns NULL (no error reporting, no exception)

**Behavioral Description:**
The solver supports a fixed set of general constraint types, each identified by a sequential integer index. This function performs a bounds check on the provided type index. If the index falls within the defined range, the function returns a pointer to the corresponding entry in a static table of constant name strings. If the index exceeds the defined range, it returns NULL.

The defined general constraint types, in index order, are:

| Category | Types |
|----------|-------|
| Aggregate | MAX, MIN |
| Unary | ABS |
| Logical | AND, OR |
| Conditional | INDICATOR |
| Nonlinear (internal) | NL |
| Piecewise | PWL |
| Polynomial | POLY |
| Transcendental | EXP, EXPA (base-a exponential), LOG, LOGA (base-a logarithm), POW, SIN, COS, TAN |
| Reserved | Two entries reserved for future use |

These constraint types correspond to the publicly documented CXF_GENCONSTR_* constants in the ConvexFeld Optimizer Reference Manual.

Callers receiving a NULL return value should provide their own fallback label (e.g., "UNKNOWN") rather than dereferencing the NULL pointer.

**Thread Safety:** Thread-safe. The function reads only from static, immutable data. No synchronization is required.

**Dependencies:** None (leaf function; uses only a compile-time constant table).

---

### cxf_get_qconstr_data

**Purpose:** Retrieve the sparse representation of a quadratic constraint's coefficient data, using lazy caching to amortize the cost of dense-to-sparse conversion.

**Signature:**
- Input: `environment` : pointer-to-Environment -- The environment, used for memory allocation
- Input: `qc_storage` : pointer-to-QConstrStorage -- The quadratic constraint data container
- Input: `qconstr_index` : int -- The zero-based index of the quadratic constraint to retrieve
- Output (via pointer): `num_nonzeros` : pointer-to-int -- Receives the count of nonzero entries in the sparse representation
- Output (via pointer): `indices` : pointer-to-pointer-to-int -- Receives a pointer to the cached array of variable indices
- Output (via pointer): `values` : pointer-to-pointer-to-double -- Receives a pointer to the cached array of coefficient values
- Return: int -- Zero on success, or an error code on failure

**Preconditions:**
- The environment must be valid for memory allocation
- The quadratic constraint storage must be properly initialized
- The constraint index must correspond to a valid quadratic constraint

**Postconditions:**
- On success (return zero): all non-NULL output pointers are populated with the sparse data for the requested constraint
- On failure: output pointers are set to zero/NULL; previously cached data for other constraints is not affected
- The cached sparse data remains valid until the model is modified

**Side Effects:**
- On first access for a given constraint index, allocates or reallocates cached arrays through the environment's memory allocator
- Updates internal cache state within the quadratic constraint storage
- If a constraint has no nonzero entries (after excluding the reference value), frees any previously cached arrays for that constraint

**Error Conditions:**
- OUT_OF_MEMORY (10001): Memory allocation failed during cache population. Output pointers remain at their initialized zero values.

**Behavioral Description:**
The function extracts the sparse (index, value) representation of a quadratic constraint's off-diagonal coefficient data. It implements a lazy caching strategy to avoid redundant conversion work.

The quadratic constraint storage maintains per-constraint cached sparse arrays. The function's behavior depends on the cache state:

1. **Output initialization:** All non-NULL output pointers are initialized to zero before any processing.

2. **Cache lookup:** The function checks whether the requested constraint index matches the storage's currently staged constraint. If it does not match, the function skips directly to the output phase, returning whatever cached data already exists for the requested index from a previous call.

3. **Cache validity check:** If the index matches but the cache is not yet populated (marked invalid or the index is negative), the function also skips to the output phase.

4. **Dense-to-sparse conversion:** When the cache must be built, the function scans a dense coefficient array associated with the constraint. Each element is compared against a stored reference value (representing the diagonal or excluded value). Elements that differ from the reference value are collected into a sparse representation: an array of integer indices and a corresponding array of double coefficient values.

5. **Cache storage:** If nonzero entries were found, the function reallocates per-constraint cached arrays to the exact required size and copies the sparse data into them. If no nonzero entries were found, any existing cached arrays for this constraint are freed.

6. **Output:** The function writes the cached nonzero count, index array pointer, and value array pointer to the caller's output locations.

The caching strategy means that the first access to a constraint incurs O(n) conversion cost (where n is the number of dense elements), while subsequent accesses return cached pointers in O(1) time.

**Thread Safety:** Unsafe. The function modifies shared cache state and performs memory allocation. Callers must hold appropriate synchronization when accessing the same quadratic constraint storage from multiple threads.

**Dependencies:**
- Memory allocation (realloc-style) via the environment
- Memory deallocation for empty-result cleanup

---

### cxf_count_genconstr_types

**Purpose:** Count the number of general constraints of each type in the model, optionally separating constraints that require nonlinear treatment from those that can be handled with standard or piecewise-linear methods.

**Signature:**
- Input: `model` : pointer-to-Model -- The model whose general constraints are to be counted
- Output: `counts` : array-of-int, length equal to the number of defined general constraint types -- Receives the count of constraints per type for the standard-method group
- Output (optional): `nl_counts` : array-of-int, length equal to the number of defined general constraint types, or NULL -- If non-NULL, receives the count of constraints per type for the nonlinear-method group
- Return: void

**Preconditions:**
- The model must be valid and its matrix data must be populated
- The counts array must be large enough to hold one entry per defined general constraint type

**Postconditions:**
- The counts array is zero-initialized and then populated with per-type constraint counts for the standard-method group
- If nl_counts is non-NULL, it is zero-initialized and populated with per-type counts for the nonlinear-method group
- For every general constraint in the model, exactly one entry in either counts or nl_counts is incremented

**Side Effects:**
- None beyond writing to the output arrays.

**Error Conditions:**
- None. The function does not return an error code. If the model has no general constraints, both output arrays contain all zeros.

**Behavioral Description:**
The function iterates over all general constraints in the model and classifies each one into either the standard-method group (counted in the counts array) or the nonlinear-method group (counted in the nl_counts array, if provided). The classification depends on the constraint type and, for transcendental constraint types, on per-constraint and global approximation settings.

The classification rules are:

1. **Zero-initialization:** Both output arrays are fully zero-initialized at the start of the function, regardless of their prior contents.

2. **True nonlinear constraints (NL type):** Constraints of the general nonlinear type are always classified as requiring nonlinear treatment. If nl_counts is provided, the NL-type count is incremented there. If nl_counts is NULL, these constraints are silently skipped (not counted in either array).

3. **Transcendental function constraints:** Constraints whose type falls in the transcendental range (exponential, logarithmic, power, trigonometric, and related types) are classified based on their per-constraint approximation mode setting:
   - If the approximation mode requests explicit piecewise-linear treatment, the constraint is counted in nl_counts (if provided), indicating it will undergo approximation processing.
   - If the approximation mode requests nonlinear treatment AND the global nonlinear-function parameter on the environment is enabled, the constraint is counted in nl_counts (if provided).
   - Otherwise (default approximation mode, or nonlinear mode requested but global parameter disabled), the constraint is counted in the standard counts array.

4. **All other constraint types** (aggregate, unary, logical, conditional, piecewise-linear, polynomial): These are always counted in the standard counts array.

The distinction between the two groups reflects the solver's ability to handle certain nonlinear constraints either through piecewise-linear approximation or through dedicated nonlinear methods, controlled by per-constraint and global parameter settings.

**Thread Safety:** Unsafe. The function reads model state (constraint array, environment parameters) and writes to output arrays. Concurrent modification of the model during iteration would produce undefined results.

**Dependencies:**
- Model structure (matrix data, general constraint array, environment reference)
- Environment (global nonlinear-function parameter)

---

### cxf_has_history

**Purpose:** Check whether the model has valid, usable optimization history data from a previous solve.

**Signature:**
- Input: `model` : pointer-to-Model -- The model to check
- Return: int -- 1 if valid history exists, 0 otherwise

**Preconditions:**
- None; the function handles NULL and invalid inputs gracefully

**Postconditions:**
- Returns 1 only if all of the following are true: the model is non-NULL, the model has an associated history structure, the history structure contains a non-NULL data pointer, the history is in a completed/usable state (as indicated by a state field), and the history contains at least one entry
- Returns 0 in all other cases

**Side Effects:**
- None. This is a pure query function.

**Error Conditions:**
- NULL model pointer: returns 0 (no error reporting)
- NULL history structure: returns 0
- History in incomplete or invalid state: returns 0
- History with zero entries: returns 0

**Behavioral Description:**
The function performs a series of null-pointer and validity checks on the model's optimization history data, returning 1 only when all checks pass.

The checks are performed in order, with early return on the first failure:

1. **Model validity:** If the model pointer is NULL, return 0.

2. **History structure existence:** The model maintains a pointer to a history data structure. If this pointer is NULL, return 0.

3. **History data pointer:** The history structure contains a pointer to the actual history entry storage. If this pointer is NULL (no history has been recorded), return 0.

4. **History state:** The history structure contains a state field indicating whether the history is in a complete, usable state. Only a specific "completed" state value indicates valid history; all other values cause a return of 0. This distinguishes between history that is still being recorded, has been cleared, or has never been initialized.

5. **History entry count:** The history structure contains a count of recorded entries. If this count is zero or negative, return 0.

Only when all five checks pass does the function return 1, confirming that the model has valid optimization history available for warm-starting, solution analysis, or debugging.

**Thread Safety:** Safe for concurrent reads, provided the model's history state is not being modified simultaneously. The function performs only read operations on model data.

**Dependencies:** None (leaf function; reads only model fields).

---

### cxf_fix_variable

**Purpose:** Record a variable-fixing operation by creating an eta vector that captures the effects of fixing a variable to a specified value, for use during simplex preprocessing, bound propagation, and crossover.

**Signature:**
- Input: `environment` : pointer-to-Environment -- The environment, used for memory allocation context
- Input: `solver_state` : pointer-to-SolverState -- The current simplex solver state containing matrix data, status arrays, and eta vector management
- Input: `variable_index` : int -- The index of the variable being fixed
- Input: `fixed_value` : double -- The value at which the variable is being fixed
- Input: `fixing_mode` : int -- A mode flag indicating the type of fixing operation (stored in the eta record for later interpretation)
- Return: int -- Zero on success, or an error code on failure

**Preconditions:**
- The solver state must be fully initialized with valid matrix data (both column-major and row-major representations)
- The variable index must refer to a valid variable in the solver's working matrix
- The solver state's memory pool must be available for allocation
- Constraint status and activity arrays must be current

**Postconditions:**
- On success: a new eta vector is created and prepended to the solver state's eta vector list. The eta vector records the variable index, fixed value, fixing mode, all affected constraints, and the coefficient ratios needed to reconstruct the fixing operation.
- The solver state's eta count and affected-row count are updated.
- The solver state's work accumulator (if active) is updated with the computational work performed.
- On failure: no eta vector is created; the solver state is not modified.

**Side Effects:**
- Allocates memory from the solver state's memory pool
- Modifies the eta vector linked list (prepends the new vector)
- Increments the eta count and affected-row counters on the solver state
- Updates the work accumulator for timing/work-limit tracking

**Error Conditions:**
- OUT_OF_MEMORY (10001): The memory pool allocation for the eta vector failed. No state is modified.

**Behavioral Description:**
When a variable is fixed to a value during simplex preprocessing or bound propagation, this function creates an eta vector that records all the information needed to later reconstruct the effects of that fixing. This is the standard approach for recording basis changes in Product Form of the Inverse (PFI) methods, where eta vectors form a chain of elementary transformations applied to the basis matrix (see Dantzig, 1963, *Linear Programming and Extensions*; Maros, 2003, *Computational Techniques of the Simplex Method*, Chapter 5).

The function proceeds in several phases:

1. **Counting affected constraints:** The function scans the variable's column in the compressed sparse column (CSC) representation. For each entry where the corresponding constraint is active (non-deleted), it counts the number of other active variables in that constraint. This total represents the "fill-in" -- the number of coefficient ratio entries that must be stored.

2. **Work tracking:** The computational work for the column scan is accumulated into the solver state's work counter (if timing is active). This enables the solver's work-limit termination criterion.

3. **Eta vector allocation:** The function calculates the total memory needed for the eta vector, which includes a fixed-size header followed by variable-length inline data arrays:
   - An array of affected constraint indices
   - An array of running offsets into the fill-in data
   - An array of fill-in variable indices
   - An array of coefficient ratios (doubles)

   All arrays are stored contiguously in a single allocation from the solver state's memory pool, with appropriate alignment.

4. **Header initialization:** The eta vector header records:
   - The eta type (variable-fixing type, distinct from bound-pivot eta vectors)
   - The variable index and fixed value
   - The fixing mode flag
   - The count of affected constraints
   - Internal pointers to each inline data array
   - A link to the previous head of the eta list (forming a singly-linked list)

5. **Fill-in data population:** For each active constraint containing the fixed variable, the function:
   - Records the constraint index
   - Records the running offset into the fill-in arrays
   - Computes the constraint-level coefficient ratio: the constraint's current activity value divided by the variable's coefficient in that constraint
   - Scans the constraint's row (using the compressed sparse row representation) to find all other active variables, and for each:
     - Records the variable index
     - Computes and stores the negated coefficient ratio (the row coefficient negated and divided by the column coefficient)
   - Updates the work accumulator for each row scanned

6. **Finalization:** The final offset is stored (representing the total fill-in count), the eta vector is linked into the solver state's eta list, and counters are updated.

The stored coefficient ratios allow later basis reconstruction or crossover to efficiently process the fixing operation without re-reading the original matrix data. The negation of row coefficients in the fill-in ratios follows from the standard elimination algebra: when a variable is fixed, its column is eliminated from the remaining constraints by subtracting scaled multiples of the pivot row.

**Thread Safety:** Unsafe. The function modifies the solver state's eta list, memory pool, and work counters. It is designed for single-threaded use within the simplex algorithm's main loop.

**Dependencies:**
- Memory pool allocation from the solver state
- Solver state's CSC and CSR matrix representations
- Solver state's constraint status, variable status, and activity arrays
- Solver state's eta vector list and counters

---

## Module-Level Behavioral Notes

### Heterogeneity of Functions

Unlike most modules in the solver, which group functions by a single algorithmic concern, this module collects five functions that share no algorithmic pipeline. Their grouping reflects a pragmatic classification: each function is too small or too unique to warrant its own module, yet they do not belong to any of the larger thematic modules (error handling, logging, state management, simplex iteration, etc.).

### Relationship Between cxf_get_genconstr_name and cxf_count_genconstr_types

These two functions are frequently used together for reporting and diagnostics. The counting function categorizes constraints by type index, and the naming function converts those type indices to human-readable labels. They share a common type-index domain (the set of defined general constraint types), and both rely on the same type enumeration ordering.

### cxf_fix_variable and the Eta Vector System

cxf_fix_variable creates type-3 eta vectors, which are one of several eta vector types in the solver's Product Form of the Inverse (PFI) system. Other functions in the simplex module (part of the Pivoting module, not this module) create type-1 eta vectors for bound pivots. The eta vectors form a singly-linked list on the solver state and are processed during basis refactorization and crossover.

### Caching Strategy in cxf_get_qconstr_data

The lazy caching in cxf_get_qconstr_data follows a common pattern in the solver: expensive conversions (dense-to-sparse, CSC-to-CSR) are deferred until first access, then cached for subsequent use. The cache is invalidated when the model is modified. This amortization strategy is standard in LP solver implementations where data may be queried zero or many times after construction.

### Error Handling Patterns

The five functions exhibit different error handling approaches reflecting their different roles:
- **cxf_get_genconstr_name:** Returns NULL for invalid input (no error state modification)
- **cxf_get_qconstr_data:** Returns an error code (OUT_OF_MEMORY) and initializes output pointers to zero on failure
- **cxf_count_genconstr_types:** Void return; silently produces zero-filled arrays if no constraints exist
- **cxf_has_history:** Returns 0 for all invalid/missing cases (boolean-style error handling)
- **cxf_fix_variable:** Returns an error code (OUT_OF_MEMORY) on allocation failure

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Algorithm references cite published work (Dantzig 1963, Maros 2003)
[x] General constraint types described by public API documentation, not binary constants
[x] Passes the Clean Room Test
```
