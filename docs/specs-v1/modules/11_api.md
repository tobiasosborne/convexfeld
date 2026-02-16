# Module: API (Public Interface)

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 30

## 1. Overview

### 1.1 Purpose

The API module provides Convexfeld's public C interface for optimization modeling and solving. It exposes functions for environment and model lifecycle management, variable and constraint definition, parameter configuration, attribute queries, optimization execution, file I/O, and callback registration. This is the primary integration point for applications using Convexfeld as an optimization engine.

The module implements a layered architecture separating user-facing convenience (wrappers, validation, lazy updates) from internal solver operations. It follows a contract-based design where functions validate inputs thoroughly, maintain consistent error semantics, and provide strong guarantees about state transitions.

### 1.2 Responsibilities

This module is responsible for:

- [ ] Environment creation and cleanup (cxf_emptyenv, cxf_loadenv, cxf_freeenv)
- [ ] Model creation, copying, updating, and destruction (cxf_newmodel, cxf_copymodel, cxf_updatemodel, cxf_freemodel)
- [ ] Variable operations (cxf_addvar, cxf_addvars, cxf_delvars)
- [ ] Linear constraint operations (cxf_addconstr, cxf_addconstrs)
- [ ] Quadratic extensions (cxf_addqpterms, cxf_addqconstr)
- [ ] General constraints (cxf_addgenconstrIndicator, etc.)
- [ ] Matrix modification (cxf_chgcoeffs)
- [ ] Matrix queries (cxf_getconstrs, cxf_getcoeff)
- [ ] Parameter management (cxf_setintparam, cxf_getintparam)
- [ ] Attribute access (cxf_getintattr, cxf_getdblattr)
- [ ] Optimization control (cxf_optimize, cxf_terminate)
- [ ] File I/O (cxf_read, cxf_write)
- [ ] Diagnostics (cxf_version, cxf_geterrormsg)
- [ ] Callback registration (cxf_setcallbackfunc)

This module is NOT responsible for:

- [ ] Core solver algorithms (simplex, barrier, branch-and-bound)
- [ ] Memory allocation policies (delegated to memory module)
- [ ] Internal data structure maintenance (managed by matrix/solver modules)
- [ ] Platform-specific implementations (abstracted by system layer)

### 1.3 Design Philosophy

The API follows several key principles:

**Lazy Update Pattern:** Modification functions queue changes in O(1) time; cxf_updatemodel applies them in batch. This enables efficient model building while maintaining a clean separation between construction and solving phases.

**Defensive Validation:** All public functions validate inputs exhaustively before delegation, ensuring internal modules receive only valid data. This centralizes error handling and simplifies internal code.

**Zero-Copy Queries:** Attribute and matrix query functions return pointers to internal data when safe, avoiding allocation overhead. Copying is only performed when necessary for thread safety or when the public API requires it.

**Strong Error Contracts:** Every function returns meaningful error codes, sets descriptive error messages via cxf_geterrormsg, and leaves the system in a consistent state even on failure.

## 2. Public Interface

### 2.1 Exported Functions

#### Environment Management (3)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_emptyenv | Create uninitialized environment | User code (rare - advanced usage) |
| cxf_loadenv | Create and initialize environment | User code (most common startup) |
| cxf_freeenv | Release environment and resources | User code (cleanup) |

#### Model Lifecycle (4)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_newmodel | Create optimization model | User code (model building) |
| cxf_freemodel | Destroy model and release resources | User code (cleanup) |
| cxf_copymodel | Create independent model duplicate | User code (parallel solving) |
| cxf_updatemodel | Apply pending modifications | User code (before queries/solve) |

#### Variable Operations (3)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_addvar | Add single variable | User code (simple models) |
| cxf_addvars | Add multiple variables (batch) | User code (large models) |
| cxf_delvars | Delete variables by index | User code (model modification) |

#### Linear Constraint Operations (2)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_addconstr | Add single linear constraint | User code (simple models) |
| cxf_addconstrs | Add multiple linear constraints (batch) | User code (large models) |

