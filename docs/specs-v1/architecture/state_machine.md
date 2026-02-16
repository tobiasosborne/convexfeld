# Convexfeld LP Solver Architecture

Based on established algorithms from optimization literature.

# LP Solver State Machine Architecture

**Version:** 1.0
**Status:** Draft

## 1. Overview

This document describes the state machines governing the LP solver's behavior. The solver operates with three interconnected state machines:
1. **Model State Machine**: Lifecycle from construction to solution
2. **Solver State Machine**: Algorithm execution states during optimization
3. **Basis State Machine**: Simplex basis evolution during iteration

## 2. Model State Machine

### 2.1 Model States

```
+-------------+
|  UNDEFINED  |  (model pointer not allocated)
+-------------+
       |
       | cxf_newmodel()
       v
+-------------+
| CONSTRUCTED |  - Matrix structure allocated
+-------------+  - validMagic = 0xC0FEFE1D
       |         - No variables/constraints yet
       |         - Can modify freely
       |
       | cxf_addvar(), cxf_addconstr()
       v
+-------------+
|  POPULATED  |  - Variables and constraints added
+-------------+  - pendingBuffer has queued changes
       |         - Matrix not yet built
       |         - Cannot solve without update
       |
       | cxf_updatemodel()
       v
+-------------+
|   UPDATED   |  - Matrix fully built
+-------------+  - CSC format constructed
       |         - Ready to optimize
       |         - Can still modify
       |
       | cxf_optimize()
       v
+-------------+
|   SOLVING   |  - modificationBlocked = 1
+-------------+  - SolverContext allocated
       |         - Simplex iterations running
       |         - Cannot modify model
       |         - Can query progress
       |
       | Optimization completes
       v
+-------------+
|   SOLVED    |  - modificationBlocked = 0
+-------------+  - solutionData populated
       |         - status in {CXF_OPTIMAL, INFEASIBLE, ...}
       |         - Can query solution
       |         - Can modify and re-solve
       |
       | [Optional: modify and re-solve]
       v
+-------------+
|  MODIFIED   |  - pendingBuffer has new changes
+-------------+  - Old solution invalid
       |         - Must update before re-solve
       |
       | cxf_freemodel()
       v
+-------------+
|    FREED    |  (memory deallocated, pointer invalid)
+-------------+
```

### 2.2 State Transitions

| From State | Trigger | To State | Actions |
|------------|---------|----------|---------|
| UNDEFINED | cxf_newmodel() | CONSTRUCTED | Allocate model, set magic, init structures |
| CONSTRUCTED | cxf_addvar() | POPULATED | Queue variable in pendingBuffer |
| POPULATED | cxf_updatemodel() | UPDATED | Build matrix, apply pending changes |
| UPDATED | cxf_optimize() | SOLVING | Lock model, allocate SolverContext |
| SOLVING | Optimal | SOLVED | Store solution, free SolverContext, unlock |
| SOLVING | Infeasible | SOLVED | Set status=3, free SolverContext, unlock |
| SOLVING | Unbounded | SOLVED | Set status=5, free SolverContext, unlock |
| SOLVING | Error | SOLVED | Set status=error, free SolverContext, unlock |
| SOLVING | cxf_terminate() | SOLVED | Set status=11 (interrupted), cleanup |
| SOLVED | cxf_addvar() | MODIFIED | Queue change, invalidate solution |
| MODIFIED | cxf_updatemodel() | UPDATED | Apply changes, ready for re-solve |
| ANY (except SOLVING) | cxf_freemodel() | FREED | Deallocate all memory |

### 2.3 State Validation

**Modification lock enforcement:**
```
if (model->modificationBlocked != 0) {
    return ERROR_INVALID_MODIFICATION;
}
```

Enforced by: cxf_addvar, cxf_addconstr, cxf_delvars, cxf_chgcoeffs

**Solution validity check:**
```
if (model->solutionData == NULL ||
    model->solutionData->status != CXF_OPTIMAL) {
    return ERROR_NO_SOLUTION;
}
```

Enforced by: cxf_getdblattr("X"), cxf_write(".sol")

