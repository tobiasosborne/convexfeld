# Structure: CxfEnv

**Spec Version:** 1.0
**Primary Module:** Environment Management

## 1. Purpose

### 1.1 What It Represents

The CxfEnv structure represents a Convexfeld optimization environment - the root context for all solver operations. It encapsulates solver configuration parameters, logging facilities, and global resources required for optimization. An environment serves as a factory for creating optimization models and provides the execution context in which those models operate.

Conceptually, a CxfEnv is analogous to a database connection or a graphics context: it establishes a session with the Convexfeld solver, maintains configuration state, and manages system resources (threads, memory pools). All optimization models must be associated with an environment, and multiple models can share a single environment to inherit common configuration.

### 1.2 Role in System

The environment sits at the top of the Convexfeld object hierarchy and serves as the foundation for all solver operations:

- **Configuration Container:** Stores ~200+ solver parameters (tolerances, time limits, algorithm choices, logging verbosity)
- **Resource Manager:** Manages thread pools and memory allocators
- **Logging Hub:** Centralizes message output, error reporting, and debug logging
- **Model Factory:** Creates and tracks optimization models
- **Session Context:** Maintains solve session state for callbacks, termination, and progress tracking

Every public API function accepts either a CxfEnv or CxfModel parameter, where models internally reference their parent environment.

### 1.3 Design Rationale

The environment abstraction decouples global solver configuration from individual model data, enabling:

1. **Configuration Reuse:** Set parameters once, create many models
2. **Multi-model Coordination:** Share resources (thread pools) across models
3. **Session Isolation:** Independent environments for concurrent solves without interference
4. **Hierarchical Configuration:** Support parent/child environments with parameter inheritance

Alternative designs (e.g., global singleton solver state) would preclude concurrent solves with different configurations.

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| active | boolean | Initialization status | 0 (inactive) or 1 (active) |
| verbosity | int | Logging level | 0=silent, 1=normal, 2+=verbose |
| outputFlag | boolean | Master output control | 0=suppress, 1=enable |
| errorBuffer | string | Last error message | Max 256 chars |
| refCount | int | Reference counter for environment lifetime | >= 0 |
| version | int | Configuration version counter | Incremented on param changes |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| infinityThreshold | Solver constant | Cached CXF_INFINITY value (1e100) | Static after initialization |
| sessionRef | Session counter | Tracks optimization sessions | Incremented per optimize call |
| sessionId | Session identifier | Unique ID for current session | Set at session start |

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| masterEnv | CxfEnv* | Child/copy environment | NULL (for root env) |
| asyncState | AsyncState* | Async optimization enabled | NULL (synchronous mode) |
| resultFile | string | ResultFile parameter set | NULL (no output file) |
| logFile | string | LogFile parameter set | NULL (stdout logging) |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| criticalSection | Lock* | Thread synchronization for environment access |
| errorBufLocked | boolean | Prevents error buffer overwrites during nested errors |
| paramTablePtr | ParamTable* | Parameter name-to-value lookup table |
| paramStorage | void* | Base address for parameter value storage |
| paramFlagsArray | uint32[]* | Flags for each parameter (modified, hidden, etc.) |
| messageBuffer | void* | Buffered log/output message storage |
| modelManager | ModelManager* | Tracks all models created in this environment |
| callbackState | CallbackContext* | Callback registration and tracking |
| terminateFlag | boolean | Signal for early termination |
| optimizing | boolean | Indicates solve in progress |
| anonymousMode | boolean | Suppress variable/constraint name tracking |
| fingerprintMode | int | Model fingerprint calculation mode |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| ParamTable | 1:1 | Exclusive - created on env init, freed on env destruction |
| CallbackContext | 0:1 | Exclusive - created on first callback registration, freed on env destruction |
| ModelManager | 0:1 | Exclusive - created on first model creation, freed on env destruction |
| AsyncState | 0:1 | Exclusive - created when enabling async optimization |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| CxfModel | 1:N | Models must be freed before environment |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| CxfEnv (master) | Child environments via copy API | Hierarchical: child inherits parent params |
| None (root environment) | N/A | Top-level structure |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] active flag is 0 or 1 (boolean)
- [x] refCount >= 0
- [x] If active = 1, then environment is initialized and ready
- [x] If active = 0, then environment is unusable (pre-init or post-free)
- [x] criticalSection is never NULL after initialization
- [x] errorBuffer is always a valid writable buffer
- [x] paramTablePtr is never NULL after initialization
- [x] version counter is monotonically increasing

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] If masterEnv != NULL, then this is a child environment
- [x] If optimizing = 1, then active = 1
- [x] If callbackState != NULL, then at least one callback is registered
- [x] If errorBufLocked = 1, then a nested error is being handled
- [x] If outputFlag = 0, then no messages are printed (unless error)

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After creation | active = 0, refCount = 1, all resources allocated |
| After initialization | active = 1 |
| During optimization | optimizing = 1, sessionRef incremented |
| Before destruction | refCount = 0, all models freed |
| After destruction | active = 0, all owned structures freed |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_loadenv | Standard: create and activate in one call | active=1, params=defaults |
| cxf_emptyenv | Advanced: create inactive, activate later | active=0, params=defaults |
| cxf_copyenv | Copy existing environment | Inherits parent params, refCount=1 |
| cxf_getconcurrentenv | Get concurrent sub-environment for parallel solves | Shares parent resources |

