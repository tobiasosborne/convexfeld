# Model

## Purpose

The Model structure is the central data container for a single optimization problem instance within an LP solver. It encapsulates all problem data (variables, constraints, objective, matrix coefficients), solver configuration, solution state, and the attribute system that exposes model properties to the public API. Every public API call that operates on a problem accepts a reference to a Model, and the structure serves as the root from which all problem-specific data is reachable. A Model is always associated with exactly one Environment, from which it inherits global configuration.

## Fields

Fields are grouped by logical purpose rather than memory layout.

### Identity and Validity

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| validity_sentinel | unsigned 32-bit integer | A fixed constant written at allocation time; every API function compares this field against the expected constant before accessing the model. Acts as a guard against use-after-free and invalid pointer casts. | Exactly the predefined sentinel constant | Must equal the expected constant for all operations; if it does not match, the model is treated as invalid and API calls return an error |
| secondary_sentinel | unsigned 64-bit integer | A second fixed constant for additional validation, providing a stronger guarantee against accidental pointer aliasing | Exactly the predefined secondary constant | Set once at allocation; never modified |

### Modification Control

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| modification_blocked | int | Prevents modifications to the model while an optimization is in progress. Set to a nonzero value when optimization begins, cleared when it completes. | 0 (modifications allowed) or nonzero (modifications blocked) | Nonzero only while an optimization or update operation is active |
| status_code | int | Stores the most recent optimization or processing status code for the model | Any valid solver status code, or 0 when cleared | Cleared to 0 at the start of each optimization |

### Initialization and Mode

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| initialized | int | Indicates whether the model has completed its full initialization sequence, including attribute table setup and internal state preparation | 0 (not yet initialized) or 1 (fully initialized) | Set to 1 only after successful completion of the full initialization sequence |
| solve_mode | int | Flags a special solve mode, such as concurrent optimization, that affects how the optimizer dispatches work | 0 (normal) or a mode-specific nonzero value (e.g., concurrent) | [UNDETERMINED: full set of mode values] |
| optimize_in_progress | int | Transient flag indicating an optimization call is currently active; cleared before and after each optimization | 0 (idle) or nonzero (optimizing) | Nonzero only between the start and end of an optimization call |

### Environment Linkage

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| environment | pointer-to-Environment | Reference to the Environment that owns or is associated with this model. Provides access to logging, parameter defaults, and thread management. | Non-null pointer to a valid Environment | Must always point to a valid, live Environment; the Environment must outlive the Model |
| environment_owned | int | Indicates whether this model owns its Environment (i.e., has a private child Environment that must be freed when the model is freed) versus borrowing a shared Environment | 0 (borrowed/shared) or 1 (owned/child) | If 1, the model's destructor must free the child Environment |

### Concurrent Optimization

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| concurrent_environments | pointer-to-array-of-Environment-pointers | Array of Environment references used for concurrent optimization, where multiple solver instances run in parallel with different parameter settings | Null (no concurrent optimization) or pointer to a valid array | Array length equals concurrent_environment_count |
| concurrent_environment_count | int | Number of concurrent environments configured | 0 or positive integer | 0 when concurrent_environments is null |

### Callback System

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| callback_count | int | Number of user-registered callback functions. When nonzero, the solver invokes callbacks at various checkpoints during optimization. | 0 or positive integer | Incremented on callback registration, decremented on removal |
| primary_model | pointer-to-Model | Reference to the "root" model for callback configuration. In typical usage, this points to the model itself; in cloned or concurrent scenarios, it may point to the original model from which callbacks were inherited. | Non-null pointer to a valid Model | Initialized to self at allocation; may be updated for cloned models |
| self_reference | pointer-to-Model | Set to point to the model itself during optimization, providing a stable reference that callback functions can use to access the model. Cleared after optimization completes. | Null (not optimizing) or pointer to self | Non-null only during an active optimization call |