## 3. Solver State Machine

### 3.1 Solver States

```
+--------------+
|     IDLE     |  (no active solve)
+--------------+
       |
       | cxf_solve_lp()
       v
+--------------+
| INITIALIZING |  - Allocate SolverContext
+--------------+  - Copy problem data
       |          - Allocate working arrays
       |
       | cxf_simplex_crash()
       v
+--------------+
|   CRASHING   |  - Construct initial basis heuristically
+--------------+  - Select basic/nonbasic variables
       |          - Goal: near-feasible starting point
       |
       | cxf_simplex_preprocess()
       v
+--------------+
| PREPROCESSING|  - Bound tightening
+--------------+  - Scaling
       |          - Redundancy elimination
       |
       | Check feasibility
       v
     +---------+
     | Feasible?|
     +---------+
      YES |      NO
          |       v
          |   +--------------+
          |   |  PHASE_I     |  - Minimize artificial variables
          |   +--------------+  - Drive toward feasibility
          |       |              - Modified objective
          |       |
          |       | Feasibility check
          |       v
          |     +---------+
          |     |Feasible?|
          |     +---------+
          |      YES |      NO
          |          |       v
          |          |   +--------------+
          |          |   |  INFEASIBLE  | <- Return status 3
          |          |   +--------------+
          |          v
          +---> +--------------+
                |  PHASE_II    |  - Optimize original objective
                +--------------+  - Maintain feasibility
                       |          - Pivot to improve objective
                       |
                       | Each iteration
                       v
                +--------------+
                |  ITERATING   |  - Pricing
                +--------------+  - FTRAN
                       |          - Ratio test
                       |          - Pivot
                       |          - Check termination
                       |
                       v
        +--------------+---------------+---------------+
        |              |               |               |
        v              v               v               v
+--------------+ +--------------+ +--------------+ +--------------+
|   OPTIMAL    | |  UNBOUNDED   | | ITERATION_LMT| |  INTERRUPTED |
|  (status 2)  | |  (status 5)  | |  (status 7)  | |  (status 11) |
+--------------+ +--------------+ +--------------+ +--------------+
        |              |               |               |
        +--------------+---------------+---------------+
                       |
                       v
                +--------------+
                |   REFINING   |  - Iterative refinement
                +--------------+  - Improve numerical accuracy
                       |
                       v
                +--------------+
                |   CLEANUP    |  - Restore eliminated variables
                +--------------+  - Unperturb bounds
                       |          - Unscale solution
                       |
                       v
                +--------------+
                |  FINALIZING  |  - Copy solution to model
                +--------------+  - Free SolverContext
                       |          - Release locks
                       |
                       v
                +--------------+
                |     IDLE     |  (solve complete)
                +--------------+
```

### 3.2 Phase Transitions

**Phase I -> Phase II:**

```
Condition: sum(artificial_variables) < feasibility_tolerance

Actions:
1. Remove artificial variables from basis
2. Restore original objective coefficients
3. Recompute reduced costs with original objective
4. Reset phase indicator: state->phase = 2
5. Continue iteration loop
```

**Phase II -> OPTIMAL:**

```
Condition: All reduced costs satisfy optimality conditions
  - For minimization:
    - c_bar[j] >= -optimality_tol for vars at lower bound
    - c_bar[j] <= +optimality_tol for vars at upper bound

Actions:
1. Set status = 2 (CXF_OPTIMAL)
2. Record final objective value
3. Exit iteration loop
```

**Any Phase -> UNBOUNDED:**

```
Condition: Ratio test finds no leaving variable
  - All pivot column entries are non-positive
  - Objective can be improved without bound

Actions:
1. Set status = 5 (UNBOUNDED)
2. Optionally construct unbounded ray
3. Exit iteration loop immediately
```

### 3.3 Iteration Sub-States

Within ITERATING state, each iteration follows:

