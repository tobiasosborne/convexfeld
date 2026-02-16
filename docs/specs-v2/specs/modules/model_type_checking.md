# Module: Model Type Checking

## Purpose

This module provides pure query functions that inspect a model's mathematical structure to determine its problem class and solution availability. The model type checkers classify the problem as LP, QP, or SOCP by examining properties of the model's matrix data (variable types, constraint types, and special structure counts) without modifying any state. Two additional state-checking functions inspect the solver state to determine whether active optimization state or dual solution data is available. All five functions are lightweight, side-effect-free queries used by the solver dispatch logic to route optimization to the appropriate algorithm and by the attribute system to determine what solution data can be reported.

## Functions

### cxf_is_quadratic

**Purpose:** Determines whether a model can be treated as a pure quadratic program (QP) solvable by continuous optimization methods.

**Signature:**
- Input: model : pointer-to-Model - The model to inspect
- Output: int - 1 if the model is a pure QP (has general constraints but no discrete elements), 0 otherwise

**Preconditions:**
- model may be null (returns 0 in that case)

**Postconditions:**
- The return value accurately reflects whether the model qualifies as a pure QP
- No state has been modified

**Side Effects:**
- None. This is a pure query function.

**Error Conditions:**
- If model is null, returns 0
- If the model's matrix data pointer is null, returns 0

**Behavioral Description:**
This function determines whether a model has general constraints (such as absolute value, min/max, or piecewise-linear constraints) that can be handled by continuous quadratic optimization, without any discrete elements that would require specialized handling. There are two paths to a positive result: (1) the Environment has a force-QP parameter set, which overrides all other checks and immediately returns 1, or (2) the model has general constraints present AND none of the following disqualifying elements: binary variables, indicator constraints, semi-continuous variables, semi-integer variables, nonlinear programming variables (when NLP mode is enabled in the Environment), other nonlinear elements, or multi-scenario configurations. If general constraints are absent (the model is a pure LP or has only standard linear/quadratic objective terms), the function returns 0 -- the QP classification applies only when general constraints are present that benefit from continuous QP handling.

**Thread Safety:** Safe. Read-only access to model, matrix data, and environment fields. No state modification.

**Dependencies:** None. Accesses the Model's matrix data and Environment.

---

### cxf_is_socp

**Purpose:** Determines whether a model contains any second-order cone programming (SOCP) elements, regardless of whether the model can be solved purely as an SOCP.

**Signature:**
- Input: model : pointer-to-Model - The model to inspect
- Output: int - 1 if the model contains SOCP-related elements, 0 otherwise

**Preconditions:**
- model may be null (returns 0 in that case)

**Postconditions:**
- The return value reflects the presence of any SOCP-related model elements
- No state has been modified

**Side Effects:**
- None. This is a pure query function.

**Error Conditions:**
- If model is null, returns 0
- If the model's matrix data pointer is null, returns 0

**Behavioral Description:**
This function checks whether the model contains any elements associated with second-order cone programming. It returns 1 if ANY of the following are present: the Environment's force-SOCP parameter is set, quadratic constraints exist, explicit cone constraints exist, indicator constraints exist, multi-scenario configurations exist, semi-continuous variables exist, semi-integer variables exist, NLP variables exist (when NLP mode is enabled), or other nonlinear elements exist. Unlike cxf_is_socp_internal, this function does not distinguish between disqualifiers and indicators -- all checked fields are treated as SOCP presence indicators. This function is used for model statistics reporting and solution display purposes rather than solver routing.

**Thread Safety:** Safe. Read-only access to model, matrix data, and environment fields. No state modification.

**Dependencies:** None. Accesses the Model's matrix data and Environment.

---

### cxf_is_socp_internal

**Purpose:** Determines whether a model is suitable for solving as a pure second-order cone program (SOCP) by checking for both the presence of cone elements and the absence of disqualifying elements.

**Signature:**
- Input: model : pointer-to-Model - The model to inspect
- Output: int - 1 if the model can be solved as pure SOCP, 0 otherwise

**Preconditions:**
- model may be null (returns 0 in that case)

**Postconditions:**
- The return value reflects whether the model qualifies for pure SOCP solving
- No state has been modified

**Side Effects:**
- None. This is a pure query function.

**Error Conditions:**
- If model is null, returns 0
- If the model's matrix data pointer is null, returns 0

**Behavioral Description:**
This function performs a two-stage check to determine if a model can be solved as a pure SOCP. First, it checks for disqualifying elements that prevent SOCP treatment: semi-continuous variables, semi-integer variables, NLP variables (when NLP mode is enabled in the Environment), and other nonlinear elements. If any disqualifier is present, the function immediately returns 0. Second, it checks for SOCP indicators: the Environment's force-SOCP parameter, quadratic constraints (which can represent rotated cones), explicit cone constraints, indicator constraints, and multi-scenario configurations. If at least one indicator is present and no disqualifiers were found, the function returns 1. If no indicators are present either, the function returns 0. This two-stage logic -- disqualify first, then confirm -- ensures that models with complex elements that cannot be handled by the SOCP solver are rejected even if they happen to contain cone-like structures.

**Thread Safety:** Safe. Read-only access to model, matrix data, and environment fields. No state modification.

**Dependencies:** None. Accesses the Model's matrix data and Environment.

---

### cxf_check_model_flags1

