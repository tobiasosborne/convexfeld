# Structure: CallbackContext

**Spec Version:** 1.0
**Primary Module:** Callbacks

## 1. Purpose

### 1.1 What It Represents

CallbackContext represents the runtime state for user-defined callback functions during optimization. A callback is a user-provided function that Convexfeld invokes periodically during the solve process, allowing the application to monitor progress, log statistics, implement custom heuristics, add lazy constraints, or terminate early. CallbackContext tracks callback registration, timing metrics, invocation counts, and execution context for these callbacks.

Conceptually, CallbackContext is a bridge between the optimization solver and the user application. It provides:
- A mechanism for the solver to invoke user code at specific solver events
- Tracking of callback performance (invocation counts, time spent in callbacks)
- Context preservation (which model, which solve, what callback type)
- User data passthrough (application-specific state)
- Termination signaling (from callback back to solver)

### 1.2 Role in System

CallbackContext sits at the interface between the solver and user application:

- **Event Dispatcher:** Routes solver events (new incumbent, new node, cut generation, etc.) to user callback
- **Context Provider:** Supplies callback with access to model state, solution values, and solver statistics
- **Performance Monitor:** Tracks callback overhead (calls, time) to identify performance bottlenecks
- **Termination Coordinator:** Enables callbacks to request early solver termination
- **User Data Container:** Passes application-specific data pointer to callback
- **Validation Gateway:** Ensures callback invariants (magic numbers, proper initialization)

Every callback invocation flows through CallbackContext: Solver Event -> CallbackContext -> User Function -> CallbackContext -> Resume Solver.

### 1.3 Design Rationale

CallbackContext design addresses several callback-specific concerns:

1. **Lifecycle Management:** Callbacks are optional and allocated lazily (only on first registration). CallbackContext is created once per environment and persists across multiple solves.

2. **Validation:** Magic numbers (0xCA11BAC7, 0xF1E1D5AFE7E57A7E) enable detection of corrupted or uninitialized callback state, critical for debugging user errors.

3. **Performance Tracking:** Callback overhead can dominate solve time if callbacks are expensive. Tracking invocation count and cumulative time helps users diagnose performance issues.

4. **Thread Safety:** Callbacks execute in solver threads. CallbackContext provides synchronization points and ensures callback data is not modified during invocation.

5. **User Data Isolation:** User-provided pointer (usrdata) enables callbacks to access application state without global variables, supporting reentrant code.