```
+-------------+
|  PRICING    |  Select entering variable
+-------------+  (most negative reduced cost)
       v
+-------------+
|    FTRAN    |  Compute pivot column
+-------------+  B^-1 * A_j
       v
+-------------+
| RATIO_TEST  |  Select leaving variable
+-------------+  (minimum ratio)
       v
+-------------+
|   PIVOTING  |  Update basis
+-------------+  Swap entering <-> leaving
       v
+-------------+
|  UPDATING   |  Update data structures
+-------------+  Append eta vector
       v
Back to PRICING (next iteration)
```

## 4. Basis State Machine

### 4.1 Basis States

The simplex basis evolves through these states:

```
+-------------+
|  UNDEFINED  |  (no basis exists)
+-------------+
       |
       | cxf_simplex_crash()
       v
+-------------+
|   CRASHED   |  - Initial basis constructed heuristically
+-------------+  - May not be optimal
       |         - May not be feasible
       |         - etaCount = 0
       |
       | cxf_basis_refactor()
       v
+-------------+
|  FACTORED   |  - Fresh LU factorization
+-------------+  - B = L * U
       |         - etaCount = 0
       |         - Accurate representation
       |
       | Simplex pivot
       v
+-------------+
|   UPDATED   |  - Eta vector appended
+-------------+  - etaCount += 1
       |         - B^-1 = E_k * ... * E_1 * L^-1 * U^-1
       |
       | Multiple pivots
       v
+-------------+
|   UPDATED   |  - Multiple eta vectors
+-------------+  - etaCount = 50-200
       |         - Fill-in accumulating
       |         - Numerical error growing
       |
       | etaCount > threshold?
       | YES
       v
+-------------+
| STALE_BASIS |  - Too many etas
+-------------+  - Requires refactorization
       |         - Temporary state
       |
       | cxf_basis_refactor()
       v
+-------------+
|  FACTORED   |  - Cycle repeats
+-------------+
```

### 4.2 Basis Update Transitions

```
State: FACTORED (etaCount = 0)
       |
       | cxf_pivot_primal()
       v
       Create eta vector from pivot column:
         eta[pivot_row] = 1 / pivot_element
         eta[i] = -pivot_col[i] / pivot_element (i != pivot_row)
       v
       Append to eta list:
         eta->next = state->etaListHead
         state->etaListHead = eta
       v
       Increment counter:
         state->etaCount++
       v
State: UPDATED (etaCount = 1)

[After 100-200 pivots]

State: UPDATED (etaCount = 150)
       |
       | Check: etaCount > refactorization_threshold?
       | YES
       v
       cxf_basis_refactor()
       v
       Extract current basis matrix B
       v
       Compute fresh LU: B = L * U
       v
       Free all eta vectors
       v
       Reset: state->etaCount = 0
       v
State: FACTORED (etaCount = 0)
```

### 4.3 Variable Status States

Each variable has a status that transitions during pivots:

```
Variable i Status States:

+--------------+
|  AT_LOWER    |  value = lower_bound
|  (status -1) |  nonbasic
+--------------+
       ^ |
       | | Selected by pricing
       | v
+--------------+
|    BASIC     |  value determined by basis
| (status > 0) |  basic in row = status
+--------------+
       ^ |
       | | Selected by ratio test
       | v
+--------------+
|  AT_UPPER    |  value = upper_bound
|  (status -2) |  nonbasic
+--------------+
       ^ |
       | +----------+
       |            | Bound flip (dual)
       |            v
       |    +--------------+
       |    | SUPERBASIC   |  free variable
       +----| (status -3)  |  between bounds
            +--------------+
                    |
                    | Fix variable
                    v
            +--------------+
            |    FIXED     |  lower = upper
            |  (status -4) |  eliminated
            +--------------+
```

## 5. Trigger Conditions

### 5.1 Model State Triggers

