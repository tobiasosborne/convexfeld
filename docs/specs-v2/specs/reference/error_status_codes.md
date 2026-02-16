# Error & Status Codes

## Overview

This reference document catalogs all numeric codes used by the solver to communicate error conditions and optimization outcomes. It covers two distinct code families:

1. **Error codes** -- returned by API functions to indicate failure conditions. These are non-zero integer values in the 10000+ range. A return value of zero universally indicates success.
2. **Optimization status codes** -- stored on the model after an optimization call to describe the outcome. These are small positive integers (1--19).

Both code families are part of the solver's public API and are documented in the ConvexFeld Optimizer Reference Manual (see Sources). The numeric values, constant names, and descriptions below are drawn from that public documentation.

This document also describes the error propagation model used internally, which governs how error codes and messages flow through nested function calls.

---

## Error Codes

Error codes are returned by solver functions to indicate the nature of a failure. Zero indicates success; any nonzero return value indicates an error. The standard error codes occupy a contiguous range starting at 10001.

### Memory Errors

| Name | Value | Description |
|------|-------|-------------|
| OUT_OF_MEMORY | 10001 | Available memory was exhausted. This error receives special treatment internally: it always overwrites the error message buffer regardless of existing content, because memory exhaustion is frequently the root cause of cascading failures. |

### Argument and Validation Errors

| Name | Value | Description |
|------|-------|-------------|
| NULL_ARGUMENT | 10002 | A NULL input value was provided for a required argument. |
| INVALID_ARGUMENT | 10003 | An invalid value was provided for a routine argument. |
| UNKNOWN_ATTRIBUTE | 10004 | Attempted to query or set an unknown attribute name. |
| DATA_NOT_AVAILABLE | 10005 | Attempted to query or set an attribute that is not accessible in the current model state (e.g., querying solution values before optimization). |
| INDEX_OUT_OF_RANGE | 10006 | An index argument was outside the valid range for the associated attribute or array. |
| UNKNOWN_PARAMETER | 10007 | Attempted to query or set an unknown parameter name. |
| VALUE_OUT_OF_RANGE | 10008 | Attempted to set a parameter to a value outside its valid range. |

### Model State Errors

| Name | Value | Description |
|------|-------|-------------|
| OPTIMIZATION_IN_PROGRESS | 10017 | Attempted to query or modify a model while optimization was in progress. Concurrent access to a model during optimization is not permitted. |
| DUPLICATES | 10018 | A constraint, variable, or SOS specification contained duplicated indices. |
| MODEL_MODIFICATION | 10029 | A model modification made the model invalid (e.g., deleting a variable that is the resultant of a general constraint). |

### Numerical Errors

| Name | Value | Description |
|------|-------|-------------|
| NUMERIC | 10014 | A numerical error was encountered during the requested operation. |
| Q_NOT_PSD | 10020 | The Q matrix in a QP model is not positive semi-definite. The solver may suggest setting the NonConvex parameter to allow non-convex optimization. |
| QCP_EQUALITY_CONSTRAINT | 10021 | A quadratic equality constraint was specified, which is non-convex. The solver may suggest setting the NonConvex parameter. |
| EXCEED_2B_NONZEROS | 10025 | The coefficient matrix or LU factorization has more than two billion nonzero entries, exceeding internal index limits. |

### I/O and File Errors

| Name | Value | Description |
|------|-------|-------------|
| CALLBACK | 10011 | An error occurred in a user-provided callback function. |
| FILE_READ | 10012 | Failed to read the requested file. |
| FILE_WRITE | 10013 | Failed to write the requested file. |

### Feature and Compatibility Errors

| Name | Value | Description |
|------|-------|-------------|
| IIS_NOT_INFEASIBLE | 10015 | Attempted to perform infeasibility analysis (IIS computation) on a model that is feasible. |
| NOT_SUPPORTED | 10024 | The requested feature is not supported in the current usage environment or configuration. |
| INVALID_PIECEWISE_OBJ | 10026 | A piecewise-linear objective function violated required properties (e.g., breakpoints not in non-decreasing order). |
| UPDATEMODE_CHANGE | 10027 | Attempted to modify the UpdateMode parameter after model creation, which is not permitted. |
| TUNE_MODEL_TYPES | 10031 | Attempted to tune models of different types simultaneously, which is not supported. |

