# Structure: CxfModel

**Spec Version:** 1.0
**Primary Module:** Model Management

## 1. Purpose

### 1.1 What It Represents

The CxfModel structure represents a complete mathematical optimization problem instance in Convexfeld. It encapsulates the problem formulation (objective function, constraints, variable bounds, variable types), solution data, and all metadata required to solve and analyze the optimization problem. A model is the central data structure that users manipulate through the Convexfeld API.

Conceptually, a model is a container for a mathematical program of the form:

```
Minimize/Maximize:  c'x + x'Qx/2 + constant
Subject to:         Ax {<=, =, >=} b
                    l <= x <= u
                    x_i in {Continuous, Binary, Integer, Semi-continuous, Semi-integer}
                    + Quadratic constraints
                    + SOS constraints
                    + General constraints (indicator, AND, OR, etc.)
```

The model tracks not just the current problem formulation, but also pending modifications (before cxf_updatemodel), solution values (after optimization), and metadata (variable/constraint names, fingerprints for determinism).

### 1.2 Role in System

The model sits at the center of the Convexfeld workflow:

- **Problem Container:** Stores complete problem formulation (matrix, bounds, senses, types)
- **Modification Buffer:** Batches incremental changes (addvar, addconstr, chgcoeff) before applying
- **Solution Repository:** Holds optimization results (objective value, variable values, duals, basis)
- **Attribute Interface:** Provides structured access to model data via attribute system
- **Solver Integration Point:** Links problem data to internal solver algorithms
- **Parent Reference:** Connects to environment for parameters and logging

Every optimization workflow follows: Create Model -> Modify Model -> Update Model -> Optimize -> Query Solution.

### 1.3 Design Rationale

The model design balances several competing concerns:

1. **Incremental Construction:** Users build models iteratively (add variables/constraints one-by-one). Batching modifications via PendingBuffer avoids expensive matrix reconstructions.
2. **Modification Tracking:** The `modificationBlocked` flag prevents model changes during optimization, ensuring solver state consistency.
3. **Matrix Format Flexibility:** Maintains both CSC (column-major) and CSR (row-major) views of constraint matrix, optimized lazily based on access patterns.
4. **Solution Persistence:** Stores solution data separately from problem data, allowing re-solve without losing previous solutions.
5. **Attribute Generalization:** Unified attribute system (numVars, status, solution values, etc.) simplifies API and enables introspection.

