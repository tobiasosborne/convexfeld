# Module: Error Handling

## Purpose

The Error Handling module provides the internal error reporting mechanism for the solver. It manages the association between numeric error codes and human-readable error messages, maintains the environment's error state (error code and error message buffer), and supports two distinct calling conventions for reporting errors from different contexts within the solver. The module implements a "first error wins" strategy that preserves the root-cause error message when multiple errors cascade during a single operation, while providing a special override for critical errors that must always be visible.

## Error Propagation Model

The solver uses a single-level error state model anchored on the Environment. Every error is ultimately stored on an Environment structure, regardless of whether the error originated from an environment-level or model-level operation. The Model does not maintain its own error state; instead, error reporting through a Model resolves to the Model's associated Environment.

The error state consists of three fields on the Environment:

- **Error code:** A numeric code identifying the error category. Set on every error report.
- **Error message buffer:** A fixed-size character buffer holding a human-readable description of the error. Written conditionally based on the overwrite policy.
- **Error buffer locked flag:** A boolean flag that, when set, prevents the error message buffer from being overwritten even when an overwrite is requested. This supports nested error handling where an outer function locks the buffer to preserve the inner function's more specific error message.

The error message is retrievable by users through the public API's "get error message" function, which simply returns the contents of the error buffer.

### First-Error Preservation

When errors cascade (e.g., an internal function fails, and its caller also reports an error), the module preserves the first error message. This is achieved through two mechanisms:

1. **Empty-buffer check:** If the error buffer already contains a message, subsequent non-overwrite error reports skip the message write while still updating the error code.
2. **Locked-buffer check:** When the buffer lock flag is set, even overwrite-mode error reports skip the message write. The lock flag is managed by higher-level code (typically around API call boundaries) to explicitly protect the error message during nested error propagation.

### Out-of-Memory Override

The out-of-memory error is treated as a critical override case. The predefined-message functions always write the out-of-memory message regardless of whether the buffer already contains a message, because memory exhaustion is frequently the root cause of subsequent errors.

## Functions

### cxf_error_env

**Purpose:** Report an error with a custom formatted message directly on an Environment.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment on which to set the error state
- Input: `error_code` : int - The numeric error code to record (must be nonzero for an error)
- Input: `overwrite` : int - Controls whether to overwrite an existing message: nonzero means attempt overwrite, zero means only write if buffer is empty
- Input: `format` : string - A printf-style format string for the error message
- Input: `...` : variadic arguments - Arguments for the format string
- Output: void

**Preconditions:**
- No preconditions beyond parameter validity; the function handles null pointers and zero error codes gracefully

**Postconditions:**
- If the environment is non-null and the error code is nonzero, the environment's error code is set to the provided value
- If the error buffer is non-null and the write conditions are met (see Behavioral Description), the error buffer contains the formatted message
- If the environment is null or the error code is zero, no state is modified

**Side Effects:**
- Modifies the environment's error code field
- May modify the environment's error message buffer

**Error Conditions:**
- Null environment pointer -> silent return, no action
- Zero error code -> silent return, no action
- Null error buffer pointer (on the environment) -> error code is set but no message is written

**Behavioral Description:**
The function first validates that the environment pointer is non-null and the error code is nonzero; if either check fails, it returns immediately. It then unconditionally sets the environment's error code to the provided value. Next, it checks the error message buffer pointer on the environment. If the buffer is non-null, it evaluates whether to write the message: the message is written if either (a) the overwrite parameter is nonzero AND the buffer lock flag is not set, or (b) the buffer is currently empty (its first character is the null terminator). If either condition is satisfied, the format string and variadic arguments are formatted into the buffer using a bounded string format operation (limited to the buffer's fixed capacity). If neither condition is satisfied, the error code is updated but the existing message is preserved.

**Thread Safety:** Unsafe. This function does not acquire any synchronization primitives. Callers accessing the same environment from multiple threads must acquire the environment's critical section before calling this function.

**Dependencies:** None (leaf function; uses only standard formatted string output).