### Security Errors

| Name | Value | Description |
|------|-------|-------------|
| SECURITY | 10032 | An authentication step failed or an operation was attempted without sufficient permissions. |

### Extended Error Codes

The following error codes use a higher numeric range and are used in specific subsystems:

| Name | Value | Description |
|------|-------|-------------|
| NOT_IN_MODEL | 20001 | Attempted to use a constraint or variable reference that does not belong to the target model. |
| FAILED_TO_CREATE_MODEL | 20002 | Failed to create the requested model object. |
| INTERNAL | 20003 | An internal solver error occurred. This typically indicates a bug in the solver itself. |

### Reserved Codes

Error code 10015 is assigned to IIS_NOT_INFEASIBLE in the public API.

---

## Optimization Status Codes

Optimization status codes describe the outcome of an optimization call. They are stored on the model and can be queried via the model's Status attribute. These are small positive integers.

### Successful Outcomes

| Name | Value | Description |
|------|-------|-------------|
| LOADED | 1 | Model is loaded, but no solution information is available. This is the initial state before optimization. |
| OPTIMAL | 2 | Model was solved to optimality (subject to tolerances), and an optimal solution is available. |
| SUBOPTIMAL | 13 | Unable to satisfy optimality tolerances; a sub-optimal solution is available. |

### Infeasibility and Unboundedness

| Name | Value | Description |
|------|-------|-------------|
| INFEASIBLE | 3 | Model was proven to be infeasible. No feasible solution exists. |
| INF_OR_UNBD | 4 | Model was proven to be either infeasible or unbounded. To distinguish between the two, re-solve with the InfUnbdInfo parameter enabled or with DualReductions disabled. |
| UNBOUNDED | 5 | Model was proven to be unbounded. An unbounded ray is available if requested. |
| CUTOFF | 6 | The optimal objective for the model was proven to be worse than the user-specified Cutoff parameter value. |

### Resource Limits

| Name | Value | Description |
|------|-------|-------------|
| ITERATION_LIMIT | 7 | Optimization terminated because the simplex or barrier iteration limit was reached. |
| TIME_LIMIT | 9 | Optimization terminated because the elapsed time exceeded the TimeLimit parameter. |
| WORK_LIMIT | 16 | Optimization terminated because the computational work expended exceeded the WorkLimit parameter. |
| MEM_LIMIT | 17 | Optimization terminated because allocated memory exceeded the SoftMemLimit parameter. |

### Interruption and Numerical Issues

| Name | Value | Description |
|------|-------|-------------|
| INTERRUPTED | 11 | Optimization was terminated by the user (e.g., via Ctrl-C or a callback requesting termination). |
| NUMERIC | 12 | Optimization was terminated due to unrecoverable numerical difficulties. |
| INPROGRESS | 14 | An asynchronous optimization call was made, but the associated optimization run is not yet complete. |

### Non-Convex Outcomes

| Name | Value | Description |
|------|-------|-------------|
| LOCALLY_OPTIMAL | 18 | Model was solved to local optimality satisfying first-order optimality conditions. This status is specific to non-convex models solved with the nonlinear barrier method. |
| LOCALLY_INFEASIBLE | 19 | The model appears locally infeasible; a first-order minimizer of the constraint violation was found. This status is specific to non-convex models. |

---

## Internal Simplex Return Codes

The simplex algorithm uses a separate set of small integer return codes internally to communicate iteration outcomes between sub-functions (e.g., the step function, bound propagation, and ratio test). These are not part of the public API and are translated to standard optimization status codes before being exposed to the user.

The simplex internal codes use the following semantic conventions:

| Semantic Meaning | Description |
|------------------|-------------|
| Continue | The simplex iteration completed normally; continue to the next iteration. |
| Optimal | The current basis is optimal within tolerances. |
| Infeasible | A bound inconsistency was detected (lower bound exceeds upper bound). |
| Unbounded | An improving direction was found with no finite bound (unbounded ray). |

These internal codes are mapped to the standard optimization status codes (OPTIMAL, INFEASIBLE, UNBOUNDED, etc.) by the solver's top-level dispatch and solution-processing logic before the status is recorded on the model.

---

## Variable Basis Status Codes