Alternative designs (e.g., immutable models) would require full copies for each modification, wasting memory and time.

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| validMagic | uint32 | Model validity marker | 0xC0FEFE1D (valid) or other (invalid) |
| modificationBlocked | boolean | Prevents changes during optimization | 0 (modifiable) or 1 (locked) |
| initialized | boolean | Model has been set up for solving | 0 (not ready) or 1 (ready) |
| env | CxfEnv* | Parent environment reference | Valid CxfEnv pointer |
| matrix | SparseMatrix* | Core constraint matrix | Valid SparseMatrix pointer |
| pendingBuffer | PendingBuffer* | Batched modifications before update | NULL or valid pointer |
| attrTablePtr | AttrTable* | Attribute metadata and getters | Valid AttrTable pointer |
| fingerprint | uint32 | Determinism checksum | Any uint32 value |
| updateTime | double | Time spent in cxf_updatemodel | >= 0 seconds |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| primaryModel | Self or parent | Root model for callbacks | Set during model creation |
| selfPtr | Self | Points to self during optimization | Set before solve, cleared after |
| primaryMatrix | Original matrix | Backup of matrix before presolve | Copied before presolve |
| workMatrix | Modified matrix | Working copy for solver | Created during presolve |

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| solutionData | void* | After successful optimization | NULL (unsolved) |
| sosData | void* | SOS constraints added | NULL (no SOS) |
| genConstrData | void* | General constraints added | NULL (no gen constrs) |
| concurrentEnvs | CxfEnv*[] | Concurrent optimization enabled | NULL (single solve) |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| callbackCount | int | Number of registered callbacks |
| solveMode | int | Special solve mode flag (concurrent, etc.) |
| envFlag | int | Environment-related flag for cleanup |
| concurrentEnvCount | int | Number of concurrent environments |
| vectors | void*[7] | Internal vector containers for solver |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| SparseMatrix | 1:1 | Exclusive - created on model creation, freed on model destruction |
| PendingBuffer | 0:1 | Exclusive - created on first modification, freed on model destruction |
| SolutionData | 0:1 | Exclusive - created after successful solve, freed on re-solve or model destruction |
| SOSData | 0:1 | Exclusive - created when SOS constraints added |
| GenConstrData | 0:1 | Exclusive - created when general constraints added |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| CxfEnv | 1:1 | Environment must outlive model |
| AttrTable | 1:1 | Shared attribute metadata (global singleton) |
| PrimaryModel | 0:1 | For child models in concurrent solve |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| CxfEnv | modelManager | Environment tracks all models |
| None | N/A | Top-level user-facing structure |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] validMagic = 0xC0FEFE1D OR model is invalid
- [x] env is never NULL
- [x] matrix is never NULL after initialization
- [x] attrTablePtr is never NULL
- [x] If modificationBlocked = 1, then optimization is in progress OR model is being freed
- [x] If pendingBuffer != NULL, then model has uncommitted changes
- [x] callbackCount >= 0

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] If initialized = 1, then matrix is fully constructed
- [x] If solutionData != NULL, then model has been solved at least once
- [x] If concurrentEnvCount > 0, then concurrentEnvs != NULL
- [x] If solveMode = 1, then concurrent optimization is configured
- [x] primaryModel points to self OR to parent model (for callbacks)
- [x] selfPtr = this during optimization, NULL otherwise
- [x] If workMatrix != NULL, then presolve has been applied
- [x] fingerprint is deterministic function of model content (when determinism enabled)

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After creation | validMagic set, env set, matrix allocated, modificationBlocked=0 |
| After modification | pendingBuffer != NULL (changes batched) |
| After cxf_updatemodel | pendingBuffer cleared, matrix updated, initialized may be 1 |
| During optimization | modificationBlocked=1, selfPtr=this, initialized=1 |
| After optimization | solutionData != NULL (if feasible), modificationBlocked=0 |
| Before destruction | validMagic = 0xC0FEFE1D, all owned structures present |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_newmodel | Create empty model | Empty matrix (0 vars, 0 constrs), no pending changes |
| cxf_readmodel | Load from file (.mps, .lp, etc.) | Full matrix, no pending changes, initialized=1 |
| cxf_copymodel | Copy existing model | Duplicate of source, independent |
| cxf_fixedmodel | Create fixed MIP model | Discrete vars fixed to solution values |

### 5.2 States

```
EMPTY -> BUILDING -> PENDING -> UPDATED -> READY -> OPTIMIZING -> SOLVED -> PENDING
  |                                                    |            |
  +------------------<---------------------------------+            |
  |                                                                 |
  +-------------------------<---------------------------------------+
                        (modification cycle)

State Descriptions:
- EMPTY: No variables or constraints
- BUILDING: Variables/constraints added, not yet updated
- PENDING: Modifications batched in pendingBuffer
- UPDATED: cxf_updatemodel applied, matrix consistent
- READY: Initialized, ready to optimize
- OPTIMIZING: Solve in progress (modificationBlocked=1)
- SOLVED: Solution available (solutionData != NULL)
```

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| EMPTY | BUILDING | cxf_addvar, cxf_addconstr | Allocate pendingBuffer, batch changes |
| BUILDING | PENDING | More modifications | Append to pendingBuffer |
| PENDING | UPDATED | cxf_updatemodel | Apply pending changes to matrix, clear buffer |
| UPDATED | READY | cxf_updatemodel (if needed) | Set initialized=1, validate model |
| READY | OPTIMIZING | cxf_optimize | Set modificationBlocked=1, selfPtr=this |
| OPTIMIZING | SOLVED | Solve completes | Allocate solutionData, store results, clear modificationBlocked |
| SOLVED | PENDING | cxf_addvar, cxf_chgcoeff | Invalidate solution, batch changes |
| SOLVED | READY | cxf_reset | Clear solution, keep matrix |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| cxf_freemodel | Free matrix, pendingBuffer, solutionData, sosData, genConstrData, concurrentEnvs, clear validMagic |

