# Module: Threading & Synchronization

## Purpose

The Threading & Synchronization module provides the solver's thread resource management and numeric locale safety infrastructure. It encompasses two distinct subsystems that share a common concern with thread-correct execution:

1. **Locale Safety:** Before optimization begins, the solver must ensure that numeric formatting uses the standard "C" locale (period as decimal separator) regardless of the user's system locale. This is critical because LP file formats, coefficient parsing, solution output, and log messages all depend on consistent decimal point formatting. The module provides an acquire/release pair that saves the calling thread's locale, switches to the "C" locale using per-thread locale isolation, and restores the original locale when optimization completes.

2. **Thread Resource Queries:** The module provides functions to query detected hardware parallelism (logical processors and physical cores), compute the effective thread count for parallel operations by reconciling multiple constraints (hardware availability and user configuration), and validate thread count choices by warning when oversubscription is requested.

3. **Error Buffer Preparation:** The module includes a function that prepares the environment's error buffer for a new API operation by clearing stale error state, unless the buffer is currently locked for nested error handling.

Despite the module name, none of the functions in this module implement mutual exclusion or thread synchronization primitives. The "lock" and "acquire" terminology in several function names is a historical misnomer; see the Module-Level Behavioral Notes section for details.

## Functions

### cxf_acquire_solve_lock

**Purpose:** Save the calling thread's locale state and switch to the standard numeric locale before optimization begins, ensuring consistent decimal point formatting.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment controlling the optimization context
- Input: `locale_state` : pointer-to-LocaleSaveData - Caller-provided output structure that will receive pointers to saved and target locale data
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The environment pointer must be valid and non-null
- The locale_state output structure must be a valid, writable, zero-initialized structure

**Postconditions:**
- On success (return value zero), one of the following has occurred:
  - If the environment is already in an optimization context (optimization-active flag or related context flag is set), no locale changes are made and the locale_state remains empty. The caller need not call the release function.
  - If the current thread's locale is already the standard "C" locale, no locale changes are made and the locale_state remains empty. The caller need not call the release function.
  - Otherwise, the current thread's locale has been switched to "C" using per-thread locale isolation, and the locale_state structure contains pointers to both the saved original locale data and the target locale data. The caller must call cxf_release_solve_lock to restore the original locale.
- On failure (nonzero return): memory allocation for locale state storage failed. The locale_state may contain a partial result that should still be passed to cxf_release_solve_lock for cleanup.

**Side Effects:**
- Allocates memory for saved locale data and target locale data structures
- Modifies the calling thread's locale setting to the standard "C" locale
- Temporarily enables per-thread locale mode on the calling thread to isolate the locale change from other threads
- Restores the thread's original locale mode setting after applying the locale change (the locale itself remains "C")

**Error Conditions:**
- Memory allocation failure for target locale structure -> returns the out-of-memory error code
- Memory allocation failure for saved locale structure -> returns the out-of-memory error code (target structure is still stored in the output for cleanup)

**Behavioral Description:**
This function ensures that the calling thread uses the standard "C" locale during optimization, which guarantees that the period character is used as the decimal separator. This is essential because LP/MPS file formats, coefficient formatting, solution output, and log messages all require consistent numeric formatting regardless of the user's system locale (which might use commas or other separators).

The function first checks whether the environment is already in an optimization context. If so, locale handling has already been performed by an outer call, and the function returns immediately with no changes. Next, it queries the current thread's locale. If the locale is already "C", no save/restore cycle is needed and the function returns immediately.