#### Quadratic Extensions (2)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_addqpterms | Add quadratic objective terms | User code (QP/QCP models) |
| cxf_addqconstr | Add quadratic constraint | User code (QCP models) |

#### General Constraints (1+)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_addgenconstrIndicator | Add indicator constraint | User code (MIP with logic) |

#### Matrix Operations (2)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_chgcoeffs | Batch modify matrix coefficients | User code (model modification) |
| cxf_getconstrs | Query constraint rows (CSR) | User code (model inspection) |
| cxf_getcoeff | Query single matrix coefficient | User code (model inspection) |

#### Parameter Management (2)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_setintparam | Set integer parameter | User code (configuration) |
| cxf_getintparam | Get integer parameter value | User code (inspection) |

#### Attribute Access (2)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_getintattr | Get integer attribute | User code (query results) |
| cxf_getdblattr | Get double attribute | User code (query results) |

#### Optimization Control (3)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_optimize | Solve optimization problem | User code (primary operation) |
| cxf_optimize_internal | Internal dispatcher | cxf_optimize wrapper |
| cxf_terminate | Request early termination | User code / signal handlers |

#### File I/O (2)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_read | Import data from files | User code (loading models/solutions) |
| cxf_write | Export models and data | User code (saving models/solutions) |

#### Diagnostics (2)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_version | Get library version | User code (diagnostics) |
| cxf_geterrormsg | Get error message text | User code (error handling) |

#### Callbacks (1)
| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_setcallbackfunc | Register user callback | User code (monitoring/control) |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| CxfEnv | Opaque environment handle |
| CxfModel | Opaque model handle |
| CXF_DoubleParam | Parameter name constants |
| CXF_IntParam | Parameter name constants |
| CXF_StringParam | Parameter name constants |
| CXF_IntAttr | Attribute name constants |
| CXF_DoubleAttr | Attribute name constants |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| CXF_OK | 0 | Success code |
| CXF_ERR_OUT_OF_MEMORY | 1001 | Allocation failed |
| CXF_ERR_NULL_ARGUMENT | 1002 | Required pointer is NULL |
| CXF_ERR_INVALID_ARGUMENT | 1003 | Invalid parameter value |
| CXF_INFINITY | 1e100 | Unbounded value representation |
| CXF_UNDEFINED | 1e101 | Undefined attribute value |
| CXF_CONTINUOUS | 'C' | Variable type: continuous |
| CXF_BINARY | 'B' | Variable type: binary |
| CXF_INTEGER | 'I' | Variable type: integer |
| CXF_LESS_EQUAL | '<' | Constraint sense: <= |
| CXF_EQUAL | '=' | Constraint sense: = |
| CXF_GREATER_EQUAL | '>' | Constraint sense: >= |

## 3. Internal Functions

### 3.1 Private Functions

| Function | Purpose |
|----------|---------|
| cxf_optimize_internal | Algorithm dispatch and setup |
| Internal validation helpers | Parameter/index validation |
| Internal update helpers | Apply pending changes |

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| ValidateModel | Check model validity | All model functions |
| ValidateEnvironment | Check environment validity | All environment functions |
| CheckModificationAllowed | Verify model not locked | Modification functions |
| NormalizeParameterName | Case-insensitive parameter lookup | Parameter functions |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| Environment pool | Global | Program lifetime | Safe (internal locking) |
| Model registry | Per-environment | Environment lifetime | Safe (locked by environment) |
| Parameter table | Per-environment | Environment lifetime | Conditional (read-safe, write-locked) |
| Attribute table | Static | Program lifetime | Safe (read-only) |
| Pending buffer | Per-model | Model lifetime | Unsafe (single-threaded) |

### 4.2 State Lifecycle

```
Environment:
  NOT_INITIALIZED
      ↓ cxf_emptyenv / cxf_loadenv
  CREATED (active=0 or active=1)
      ↓ cxf_freeenv
  DESTROYED

Model:
  NOT_CREATED
      ↓ cxf_newmodel
  CREATED (initialized=0, pending changes in buffer)
      ↓ cxf_updatemodel / cxf_optimize
  UPDATED (initialized=1, matrix committed)
      ↓ cxf_optimize
  OPTIMIZED (solution available)
      ↓ Modification functions
  MODIFIED (initialized=0, pending changes)
      ↓ cxf_freemodel
  DESTROYED
```

