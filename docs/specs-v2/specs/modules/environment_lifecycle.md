# Module: Environment Lifecycle

## Purpose

The Environment Lifecycle module manages the complete lifecycle of the Environment structure, from initial allocation through activation to eventual destruction. The Environment is the top-level context object for an LP solver session, and its lifecycle involves five distinct operations: creating the raw structure with default values, finalizing it into an active session, initializing the logging subsystem, tracking which model is currently active, and releasing all owned resources on destruction.

The creation and destruction operations form a symmetrical pair: creation allocates and initializes a zeroed Environment with parameter defaults, system information, and a mutex, while destruction systematically releases every owned resource in reverse allocation order. Between these, the finalization operation is the most complex: it transitions the Environment from its initial inactive state to an active state by validating hardware capabilities, detecting system resources, and reading configuration files. The log file and active model tracking functions provide narrower lifecycle management for specific subsystems owned by the Environment.

## Functions

### cxf_env_create_internal

**Purpose:** Allocate and initialize an Environment structure with default parameter values, system information, and threading primitives.

**Signature:**
- Input: `extra_flags` : int - Additional configuration flags affecting initialization behavior
- Input: `parent_environment` : pointer-to-Environment or null - Parent environment from which to inherit parameter defaults; null for standalone environments
- Output: `created_environment` : pointer-to-pointer-to-Environment - On success, receives the newly created Environment pointer; on failure, set to null
- Output: int - Zero on success, or an error code (OUT_OF_MEMORY, INVALID_ARGUMENT, UNKNOWN_PARAMETER)

**Preconditions:**
- The output pointer must be non-null and writable

**Postconditions:**
- On success: a fully initialized but not yet activated Environment is stored in the output pointer. The Environment has its validation sentinels set, a mutex allocated, system information collected (CPU, platform, hostname), a parameter table built from the static parameter definition table, and a reference count of one. The activation state is INACTIVE.
- On failure: the output pointer is set to null and all partially allocated resources have been freed. No memory is leaked.

**Side Effects:**
- Allocates a large block of zeroed memory for the Environment structure
- Allocates a string buffer for error messages
- Initializes a process-level global state object (once per process)
- Allocates and initializes a mutex (critical section)
- Queries the operating system for CPU information, platform details, hostname, and OS distribution
- Allocates and populates the parameter table from a static definition table containing parameter names, types, default values, minimum/maximum bounds, and flags
- When a parent environment is provided, inherits current parameter values from the parent's parameter table instead of using the static defaults

**Error Conditions:**
- Memory allocation failure at any stage -> OUT_OF_MEMORY; all partially allocated resources are cleaned up before returning
- Parameter registration failure during table construction -> propagated error code; parameter table resources are freed

**Behavioral Description:**

1. Set the output pointer to null.
2. Allocate a zeroed block of memory for the Environment structure. If allocation fails, return OUT_OF_MEMORY.
3. Write both validation sentinels (primary and secondary) into the structure to enable integrity checking.
4. Initialize internal bookkeeping pointers, including a circular linked list for internal tracking and the extra flags field.
5. Set the batch size limit to its large default value.
6. Initialize the process-level global state (idempotent; safe to call multiple times).
7. Allocate and initialize a mutex for thread-safe reference count manipulation. If this fails, clean up and return the error.
8. Set the root environment pointer to self (for standalone environments) and initialize the reference count to one.
9. Allocate the error message buffer. If allocation fails, clean up and return OUT_OF_MEMORY. Initialize the buffer to an empty string.
10. Query the operating system for CPU information, platform details, OS distribution, and hostname, storing each in fixed-size buffers within the Environment.
11. If the parameter table has not already been allocated, build it:
    a. Count the number of parameters in the static definition table by scanning for the end-of-public-parameters marker and the overall end sentinel.
    b. Allocate the parameter entry array and per-parameter flags array.
    c. For each parameter, copy its metadata (name, type, minimum, maximum, default) from the static definition to the entry array. If a parent environment is provided, inherit the current value from the parent; otherwise use the static default. String-type parameters receive a default empty string, with one specific directory parameter receiving a platform-appropriate default path.
    d. Register each parameter name (converted to uppercase) in a lookup structure for efficient name-based access.
12. Execute a secondary initialization phase that configures additional Environment subsystems.
13. Execute a final initialization step.
14. On success, store the Environment pointer in the output and return zero.
15. On any failure during steps 7-13, perform cleanup: decrement the reference count under the mutex, and if the count reaches zero, free the Environment via the internal destructor. Return the error code.