If the locale must be changed, the function allocates two locale data structures: one to hold the target "C" locale configuration, and one to save the original locale settings (the thread's locale mode, the locale category, and the locale string). It then enables per-thread locale mode on the calling thread so that the subsequent locale change does not affect other threads in the process. After applying the "C" locale, it restores the thread's original locale mode (the locale change persists independently of the mode setting). Both structure pointers are stored in the caller's output structure for later use by cxf_release_solve_lock.

The per-thread locale isolation mechanism is critical for correctness in multi-threaded applications where different threads may need different locale settings.

**Thread Safety:** Safe. The function operates on per-thread locale state and uses per-thread locale isolation to prevent cross-thread interference. The allocated structures are owned by the caller and not shared.

**Dependencies:**
- Memory allocation (from the Memory Primitives module)

---

### cxf_release_solve_lock

**Purpose:** Restore the calling thread's original locale state after optimization completes, reversing the locale change made by cxf_acquire_solve_lock.

**Signature:**
- Input: `locale_state` : pointer-to-LocaleSaveData - The structure populated by a prior call to cxf_acquire_solve_lock
- Output: void

**Preconditions:**
- The locale_state must have been populated by a prior call to cxf_acquire_solve_lock (or be zero-initialized, in which case this function is a no-op)

**Postconditions:**
- If the locale_state contained saved locale data, the calling thread's locale has been restored to its original setting (the locale that was active before cxf_acquire_solve_lock was called)
- All allocated locale data structures have been freed
- All pointers in the locale_state structure have been set to null

**Side Effects:**
- Restores the calling thread's locale setting from the saved state
- Temporarily enables per-thread locale mode on the calling thread during restoration to isolate the change
- Restores the thread's original locale mode after applying the saved locale
- Frees the saved locale and target locale structures
- Clears the pointers in the locale_state structure

**Error Conditions:**
- None. This function always succeeds. If the locale_state contains null pointers (because cxf_acquire_solve_lock determined no locale change was needed), the function simply returns.

**Behavioral Description:**
This function is the cleanup companion to cxf_acquire_solve_lock. It first frees the target locale structure (which held the "C" locale configuration) if one was allocated. Then, if a saved locale structure exists, it enables per-thread locale mode, restores the original locale by applying the saved locale category and string, restores the original thread locale mode, and frees the saved locale structure. Both pointers in the locale_state are cleared to null to prevent double-free.

The function handles partial initialization gracefully: if cxf_acquire_solve_lock returned early (because the locale was already "C" or the environment was already in an optimization context), the locale_state contains null pointers and this function becomes a no-op.

**Thread Safety:** Safe. Operates on per-thread locale state using per-thread locale isolation. The locale_state structure is caller-owned and not shared.

**Dependencies:**
- Memory deallocation (from the Memory Primitives module)

---

### cxf_env_acquire_lock

**Purpose:** Clear the environment's error buffer state in preparation for a new API operation, unless the error buffer is locked for nested error handling.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment whose error state should be cleared
- Output: void

**Preconditions:**
- None (the function handles a null environment gracefully)

**Postconditions:**
- If the environment is non-null and the error buffer is not locked:
  - The error buffer state flag has been cleared to its initial state
  - The error message buffer contents have been cleared to an empty string
- If the environment is null, or the error buffer is locked, no changes are made

**Side Effects:**
- Clears the environment's error buffer state flag
- Clears the first byte of the error message buffer to produce an empty string

**Error Conditions:**
- Null environment pointer -> silent return, no action
- Error buffer locked (nested error handling in progress) -> silent return, preserving existing error state

**Behavioral Description:**
This function prepares the environment for a new API operation by resetting the error state so that any errors from previous operations do not persist into the new operation's results. It checks the error buffer lock flag first: if the flag indicates that the error buffer is protected (because a higher-level operation has locked it to preserve the root cause error during nested error handling), the function returns without modifying the error state. Otherwise, it resets the error state flag and clears the error message buffer to an empty string.

This function is typically called at the entry point of public API functions to ensure a clean error state before the operation begins. The error buffer lock mechanism prevents inner operations from clearing errors set by outer operations, which is important for preserving the root cause when errors cascade.

Note: Despite the name "acquire_lock," this function does not acquire a mutex or perform any synchronization. It clears error state. The name reflects a conceptual "acquisition" of the right to write new errors, gated by the locked state. See the Module-Level Behavioral Notes section for further discussion.

**Thread Safety:** Unsafe. The caller is responsible for synchronizing access to the environment's error buffer. This function does not use the environment's critical section.

**Dependencies:**
- None (direct field access on the Environment structure)

---

### cxf_get_logical_processors

**Purpose:** Return the number of logical processors (hardware threads) detected on the system, as stored in the environment.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment containing hardware detection results
- Output: int - The number of logical processors

**Preconditions:**
- The environment must be initialized (hardware detection completed during environment creation)

**Postconditions:**
- Returns the logical processor count stored in the environment, unchanged

**Side Effects:**
- None (pure accessor)

**Error Conditions:**
- None

**Behavioral Description:**
This is a simple accessor that returns the logical processor count that was detected and stored in the environment during initialization. The logical processor count represents the total number of hardware threads available, including those provided by simultaneous multithreading (e.g., Intel Hyper-Threading). On a system with 4 physical cores and 2-way simultaneous multithreading, this would return 8.

The value is detected at environment creation time using platform-specific system information APIs and may be overridden by environment variable configuration. Once set during initialization, the value is read-only for the lifetime of the environment.

**Thread Safety:** Safe. Returns an immutable value set during initialization.

**Dependencies:**
- None (direct field access on the Environment structure)

---

### cxf_get_physical_cores

**Purpose:** Return the effective number of physical CPU cores, computed as the minimum of the detected logical processor count and the detected physical core count.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment containing hardware detection results
- Output: int - The effective physical core count (minimum of logical and physical)

**Preconditions:**
- The environment must be initialized (hardware detection completed during environment creation)

**Postconditions:**
- Returns min(logicalProcessorCount, physicalCoreCount) as stored in the environment

**Side Effects:**
- None (pure accessor with a min computation)

**Error Conditions:**
- None

**Behavioral Description:**
This function returns a conservative estimate of physical CPU cores by taking the minimum of two detected values: the logical processor count and the physical core count. Under normal circumstances, the physical core count is always less than or equal to the logical processor count (since simultaneous multithreading adds logical processors beyond the physical count). However, hardware detection APIs can occasionally return inconsistent values if detection fails or returns partial results. By taking the minimum, the function ensures that the returned value never overestimates the available physical parallelism.

For CPU-bound workloads such as LP solving, physical core count is often a better guide for thread allocation than logical processor count, because simultaneous multithreading provides diminishing returns for compute-intensive tasks.

**Thread Safety:** Safe. Reads immutable values set during initialization.

**Dependencies:**
- None (direct field access on the Environment structure)

---

### cxf_get_threads

**Purpose:** Compute the effective thread count for parallel operations by reconciling the model-level override, automatic hardware detection with capping, and the user's Threads parameter.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment containing thread configuration and hardware info
- Output: int - The effective number of threads to use for parallel operations

**Preconditions:**
- The environment must be initialized with valid hardware detection results and parameter system

**Postconditions:**
- Returns a positive integer representing the number of threads to use, constrained by all applicable limits

**Side Effects:**
- None (read-only computation)

**Error Conditions:**
- None

**Behavioral Description:**
This function determines the actual number of threads the solver should use for parallel operations. It applies a hierarchy of constraints, always choosing the most restrictive:

1. **Model-level override check:** If a model-level thread override is set (value >= 1), that value is used directly as the base thread count, bypassing automatic detection.

2. **Automatic detection (when no model-level override):** The function starts with the logical processor count from the environment. If this count exceeds an internal cap (an efficiency threshold on the order of 32 threads), the function reduces the count by preferring the physical core count (if it is lower than the logical count) and then clamping to the cap. This prevents the solver from automatically using excessive threads on large servers where diminishing returns and synchronization overhead would reduce performance.

3. **User parameter application:** The function reads the user-configured Threads parameter from the environment's parameter system. If this value is less than the auto-detected count, the user's value is used. (A Threads parameter value of zero means "automatic," meaning the auto-detected value is used without further reduction.)

The function always returns the most restrictive of all applicable limits.

**Thread Safety:** Safe. Reads configuration values that are stable during optimization (the Threads parameter is not modified during a solve). The model-level thread override is set before optimization begins.

**Dependencies:**
- Parameter system lookup (reads the Threads parameter by name from the environment's parameter table)

---

### cxf_set_thread_count

**Purpose:** Validate a requested thread count against available hardware and emit a warning via the logging system if the thread count exceeds the logical processor count.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment for log output and hardware info
- Input: `thread_count` : int - The thread count to validate
- Output: void

**Preconditions:**
- The environment must be initialized with a valid logical processor count

**Postconditions:**
- If thread_count exceeds the logical processor count, a warning message has been emitted via the logging system
- If thread_count does not exceed the logical processor count, no output is produced

**Side Effects:**
- May emit warning messages via the environment's logging system (up to four log lines: a leading separator, the warning with the thread and processor counts, a suggestion to reduce the Threads parameter, and a trailing separator)

**Error Conditions:**
- None. This function always succeeds (it is purely diagnostic).

**Behavioral Description:**
This function checks whether a requested thread count exceeds the number of logical processors detected on the system. If so, it emits a formatted warning message through the environment's logging system advising the user to reduce the Threads parameter. The warning explains that thread oversubscription (more threads than processors) causes context switching overhead, cache thrashing, and memory contention, resulting in degraded performance rather than improvement.

The function does not modify the thread count or any thread-related state. It is purely a validation and diagnostic function.

Note: Despite the name "set_thread_count," this function does not set or store any thread count value. It only validates and warns. See the Module-Level Behavioral Notes section for further discussion of this misnomer.

**Thread Safety:** Conditional. Thread safety depends on the logging system's thread safety. If cxf_log is called from multiple threads, the caller must ensure the environment's logging state is properly synchronized. In practice, this function is called during solver initialization when logging is typically single-threaded.

**Dependencies:**
- Logging module: cxf_log (for emitting the warning message)

---

## Module-Level Behavioral Notes

### Naming Misnomers

This module contains several functions whose names are historically misleading. Understanding these misnomers is important for correct usage:

| Function Name | What the Name Suggests | What the Function Actually Does |
|---------------|----------------------|-------------------------------|
| cxf_acquire_solve_lock | Acquires a mutex for solving | Saves locale state and switches to "C" locale |
| cxf_release_solve_lock | Releases a mutex after solving | Restores original locale state |
| cxf_env_acquire_lock | Acquires a mutex on the environment | Clears the error buffer state |
| cxf_set_thread_count | Sets the thread count | Validates and warns about thread oversubscription |

The "lock" terminology in cxf_acquire_solve_lock / cxf_release_solve_lock may reflect a conceptual "locking" of the numeric locale into the "C" setting for the duration of optimization. The acquire/release pairing follows the RAII-like pattern of save-modify-restore, which is similar to lock/unlock semantics in resource management.

The "lock" in cxf_env_acquire_lock may reflect a conceptual "acquisition" of the right to write errors: the function checks whether the error buffer is "locked" (protected by nested error handling) and, if not, clears it for new error reporting. It "acquires" permission to write new error messages.

The "set" in cxf_set_thread_count may reflect an earlier design where the function both set and validated the thread count. In its current form, it only validates and warns.

### Locale Safety Architecture

The locale acquire/release pair (cxf_acquire_solve_lock / cxf_release_solve_lock) implements a critical safety mechanism for international LP solver deployment. Different system locales use different decimal separators:

- "C" / "POSIX" locale: period (1234.567)
- Many European locales: comma (1234,567)
- Some locales: space grouping (1 234,567)

LP and MPS file formats universally use the period as the decimal separator. If the solver were to parse or format numbers using a locale that uses commas, coefficient values would be silently corrupted. The locale safety mechanism prevents this by:

1. Saving the user's locale before optimization
2. Switching to "C" locale for all numeric operations during optimization
3. Using per-thread locale isolation so that other threads in the application (which may need their original locale for GUI display, for example) are not affected
4. Restoring the user's locale after optimization completes

The optimization-active check at the beginning of cxf_acquire_solve_lock prevents redundant save/restore cycles when the function is called from within an already-active optimization context.

### Thread Count Resolution Hierarchy

The thread count determination (cxf_get_threads) follows a principled hierarchy of constraints, where each level can only reduce the thread count, never increase it:

1. **Model-level override** (highest priority if set): Direct specification, bypasses all auto-detection
2. **Hardware detection with cap**: Logical processors, reduced by physical cores on large systems, capped at an efficiency threshold
3. **User Threads parameter**: User's explicit configuration acts as an upper bound

This "most restrictive wins" design ensures the solver never exceeds any applicable limit, whether hardware or user preference.

### Relationship to Other Modules

This module interacts with several other modules:

- **Environment Lifecycle (P3.30):** The hardware detection fields (logical processor count, physical core count) are populated during environment initialization. The Threading fields in the Environment data model (Layer 1) document these fields.
- **Logging (P3.10):** cxf_set_thread_count uses the logging system to emit oversubscription warnings.
- **Error Handling (P3.09):** cxf_env_acquire_lock operates on the error buffer, which is shared with the Error Handling module. It clears error state at the start of API operations; the Error Handling module sets error state when errors occur.
- **Memory Primitives (P3.01):** cxf_acquire_solve_lock / cxf_release_solve_lock allocate and free locale state structures.
- **Parameter System:** cxf_get_threads reads the Threads parameter from the environment's parameter table.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_acquire_solve_lock | Safe | Operates on per-thread locale state with per-thread isolation |
| cxf_release_solve_lock | Safe | Operates on per-thread locale state with per-thread isolation |
| cxf_env_acquire_lock | Unsafe | Caller must synchronize access to environment error buffer |
| cxf_get_logical_processors | Safe | Returns immutable value set during initialization |
| cxf_get_physical_cores | Safe | Reads immutable values set during initialization |
| cxf_get_threads | Safe | Reads stable configuration values |
| cxf_set_thread_count | Conditional | Depends on logging system thread safety |

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Passes the Clean Room Test
```