The simplex method assigns a basis status to each variable, indicating its role in the current basis. These are internal codes used by the basis management subsystem:

| Semantic Meaning | Description |
|------------------|-------------|
| Basic | The variable is in the basis, occupying a specific constraint row. Basic variables are encoded as non-negative integers identifying the constraint row. |
| Nonbasic at Lower Bound | The variable is at its lower bound and not in the basis. |
| Nonbasic at Upper Bound | The variable is at its upper bound and not in the basis. |
| Superbasic | The variable is between its bounds but not in the basis. This occurs with free variables or during degenerate pivots. |
| Fixed | The variable has equal lower and upper bounds (effectively a constant). |

These internal status codes are standard in simplex implementations (see Dantzig, 1963; Chvatal, 1983) and are not directly exposed through the public API. The public API exposes basis status through the VBasis and CBasis attributes using a different encoding convention.

---

## Error Propagation Patterns

The solver uses a structured approach to error propagation that ensures the root-cause error message is preserved through chains of nested function calls.

### Return Code Convention

All internal functions return zero for success and a nonzero error code for failure. Callers check the return value and propagate errors upward:

1. If a called function returns nonzero, the caller typically returns the same error code to its own caller.
2. The error code propagates up the call stack until it reaches the API boundary, where it becomes the return value visible to the user.

### First-Error Message Preservation

The solver maintains an error message buffer on the Environment structure. When multiple errors occur in sequence (e.g., an allocation failure triggers cleanup code that also encounters errors), the system preserves the first error message, which is typically the most informative:

- **Empty-buffer rule:** If the error buffer already contains a message, subsequent error reports skip the message write while still updating the numeric error code.
- **Buffer-lock rule:** Higher-level code can lock the error buffer to explicitly protect the current message from being overwritten, even by callers that request an overwrite.
- **Out-of-memory override:** The out-of-memory error always overwrites the existing message, because memory exhaustion is the most critical error and is often the root cause of subsequent failures.

### Two Reporting Paths

Error messages reach the error buffer through two mechanisms:

1. **Custom messages:** Functions can format a context-specific message (e.g., including the index that was out of range, or the size of the failed allocation). These provide the most diagnostic value.
2. **Predefined messages:** Functions can set a standard message from a built-in table indexed by error code. These provide consistent, user-facing messages at API boundaries.

Both paths respect the first-error preservation rules described above.

### Error Clearing

Setting an error code of zero clears the error message buffer. This is done at the start of each API call to reset the error state from any previous operation.

---

## Solver Method Codes

The solver supports multiple optimization algorithms, selected via the Method parameter. These codes determine which algorithm is used for continuous optimization:

| Name | Value | Description |
|------|-------|-------------|
| AUTO | -1 | Automatic method selection based on model characteristics. |
| PRIMAL_SIMPLEX | 0 | Primal simplex algorithm. |
| DUAL_SIMPLEX | 1 | Dual simplex algorithm (default for LP). |
| BARRIER | 2 | Interior point (barrier) method. |
| CONCURRENT | 3 | Concurrent optimization: run multiple algorithms in parallel. |
| DETERMINISTIC_CONCURRENT | 4 | Deterministic concurrent optimization (reproducible results). |
| CONCURRENT_3 | 5 | Concurrent optimization with three methods (primal, dual, barrier). |
| PDHG | 6 | Primal-Dual Hybrid Gradient method (LP only). |

---

## Sources

All error code values, optimization status code values, and their descriptions are drawn from the publicly documented ConvexFeld Optimizer Reference Manual:

- [Error Codes - ConvexFeld Optimizer Reference Manual](https://docs.convexfeld.com/projects/optimizer/en/current/reference/numericcodes/errors.html)
- [Optimization Status Codes - ConvexFeld Optimizer Reference Manual](https://docs.convexfeld.com/projects/optimizer/en/current/reference/numericcodes/statuscodes.html)

Variable basis status semantics reference standard LP textbooks:
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Chvatal, V. (1983). *Linear Programming*. W.H. Freeman.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All error code values traceable to public ConvexFeld API documentation
[x] All optimization status values traceable to public ConvexFeld API documentation
[x] Variable basis status described semantically, not by numeric encoding
[x] Passes the Clean Room Test (Rule 10)
```