**Thread Safety:** Unsafe. This function allocates new resources and is not designed for concurrent invocation. The resulting Environment, once created, supports thread-safe reference count manipulation through its mutex.

**Dependencies:**
- Memory allocation subsystem (zeroed allocation and sized allocation)
- Mutex initialization
- System information queries (CPU, platform, hostname, distribution)
- Parameter table construction from static definitions
- Parameter setter functions (int, double, string variants)

---

### cxf_env_finalize

**Purpose:** Transition an Environment from the INACTIVE state to the ACTIVE state by validating hardware capabilities, initializing subsystems, loading configuration files, and finalizing system-dependent parameters.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment to finalize
- Input: `read_config_file` : bool - Whether to load the optional configuration file
- Output: int - Zero on success, or an error code

**Preconditions:**
- The environment must pass structural validation (valid sentinel)
- The environment must be in the INACTIVE state (activation state equals zero); attempting to finalize an already-started environment is an error

**Postconditions:**
- On success: the environment's activation state is ACTIVE, all subsystems are initialized, and the environment is ready to create models
- On failure: all resources allocated during finalization are freed, the environment's state is fully rolled back to its pre-finalization state using a snapshot/restore mechanism, and the environment remains in the INACTIVE state

**Side Effects:**
- Takes a snapshot of the entire Environment state at the beginning of finalization to enable atomic rollback on failure
- Checks hardware capabilities (requires SIMD instruction support; fails with an error if not available)
- Detects logical and physical CPU core counts from the operating system
- Reads and applies system environment variable overrides for core counts, maximum cores, and memory limits
- Initializes core affinity data
- Initializes four subsystem phases (memory management, parameter handling, logging infrastructure, and solver configuration)
- Reads parameters from the previously saved backup to restore any programmatic settings
- Processes the no-local-disk flag if set
- Discovers and parses the configuration file, extracting configuration parameters into the environment's parameter table
- Loads the optional configuration file if requested, applying parameter overrides from it
- Initializes the recording subsystem if enabled
- Applies memory limits from environment variables
- Initializes thread pool infrastructure
- Opens the log file if a log file parameter is configured

**Error Conditions:**
- Environment validation failure -> INVALID_ARGUMENT
- Environment already started -> INVALID_ARGUMENT with error message
- Memory allocation failure (buffers) -> OUT_OF_MEMORY
- Hardware capability check failure (missing required SIMD instructions) -> error with descriptive message
- Configuration file not found or unreadable -> error with descriptive message
- Version mismatch between client and library -> warning logged (not an error)

**Behavioral Description:**

The finalization process proceeds through seven behavioral stages. If any stage fails, execution jumps to the cleanup stage, which rolls back the environment to its pre-finalization state.

**Stage 1 -- Validation and State Backup:**
Validate the environment pointer using the structural validation check. Verify that the activation state is INACTIVE; if not, report an error. Take a full snapshot of the current environment state (all fields) into a backup buffer. Set the activation state to INITIALIZING.

**Stage 2 -- Hardware and System Resource Detection:**
Verify that the CPU supports the required SIMD instruction set. If not, report an error indicating the processor is unsupported. Detect the CPU feature flags. Query the operating system for the logical and physical core counts. Read system environment variables that override the detected core counts (valid range: 1 to 1024 for each). Read the max-cores environment variable and apply it as an upper bound. Initialize the core affinity data structure, marking all cores as available.

**Stage 3 -- Subsystem and Parameter Initialization:**
Execute four sequential subsystem initialization phases, each of which configures a different aspect of the environment (memory management, secondary initialization, logging, and solver configuration). Restore parameter values from the backup to preserve any programmatic settings that were made between creation and finalization, with special handling for the log file parameter to avoid overwriting an existing log file setting. Process the no-local-disk flag if it is set, propagating it to all parameters that depend on local disk availability. Initialize a mutex for the first thread pool.

**Stage 4 -- Configuration Loading:**
Discover the configuration file path. If no path was set programmatically, search for the configuration file in the standard platform-specific locations. Parse the configuration file and extract all configuration parameters into the environment's parameter table, setting each only if it has a non-default value in the configuration file. If the read-config-file flag is set, attempt to load the optional solver configuration file from the current working directory; silently ignore if the file is not found.

**Stage 5 -- Post-Configuration Validation:**
Check the version code and display a warning if the client version differs from the library version. Check batch mode restrictions.