### Matrix Data

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| matrix | pointer-to-MatrixData | The active matrix data for the model, used for general access to problem dimensions, variable types, constraint data, and coefficient storage. This is the primary entry point for reading model structure. | Null before first update, non-null after model is populated | Points to the same structure as either primary_matrix or working_matrix depending on solve state |
| primary_matrix | pointer-to-MatrixData | The original, unmodified problem matrix as specified by the user. Preserved across solves for reference and re-optimization. | Null before first update, non-null after model population | Stores the user-specified problem in compressed sparse column (CSC) format |
| working_matrix | pointer-to-MatrixData | A working copy of the matrix that may be modified during optimization (e.g., by scaling, presolve transformations, or bound tightening). The solver operates on this copy to preserve the original. | Null if no working copy exists, non-null during solve | May differ from primary_matrix during and after solve operations |

### Solution Data

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| solution_data | pointer-to-SolutionData | Container for all solution-related information including primal values, dual values, reduced costs, slack values, basis status, and objective value | Null before solve, non-null after a successful solve | Populated by the solver upon finding a feasible or optimal solution |

### Special Constraints

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| sos_data | pointer-to-SOSData | Storage for Special Ordered Set (SOS1 and SOS2) constraint definitions, including set membership, weights, and types | Null if no SOS constraints, non-null otherwise | Consistent with the SOS constraint count in the matrix data |
| general_constraint_data | pointer-to-GenConstrData | Storage for general constraints such as indicator constraints, piecewise-linear constraints, min/max/abs constraints, and other non-standard constraint types | Null if no general constraints, non-null otherwise | Consistent with the general constraint count in the matrix data |

### Pending Modifications

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| pending_buffer | pointer-to-PendingBuffer | Buffer that accumulates batched model modifications (variable additions, constraint additions, coefficient changes, bound changes, etc.) before they are applied to the matrix data by an explicit update call. This implements the lazy update pattern. | Null (no pending changes) or pointer to a valid buffer | Flushed to the matrix data on explicit update; must be empty during optimization |

### Attribute System

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| attribute_table | pointer-to-AttributeTable | Reference to the attribute table that provides the model's attribute access system. The table contains metadata entries for every queryable and settable attribute, supporting the public attribute API. See the Attribute Table Architecture section below. | Non-null after initialization | Allocated during model creation; freed during model destruction |

### Timing

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| update_time | double | Cumulative time (in seconds) spent applying pending modifications to the matrix data during update operations | Non-negative | Monotonically non-decreasing over the model's lifetime |

### Fingerprint

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| fingerprint | unsigned 32-bit integer | A hash or checksum of the model's problem data, used for determinism checking. Two models with identical problem data should produce the same fingerprint, enabling verification that solver runs are reproducible. | Any 32-bit value | Updated when the model data changes |

### Internal Work Storage

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| internal_vectors | array-of-pointers | A fixed-size array of internal vector containers used as scratch space during optimization. These provide pre-allocated workspace for solver algorithms to avoid repeated allocation/deallocation. | Null entries (unused) or valid pointers | Allocated as needed during solve; may persist across solves for reuse |

### History

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| history | pointer-to-HistoryData | Stores optimization history entries recording solver progress across multiple solve calls, enabling warm-start and solution pool queries | Null if no history, non-null if history tracking is active | [UNDETERMINED: exact conditions for history creation] |

## Attribute Table Architecture

The Model exposes its properties through a table-driven attribute system that supports three types of attributes:

- **Integer attributes** (e.g., optimization status, variable/constraint counts, model type flags)
- **Double attributes** (e.g., objective value, objective bound, runtime)
- **String attributes** (e.g., model name, variable names, constraint names)

Each attribute can be either **scalar** (one value per model, such as the optimization status) or **array** (one value per element, such as solution values per variable).

### Attribute Table Structure

The attribute table wrapper contains:

| Field | Type | Purpose |
|-------|------|---------|
| metadata | pointer | Table-level metadata or synchronization data |
| count | int | Number of registered attribute entries |
| entries | pointer-to-array-of-AttributeEntry | The array of attribute entry descriptors |