| Trigger | Condition | Effect |
|---------|-----------|--------|
| cxf_newmodel() | Always | UNDEFINED -> CONSTRUCTED |
| cxf_addvar() | model not locked | CONSTRUCTED -> POPULATED |
| cxf_updatemodel() | pendingBuffer not empty | POPULATED -> UPDATED |
| cxf_optimize() | model updated | UPDATED -> SOLVING |
| Optimal found | all reduced costs satisfy conditions | SOLVING -> SOLVED (status 2) |
| Infeasible | Phase I fails | SOLVING -> SOLVED (status 3) |
| Unbounded | ratio test fails | SOLVING -> SOLVED (status 5) |
| Iteration limit | iterations >= IterationLimit | SOLVING -> SOLVED (status 7) |
| Time limit | elapsed >= TimeLimit | SOLVING -> SOLVED (status 9) |
| User interrupt | cxf_terminate() called | SOLVING -> SOLVED (status 11) |
| Modification | cxf_addvar/constr() after solve | SOLVED -> MODIFIED |
| cxf_freemodel() | Always | ANY -> FREED |

### 5.2 Solver State Triggers

| Trigger | Condition | Effect |
|---------|-----------|--------|
| cxf_solve_lp() entry | Always | IDLE -> INITIALIZING |
| Init complete | SolverContext allocated | INITIALIZING -> CRASHING |
| Crash complete | Initial basis constructed | CRASHING -> PREPROCESSING |
| Preprocess done | Scaling/tightening complete | PREPROCESSING -> PHASE_I or PHASE_II |
| Phase I infeasible | artificial_sum > tolerance | PHASE_I -> INFEASIBLE |
| Phase I feasible | artificial_sum <= tolerance | PHASE_I -> PHASE_II |
| Optimality | reduced costs optimal | PHASE_II -> OPTIMAL |
| Unbounded dir | ratio test fails | PHASE_II -> UNBOUNDED |
| Iteration limit | iterations >= limit | ITERATING -> ITERATION_LIMIT |
| Terminate signal | terminateFlag == 1 | ITERATING -> INTERRUPTED |
| Solve complete | status determined | ANY_TERMINAL -> REFINING |
| Refinement done | accuracy improved | REFINING -> CLEANUP |
| Cleanup done | variables restored | CLEANUP -> FINALIZING |
| Finalize done | solution copied | FINALIZING -> IDLE |

### 5.3 Basis State Triggers

| Trigger | Condition | Effect |
|---------|-----------|--------|
| cxf_simplex_crash() | Always | UNDEFINED -> CRASHED |
| cxf_basis_refactor() | Always | ANY -> FACTORED, etaCount = 0 |
| cxf_pivot_primal() | Always | FACTORED/UPDATED -> UPDATED, etaCount++ |
| etaCount > threshold | Typically threshold = 100-200 | UPDATED -> STALE_BASIS |
| Numerical issues | Accuracy degrades | Any state triggers refactor |

## 6. Recovery States

### 6.1 Error Recovery

```
+--------------+
|   SOLVING    |
+--------------+
       |
       | Error detected (e.g., OUT_OF_MEMORY)
       v
+--------------+
|  RECOVERING  |  - Capture error code
+--------------+  - Begin cleanup
       |          - Skip remaining operations
       |
       | cxf_simplex_final()
       v
+--------------+
|  PARTIAL_CLN |  - Free SolverContext
+--------------+  - Free working arrays
       |          - Preserve error code
       |
       | cxf_release_solve_lock()
       v
+--------------+
|  SOLVED      |  - status = ERROR_CODE
+--------------+  - solutionData = NULL
       |          - modificationBlocked = 0
       |
       v
Model ready for inspection/modification
```

### 6.2 Interrupt Recovery

```
+--------------+
|  ITERATING   |
+--------------+
       |
       | User calls cxf_terminate()
       v
+--------------+
| INTERRUPTED  |  - terminateFlag = 1
+--------------+  - Current iteration completes
       |          - No new iteration started
       |
       | Check flag at iteration boundary
       v
+--------------+
|  GRACEFUL    |  - Finish current pivot
+--------------+  - Don't start next pricing
       |          - status = INTERRUPTED (11)
       |
       | Normal cleanup path
       v
+--------------+
|  REFINING    |  - Solution may be suboptimal
+--------------+  - But is valid and usable
       |
       v
Complete normally with status 11
```

### 6.3 Numerical Recovery