**Stage 6 -- Final Configuration:**
Initialize the recording subsystem if the recording flag is set. Read and apply the memory limit environment variable, setting either the hard or soft memory limit depending on the sign of the value. Initialize the second thread pool mutex. Transition the activation state to ACTIVE. Open the log file if configured.

**Stage 7 -- Error Cleanup (on failure only):**
Free the configuration file path. Destroy the first thread pool mutex. Free parameter storage pools. Close the log file if it was opened. Free all string parameter values that were allocated during initialization. Free the parameter flags array. Restore the environment state from the snapshot taken in Stage 1, returning the environment to its pre-finalization state. Report the final error through the error handling subsystem.

**Thread Safety:** Unsafe. Finalization must be called from a single thread before the environment is shared. After successful finalization, the environment supports concurrent access through its mutex.

**Dependencies:**
- Environment validation
- Hardware capability detection (SIMD check, CPU feature detection, core count detection)
- Memory allocation subsystem
- Parameter setter and getter functions
- Configuration file reader
- Mutex initialization and destruction
- Thread creation and synchronization
- Log file initialization

---

### cxf_env_load_logfile

**Purpose:** Initialize or reconfigure the logging subsystem by opening a log file for the environment, optionally writing a version and timestamp header.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment whose logging is being configured
- Input: `filename` : string or null - Path to the log file; null or empty string disables file logging
- Input: `host_info` : string or null - Optional hostname string to include in the log header
- Input: `write_header` : bool - Whether to write the version and timestamp header to the log file
- Output: int - Zero on success, or an error code (OUT_OF_MEMORY, FILE_WRITE)

**Preconditions:**
- The environment must pass structural validation

**Postconditions:**
- On success with a non-empty filename: the log file is open in append mode, the environment holds both the file handle and an allocated copy of the filename, and (if requested) a header line has been written containing the solver version, platform, and current timestamp
- On success with a null or empty filename: any previously open log file is closed, the file handle and filename are cleared
- On failure: the previous log file state is unchanged (or partially updated); the error status is set on the environment

**Side Effects:**
- May close a previously open log file
- May free a previously allocated log filename string
- Allocates a copy of the new filename
- Opens a file on the filesystem in append mode
- May write a header line to the log file

**Error Conditions:**
- Environment validation failure -> propagated error code
- Output suppressed (verbosity level is negative) -> returns success without action
- Memory allocation failure for filename copy -> OUT_OF_MEMORY
- File open failure -> FILE_WRITE with warning logged
- String length overflow (pathological input) -> filename stored as null

**Behavioral Description:**

1. Validate the environment. If validation fails, set the error status and return.
2. If the verbosity level is negative (output suppressed), return success without action.
3. If the filename is null or empty:
   a. Close the existing log file handle if one is open. Set the handle to null.
   b. Free the existing filename string if one is allocated. Set the filename to null.
   c. Set the error status and return success.
4. If the environment is not yet active (activation state is zero, indicating this is being called during early initialization before finalization):
   a. Free any existing filename string.
   b. Allocate a copy of the new filename and store it. If allocation fails, return OUT_OF_MEMORY.
   c. Return success. The file will be opened later when the environment is finalized.
5. If the environment is active:
   a. Attempt to open the file in append mode. If the open fails, log a warning and return FILE_WRITE.
   b. Close any existing log file handle.
   c. Store the new file handle.
   d. Free any existing filename string, allocate a copy of the new filename, and store it.
   e. If the write-header flag is set:
      - Query the current date and time, format it into a human-readable string.
      - Query the solver version number (major, minor, technical revision).
      - Query the platform identifier string.
      - Write a header line to the log file in the format: "ConvexFeld M.m.t (platform) logging started TIMESTAMP" (optionally including the host info string if provided).
      - Flush the log file.
6. Set the error status on the environment and return.

**Thread Safety:** Unsafe. The caller must ensure exclusive access to the environment's logging state.

**Dependencies:**
- Environment validation
- Memory allocation and deallocation
- File system operations (open, close, flush)
- Platform and version query functions
- Time formatting functions
- Error status reporting

---

### cxf_env_update_active_model

**Purpose:** Tear down a model manager structure by freeing all tracked model pointers, releasing the associated mutex, and deallocating the manager itself.

**Signature:**
- Input: `allocator_environment` : pointer-to-Environment - Environment used for memory deallocation
- Input: `model_manager` : pointer-to-pointer-to-ModelManager - Double pointer to the model manager to destroy; set to null on return
- Output: void