### 4.3 State Invariants

At all times, the following must be true:

- [ ] Freed models have magic number 0
- [ ] Models always reference a valid parent environment
- [ ] Environments are freed AFTER all their models
- [ ] Pending buffer is consistent (all indices valid, no duplicates)
- [ ] Matrix data matches numVars/numConstrs counts
- [ ] Solution data is only valid when solStatus indicates optimization completed

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_malloc, cxf_free, cxf_calloc, cxf_realloc | Allocate structures, arrays |
| Validation | Input validation helpers | Parameter range checking |
| Error/Logging | Error message setting, logging | User diagnostics |
| Matrix | CSC/CSR operations | Sparse matrix management |
| Optimization | Simplex, barrier, MIP solvers | Core algorithms |
| Threading | Environment locks | Thread safety |
| Callbacks | Callback infrastructure | User extensibility |
| Parameters | Parameter storage/lookup | Configuration |
| Attributes | Attribute storage/lookup | Result queries |
| File I/O | Format readers/writers | Import/export |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Validation, Error/Logging, Threading (core infrastructure)
- **Before:** User code (public API)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| Compression libraries | System libraries | File I/O (.gz, .bz2, .zip, .7z) |
| Math library | System library | Mathematical operations |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| User applications | All public functions | Stable - public API contract |
| Language bindings | C API | Stable - ABI compatibility required |
| Test suites | All functions | Stable - regression testing |

### 6.2 API Stability

The following interfaces are stable and must not change:

- [ ] Function signatures (C ABI contract)
- [ ] Error code values (documented behavior)
- [ ] Magic numbers (binary compatibility)
- [ ] Structure offsets used by language bindings
- [ ] Attribute/parameter name strings

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- [ ] All functions validate inputs before modifying state
- [ ] Error codes are consistent and documented
- [ ] Error messages are set on all failures
- [ ] State transitions are atomic (all or nothing)
- [ ] Freed resources never accessed (use-after-free prevention)
- [ ] Thread safety guarantees documented per function
- [ ] Lazy update semantics preserved (modifications are queued)

### 7.2 Required Invariants

What this module requires from others:

- [ ] Memory allocator returns NULL on failure (never crashes)
- [ ] Solver modules respect termination flags
- [ ] Parameter tables remain immutable during queries
- [ ] Lock primitives provide mutual exclusion

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| NULL pointers | Explicit NULL checks at function entry |
| Invalid indices | Bounds validation against numVars/numConstrs |
| Invalid parameter values | Range checks, NaN/Inf detection |
| Memory allocation failure | NULL return from allocator |
| Invalid model state | Magic number validation |

### 8.2 Error Propagation

How errors flow through this module:

```
User calls API function
    ↓
Validate inputs (return error code if invalid)
    ↓
Acquire locks (if needed)
    ↓
Delegate to internal function
    ↓
If error: set error message, release locks, return error code
    ↓
If success: update state, release locks, return 0
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Allocation failure | Return OUT_OF_MEMORY, leave state unchanged |
| Invalid input | Return appropriate error code, no state change |
| Partial failure | Roll back changes, return error code |

## 9. Thread Safety

### 9.1 Concurrency Model

**Per-environment safety:** Multiple threads can create/modify different models within the same environment (internal locking protects environment state).

**Per-model exclusivity:** Each model should be accessed by only one thread at a time. Concurrent access to the same model requires external synchronization.

**Read-only queries:** Multiple threads can query model attributes concurrently (read-safe).

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| Environment lock | Model creation, parameter changes | Coarse (entire operation) |
| Solve lock | Optimization execution | Coarse (entire solve) |
| Copy lock | Model copying | Coarse (entire copy) |

### 9.3 Thread Safety Guarantees

- [ ] cxf_newmodel is thread-safe (acquires environment lock)
- [ ] cxf_freemodel is conditionally safe (model must not be in use)
- [ ] cxf_copymodel is thread-safe (acquires copy lock)
- [ ] cxf_updatemodel is conditionally safe (requires exclusive model access)
- [ ] cxf_optimize is conditionally safe (acquires solve lock)
- [ ] cxf_terminate is unconditionally safe (atomic flag writes)
- [ ] Parameter get/set are conditionally safe (one writer at a time)
- [ ] Attribute queries are conditionally safe (safe for concurrent reads)
- [ ] File I/O is thread-safe (acquires environment lock)

### 9.4 Known Race Conditions

- Concurrent modification of the same model without external locking
- Querying attributes while model is being modified
- Accessing model after cxf_freemodel from another thread

## 10. Performance Characteristics

### 10.1 Complexity Summary

#### Environment/Model Management
| Operation | Time | Space |
|-----------|------|-------|
| Create environment | O(1) | O(1) - ~2 KB |
| Create empty model | O(1) | O(1) - ~1 KB |
| Create model with n vars | O(n) | O(n) - ~44n bytes |
| Free model | O(m + n + nnz) | O(1) |
| Copy model | O(m + n + nnz) | O(m + n + nnz) |
| Update model (no changes) | O(1) | O(1) |
| Update model (structural) | O(m + n + nnz) | O(m + n + nnz) |

#### Modification Operations
| Operation | Time | Space |
|-----------|------|-------|
| Add variable | O(1) | O(1) - queued |
| Add variables (batch) | O(k) | O(k) - queued |
| Add constraint | O(1) | O(1) - queued |
| Add constraints (batch) | O(nnz) | O(nnz) - queued |
| Change coefficients | O(k) | O(k) - queued |

#### Query Operations
| Operation | Time | Space |
|-----------|------|-------|
| Get parameter | O(log p) | O(1) |
| Set parameter | O(log p) | O(1) |
| Get scalar attribute | O(log a) | O(1) |
| Get constraint row | O(nnz_row) | O(1) - zero-copy |
| Get coefficient | O(nnz_col) | O(1) |

Where:
- n = variables, m = constraints, nnz = nonzeros
- k = number of modifications
- p = number of parameters, a = number of attributes

### 10.2 Hot Paths

Performance-critical frequently-called functions:

- **Modification functions during model building:** Designed for O(1) queuing
- **Attribute queries during and after optimization:** Optimized with direct pointers
- **cxf_check_terminate during solver loops:** ~10-20 cycles

Not performance-critical:
- Environment/model creation (one-time)
- File I/O (disk-bound)
- Error message formatting (error path only)

### 10.3 Memory Usage

Typical memory consumption for a model with m constraints, n variables, nnz nonzeros:

```
Base model structure:      ~1 KB
Variable data:             n × 32 bytes
Constraint data:           m × 16 bytes
CSC matrix:                nnz × 12 bytes
CSC structure:             n × 12 bytes
Pending buffer:            ~344 bytes (reusable)
Names (optional):          Variable (string storage)