```
+--------------+
|  ITERATING   |  etaCount = 180
+--------------+  Numerical accuracy degrading
       |
       | Detect: residual error > threshold
       v
+--------------+
| REFACTORING  |  - Pause iteration
+--------------+  - Call cxf_basis_refactor()
       |          - Recompute LU factors
       |          - Clear eta list
       |
       | Refactor complete
       v
+--------------+
|  ITERATING   |  etaCount = 0
+--------------+  Accuracy restored
       |          Continue iterations
       v
```

## 7. State Invariants

### 7.1 Model State Invariants

```
CONSTRUCTED:
  [check] model->validMagic == 0xC0FEFE1D
  [check] model->env != NULL
  [check] model->matrix != NULL
  [check] model->modificationBlocked == 0

SOLVING:
  [check] model->modificationBlocked == 1
  [check] SolverContext allocated (opaque to model)
  [check] model->solutionData may be NULL (in-progress)

SOLVED:
  [check] model->modificationBlocked == 0
  [check] model->solutionData != NULL
  [check] model->solutionData->status in {2,3,5,7,9,11,...}
  [check] SolverContext deallocated
```

### 7.2 Solver State Invariants

```
PHASE_I:
  [check] state->phase == 1
  [check] Artificial variables exist
  [check] Objective = sum(artificials)
  [check] May be infeasible

PHASE_II:
  [check] state->phase == 2
  [check] No artificial variables
  [check] Objective = original
  [check] Primal feasible (within tolerance)

ITERATING:
  [check] state->basisHeader[i] in [0, numVars) for all i
  [check] state->variableStatus[j] > 0 => j is basic
  [check] state->variableStatus[j] < 0 => j is nonbasic
  [check] state->etaCount >= 0
```

### 7.3 Basis State Invariants

```
FACTORED:
  [check] state->etaCount == 0
  [check] LU factors valid
  [check] B = L * U (exactly)
  [check] Numerical accuracy = machine precision

UPDATED:
  [check] state->etaCount > 0
  [check] state->etaListHead != NULL
  [check] Eta list forms valid linked list
  [check] Each eta corresponds to one pivot
  [check] Numerical accuracy decreases with etaCount

Variable Status:
  [check] Exactly m variables have status > 0 (basic)
  [check] Remaining n-m variables have status < 0 (nonbasic)
  [check] basisHeader[i] = j => variableStatus[j] = i
```

## 8. Timing Diagram

### 8.1 Typical Solve Timeline

```
Time
  |
  0 ------ cxf_optimize() called
  |
  |        [INITIALIZING] 0.1s
  |        Allocate, copy data
  |
  1 ------ [CRASHING] 0.2s
  |        Construct initial basis
  |
  3 ------ [PREPROCESSING] 0.5s
  |        Scale, tighten bounds
  |
  8 ------ [PHASE_II] (no Phase I needed)
  |
  |        Iteration 1-50
  |        Pricing, FTRAN, Pivot
  |        +- Refactor at iter 100
  |        Iteration 51-100
  |        +- Refactor at iter 200
  |        Iteration 101-200
  |        +- Refactor at iter 300
  |        ...
  |        Iteration 701-723
  |
  75 ----- [OPTIMAL] detected
  |
  |        [REFINING] 0.1s
  |        Improve accuracy
  |
  76 ----- [CLEANUP] 0.1s
  |        Restore variables
  |
  77 ----- [FINALIZING] 0.05s
  |        Copy solution
  |
  77.5 --- Return to user
```

### 8.2 Iteration Cycle Timing

```
One Iteration (typical):

0 ms ---- PRICING (0.5 ms)
          - BTRAN
          - Scan candidates
          - Select entering var

0.5 ms -- FTRAN (0.3 ms)
          - Apply eta vectors
          - Compute pivot column

0.8 ms -- RATIO_TEST (0.2 ms)
          - Scan basic variables
          - Find min ratio

1.0 ms -- PIVOTING (0.3 ms)
          - Update basis arrays
          - Create eta vector

1.3 ms -- UPDATING (0.2 ms)
          - Append eta
          - Update objective

1.5 ms -- Next iteration begins
```

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