### Attribute Entry Structure

Each attribute entry contains complete metadata for one queryable/settable attribute:

| Field | Type | Purpose |
|-------|------|---------|
| name | string | The attribute name used in API calls (e.g., "Status", "ObjVal", "NumVars", "X") |
| type_code | int | Identifies the value type: integer, double, or string |
| is_array | int | 0 for scalar (model-level) attributes, nonzero for array (element-level) attributes |
| scalar_getter | function-pointer | Function that retrieves a scalar value for this attribute from the model |
| array_getter | function-pointer | Function that retrieves an element-level value for this attribute |
| setter | function-pointer | Function that sets the value of this attribute |
| direct_value | pointer | Optional direct pointer to the cached value, bypassing getter functions for performance |

### Value Retrieval Priority

When a scalar attribute value is requested through the public API, the system uses a three-tier retrieval strategy:

1. **Direct value pointer** (fastest path): If the attribute entry has a non-null direct value pointer, the value is read directly from the pointed-to location. This is used for frequently accessed attributes whose values are stored at fixed locations in the model or matrix data (e.g., variable count, constraint count).

2. **Scalar getter function**: If the direct value pointer is null but a scalar getter function is registered, it is invoked to compute and return the value. This is used for derived or computed attributes (e.g., IsQP, which must inspect the objective structure).

3. **Array getter fallback**: If both the direct value pointer and scalar getter are null, but an array getter is registered, it is invoked with parameters indicating a full-model scope. This is a rare fallback for attributes that are primarily element-level but can also report a model-level aggregate.

If all three methods are unavailable (all null), the system returns an "attribute not available" error.

### Attribute Lookup

Attribute lookup is performed by name: the system searches the entry array for an entry whose name matches the requested attribute name. The search returns the entry's index, or a sentinel indicating the attribute was not found.

## Relationships

- **Environment** (owns-or-borrows): Every Model references exactly one Environment. If environment_owned is set, the Model has a private child Environment that it must free on destruction. Otherwise, the Environment is shared and must outlive the Model.

- **MatrixData** (owns): The Model owns its primary and working matrix structures. These are allocated and freed with the Model. The matrix field provides a convenience alias to whichever matrix is currently active.

- **PendingBuffer** (owns): The pending modifications buffer is owned by the Model and freed on destruction.

- **AttributeTable** (owns): The attribute table and its entries are allocated during Model creation and freed during destruction.

- **SolutionData** (owns): Solution storage is allocated by the solver and freed with the Model.

- **SOSData, GenConstrData** (owns): Special constraint data is owned by the Model.

- **HistoryData** (owns): Optimization history, if present, is owned by the Model.

- **Internal vectors** (owns): Scratch workspace is allocated and freed with the Model.

## Lifecycle

### Creation

1. Memory is allocated for the Model structure and zero-initialized.
2. The validity sentinel and secondary sentinel are written to their fixed values.
3. The primary_model reference is set to point to the Model itself.
4. Modification control flags are cleared.
5. The Environment reference is set to the parent Environment.
6. Optionally, a child Environment is created (inheriting parameters from the parent), and environment_owned is set to 1.
7. The attribute table is allocated and populated with entries for all supported attributes, registering getter/setter functions and direct value pointers as appropriate.
8. Internal initialization routines configure the Model's remaining subsystems.
9. The initialized flag is set to 1.

### Modification (Lazy Update Pattern)

- When the user adds variables, constraints, or changes coefficients/bounds, changes are recorded in the pending_buffer rather than applied immediately to the matrix data.
- An explicit update call flushes all pending changes to the matrix data, rebuilds internal structures as needed, and clears the pending buffer.
- This batching strategy avoids redundant internal rebuilds when many changes are made in sequence.

### Optimization

1. The Model is validated by checking the validity sentinel.
2. modification_blocked is set to a nonzero value; status_code and optimize_in_progress are cleared/set.
3. self_reference is set to point to the Model.
4. The optimizer is invoked (dispatching to LP, barrier, or concurrent solvers as appropriate).
5. On completion, modification_blocked and self_reference are cleared.
6. Solution data is populated if the solve was successful.

