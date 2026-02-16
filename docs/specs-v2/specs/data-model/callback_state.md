# CallbackState

## Purpose

CallbackState is the runtime data structure that manages the user callback system for optimization. It holds the registration state, synchronization primitives, timing information, and control flags needed to support user-defined callbacks during solver execution. A single CallbackState instance is lazily allocated within the Environment the first time any callback is registered (either a log callback or an optimization callback), and is shared across both callback types. It provides the mechanism by which users can monitor optimization progress, receive log messages programmatically, and request early termination of the solver. The structure also supports callback inheritance when child environments or model copies are created from a parent that has active callbacks.

## Fields

### Validation

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| validationTag1 | int | Primary sentinel value for detecting memory corruption or use-after-free | Implementation-defined constant | Set immediately upon allocation; cleared on destruction |
| validationTag2 | int64 | Secondary sentinel value providing defense-in-depth validation | Implementation-defined constant | Set during initialization; cleared on destruction |

### Synchronization

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| mutex | pointer-to-Mutex | Mutex protecting concurrent access to callback invocation and state modification | Non-null after initialization | Allocated during first callback registration; destroyed during environment cleanup |

### Back-References

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| environment | pointer-to-Environment | Back-pointer to the owning environment for accessing solver parameters and error reporting | Non-null after initialization | Set once during initialization; read-only thereafter |
| primaryModel | pointer-to-Model | Reference to the model that originally registered the callback, used for callback context | Valid model pointer or null | Null when no model-specific callback is registered; set during callback registration |
| parentCallbackState | pointer-to-CallbackState | Link to the parent environment's CallbackState when this is a child or copied callback state | Valid pointer or null | Non-null only for inherited callback states; used for configuration sharing |

### User Callback Registration

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| userData | pointer | Opaque user-provided data pointer passed to the callback function on every invocation | Any pointer value, including null | Set during callback registration; passed through unchanged to user callback |

### Timing

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| registrationTimestamp | int64 | Wall-clock timestamp recorded when the callback was first registered, used as the time origin for elapsed time reporting | Non-negative | Set once during initialization; may be overridden when inheriting from a parent |
| baselineTimestamp | int64 | Secondary timestamp used as a baseline for computing elapsed time intervals during callback invocations | Non-negative | Typically initialized to the same value as registrationTimestamp; may diverge during inheritance |
| callbackInvocationCount | double | Cumulative count of how many times the user callback has been invoked | >= 0.0 | Monotonically increasing; reset to zero on initialization |
| callbackCumulativeTime | double | Cumulative wall-clock time in seconds spent executing user callback code | >= 0.0 | Monotonically increasing; reset to zero on initialization |

### Configuration

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| configField1 | int64 | Configuration parameter inherited from the parent model or environment during callback setup | Any int64 | Copied from parent during inheritance; zero when not inherited |
| configField2 | int64 | Secondary configuration parameter inherited from the parent model or environment | Any int64 | Copied from parent during inheritance; zero when not inherited |

### Lifecycle Control

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| enabled | bool | Master enable flag controlling whether callbacks are invoked | true or false | Set to true on initialization; can be toggled to suppress callbacks without unregistering |
| suppressStatisticsLog | bool | When true, suppresses logging of callback performance statistics (invocation count, cumulative time) at solve completion | true or false | Set during callback registration; useful for benchmarking without callback overhead logging |

### Sentinel Guards

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| sentinel1 | int | Guard value set to a distinguished constant to detect buffer overruns from adjacent fields | -1 (sentinel) | Must always equal -1; any other value indicates memory corruption |
| sentinel2 | int | Second guard value for defense-in-depth corruption detection | -1 (sentinel) | Must always equal -1; any other value indicates memory corruption |

## Callback Event Types

The callback system supports distinct event types that indicate the solver phase during which the callback is invoked. These correspond to the standard callback "where" codes documented in commercial LP solver APIs:

| Event Type | Description |
|------------|-------------|
| POLLING | Periodic polling during long operations |
| PRESOLVE | During presolve phase |
| SIMPLEX | During simplex iterations |
| BARRIER | During barrier (interior point) iterations |
| MESSAGE | When a log message is generated |

The active event type is communicated to the user callback function as a parameter, not stored persistently in the CallbackState. The user callback function signature receives the event type, the CallbackState reference (for querying progress data), and the user data pointer.

## Relationships