### 5.2 States

```
UNINITIALIZED -> INACTIVE -> ACTIVE -> OPTIMIZING -> ACTIVE -> TERMINATED
                   ^                      |
                   +---------<-----------+
                         (reusable)

State Descriptions:
- UNINITIALIZED: Memory allocated but not initialized
- INACTIVE: Initialized but not yet activated (active=0)
- ACTIVE: Initialized and ready (active=1, optimizing=0)
- OPTIMIZING: Solve in progress (active=1, optimizing=1)
- TERMINATED: Freed (active=0, memory deallocated)
```

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| UNINITIALIZED | INACTIVE | cxf_emptyenv call | Allocate resources, init params, active=0 |
| UNINITIALIZED | ACTIVE | cxf_loadenv call | Allocate, init, active=1 |
| INACTIVE | ACTIVE | cxf_startenv call | Finalize initialization, active=1 |
| ACTIVE | OPTIMIZING | cxf_optimize call | Set optimizing=1, increment sessionRef |
| OPTIMIZING | ACTIVE | Solve completion | Set optimizing=0, store results |
| OPTIMIZING | ACTIVE | Termination request | Set terminateFlag, early exit |
| ACTIVE | TERMINATED | cxf_freeenv call | Free models, resources, set active=0 |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| cxf_freeenv | Decrement refCount; if 0: free all models, free params/callbacks/resources, zero memory |

### 5.5 Ownership Rules

- **Who creates:** Application via cxf_loadenv, cxf_emptyenv, or internal via cxf_copyenv
- **Who owns:** Application (root env) or parent environment (child env)
- **Who destroys:** Application via cxf_freeenv, or automatic when refCount reaches 0
- **Sharing allowed:** Yes (via refCount), but caller must ensure thread-safe access

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| cxf_getintparam | Parameter integer value | O(1) - hash table lookup |
| cxf_getdblparam | Parameter double value | O(1) - hash table lookup |
| cxf_getstrparam | Parameter string value | O(1) - hash table lookup |
| cxf_geterrormsg | Last error message | O(1) - buffer read |
| cxf_getenv (from model) | Environment reference | O(1) - dereference |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| cxf_setintparam | Update parameter value | Increments version counter |
| cxf_setdblparam | Update parameter value | Increments version counter |
| cxf_setstrparam | Update parameter value | Increments version counter |
| cxf_resetparams | Reset all to defaults | Increments version counter |
| cxf_setcallbackfunc | Register callback | Allocates callbackState if needed |
| cxf_terminate | Set termination flag | Signals early exit to optimizing solver |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| cxf_readparams | Load parameters from file | Partial - stops on first error |
| cxf_writeparams | Save parameters to file | All-or-nothing (file I/O) |
| cxf_startenv | Finalize initialization | All-or-nothing |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed overhead | ~10.5 KB |
| Parameter storage | ~2 KB (depends on param count) |
| Per-callback overhead | 848 bytes (CallbackContext) |
| Per-model tracking | 16 bytes per model (ModelManager) |
| Total | ~12-15 KB base + dynamic allocations |

### 7.2 Allocation Strategy

Single monolithic allocation for the main structure (~10.5 KB). Parameter table, callback state, and model manager are allocated lazily on first use. Async state allocated only when that feature is enabled.

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| Entire structure | 8 bytes (pointer alignment) |
| Parameter values | 8 bytes (double alignment) |
| Critical section | Platform-dependent (typically 8 bytes) |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Fully thread-safe with external synchronization