---

### cxf_error_model

**Purpose:** Report an error with a custom formatted message via a Model, which delegates to the Model's associated Environment.

**Signature:**
- Input: `model` : pointer-to-Model - The model through which to report the error
- Input: `error_code` : int - The numeric error code to record (must be nonzero for an error)
- Input: `overwrite` : int - Controls whether to overwrite an existing message: nonzero means attempt overwrite, zero means only write if buffer is empty
- Input: `format` : string - A printf-style format string for the error message
- Input: `...` : variadic arguments - Arguments for the format string
- Output: void

**Preconditions:**
- No preconditions beyond parameter validity; the function handles null pointers gracefully

**Postconditions:**
- If all pointers are valid and the error code is nonzero, the model's environment's error code is set to the provided value
- If the write conditions are met, the environment's error buffer contains the formatted message
- If any pointer is null or the error code is zero, no state is modified

**Side Effects:**
- Modifies the model's environment's error code field
- May modify the model's environment's error message buffer

**Error Conditions:**
- Null model pointer -> silent return, no action
- Null environment pointer (extracted from model) -> silent return, no action
- Zero error code -> silent return, no action

**Behavioral Description:**
The function first validates that the model pointer is non-null, then extracts the environment reference from the model. If the environment is null or the error code is zero, it returns immediately. It then performs identical logic to cxf_error_env: unconditionally sets the error code on the environment, then conditionally writes the formatted message to the error buffer based on the same overwrite/lock/empty-buffer rules. The core error-reporting logic is identical between this function and cxf_error_env; the only difference is the additional model-to-environment indirection at the start.

**Thread Safety:** Unsafe. Same constraints as cxf_error_env. The caller is responsible for thread-safe access to the environment.

**Dependencies:** None (leaf function; identical core logic to cxf_error_env).

---

### cxf_set_error_message

**Purpose:** Set a predefined error message on the environment's error buffer based on a standard error code, using a built-in mapping from error codes to human-readable message strings.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose environment will receive the error message
- Input: `error_code` : int - A standard solver error code, or zero to clear the error buffer
- Output: void

**Preconditions:**
- The model must pass structural validation (valid sentinel, non-null)

**Postconditions:**
- If error_code is zero, the error buffer is cleared (set to empty string)
- If the error code matches a known standard error, the corresponding predefined message is written to the buffer (subject to the preservation rules below)
- If the error code does not match any known standard error, a generic message including the numeric code is written
- The error code field on the environment is NOT modified by this function (it only sets the message text)

**Side Effects:**
- May modify the environment's error message buffer contents

**Error Conditions:**
- Invalid model (fails structural validation) -> silent return, no action
- Null environment pointer (extracted from model) -> silent return, no action
- Null error buffer pointer (on the environment) -> silent return, no action

**Behavioral Description:**
The function first validates the model structure using the standard model validation check. If validation fails, it returns. It then extracts the environment and error buffer pointer from the model. If the error code is zero, the buffer is cleared to an empty string and the function returns. For nonzero error codes, the function applies the first-error preservation rule: it writes the message only if the error code is the out-of-memory error (which always overwrites) or the buffer is currently empty. If neither condition is met, the function returns without modifying the buffer. When the message is written, the function maps the error code to a predefined message string using a table of standard error codes. Each standard error code has an associated fixed message. Error codes that do not appear in the table receive a generic fallback message that includes the numeric error code value.

The standard error codes cover the following categories:
- Memory and argument errors (null argument, invalid argument, out of memory)
- Attribute and parameter errors (unknown attribute, unknown parameter, value out of range, data not available)
- Index errors (index out of range)
- Size limit errors (size limit exceeded)
- I/O errors (file read, file write, callback, node file)
- Numerical errors (numeric error)
- Model state errors (optimization in progress, duplicates, model modification)
- Quadratic programming errors (Q not PSD, non-convex QCP equality, with guidance about solver parameters)
- Network and server errors (network, job rejected, cloud, remote solver worker)
- Miscellaneous (unsupported operation, excess nonzeros, invalid piecewise objective, update mode change, tune model types)