**Purpose:** Checks whether a model has active optimization state, specifically a concurrent solve state or a valid basis factorization from a previous solve.

**Signature:**
- Input: model : pointer-to-Model - The model to check
- Output: int - 1 if active state exists, 0 otherwise

**Preconditions:**
- model may be null (returns 0 in that case)

**Postconditions:**
- The return value reflects the current state of the model
- No state has been modified

**Side Effects:**
- None. This is a pure query function.

**Error Conditions:**
- If model is null, returns 0

**Behavioral Description:**
This function checks whether a model has retained optimization state that may be relevant for warm-starting or that indicates an operation is in progress. It checks two conditions: (1) whether the model has an active concurrent solve state (a non-null pointer in the model's concurrent solve field), and (2) whether the model has a SolverState with an active flag set to a positive value and a non-null basis factorization pointer. If either condition is true, the function returns 1. This is used to determine whether certain operations (such as model modifications) are permitted or whether warm-start data is available from a previous solve.

**Thread Safety:** Safe. Read-only access to model and solver state fields. No state modification.

**Dependencies:** None. Accesses the Model's concurrent solve pointer and SolverState fields (active flag and basis factorization pointer).

---

### cxf_check_model_flags2

**Purpose:** Checks whether a model has available dual solution data (shadow prices, reduced costs) from a completed solve.

**Signature:**
- Input: model : pointer-to-Model - The model to check
- Output: int - 1 if dual data is available, 0 otherwise

**Preconditions:**
- model may be null (returns 0 in that case)

**Postconditions:**
- The return value reflects the current availability of dual solution data
- No state has been modified

**Side Effects:**
- None. This is a pure query function.

**Error Conditions:**
- If model is null, returns 0

**Behavioral Description:**
This function determines whether dual solution data (such as shadow prices and reduced costs) can be retrieved from the model's current solver state. It checks three conditions that must all be met: (1) the model has a non-null SolverState, (2) the SolverState has a non-null dual data pointer, and (3) the SolverState's status code indicates a solve outcome that produces dual information AND the active flag is positive. Dual data is available when the status indicates an optimal solution, an infeasibility proof, or an infeasible-or-unbounded determination. These are the solve outcomes for which the LP solver computes valid dual values as part of the optimality or infeasibility certificate. If all conditions are met, the function returns 1; otherwise it returns 0.

**Thread Safety:** Safe. Read-only access to model and solver state fields. No state modification.

**Dependencies:** None. Accesses the Model's SolverState fields (active flag, status code, and dual data pointer).

---

## Module-Level Design Notes

### Solver Dispatch Architecture

The three model type checking functions (cxf_is_quadratic, cxf_is_socp_internal, cxf_is_socp) form the classification layer that the optimizer uses to route a problem to the appropriate solver algorithm. The dispatch logic typically evaluates these functions in a priority order:

1. **cxf_is_socp_internal**: If true, route to the SOCP/barrier solver
2. **cxf_is_quadratic**: If true, route to the QP solver
3. **Default**: Route to the LP simplex or barrier solver

cxf_is_socp (the public variant) is used for reporting and display rather than dispatch.

### Two-Level SOCP Classification

The system provides two distinct SOCP-checking functions because the needs of solver dispatch and model reporting differ:

- **cxf_is_socp_internal** (for dispatch): Must confirm the model can actually be solved as pure SOCP. Returns 1 only when cone elements are present AND no disqualifying elements exist. Uses a "reject first, then confirm" pattern.
- **cxf_is_socp** (for reporting): Only needs to know whether SOCP-related elements exist in the model, regardless of whether they can be solved as pure SOCP. All checked fields are treated as presence indicators.

This distinction is important because a model can "have SOCP elements" (for reporting purposes) while simultaneously being disqualified from SOCP solving due to the presence of semi-continuous variables or other non-SOCP elements.

### Environment Parameter Overrides

Both cxf_is_quadratic and the cxf_is_socp functions check for environment-level force parameters (force-QP and force-SOCP) that override the automatic detection logic. When set, these parameters cause the functions to return 1 immediately regardless of the model's actual content. This allows users to override the solver's automatic routing for testing or specialized use cases.

### Model Properties Inspected

The type-checking functions inspect the following categories of model properties from MatrixData:

| Category | Properties Checked |
|----------|-------------------|
| **Discrete variables** | Semi-continuous variable count, semi-integer variable count |
| **Special constraints** | SOS constraint count, indicator constraint count, general constraint count, quadratic constraint count, explicit cone count |
| **Nonlinear elements** | NLP variable count (conditional on NLP mode), other nonlinear element count, piecewise-linear objective count |
| **Configuration flags** | Optimization flag, force-non-convex flag, multi-scenario count |
| **Solve state** | Solution status, solver active flag, dual data availability, basis factorization availability |

### Pure Query Guarantee

All five functions in this module are guaranteed to be side-effect free. They perform no memory allocation, no state modification, and no I/O. They read only from existing model structures and return a boolean-like integer. This makes them safe to call from any context, including callbacks, attribute getters, and concurrent read operations.

## References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press. (LP classification and simplex optimality conditions.)
- Lobo, M.S., Vandenberghe, L., Boyd, S., and Lebret, H. (1998). "Applications of Second-Order Cone Programming." *Linear Algebra and its Applications*, 284(1-3):193-228. (SOCP problem classification and solver routing.)

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