- **Owned by** the Environment. The Environment holds a pointer to CallbackState and is responsible for its allocation and deallocation.
- **Borrows** the parent Model reference (primaryModel). The CallbackState does not own or control the model's lifetime.
- **Optionally references** a parent CallbackState for configuration inheritance when child environments are created.
- **Owns** the mutex, which is allocated as part of callback initialization and must be destroyed during cleanup.
- The log callback function pointer and its user data are stored **in the Environment itself**, not in the CallbackState. Both callback types share the same CallbackState for synchronization and timing, but the function pointers reside at the Environment level.

**Ownership semantics:**
- The Environment **owns** the CallbackState (responsible for allocation and deallocation).
- The CallbackState **owns** its mutex.
- The CallbackState **borrows** references to the Environment, the primary Model, and any parent CallbackState.

## Lifecycle

### Creation

1. **Lazy allocation:** CallbackState is not allocated when the Environment is created. It is allocated on-demand the first time a callback is registered (either a log callback or an optimization callback). This avoids the overhead of the structure for environments that never use callbacks.
2. **Zero-initialized allocation:** The structure is allocated using zero-initialized memory allocation to ensure all pointer fields start as null and all counters start at zero.
3. **Validation tag setup:** The primary and secondary validation tags are written immediately after allocation to enable corruption detection.
4. **Mutex initialization:** A mutex is allocated and initialized for thread-safe callback invocation. The mutex pointer is stored in the CallbackState.
5. **Timestamp recording:** The current wall-clock time is recorded in both the registration timestamp and baseline timestamp fields.
6. **Enable flag:** The enabled flag is set to true.
7. **Sentinel initialization:** Both sentinel guard values are set to -1.
8. **Environment back-pointer:** The environment back-pointer is stored.
9. **Statistics zeroed:** Invocation count and cumulative time are initialized to zero.

If the allocation fails at step 2, the function returns an out-of-memory error and the Environment's callback state pointer remains null.

### Mutation

- **Callback registration:** When a user registers a new callback (or changes the registered callback), the user data pointer, suppress flag, and optionally the primary model reference are updated. If a model is provided, configuration fields and timestamps may be inherited from the model's environment's CallbackState.
- **Callback invocation:** Each time the solver invokes the user callback, the invocation count is incremented and the cumulative time is updated with the elapsed time of the callback execution. These updates are performed under the mutex.
- **Inheritance/copy:** When a child environment is created from a parent with an active CallbackState, a new CallbackState is allocated for the child. The child inherits the user data, suppress flag, timestamps, and configuration fields from the parent. The child's parentCallbackState pointer is set to the parent's CallbackState.
- **Enable/disable toggling:** The enabled flag can be set to false to suppress callback invocations without unregistering the callback.

### Destruction

1. The mutex is destroyed and its memory is freed.
2. The validation tags are cleared to zero, invalidating the structure for use-after-free detection.
3. The CallbackState memory is freed.
4. The Environment's pointer to the CallbackState is set to null.

For environments connected to a remote solver, additional cleanup is required before the CallbackState can be freed:
- If an optimization is in progress on the remote server, the solver attempts to terminate it and waits for the remote operation to complete (with a bounded polling loop).
- A disconnect message is sent to the remote server.
- The response is processed, and any remote error conditions are reported through the environment's error handling system.
- After remote cleanup is complete, the local CallbackState is freed as described above.

## Invariants

1. **Validation tag integrity:** A valid CallbackState always has both validation tags set to their expected sentinel values. Zero validation tags indicate a freed or invalid structure.
2. **Mutex availability:** The mutex pointer is non-null for any valid CallbackState. All callback invocations and state modifications must acquire this mutex.
3. **Sentinel guard integrity:** Both sentinel guard values must equal -1. Any other value indicates memory corruption.
4. **Environment back-pointer consistency:** The environment field is non-null and points to the Environment that owns this CallbackState.
5. **Timing monotonicity:** The callbackInvocationCount and callbackCumulativeTime fields are monotonically non-decreasing during the lifetime of the structure.
6. **Lazy allocation:** If the Environment's pointer to CallbackState is null, no callback has been registered and no callback invocations will occur.
7. **Shared state:** The same CallbackState instance is used for both log callbacks and optimization callbacks within a single Environment. The log callback function pointer resides in the Environment, not in the CallbackState.

## Thread Safety

CallbackState is designed to be accessed from multiple threads during parallel optimization.

