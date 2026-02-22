# Environment

## Purpose

The Environment structure is the top-level context object for an LP solver session. It holds all global configuration state including licensing credentials, solver parameters, error handling state, logging configuration, threading resources, and server connection information. Every model created within the solver must be associated with an environment, which acts as both a configuration provider and a resource manager. The environment owns its parameter table, manages child environments and associated models, and provides thread-safe error reporting.

## Fields

### Core State

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| validationTag | int | Sentinel value for detecting use-after-free and invalid pointers | Implementation-defined constant | Must be set on creation; cleared to zero on destruction |
| secondaryTag | int64 | Secondary validation sentinel for defense-in-depth pointer checking | Implementation-defined constant | Set once at creation; never modified until destruction |
| activationState | int | Tracks the environment's lifecycle phase | INACTIVE=0, INITIALIZING=1, ACTIVE=2 | Transitions only forward (0->1->2) on success; reverts to 0 on failure |
| deploymentType | int | Identifies the licensing backend in use | See License Types enumeration below | Set during initialization; immutable after activation |
| licenseMode | int | Controls initialization validation strategy | NORMAL=0, ISV variants, WLS variants, SPECIAL modes | Set before initialization validation; read-only after |
| licensedFlag | bool | Indicates whether initialization validation succeeded | true/false | Must be true before any model can be created |
| licenseChecksum | int | Integrity check value for the license | Implementation-defined | Set during initialization validation |
| licenseCode | string (fixed-length) | The configuration key string | Alphanumeric string, up to 10 characters | Set during configuration file parsing |
| licenseFilePath | string | Filesystem path to the configuration file | Valid file path or empty | Set during initialization from default location or config |
| versionCode | int | Encoded solver version number | Encoded major.minor.patch | Set once at creation; immutable |

### Parameter System

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| parameterTable | pointer-to-ParameterTable | Metadata table describing all solver parameters (name, type, bounds, defaults) | Non-null after initialization | Allocated during creation; freed on destruction |
| parameterFlags | pointer-to-array-of-int | Per-parameter flags array indexed by parameter ID, tracking modification state and protection | Non-null after initialization | Length equals total parameter count |
| parameterStorage1 | pointer-to-MemoryPool | Primary storage area for parameter values | Non-null after initialization | Managed as a memory pool |
| parameterStorage2 | pointer-to-MemoryPool | Secondary storage area for parameter values (used for save/restore) | Non-null after initialization | Managed as a memory pool |
| stringParameterPointers | pointer-to-array-of-string | Array of string parameter value pointers indexed by parameter offset | Non-null for string parameters | Each entry either null or points to allocated string |
| stringParameterCount | int | Number of string parameters in the table | Non-negative | Equals count of string-typed entries in parameterTable |
| stringParameterArray1 | pointer-to-array-of-string | First string parameter value array (primary) | Non-null if stringParameterCount > 0 | Length equals stringParameterCount |
| stringParameterArray2 | pointer-to-array-of-string | Second string parameter value array (for save/restore) | Non-null if stringParameterCount > 0 | Length equals stringParameterCount |
| noLocalDiskFlag | bool | Configuration flag indicating no local disk is available | true/false | Read from environment variable or parameter |
| envFileLoaded | bool | Indicates whether the configuration file was successfully loaded | true/false | Set once during initialization |

### Error Handling

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| errorBuffer | pointer-to-char-array | Buffer holding the most recent error message string, retrievable by the user | Non-null after initialization; sized to hold typical error messages | Cleared at the start of each API operation |
| errorCode | int | Numeric error code for the most recent error | 0 for success, or a defined error code | Set alongside errorBuffer content |
| errorBufferLocked | bool | Prevents overwriting the error buffer during nested error handling | true/false | When true, new errors set errorCode but do not overwrite the message |

### Logging

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| logFileHandle | pointer-to-file | Open file handle for solver log output | Valid file handle or null | Opened in append mode; closed on destruction |
| logFileName | string | Allocated copy of the log file path | Valid path or null | Non-null only when logFileHandle is open |
| outputFlag | int | Controls verbosity of solver output; positive enables logging | Non-negative integer | Read from parameter system; affects all log output |