### 5.5 Ownership Rules

- **Who creates:** Application via cxf_newmodel, cxf_readmodel, or internal via cxf_copymodel
- **Who owns:** Application (must explicitly free)
- **Who destroys:** Application via cxf_freemodel
- **Sharing allowed:** No (each model is independent, but can share environment)

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| cxf_getintattr | Integer attribute value | O(1) for scalar, O(n) for array |
| cxf_getdblattr | Double attribute value | O(1) for scalar, O(n) for array |
| cxf_getstrattr | String attribute value | O(1) for scalar, O(n) for array |
| cxf_getvars | Variable data for range | O(k) where k = range size |
| cxf_getconstrs | Constraint data for range | O(k + nnz) where nnz = nonzeros in range |
| cxf_getcoeff | Single coefficient | O(log n) - binary search in column |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| cxf_addvar | Add variable | Appends to pendingBuffer, increments pending var count |
| cxf_addconstr | Add constraint | Appends to pendingBuffer, increments pending constr count |
| cxf_chgcoeff | Change coefficient | Records in pendingBuffer coefficient change list |
| cxf_setintattr | Set integer attribute | Modifies attribute value (may batch in pendingBuffer) |
| cxf_setdblattr | Set double attribute | Modifies attribute value (may batch in pendingBuffer) |
| cxf_updatemodel | Apply pending changes | Rebuilds matrix, clears pendingBuffer, sets initialized=1 |
| cxf_delconstrs | Delete constraints | Marks for deletion, requires update |
| cxf_delvars | Delete variables | Marks for deletion, requires update |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| cxf_addvars | Add multiple variables | All-or-nothing (rollback on error) |
| cxf_addconstrs | Add multiple constraints | All-or-nothing (rollback on error) |
| cxf_optimize | Solve model | Atomic - either completes or rolls back |
| cxf_read | Load from file | All-or-nothing (file parse) |
| cxf_write | Save to file | All-or-nothing (file write) |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed overhead | ~2 KB (model structure itself) |
| SparseMatrix | ~1 KB base + (8n + 16m + 24nnz) bytes where n=vars, m=constrs, nnz=nonzeros |
| PendingBuffer | ~512 bytes + pending data |
| SolutionData | ~100 bytes + (8n + 8m) bytes |
| Total (typical) | ~5 KB + 24 bytes/var + 16 bytes/constr + 24 bytes/nonzero |

### 7.2 Allocation Strategy

Main structure allocated as single block. SparseMatrix allocated separately on creation. PendingBuffer, SolutionData, SOSData, GenConstrData allocated lazily on first use. Matrix arrays (colStart, rowIndices, coeffValues) allocated in contiguous blocks for cache efficiency.

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| Entire structure | 8 bytes (pointer alignment) |
| SparseMatrix arrays | 16 bytes (SIMD optimization) |
| Double arrays | 8 bytes (natural alignment) |
| Coefficient values | 16 bytes (vectorization) |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Not thread-safe

Models are not internally synchronized. Concurrent access requires external locking.

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Read attributes | External lock (if concurrent with modifications) |
| Write attributes | External lock (always) |
| Optimize | External lock (model must be exclusively owned during optimization) |
| Modification | External lock (always) |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- Single thread owns model exclusively
- Multiple threads each own different models (sharing environment is safe)

**Unsafe patterns:**
- Two threads modifying same model -> data race
- One thread optimizing while another queries attributes -> race condition
- One thread freeing model while another uses it -> use-after-free

**Note:** Convexfeld supports concurrent optimization via concurrent environments (multiple solves of same model with different parameter settings), but this is coordinated internally with thread-safe structures.

## 9. Serialization

### 9.1 Persistent Format

Models can be saved/loaded in multiple formats:

- **MPS format:** Industry-standard mathematical programming format (text)
- **LP format:** Human-readable format (text)
- **REW format:** Native binary format (fast, compact)
- **SAV format:** State format (includes solution data)
- **JSON format:** Structured data format

### 9.2 Version Compatibility

| Version | Compatible | Migration |
|---------|------------|-----------|
| 12.x | Yes | Native read/write |
| 11.x | Yes | Can read 11.x files, 11.x can read 12.x MPS/LP (not REW) |
| 10.x | Partial | Use MPS/LP formats (universal) |
| Non-Convexfeld solvers | Partial | Use MPS/LP formats (may lose Convexfeld-specific extensions) |

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Sequential var/constr addition | Yes | Batched in pendingBuffer, amortized O(1) |
| Random coefficient access | Moderate | O(log n) binary search in CSC format |
| Full matrix traversal | Yes | Contiguous arrays, cache-friendly |
| Solution value queries | Yes | O(1) direct array access |
| Attribute queries | Yes | O(1) hash table + direct pointer |

### 10.2 Cache Behavior

Critical arrays (colStart, rowIndices, coeffValues, bounds, solution) are allocated contiguously for sequential access. Hot attributes (numVars, status, objVal) use direct pointers to avoid hash lookups.

### 10.3 Memory Bandwidth

Matrix operations (BTRAN, FTRAN, pricing) are memory-bandwidth-bound. CSC format optimizes for column operations (typical in simplex). CSR format built lazily only when needed (cxf_getconstrs).

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| validMagic != 0xC0FEFE1D | Magic check | Return CXF_ERR_INVALID_MODEL |
| modificationBlocked = 1 | Check before modification | Return CXF_ERR_OPTIMIZATION_IN_PROGRESS |
| Pending changes | pendingBuffer != NULL | Call cxf_updatemodel |
| Unsolved | solutionData = NULL | Return CXF_ERR_DATA_NOT_AVAILABLE |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| Magic number | Every API call | O(1) |
| Modification blocked | Before modification | O(1) |
| Index bounds | On element access | O(1) |
| NaN/Inf values | On coefficient set | O(1) |
| Matrix consistency | After cxf_updatemodel | O(nnz) |

## 12. Examples

### 12.1 Typical Instance

**Small LP: 100 variables, 50 constraints, 500 nonzeros**

```
Model:
  validMagic = 0xC0FEFE1D
  modificationBlocked = 0
  initialized = 1
  env = <valid CxfEnv*>

  matrix:
    numVars = 100
    numConstrs = 50
    numNonzeros = 500
    CSC format: colStart[101], rowIndices[500], coeffValues[500]

  pendingBuffer = NULL (no pending changes)
  solutionData = <valid*> (solved)

  Memory: ~5 KB structure + 15 KB matrix + 2 KB solution = ~22 KB
```

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| Empty | No variables, no constraints | numVars=0, numConstrs=0, matrix minimal |
| Minimal | Single var, single constraint | numVars=1, numConstrs=1, numNonzeros=1 |
| Large | 1M vars, 500K constrs, 10M nonzeros | ~250 MB total memory |
| Infeasible | No feasible solution | solutionData=NULL after optimize, status=INFEASIBLE |
| Unbounded | Objective unbounded | solutionData has ray, status=UNBOUNDED |
| MIP | Mixed-integer program | vtype array has 'B', 'I' entries |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| CxfEnv | Parent structure - provides parameters and logging |
| SparseMatrix | Owned structure - core constraint matrix |
| PendingBuffer | Owned structure - batched modifications |
| SolutionData | Owned structure - optimization results |
| AttrTable | Referenced structure - attribute metadata |

## 14. References

- "Building and Modifying a Model" - Convexfeld documentation
- Sparse matrix formats (CSC/CSR) - standard references
- Mathematical programming problem formulations

## 15. Validation Checklist

Before finalizing:

- [x] All fields documented
- [x] All invariants identified
- [x] Lifecycle complete
- [x] Thread safety analyzed
- [x] No implementation details leaked (no byte offsets)
- [x] Could implement structure from this spec

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