### Destruction

1. All owned sub-structures are freed: solution data, matrix data (primary and working), pending buffer, SOS data, general constraint data, internal vectors, history data, and the attribute table.
2. If environment_owned is set, the child Environment is freed.
3. The validity sentinel is zeroed or overwritten to prevent use-after-free.
4. The Model memory is deallocated.

## Invariants

1. **Sentinel integrity**: The validity_sentinel must equal the predefined constant at all times between creation and destruction. Any API call that finds a mismatched sentinel must return an error without accessing other fields.

2. **Environment liveness**: The environment pointer must always reference a valid, initialized Environment. The Environment must not be freed before all its Models.

3. **Modification exclusion**: While modification_blocked is nonzero, no API call that modifies the model (add variable, add constraint, change coefficient, etc.) may proceed. Such calls must return a modification-during-optimization error.

4. **Pending buffer consistency**: The pending_buffer must be flushed (or empty) before optimization begins. The optimizer must not encounter stale or partial modifications.

5. **Matrix pointer consistency**: The matrix convenience pointer must always reference either primary_matrix or working_matrix (or be null before any matrix exists). It must not point to a freed or unrelated structure.

6. **Attribute table completeness**: After initialization, the attribute table must contain entries for all documented public attributes. Getter/setter function pointers must be non-null for attributes that support the corresponding operation.

7. **Concurrent environment array**: concurrent_environment_count must exactly match the number of valid entries in the concurrent_environments array.

8. **Callback count non-negative**: callback_count must be non-negative and must accurately reflect the number of registered callbacks.

## Thread Safety

- **Single-model, single-thread**: A Model is not inherently thread-safe. Concurrent access to the same Model from multiple threads requires external synchronization.

- **Optimization locking**: The Environment provides a solve lock that ensures only one optimization can be active at a time per Environment. This lock is acquired at the start of optimization and released at the end.

- **Concurrent optimization**: When concurrent solve mode is active, the solver may internally create multiple threads, each operating on its own copy or view of the data. The concurrent_environments array provides per-thread parameter isolation. User code must not modify the Model while concurrent optimization is running.

- **Callback safety**: When a user callback is invoked during optimization, the callback receives a reference to the Model. The callback may query certain attributes (solution values, objective, etc.) but must not make structural modifications.

## Design Rationale

- **Validity sentinel pattern**: The sentinel field provides a lightweight, constant-time check against common programming errors: use of freed models, corrupted pointers, and type confusion. This pattern is standard in C-based systems that expose opaque handles to users. The use of two sentinels (primary and secondary) further reduces the probability of false positives from random memory.

- **Lazy update model**: Batching modifications in a pending buffer and applying them on explicit update avoids the cost of rebuilding internal data structures (sparse matrix indices, presolve state) after every individual change. This is a well-known optimization in commercial LP solvers, commonly referred to as the "lazy update" pattern.

- **Child environment ownership**: Allowing a Model to own a private child Environment enables per-model parameter overrides without affecting the parent Environment or sibling Models. This supports the common pattern of solving the same problem with different parameter settings.

- **Table-driven attribute system**: Rather than hard-coding getter/setter logic for each attribute, the attribute table provides a uniform, extensible mechanism. New attributes can be added by registering an entry with appropriate function pointers. The three-tier retrieval priority (direct value, scalar getter, array getter) optimizes for the common case (cached values) while supporting computed and element-level attributes through the same interface.

- **Primary/working matrix separation**: Preserving the original problem matrix while operating on a working copy during solve allows the solver to apply transformations (scaling, presolve reduction, bound tightening) without destroying the user's problem. After solve, the original matrix is available for re-optimization, modification, or reporting.

- **Self-reference during optimization**: Setting self_reference during optimization provides a stable, reentrant-safe handle for callback functions and internal routines that need to navigate back to the Model from intermediate data structures.