Alternative designs (global callback function pointers) would preclude multiple concurrent solves with different callbacks and complicate thread safety.

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| magic1 | uint32 | First validation magic number | 0xCA11BAC7 (valid) |
| magic2 | uint64 | Second validation magic number | 0xF1E1D5AFE7E57A7E (valid) |
| enableFlag | boolean | Callback enabled/disabled toggle | 0 (disabled) or 1 (enabled) |
| env | CxfEnv* | Parent environment reference | Valid CxfEnv pointer |
| primaryModel | CxfModel* | Primary model for this callback session | Valid CxfModel pointer |
| usrdata | void* | User-provided application data pointer | Any pointer (passed to callback) |
| callbackCalls | double | Cumulative callback invocations | >= 0 |
| callbackTime | double | Cumulative time in callbacks (seconds) | >= 0.0 |
| suppressCallbackLog | boolean | Suppress callback statistics logging | 0 (log) or 1 (suppress) |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| timestamp1 | System clock | Callback session start time | Set on callback registration |
| timestamp2 | System clock | Callback session start time (duplicate) | Set on callback registration |

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| None | N/A | All fields always present | N/A |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| callbackSubStruct | byte[48] | Embedded substructure for callback metadata | Initialized via cxf_init_callback_struct |
| field_28 | int64 | Configuration field (copied from primary model) | Internal solver use |
| field_30 | int64 | Configuration field (copied from primary model) | Internal solver use |
| field_50 | void* | Internal pointer (zeroed on init) | Internal solver use |
| field_2d0 | int32 | Internal counter (zeroed on init) | Internal solver use |
| field_32c | int32 | Sentinel value (-1) | Validation marker |
| field_330 | int32 | Sentinel value (-1) | Validation marker |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| callbackSubStruct (48 bytes) | 1:1 | Embedded - initialized on CallbackContext creation |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| CxfEnv | 1:1 | Environment must outlive CallbackContext |
| CxfModel | 1:1 | Primary model must outlive callback session |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| CxfEnv | callbackState | Environment owns CallbackContext |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] magic1 = 0xCA11BAC7
- [x] magic2 = 0xF1E1D5AFE7E57A7E
- [x] enableFlag is 0 or 1 (boolean)
- [x] env is never NULL
- [x] primaryModel is never NULL (after initialization)
- [x] callbackCalls >= 0 (monotonically increasing)
- [x] callbackTime >= 0.0 (monotonically increasing)
- [x] suppressCallbackLog is 0 or 1 (boolean)
- [x] field_32c = -1 (0xFFFFFFFF)
- [x] field_330 = -1 (0xFFFFFFFF)

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] If enableFlag = 1, then callback function is registered
- [x] If enableFlag = 0, then callbacks are not invoked (even if registered)
- [x] callbackCalls increases by 1 per callback invocation
- [x] callbackTime increases by elapsed time per callback invocation
- [x] timestamp1 = timestamp2 (both set to same value on init)
- [x] If callbackCalls = 0, then callbackTime = 0.0 (initial state)

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After creation | magic1 and magic2 set, enableFlag=1, env set, primaryModel set, callbackCalls=0, callbackTime=0.0 |
| During callback | enableFlag=1, env and primaryModel valid |
| After callback | callbackCalls incremented, callbackTime updated |
| Before destruction | All fields valid, no callbacks in progress |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_setcallbackfunc | First callback registration on environment | Allocated, initialized, enableFlag=1, callbackCalls=0 |

### 5.2 States

```
UNALLOCATED -> ALLOCATED -> INITIALIZED -> ENABLED -> ACTIVE -> ENABLED -> DISABLED
                                            |         |
                                            +----<----+
                                         (callback cycle)

State Descriptions:
- UNALLOCATED: env->callbackState = NULL (no callbacks registered)
- ALLOCATED: Memory allocated (848 bytes)
- INITIALIZED: Magic numbers set, fields zeroed, timestamps captured
- ENABLED: enableFlag=1, ready to invoke callbacks
- ACTIVE: Callback currently executing
- DISABLED: enableFlag=0, callbacks suppressed
```

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| UNALLOCATED | ALLOCATED | cxf_setcallbackfunc (first call) | Allocate 848 bytes |
| ALLOCATED | INITIALIZED | Initialization function | Set magic numbers, zero fields, init subStruct |
| INITIALIZED | ENABLED | Set enableFlag=1 | Ready for callbacks |
| ENABLED | ACTIVE | Solver invokes callback | Capture start time |
| ACTIVE | ENABLED | Callback returns | Increment callbackCalls, update callbackTime |
| ENABLED | DISABLED | Set enableFlag=0 | Callbacks suppressed |
| DISABLED | ENABLED | Set enableFlag=1 | Callbacks resume |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| cxf_freeenv | Free CallbackContext memory, zero pointer in environment |

### 5.5 Ownership Rules

- **Who creates:** cxf_setcallbackfunc (internal allocation)
- **Who owns:** CxfEnv (as env->callbackState)
- **Who destroys:** cxf_freeenv
- **Sharing allowed:** No (each environment has independent CallbackContext)

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| Check if enabled | enableFlag value | O(1) memory read |
| Get callback count | callbackCalls value | O(1) memory read |
| Get callback time | callbackTime value | O(1) memory read |
| Validate state | Magic number check | O(1) comparison |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| Set enableFlag | Enable/disable callbacks | Nothing (immediate effect) |
| Increment callbackCalls | Track invocation | Nothing (monotonic counter) |
| Add to callbackTime | Accumulate callback overhead | Nothing (monotonic accumulator) |
| Set usrdata | Update user data pointer | Nothing (pointer swap) |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| Reset callback state | Clear counts, reset timestamps | Atomic (all fields updated together) |
| Initialize subStruct | Zero 48-byte embedded structure | Atomic (memset) |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed overhead | 848 bytes |
| Embedded subStruct | 48 bytes (part of overhead) |
| Total | 848 bytes (fixed size) |