### Threading

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| criticalSection | pointer-to-mutex | Primary mutex protecting shared environment state across threads | Non-null after initialization | Allocated during creation; destroyed on destruction |
| threadPool1 | pointer-to-ThreadPool | First thread pool handle for parallel operations | Valid handle or null | Created during initialization; destroyed on cleanup |
| threadPool2 | pointer-to-ThreadPool | Second thread pool handle (secondary parallelism) | Valid handle or null | Created on demand; destroyed on cleanup |
| threadPoolMutex | pointer-to-mutex | Mutex protecting thread pool state | Valid handle or null | Created with thread pools |
| threadPoolInitialized | bool | Indicates whether the thread pool subsystem has been initialized | true/false | Set once when thread pools are first created |
| logicalCoreCount | int | Number of logical CPU cores detected or overridden | Positive integer | Set during initialization from system query or environment variable override |
| physicalCoreCount | int | Number of physical CPU cores detected or overridden | Positive integer | Set during initialization; physicalCoreCount <= logicalCoreCount |
| maxCoresLimit | int | Upper bound on cores the solver may use, set via environment variable | Positive integer or 0 (unlimited) | When non-zero, effective core count = min(logicalCoreCount, maxCoresLimit) |
| coreAffinityMask | array-of-int | Bitmask array controlling CPU core affinity for solver threads | Array of bitmask values | Length corresponds to available cores |
| threadsParameter | int | User-configured thread count from the parameter system | Non-negative integer | 0 means auto-detect; positive means explicit count |
| cpuFeatureFlags | int | Detected CPU instruction set features (e.g., SIMD support) | Bitmask of supported features | Set once during initialization; read-only after |

### Server Connections

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| computeServerAddress | string | Address of the remote remote solver | Valid hostname/IP or null | Allocated string; freed on destruction |
| computeServerConnection | pointer-to-ComputeServerConnection | Active connection state for remote solver communication | Valid pointer or null | Non-null only when connected to a remote solver |
| computeServerQueueTimeout | int | Timeout in seconds for waiting in the remote solver job queue | Non-negative integer | Read from parameter system |
| tokenServerAddress | string | Address of the token configuration server | Valid hostname/IP or null | Allocated string; freed on destruction |
| tokenServerPort | int | Port number for the token configuration server | Valid port number | Has a default value; configurable |
| cloudServerAddress | string | Address of the cloud optimization service | Valid hostname/IP or null | Allocated string; freed on destruction |
| clusterManagerAddress | string | Address of the cluster manager for distributed computing | Valid hostname/IP or null | Allocated string; freed on destruction |

### Web License Service (WLS)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| wlsConnection | pointer-to-WLSConnection | Active connection handle for the Web License Service | Valid handle or null | Non-null when WLS licensing is active |
| wlsThreadData | pointer-to-ThreadData | Synchronization data for WLS background operations | Valid pointer or null | Created alongside wlsConnection |
| wlsAccessId | pointer | WLS access credential identifier | Valid credential or null | Set during WLS initialization |
| wlsSecretKey | pointer | WLS secret key for authentication | Valid credential or null | Set during WLS initialization |
| wlsLicenseType | int | Type code for the WLS license variant | WLS-specific type enumeration | Set during WLS handshake |
| wlsLicenseData | pointer | Opaque license data received from WLS | Valid pointer or null | Freed on cleanup |
| wlsLicenseInfo | pointer | Parsed license information structure | Valid pointer or null | Derived from wlsLicenseData |
| wlsTokenId | string | Token identifier for token-based WLS licensing | Valid token string or null | Set when using WLS+token mode |
| wlsTokenData | pointer | Opaque token session data | Valid pointer or null | Freed on environment destruction |
| wlsTokenMode | bool | Indicates whether WLS is operating in token mode | true/false | Set based on deployment type during initialization |
| wlsTokenSecret | pointer | Secret credential for token refresh operations | Valid pointer or null | Freed on cleanup |
| wlsHostname | string | Hostname override for WLS identification | Valid hostname or null | Optional; overrides auto-detected hostname |
| wlsInitialized | bool | Indicates WLS subsystem has completed initialization | true/false | Set after successful WLS handshake |
| wlsFeatures | int | Bitmask of WLS-licensed feature flags | Bitmask | Set during initialization validation |

### ISV (Independent Software Vendor) Data

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| isvVendorName | string | Name of the ISV embedding the solver | Non-empty string or null | Set by ISV during environment creation; immutable after |
| isvProductName | string | Name of the ISV's product | Non-empty string or null | Set by ISV during environment creation; immutable after |
| isvLicenseCode | string | ISV-specific configuration key (up to 9 characters) | Alphanumeric string or null | Set during ISV initialization validation |
| isvFeatureCount | int | Number of licensed ISV features | Non-negative | Set during ISV initialization validation |
| isvParameters | array-of-pointer (7 elements) | Opaque ISV configuration parameters passed at creation time | Implementation-defined | Stored at creation; preserved through destruction for potential reuse |