**Preconditions:**
- The allocator environment must be valid for memory deallocation
- The model manager double pointer must be non-null

**Postconditions:**
- All model pointers tracked by the model manager have been freed
- The model pointer array has been freed
- The model and capacity counts are reset to zero
- The model manager's mutex has been destroyed
- The model manager's state flags are cleared
- The model manager structure itself has been freed
- The caller's pointer has been set to null

**Side Effects:**
- Frees each model pointer in the model manager's array
- Frees the model pointer array
- Destroys the model manager's mutex
- Frees the model manager structure
- Sets the caller's pointer to null via the double pointer

**Error Conditions:**
- Null model manager (dereferenced pointer is null) -> returns immediately with no action

**Behavioral Description:**

1. Dereference the model manager pointer. If the result is null, return immediately.
2. Read the model count and model pointer array from the model manager.
3. If the model count is positive, iterate through the model pointer array:
   a. For each non-null entry, free the model pointer using the allocator environment.
   b. Set the slot to null.
   c. Defensively re-read the array pointer and count from the model manager after each free (guards against concurrent modification).
4. If the model pointer array is non-null, free it. Set the array pointer to null within the model manager.
5. Reset the model capacity and model count to zero.
6. Destroy the mutex associated with the model manager.
7. Clear the state flags on the model manager.
8. Free the model manager structure itself.
9. Set the caller's pointer to null.

**Thread Safety:** Unsafe. The caller must ensure no concurrent access to the model manager during destruction.

**Dependencies:**
- Memory deallocation function
- Mutex destruction function

---

### cxf_env_free_internal

**Purpose:** Deallocate an Environment structure and all resources it owns, including child environments, associated models, server connections, thread pools, mutexes, parameter storage, and allocated string fields.

**Signature:**
- Input: `environment_ptr` : pointer-to-pointer-to-Environment - Double pointer to the environment to free; set to null on return
- Output: void

**Preconditions:**
- None beyond pointer validity; the function handles null input gracefully

**Postconditions:**
- All resources owned by the environment have been freed
- The environment's validation sentinel has been cleared (invalidating it for use-after-free detection)
- The caller's pointer has been set to null
- If the environment had child environments with outstanding references, those children receive a deferred-free warning and any remote jobs are terminated

**Side Effects:**
- Terminates any active remote solver session (if this is the root environment)
- Cleans up asynchronous operation state
- Recursively frees all child environments, managing reference counts under the parent's mutex
- Frees all associated model entries and the model array
- Frees all allocated string fields (covering system info, server addresses, and other configuration)
- Frees parameter string arrays (root environment only)
- Destroys thread pools and their mutexes
- Cleans up callback state
- Frees parameter storage memory pools and the parameter table
- Frees the error message buffer
- Destroys all mutexes (thread pool, main critical section)
- Closes the log file
- Clears the validation sentinel
- Frees the environment memory block itself

**Error Conditions:**
- Null pointer input (either level of the double pointer) -> returns immediately with no action
- Child environment still referenced (reference count > 0 after decrement) -> logs a warning about deferred free; if the child has an active remote solver job, attempts to terminate it with a bounded polling loop, then sends a termination message and logs a warning

**Behavioral Description:**

1. If the input pointer or the dereferenced environment pointer is null, return immediately.
2. Set the caller's pointer to null.
3. **remote solver cleanup:** If this is the root environment and has an active remote solver connection, terminate the session. If this is the root environment with an active server socket, clean up asynchronous state.
4. **Child environment cleanup:** Iterate through the child environment array. For each child:
   a. Acquire the parent's mutex, decrement the parent's reference count, and check if it reached zero.
   b. If the child is the same as its parent and the reference count is still positive, log a deferred-free warning. If the child has an active remote job, attempt to terminate it: set a termination flag, poll for completion (with a bounded iteration count and sleep intervals), send a kill message to the remote server, and log a warning about the killed job. Clear the child slot and continue.
   c. Otherwise, recursively call this function on the child. If the parent's reference count reached zero and the parent is a different object from the child, recursively free the parent as well.
   d. After processing all children, clear the child count and free the child array.