Total: ~1.4 KB + 44n + 16m + 12nnz bytes (plus names)
```

Example: 10,000 variables, 5,000 constraints, 50,000 nonzeros ≈ 1.5 MB

## 11. Function Index

Complete list of functions in this module organized by category:

### Environment Management
1. [cxf_emptyenv](../functions/api/cxf_emptyenv.md) - Create uninitialized environment
2. [cxf_loadenv](../functions/api/cxf_loadenv.md) - Create and initialize environment
3. [cxf_freeenv](../functions/api/cxf_freeenv.md) - Free environment

### Model Lifecycle
4. [cxf_newmodel](../functions/api/cxf_newmodel.md) - Create optimization model
5. [cxf_freemodel](../functions/api/cxf_freemodel.md) - Free model
6. [cxf_copymodel](../functions/api/cxf_copymodel.md) - Duplicate model
7. [cxf_updatemodel](../functions/api/cxf_updatemodel.md) - Apply pending modifications

### Variables
8. [cxf_addvar](../functions/api/cxf_addvar.md) - Add single variable
9. [cxf_addvars](../functions/api/cxf_addvars.md) - Add variables (batch)
10. [cxf_delvars](../functions/api/cxf_delvars.md) - Delete variables

### Linear Constraints
11. [cxf_addconstr](../functions/api/cxf_addconstr.md) - Add single constraint
12. [cxf_addconstrs](../functions/api/cxf_addconstrs.md) - Add constraints (batch)

### Quadratic Extensions
13. [cxf_addqpterms](../functions/api/cxf_addqpterms.md) - Add quadratic objective terms
14. [cxf_addqconstr](../functions/api/cxf_addqconstr.md) - Add quadratic constraint

### General Constraints
15. [cxf_addgenconstrIndicator](../functions/api/cxf_addgenconstrIndicator.md) - Add indicator constraint

### Matrix Operations
16. [cxf_chgcoeffs](../functions/api/cxf_chgcoeffs.md) - Modify matrix coefficients
17. [cxf_getconstrs](../functions/api/cxf_getconstrs.md) - Query constraint rows
18. [cxf_getcoeff](../functions/api/cxf_getcoeff.md) - Query single coefficient

### Parameters
19. [cxf_setintparam](../functions/api/cxf_setintparam.md) - Set integer parameter
20. [cxf_getintparam](../functions/api/cxf_getintparam.md) - Get integer parameter

### Attributes
21. [cxf_getintattr](../functions/api/cxf_getintattr.md) - Get integer attribute
22. [cxf_getdblattr](../functions/api/cxf_getdblattr.md) - Get double attribute

### Optimization
23. [cxf_optimize](../functions/api/cxf_optimize.md) - Solve optimization problem
24. [cxf_optimize_internal](../functions/api/cxf_optimize_internal.md) - Internal dispatcher
25. [cxf_terminate](../functions/api/cxf_terminate.md) - Request early termination

### File I/O
26. [cxf_read](../functions/api/cxf_read.md) - Import data from files
27. [cxf_write](../functions/api/cxf_write.md) - Export models/data

### Diagnostics
28. [cxf_version](../functions/api/cxf_version.md) - Get library version
29. [cxf_geterrormsg](../functions/api/cxf_geterrormsg.md) - Get error message

### Callbacks
30. [cxf_setcallbackfunc](../functions/api/cxf_setcallbackfunc.md) - Register callback

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Lazy update pattern | O(1) per modification, O(m+n+nnz) single rebuild | Immediate update (rejected: too slow) |
| Opaque handles | Encapsulation, ABI stability | Exposed structures (rejected: fragile ABI) |
| Zero-copy queries | Avoids allocation overhead | Always copy (rejected: wasteful) |
| Deep copy semantics | Independent models for parallel solving | Shallow copy (rejected: shared state issues) |
| Table-driven attributes | Extensibility, metadata-driven validation | Switch statements (rejected: less flexible) |
| Magic number validation | Use-after-free detection | No validation (rejected: unsafe) |

### 12.2 Known Limitations

- [ ] Maximum 2 billion nonzeros (32-bit indices)
- [ ] Single optimization per model at a time (no concurrent solves on same model)
- [ ] Pending changes not visible to queries until update
- [ ] cxf_copymodel does not copy pending changes
- [ ] No transaction rollback for partial failures
- [ ] Parameter changes may require re-optimization

### 12.3 Future Improvements

- [ ] Support 64-bit indices for >2B nonzeros
- [ ] Incremental update for small modifications
- [ ] Transaction semantics for batch operations
- [ ] Copy-on-write for efficient model variants
- [ ] Asynchronous optimization API
- [ ] Streaming file I/O for huge models

## 13. References

- Convexfeld Performance Tuning: Lazy Update Mechanism
- Sparse Matrix Storage Formats (CSC/CSR) - Y. Saad (2003)
- "Linear Programming and Extensions" - Dantzig (1963)

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear

---

*Reviewed by: Pending*
*Function coverage: 30/30 (100%)*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