### Child Management

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| childEnvironmentArray | pointer-to-array-of-pointer-to-Environment | Array tracking child environments created from this environment | Valid pointer or null | Grows dynamically; each child increments parent refCount |
| childEnvironmentCount | int | Number of active child environments | Non-negative | childEnvironmentCount <= length of childEnvironmentArray |
| modelArray | pointer-to-ModelEntryArray | Array of model entries associated with this environment | Valid pointer or null | Each entry tracks model pointers and metadata |
| modelCount | int | Number of models currently associated with this environment | Non-negative | modelCount <= modelCapacity |
| modelCapacity | int | Allocated capacity of the model array | Non-negative | modelCapacity >= modelCount; grows on demand |

### Environment Relationships

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| rootEnvironment | pointer-to-Environment | Points to the root/master environment (self for standalone environments) | Non-null | For standalone: rootEnvironment == self. For children: rootEnvironment == parent |
| relatedEnvironment | pointer-to-Environment | Pointer to a related environment for license sharing across sessions | Valid pointer or null | Used for WLS and shared licensing scenarios |
| referenceCount | int | Tracks how many entities (children, models) reference this environment | Positive integer while alive | Starts at 1; incremented by children; environment freed when it reaches 0 |

### Recording

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| recordingEnabled | bool | Whether API call recording is active | true/false | Toggled via recording API |
| recordingData | pointer | Opaque recording session data | Valid pointer or null | Non-null only when recordingEnabled is true |

### Session Tracking

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| sessionReferenceCounter | int64 | Counter tracking the number of active sessions using this environment | Non-negative | Incremented on session start; decremented on session end |
| sessionIdentifier | int64 | Unique identifier for the current session | Non-negative | Assigned during session creation |
| versionCounter | int | Monotonically increasing counter incremented on environment state changes | Non-negative | Never decremented; used for cache invalidation |
| optimizingFlag | bool | Indicates whether an optimization operation is currently in progress | true/false | Set at optimization start; cleared at end |

### Async State

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| asyncState | pointer-to-AsyncState | State for asynchronous optimization operations | Valid pointer or null | Contains a termination flag for graceful async cancellation |

### Batch and Miscellaneous

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| batchMode | int | Batch optimization mode setting | Mode enumeration value | Controls batch optimization behavior |
| batchSizeLimit | int | Maximum number of items in a batch operation | Positive integer | Has a large default value (on the order of billions) |
| anonymousMode | bool | When set, suppresses tracking of variable and constraint names | true/false | Read from parameter system |
| fingerprintMode | int | Controls model fingerprinting behavior | AUTO=-1, or specific mode values | AUTO means the solver decides; explicit values override |

### System Information

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| cpuInfoBuffer | fixed-size-string | CPU identification string from system query | System-dependent string | Set once during initialization; read-only |
| platformInfoBuffer | fixed-size-string | Operating system and platform description | System-dependent string | Set once during initialization; read-only |
| hostnameBuffer | fixed-size-string | Machine hostname | System-dependent string | Set once during initialization; read-only |
| distributionInfoBuffer | fixed-size-string | OS distribution information | System-dependent string | Set once during initialization; read-only |

### Memory Limits

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| memoryLimit | double | Hard memory limit in GB; solver aborts if exceeded | Non-negative (0 means unlimited) | Read from parameter system or environment variable |
| softMemoryLimit | double | Soft memory limit in GB; solver may attempt to reduce memory usage | Non-negative (0 means unlimited) | softMemoryLimit <= memoryLimit when both are set |

### License Data

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| licenseData | pointer | Opaque license state data used by the licensing subsystem | Valid pointer or null | Allocated during initialization validation; freed during cleanup |

### Solver Parameters (Selected Key Parameters)

The environment stores the full set of solver parameters in its parameter table. A few key parameters are accessed frequently enough to warrant direct mention:

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| infinityThreshold | double | Values at or above this magnitude are treated as infinity | Typically 1e100 | Standard LP convention |
| presolveFlag | int | Controls whether presolve is applied | OFF=0, AUTO=-1, or specific level | Read from parameter system |
| nonConvexSetting | int | Controls handling of non-convex quadratic programs | -1 (auto), 0 (error), 2 (solve) | Required for non-convex QP/QCP models |

## License Types

The environment supports multiple licensing backends, enumerated as follows:

| Name | Description |
|------|-------------|
| LOCAL | Standard local configuration file |
| ISV | Independent Software Vendor embedded license |
| TOKEN_SERVER | License served from a token server |
| COMPUTE_SERVER | License managed by a remote remote solver |
| CLOUD | Cloud-based license service |
| MANAGER | License managed via a cluster manager |
| WLS | Web License Service (cloud-based licensing) |
| WLS_TOKEN | Web License Service with token-based authentication |
| HYBRID | Hybrid/fallback mode combining multiple licensing backends |

## Relationships

- **Owns** a ParameterTable structure containing all solver parameter metadata (names, types, bounds, defaults).
- **Owns** zero or more child Environment instances, which share the parent's license but maintain their own parameter state.
- **Owns** zero or more Model associations tracked via the model management array.
- **References** a root Environment (which may be itself for standalone environments) for shared license resource management.
- **Optionally references** a related Environment for license sharing across sessions (particularly in WLS scenarios).
- **Owns** an AsyncState structure when asynchronous operations are in progress.
- **Owns** ComputeServerConnection, WLSConnection, and ThreadPool resources when those subsystems are active.
- **Owns** its error buffer, log file handle, and all allocated string fields.

**Ownership semantics:**
- The environment **owns** (responsible for allocating and freeing) all its child structures, string fields, and connection objects.
- Models **borrow** a reference to the environment (the model does not own the environment).
- Child environments share the parent's license but are owned by the parent environment.

## Lifecycle

### Creation

1. **Allocation:** A zeroed block of memory is allocated for the environment structure.
2. **Sentinel initialization:** Validation tags are written to enable use-after-free detection.
3. **Mutex creation:** The critical section is initialized for thread safety.
4. **Reference count:** Set to 1. The root environment pointer is set to self for standalone environments.
5. **Error buffer allocation:** An error message buffer is allocated (large enough for typical error messages) and initialized to empty.
6. **System information collection:** CPU info, platform, hostname, and distribution are queried from the operating system.
7. **Parameter table construction:** The parameter table is built from a static definition table containing parameter names, types, default values, bounds, and flags. Each parameter gets an entry with its metadata and current value (initialized to default). A per-parameter flags array is also allocated.
8. **ISV parameter handling:** If the environment is configured for ISV mode, ISV-specific parameters are reset to their defaults and marked as protected.
9. **Phase 2 initialization:** Additional subsystem initialization is performed.
10. **Output pointer:** On success, the created environment pointer is returned to the caller.

The public API provides two creation paths:
- **loadenv:** Creates an environment and immediately initializes licensing (proceeds to activation).
- **emptyenv:** Creates an environment without licensing, allowing configuration before explicit activation.

### Activation (Finalization)

After creation, the environment must be activated (finalized) before it can be used to create models:

1. **State transition:** activationState moves from INACTIVE to INITIALIZING.
2. **Configuration file loading:** If present, the solver configuration file is parsed and parameters are applied.
3. **Environment variable processing:** System environment variables for core counts, max cores, and memory limits are read and applied.
4. **initialization validation:** Based on deploymentType, the appropriate license backend is contacted and validated. This may involve file I/O (local license), network communication (token server, WLS, cloud), or ISV credential verification.
5. **Parameter finalization:** Thread counts, memory limits, and other system-dependent parameters are finalized.
6. **State transition:** On success, activationState moves to ACTIVE and licensedFlag is set. On failure, activationState reverts to INACTIVE.

### Mutation

The environment is mutated by the following operations:
- **Parameter changes:** Setting integer, double, or string parameters via the parameter API.
- **Error reporting:** Writing error codes and messages to the error buffer.
- **Log file operations:** Opening, writing to, or closing the log file.
- **Model association:** Adding or removing models from the model tracking array.
- **Child environment creation:** Adding child environments to the child array and incrementing refCount.
- **Optimization state:** Setting and clearing the optimizingFlag.
- **Session management:** Updating session counters and version counter.

### Destruction

Environment destruction follows a strict ordering to avoid resource leaks and dangling references:

1. **remote solver session termination:** If connected to a remote solver, terminate the session and wait for remote job completion.
2. **Child environment cleanup:** Recursively free all child environments, decrementing reference counts and freeing parents when counts reach zero.
3. **Model cleanup:** Free all associated model entries and the model array.
4. **License cleanup:** Release license resources (tokens, WLS connections, server sessions).
5. **String and buffer deallocation:** Free all allocated string fields (server addresses, ISV data, WLS credentials, etc.).
6. **Parameter string arrays:** Free both primary and secondary string parameter arrays (root environment only).
7. **WLS resource cleanup:** Free WLS tokens, credentials, and connection handles.
8. **Thread pool destruction:** Destroy thread pools and their associated mutexes.
9. **Async cleanup:** Clean up any pending asynchronous operation state.
10. **Parameter table cleanup:** Free the parameter entry array, the parameter table header, and the flags array. Destroy parameter storage memory pools.
11. **Error buffer deallocation:** Free the error message buffer.
12. **Mutex destruction:** Destroy the primary critical section.
13. **Validation tag invalidation:** Clear the validation tag to zero so any subsequent access is detected as use-after-free.
14. **Log file closure:** Close the log file handle.
15. **Final deallocation:** Free the environment memory block itself.