One error code in the standard range (10015) is reserved and has no predefined message.

**Thread Safety:** Unsafe. Same constraints as cxf_error_env.

**Dependencies:**
- Model validation (structural validation check)

---

### cxf_env_set_status

**Purpose:** Set a predefined error message on the environment's error buffer based on a standard error code, operating directly on an Environment rather than through a Model.

**Signature:**
- Input: `environment` : pointer-to-Environment - The environment whose error buffer will be updated
- Input: `error_code` : int - A standard solver error code, or zero to clear the error buffer
- Output: void

**Preconditions:**
- No preconditions beyond pointer validity

**Postconditions:**
- If error_code is zero, the error buffer is cleared (set to empty string)
- If the error code matches a known standard error, the corresponding predefined message is written to the buffer (subject to the preservation rules)
- If the error code does not match any known standard error, a generic fallback message is written

**Side Effects:**
- May modify the environment's error message buffer contents

**Error Conditions:**
- Null environment pointer -> silent return, no action
- Null error buffer pointer (on the environment) -> silent return, no action

**Behavioral Description:**
This function is the environment-level equivalent of cxf_set_error_message. It operates directly on an Environment pointer rather than extracting the environment from a model. After validating the environment pointer and error buffer pointer, it applies the same logic: clearing the buffer for a zero error code, applying the first-error preservation rule (out-of-memory always overwrites; other errors only write to an empty buffer), and mapping the error code to a predefined message string from the same standard error code table. The error code-to-message mapping is identical to cxf_set_error_message.

The key difference from cxf_set_error_message is the entry point: this function does not require a Model and does not perform model validation. It is used by environment-level operations (initialization, parameter configuration) that may need to set error messages before any Model exists.

**Thread Safety:** Unsafe. The caller is responsible for thread-safe access to the environment.

**Dependencies:** None (leaf function).

---

## Module-Level Behavioral Notes

### Relationship Between the Four Functions

The four error handling functions form two pairs along two dimensions:

| | Custom message (format string) | Predefined message (code lookup) |
|---|---|---|
| **Via Environment** | cxf_error_env | cxf_env_set_status |
| **Via Model** | cxf_error_model | cxf_set_error_message |

- **Custom message functions** (cxf_error_env, cxf_error_model): Accept a printf-style format string and variadic arguments, producing a context-specific error message. Used for detailed internal error messages that include runtime values (e.g., "Failed to allocate N bytes", "Variable index K out of range").
- **Predefined message functions** (cxf_set_error_message, cxf_env_set_status): Map a standard error code to a fixed human-readable string from a built-in table. Used at API boundaries and for standard error conditions where a consistent message is preferred.
- **Environment-entry functions** (cxf_error_env, cxf_env_set_status): Operate directly on an Environment pointer. Used during environment initialization and other operations that do not have a Model context.
- **Model-entry functions** (cxf_error_model, cxf_set_error_message): Accept a Model pointer and resolve to the Model's associated Environment. Used during model manipulation, optimization, and API functions that operate on models.

### Error Code Semantics

Error codes are non-negative integers. Zero indicates success and is used as the "clear error" signal. Nonzero values identify specific error categories. The standard error codes are sequential integers in a contiguous range starting above 10000, with each code having a fixed semantic meaning and associated message string. One code in the standard range (10015) is reserved and not mapped to a message.

### Common Calling Patterns

1. **Immediate error detection:** When a function detects an error condition directly, it calls cxf_error_env or cxf_error_model with overwrite=1 (nonzero) and a descriptive format string, then returns the error code.

2. **Error propagation:** When a function calls a helper that returns an error, it may call cxf_error_env or cxf_error_model with overwrite=0 (zero) to set its own error context. Because overwrite is zero, the original error message from the helper is preserved.

3. **Standard error reporting:** At API boundaries, functions call cxf_set_error_message or cxf_env_set_status with the error code to ensure a consistent, user-facing message.

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