**Protected by the mutex:**
- All callback invocations are serialized through the mutex. When the solver invokes a user callback, it acquires the mutex before calling the user function and releases it afterward. This prevents concurrent callback invocations from different solver threads from interleaving.
- Updates to timing statistics (invocation count, cumulative time) are performed under the mutex.
- Callback registration changes (user data, enable flag, model reference) are performed under the mutex when callbacks may be concurrently invoked.

**Thread-safe by design:**
- Validation tags are written once at initialization and cleared once at destruction. No concurrent read/write conflict is possible during normal operation.
- Sentinel guard values are write-once; they are only checked for diagnostic purposes.

**Caller responsibilities:**
- The Environment must ensure that no concurrent callback invocation is in progress when the CallbackState is being destroyed.
- Callback registration should not occur concurrently with callback invocation without external synchronization at the Environment level.

**Termination signaling:**
- The termination mechanism (by which a user requests early solver termination from within a callback) does not use the CallbackState directly. Instead, termination is signaled through a separate flag in the Environment's asynchronous state structure. This flag is checked by the solver at each iteration boundary, and its write is atomic with respect to the solver's read. This design allows termination to be requested without requiring the solver's main loop to acquire the callback mutex.

## Design Rationale

**Lazy allocation pattern:**
The CallbackState is only allocated when first needed. Many solver sessions never register callbacks, so deferring allocation avoids wasting memory. This pattern is common in extensible library designs where optional features should impose zero overhead when not used.

**Shared state for multiple callback types:**
Rather than maintaining separate synchronization and timing structures for log callbacks and optimization callbacks, a single CallbackState is shared. This simplifies the design and ensures that timing statistics reflect the total overhead of all callback activity. The distinct function pointers for each callback type are stored in the Environment itself, which already has fields for each pointer and its associated user data.

**Validation sentinels:**
Two validation sentinels (a short one near the beginning and a long one near the end of the structure) provide defense-in-depth against memory corruption. The first sentinel catches corruption of the initial fields, while the second catches buffer overflows from the middle of the structure. This dual-sentinel pattern is a well-known technique in defensive programming for heap-allocated structures (see McConnell, *Code Complete*, 2nd ed., Microsoft Press, 2004, Chapter 24).

**Guard values for corruption detection:**
The sentinel guard fields set to -1 serve as canary values between groups of fields. If adjacent buffer writes overflow into these fields, the sentinel will be overwritten, and the corruption can be detected on the next validation check. This is analogous to stack canaries used by compilers for stack buffer overflow detection (Cowan et al., "StackGuard: Automatic Adaptive Detection and Prevention of Buffer-Overflow Attacks," USENIX Security Symposium, 1998).

**Mutex-based synchronization:**
A mutex (critical section) is used rather than lock-free techniques because callback invocations are relatively infrequent (typically once per simplex iteration or less) and the critical section is held only for the duration of the user callback. The overhead of mutex acquisition is negligible compared to the cost of a user callback function. Using a mutex also simplifies reasoning about correctness in the presence of multiple worker threads.

**Separated termination mechanism:**
Termination signaling is intentionally kept separate from the CallbackState. The solver's main loop checks a simple flag at each iteration boundary. If termination were gated on the callback mutex, the solver would need to acquire the mutex at every iteration just to check the flag, which would serialize the solver's main loop with callback invocations. By using a separate atomic flag, the termination check is effectively free from a synchronization perspective.

**Callback inheritance for child environments:**
When a child environment or model copy is created, it may need to inherit the parent's callback configuration so that the user continues to receive progress events for operations on the child. The parentCallbackState link enables configuration inheritance while allowing the child to maintain its own independent timing statistics and synchronization.

## References

- McConnell, S. (2004). *Code Complete*, 2nd edition. Microsoft Press. Chapter 24: Defensive Programming.
- Cowan, C., Pu, C., Maier, D., et al. (1998). "StackGuard: Automatic Adaptive Detection and Prevention of Buffer-Overflow Attacks." *Proceedings of the 7th USENIX Security Symposium*.
- Butenhof, D.R. (1997). *Programming with POSIX Threads*. Addison-Wesley. (Mutex design patterns for callback synchronization.)
- ConvexFeld Optimization, LLC. *ConvexFeld Optimizer Reference Manual* (public API documentation). Callback types, callback codes, and the cxf_setcallbackfunc / cxf_cbget API.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants
[x] No copied code fragments
[x] All algorithms cite published sources
[x] All data structures described semantically
[x] Passes the Clean Room Test (Rule 10)
```