The structure itself provides a critical section lock, but callers must explicitly acquire it for safe concurrent access.

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Read parameters | criticalSection (shared read lock if available, exclusive otherwise) |
| Write parameters | criticalSection (exclusive) |
| Create model | criticalSection (exclusive - updates modelManager) |
| Free environment | criticalSection (exclusive) |
| Query error message | None (read-only, atomic) |
| Set termination flag | None (atomic write) |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- Multiple threads reading different parameters (with lock)
- Multiple threads creating models on same env (serialized by lock)
- One thread optimizing while another reads params (with lock)

**Unsafe patterns:**
- Two threads modifying parameters without lock -> data race
- Thread destroying env while another is optimizing -> use-after-free
- Reading active flag without lock for critical decisions -> race condition (acceptable for fast-path checks)

## 9. Serialization

### 9.1 Persistent Format

The environment itself is not directly serializable. Only parameter values can be saved/loaded:

- **Format:** Plain text key-value pairs (INI-like format)
- **File extension:** `.prm`
- **Example:**
  ```
  TimeLimit 100.0
  MIPGap 0.01
  Threads 4
  ```

### 9.2 Version Compatibility

| Version | Compatible | Migration |
|---------|------------|-----------|
| 11.x | Yes | Load 11.x params into 12.x (may warn on deprecated) |
| 10.x | Partial | Some params renamed/removed, requires manual editing |
| 9.x | No | Significant param changes, not recommended |

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Parameter reads | Yes | Hash table O(1) lookup, cached addresses |
| Parameter writes | Moderate | Hash lookup + version increment |
| Error message reads | Yes | Direct buffer access |
| Lock acquisition | Moderate | System critical section (OS-dependent) |

### 10.2 Cache Behavior

Environment structure is large (~10 KB) but most accesses are to hot fields:
- `active`: Very hot, likely in L1 cache
- `errorBuffer`: Hot during error paths
- `criticalSection`: Hot during concurrent access
- Parameter storage: Warm (accessed during solve setup)

### 10.3 Memory Bandwidth

Environment is read-heavy (parameters queried frequently during solve setup) but write-light (parameters typically set once at startup). The large structure size is acceptable because most of it is cold data (unused parameters).

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| active = 0 | cxf_checkenv call | Return CXF_ERR_NOT_IN_MODEL |
| NULL pointer | Null check in API | Return CXF_ERR_NULL_ARGUMENT |
| Concurrent free | refCount < 0 | Assertion failure (debug builds) |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| active flag check | Every API call | O(1) - single memory read |
| refCount check | On free | O(1) - integer comparison |
| Lock state check | Debug builds only | O(1) - assert |

## 12. Examples

### 12.1 Typical Instance

**Standard LP solve with custom parameters:**

```
Environment:
  active = 1
  verbosity = 1
  outputFlag = 1
  refCount = 1
  version = 3 (3 param changes)

  Parameters:
    Method = 1 (dual simplex)
    TimeLimit = 300.0
    Threads = 8
    FeasibilityTol = 1e-6
    OptimalityTol = 1e-6

  State:
    modelManager: 1 model tracked
    callbackState: NULL (no callbacks)
    optimizing: 0 (ready)
    sessionRef: 0
```

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| Empty | Environment created but not activated | active=0, all params at defaults |
| Minimal | Single active env, no models | active=1, modelManager=NULL |
| Large | Env with 100+ models, callbacks, async | active=1, large modelManager, callbackState allocated |
| Concurrent | Parent env with 4 concurrent children | 5 env structures, parent with concurrent array |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| CxfModel | Child structure - every model references parent environment |
| CallbackContext | Owned structure - tracks callback registrations |
| ParamTable | Owned structure - parameter name-to-value mapping |
| ModelManager | Owned structure - tracks all models in this environment |

## 14. References

- "Design Patterns for Mathematical Optimization Software" (theoretical background)
- Thread safety patterns in C APIs
- Reference counting for resource management

## 15. Validation Checklist

Before finalizing:

- [x] All fields documented
- [x] All invariants identified
- [x] Lifecycle complete
- [x] Thread safety analyzed
- [x] No implementation details leaked (no byte offsets, no memory addresses)
- [x] Could implement structure from this spec

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