5. **Model cleanup:** Iterate through the model entry array, freeing each model pointer within each entry. Free the model array itself and any additional model data.
6. **String field deallocation:** Free each of the individually allocated string fields, covering system information strings, server address strings, and other configuration strings.
7. **Parameter string arrays (root environment only):** Iterate through both string parameter arrays, freeing each allocated string. Free the arrays themselves.
8. **Thread pool and async cleanup:** Destroy the thread pool region, clean up asynchronous state, and destroy the thread pool mutex. For the root environment with specific CPU feature flags, finalize thread pools and destroy parameter storage memory pools.
9. **Final field cleanup:** Free the parameter flags array. Clean up callback state. Destroy the thread pool mutex and main critical section mutex.
10. **Invalidate the environment:** Clear the validation sentinel to zero, preventing any future use-after-free from passing validation checks.
11. **Close log file:** If a log file handle is open, close it.
12. **Final deallocation:** If this is the root environment (or the root is null), free the environment memory block using a temporary zeroed context for the allocator. If this is a child environment, free the memory using the root environment as the allocator context.

**Thread Safety:** Conditional. The function acquires the parent environment's mutex when manipulating reference counts for child environment cleanup. The function itself should not be called concurrently on the same environment from multiple threads.

**Dependencies:**
- Remote solver session management (terminate, cleanup async, send terminate, free connection)
- Memory deallocation function
- Thread pool destruction
- Async state cleanup
- Mutex destruction
- Memory pool destruction
- Callback state cleanup
- Log file closure

---

## Module-Level Behavioral Notes

### Lifecycle State Machine

The Environment progresses through a strict state machine:

```
INACTIVE (0)  --[create]--> INACTIVE (0)  --[finalize success]--> ACTIVE (2)
                                           --[finalize failure]--> INACTIVE (0)
ACTIVE (2)    --[free]----> DESTROYED
```

The INITIALIZING state (1) is a transient state that exists only during finalization. If finalization fails, the state reverts to INACTIVE through the snapshot/restore mechanism. An Environment in the INACTIVE state can be configured programmatically (parameters set, log file path specified) but cannot create models. Only an ACTIVE environment can serve as the context for model creation and optimization.

### Atomic Initialization via Snapshot/Restore

The finalization function implements an atomic-like initialization pattern: the entire Environment state is snapshotted at the beginning, and on any failure, the snapshot is restored. This ensures that a failed finalization attempt does not leave the Environment in a partially initialized state. The caller can retry finalization (possibly with different parameters) or simply free the Environment.

### Reference Counting and Deferred Free

Environments use reference counting to manage shared lifetime across parent-child relationships. When a child environment is created, the parent's reference count is incremented. When the child is freed, the parent's count is decremented under the parent's mutex. The parent is only freed when its count reaches zero. If destruction is requested while the count is still positive, a warning is logged and the free is deferred until the last reference is released.

### Parameter Initialization Precedence

During finalization, parameters are resolved through a layered system where later layers override earlier ones:

1. Built-in defaults (from the static parameter definition table, established during creation)
2. Configuration file parameters (loaded from the optional solver configuration file)
3. Programmatic settings (preserved through the snapshot/restore mechanism)

This precedence system ensures that programmatic settings always take priority, while configuration file settings override defaults without overriding explicit user choices.

### Resource Cleanup Ordering

The destruction function follows a strict ordering that mirrors the reverse of creation and initialization:

1. Active connections first (remote solver sessions, remote jobs)
2. Dependent structures (child environments via recursive descent, then models)
3. Allocated string fields and parameter arrays
4. Threading infrastructure (thread pools, async state, mutexes)
5. Core infrastructure (parameter table, error buffer, main mutex)
6. Validation sentinel invalidation
7. Log file closure
8. Final memory deallocation

This ordering prevents dangling references: connections are terminated before the environment state they depend on is freed, child environments are freed before their parent's shared resources, and the validation sentinel is cleared before the memory block is freed so that any subsequent use-after-free attempt fails validation.

### Relationship Between Functions

| Function | Creates | Depends On | Destroyed By |
|----------|---------|------------|--------------|
| cxf_env_create_internal | Environment (INACTIVE) | Static param definitions, system queries | cxf_env_free_internal |
| cxf_env_finalize | Transitions INACTIVE -> ACTIVE | cxf_env_create_internal (prior call) | cxf_env_free_internal |
| cxf_env_load_logfile | Log file handle, filename copy | Valid Environment | cxf_env_free_internal |
| cxf_env_update_active_model | -- (destructor) | Valid model manager | -- |
| cxf_env_free_internal | -- (destructor) | cxf_env_create_internal (prior call) | -- |

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] No version-specific packed encoding values
[x] No structure sizes in bytes or field offsets
[x] Passes the Clean Room Test: someone who never saw the binary could write this from public ConvexFeld documentation and standard solver architecture knowledge
```