**Reference counting:** The environment is not freed until its referenceCount reaches zero. If children still reference it when destruction is requested, a warning is logged and the free is deferred until the last reference is released.

## Invariants

1. **Validation tag integrity:** A valid environment always has its validationTag and secondaryTag set to the expected sentinel values. A zero validationTag indicates a freed or invalid environment.
2. **Root environment chain:** rootEnvironment is never null. For standalone environments, rootEnvironment == self. For children, rootEnvironment points to the actual root.
3. **Reference count consistency:** referenceCount >= 1 for any live environment. It equals 1 + (number of child environments that reference this as their root).
4. **Activation prerequisite:** Models can only be created against an environment with activationState == ACTIVE.
5. **Error buffer availability:** errorBuffer is non-null whenever activationState > INACTIVE.
6. **Parameter table completeness:** After successful initialization, parameterTable is non-null and contains entries for all solver parameters, each with valid type, bounds, and default values.
7. **Thread safety of error buffer:** The errorBufferLocked flag is respected by all error-setting functions. When locked, error codes are updated but the message text is preserved.
8. **Memory limit ordering:** When both are set, softMemoryLimit <= memoryLimit.
9. **License immutability:** deploymentType and licenseMode are not modified after the environment reaches ACTIVE state.
10. **Destruction ordering:** All models must be freed before the environment. All child environments must be freed (or deferred) before the parent.

## Thread Safety

**Thread-safe fields (protected by criticalSection):**
- referenceCount: All increments and decrements acquire the critical section.
- childEnvironmentArray and childEnvironmentCount: Modifications are protected.
- Model association changes are protected when accessed from multiple threads.

**Error buffer threading:**
- The error buffer does NOT use the critical section for synchronization. Instead, it uses the errorBufferLocked flag as a lightweight guard against nested overwrites within a single thread.
- Callers are responsible for acquiring the environment's critical section before calling error-setting functions from multiple threads.

**Thread pool operations:**
- Thread pool initialization and destruction use their own mutex (threadPoolMutex).
- Thread pool handles are only modified during initialization and cleanup, not during normal operation.

**Read-only after initialization:**
- System information fields (CPU, platform, hostname, distribution) are set once and never modified.
- License type and mode are immutable after activation.
- The parameter table structure is immutable after initialization (though parameter values within it are mutable).

## Design Rationale

**Why a separate Environment structure?**

The environment serves as a "session context" pattern common in commercial solver libraries. Separating global configuration (licensing, logging, threading) from per-model state (constraints, variables, solutions) provides several benefits:

1. **License management isolation:** A single license can be shared across multiple models without each model needing its own license negotiation.

2. **Parameter inheritance:** Child environments and models inherit parameter defaults from the parent environment while allowing per-model overrides.

3. **Resource sharing:** Thread pools, remote solver connections, and WLS sessions are expensive resources that are amortized across all models in an environment.

4. **Error scoping:** Each environment maintains its own error buffer, so multi-model workflows can isolate error reporting.

**Reference counting for safe deallocation:**

The reference counting scheme allows child environments and models to safely share a parent's resources. This is particularly important for remote solver and WLS scenarios where network connections must outlive individual model operations.

**Error buffer locking pattern:**

The error buffer lock (as opposed to using the main mutex) is a lightweight mechanism specifically designed for the common case of cascading errors during a single API call. When an inner function fails and reports an error, the outer function may also want to report a different error. The lock ensures the original (root cause) error message is preserved, which is more useful for debugging. This pattern avoids the overhead of full mutex acquisition for every error report while still preventing message corruption in the single-threaded cascading error case.

**Parameter table design:**

The table-based parameter system with per-parameter metadata (name, type, bounds, defaults, flags) enables a generic parameter API where parameters can be accessed by name string. This allows new parameters to be added without changing the API. The dual storage areas (primary and secondary) support parameter save/restore operations, which are useful for solver tuning workflows.

**Environment variable overrides:**

Core count and memory limit parameters can be overridden by system environment variables. This allows system administrators to impose resource limits without modifying application code, which is important in shared computing environments.