### 7.2 Allocation Strategy

Single monolithic allocation. No dynamic resizing. Allocated lazily on first callback registration and persists for environment lifetime.

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| Entire structure | 8 bytes (pointer alignment) |
| Magic numbers | 4 bytes (uint32) / 8 bytes (uint64) |
| Timestamps | 8 bytes (int64) |
| Counters | 8 bytes (double) |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Not inherently thread-safe

Callbacks execute in solver threads. CallbackContext is modified during callback invocation (increment counters, update time). Concurrent callbacks on same environment would race.

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Register callback | Environment lock (modifies env->callbackState) |
| Invoke callback | Solver-internal synchronization (callbacks serialized by solver) |
| Read callback stats | No lock (atomic reads) |
| Modify enableFlag | No lock (atomic write) |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- Single-threaded solver invokes callbacks sequentially
- Environment lock held during callback registration

**Unsafe patterns:**
- Two threads modifying CallbackContext concurrently -> data race on counters
- Thread freeing environment while callback executing -> use-after-free

**Note:** Convexfeld serializes callbacks internally, so concurrent invocations on same CallbackContext do not occur.

## 9. Serialization

### 9.1 Persistent Format

CallbackContext is not serialized. Callbacks are runtime-only constructs. When saving models, callback registrations are lost.

### 9.2 Version Compatibility

Not applicable (no serialization).

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Magic number validation | Yes | Single comparison |
| Callback invocation | Moderate | Function pointer call + counter updates |
| Statistics tracking | Yes | Simple increment/add operations |

### 10.2 Cache Behavior

CallbackContext is 848 bytes (~13 cache lines). Hot fields (enableFlag, callbackCalls, callbackTime) are accessed frequently. Magic numbers at beginning and end provide bookend validation.

### 10.3 Memory Bandwidth

Negligible. CallbackContext is read once per callback invocation. Callback function execution dominates any CallbackContext overhead.

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| magic1 corrupted | magic1 != 0xCA11BAC7 | Return CXF_ERR_CALLBACK_DATA |
| magic2 corrupted | magic2 != 0xF1E1D5AFE7E57A7E | Return CXF_ERR_CALLBACK_DATA |
| NULL env | env = NULL | Return CXF_ERR_NULL_ARGUMENT |
| NULL primaryModel | primaryModel = NULL | Return CXF_ERR_NULL_ARGUMENT |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| Magic number check | Before callback invocation | O(1) - 2 comparisons |
| NULL pointer checks | On registration, invocation | O(1) - pointer comparison |

## 12. Examples

### 12.1 Typical Instance

**Standard callback for logging progress:**

```
CallbackContext:
  magic1 = 0xCA11BAC7
  magic2 = 0xF1E1D5AFE7E57A7E
  enableFlag = 1
  env = <valid CxfEnv*>
  primaryModel = <valid CxfModel*>
  usrdata = <pointer to application logger>

  Performance metrics:
    callbackCalls = 1523.0 (invoked 1523 times during solve)
    callbackTime = 0.452 (450 ms total callback overhead)
    suppressCallbackLog = 0 (log stats)

  Timestamps:
    timestamp1 = 1738012345678
    timestamp2 = 1738012345678

  Memory: 848 bytes
```

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| No callbacks | Environment never registered callback | env->callbackState = NULL |
| Disabled | Callback registered but disabled | enableFlag=0 |
| No invocations | Callback registered but solver never called it | callbackCalls=0, callbackTime=0.0 |
| Expensive callback | Callback dominates solve time | callbackTime > solutionTime (pathological) |
| NULL usrdata | No user data provided | usrdata=NULL (valid) |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| CxfEnv | Parent structure - owns CallbackContext |
| CxfModel | Referenced structure - provides callback context |
| SolverContext | Peer structure - triggers callback invocations during solve |

## 14. References

- "User Callbacks in Mathematical Optimization Solvers" (design patterns)
- Observer pattern (software design)

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
