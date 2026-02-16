# Convexfeld LP Solver Architecture

Based on established algorithms from optimization literature.

# LP Solver Data Flow Architecture

**Version:** 1.0
**Status:** Draft

## 1. Overview

This document describes how data flows through the LP solver from the initial optimization request through solution extraction. The solver follows a pipeline architecture with clear phase boundaries and well-defined data transformations at each stage.

## 2. Optimization Flow: Top-Level

### 2.1 Entry to Solution

```
User calls cxf_optimize()
         |
+---------------------------------------------------------+
| 1. VALIDATION PHASE                                      |
|    - Check model validity (magic number)                 |
|    - Validate environment initialization                 |
|    - Check modification state                            |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| 2. PREPARATION PHASE                                     |
|    - Acquire solve lock (thread safety)                  |
|    - Flush pending modifications (cxf_updatemodel)       |
|    - Build row-major matrix (if needed)                  |
|    - Detect model type (LP/QP/MIP)                       |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| 3. CALLBACK INITIALIZATION                               |
|    - Initialize callback state structure                 |
|    - Invoke pre-optimize callback                        |
|    - Set termination flags                               |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| 4. ALGORITHM SELECTION                                   |
|    - Check for quadratic terms -> Barrier                |
|    - Check for conic constraints -> Barrier              |
|    - Check user Method parameter                         |
|    - Default: Dual Simplex for LP                        |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| 5. CORE SOLVE (cxf_solve_lp)                             |
|    See "Main LP Solution Pipeline" below                 |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| 6. SOLUTION FINALIZATION                                 |
|    - Store objective value in model                      |
|    - Store variable values (X attribute)                 |
|    - Store dual values (Pi attribute)                    |
|    - Store reduced costs (RC attribute)                  |
|    - Set solution status code                            |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| 7. CLEANUP PHASE                                         |
|    - Invoke post-optimize callback                       |
|    - Write result file (if configured)                   |
|    - Release solve lock                                  |
|    - Return status code                                  |
+---------------------------------------------------------+
         |
      Return to user
```

### 2.2 Data Transformations

| Phase | Input | Output | Transformation |
|-------|-------|--------|----------------|
| Validation | CxfModel* | Error code | Integrity checks |
| Preparation | Pending changes | Updated matrix | Batch application |
| Algorithm Selection | Model characteristics | Method enum | Type detection |
| Core Solve | Matrix + bounds | Basis + solution | Simplex iterations |
| Finalization | Basis variables | Attribute arrays | Solution extraction |

## 3. Main LP Solution Pipeline

### 3.1 cxf_solve_lp Flow

```
+---------------------------------------------------------+
| INITIALIZATION                                           |
+---------------------------------------------------------+
| cxf_simplex_init()                                       |
|   -> Allocate SolverContext (1192 bytes)                 |
|   -> Copy model dimensions (m, n, nnz)                   |
|   -> Allocate working arrays:                            |
|     - lb_working, ub_working (bounds)                    |
|     - basisHeader[m] (row -> basic variable)             |
|     - variableStatus[n] (variable -> basis status)       |
|     - etaList (empty linked list)                        |
|   -> Initialize phase = 0 (undefined)                    |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| CRASH BASIS                                              |
+---------------------------------------------------------+
| cxf_simplex_crash()                                      |
|   -> Heuristically select initial basis                  |
|   -> Prioritize variables:                               |
|     1. Variables with tight bounds (fixed)               |
|     2. Variables with coefficients near objective        |
|     3. Slacks for infeasible constraints                 |
|   -> Set basisHeader[] and variableStatus[]              |
|   -> Goal: Feasible or near-feasible starting point      |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| PREPROCESSING                                            |
+---------------------------------------------------------+
| cxf_simplex_preprocess()                                 |
|   -> Bound tightening:                                   |
|     - Propagate implications (if x <= 5, then 2x <= 10)  |
|     - Detect fixed variables (lb = ub)                   |
|   -> Scaling:                                            |
|     - Compute row/column scale factors                   |
|     - Balance matrix coefficients                        |
|   -> Redundancy elimination:                             |
|     - Detect always-satisfied constraints                |
|     - Mark for removal                                   |
|   -> Update statistics (removed rows/cols)               |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| SETUP ITERATION STATE                                    |
+---------------------------------------------------------+
| cxf_simplex_setup()                                      |
|   -> Initialize pricing structures:                      |
|     - Candidate chunks (for partial pricing)             |
|     - Edge weights (for steepest edge)                   |
|   -> Compute initial reduced costs:                      |
|     c_bar = c - A^T * y (where y are dual prices)        |
|   -> Determine starting phase:                           |
|     - Phase II if basis is primal feasible               |
|     - Phase I if infeasibilities exist                   |
|   -> Allocate iteration counters                         |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| INITIAL FACTORIZATION                                    |
+---------------------------------------------------------+
| cxf_basis_refactor()                                     |
|   -> Extract basis matrix B from A                       |
|   -> Compute LU factorization: B = L * U                 |
|   -> Store L and U factors                               |
|   -> Reset eta list (empty)                              |
|   -> Mark factorization as fresh                         |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| MAIN ITERATION LOOP                                      |
+---------------------------------------------------------+
| WHILE not optimal AND iterations < limit:                |
|                                                          |
|   -> Snapshot basis (for convergence check)              |
|   -> Start iteration timer                               |
|                                                          |
|   +----------------------------------------------------+ |
|   | cxf_simplex_iterate()                              | |
|   |   -> Display progress (if logging enabled)         | |
|   +----------------------------------------------------+ |
|                                                          |
|   +----------------------------------------------------+ |
|   | PRICING: Select Entering Variable                  | |
|   | cxf_pricing_candidates()                           | |
|   |   -> BTRAN: Solve y^T B = c_B^T (dual prices)      | |
|   |   -> Compute reduced costs: c_bar[j] = c[j] - y^T A[j]| |
|   |   -> Scan candidate chunks                         | |
|   |   -> Select j with most negative c_bar[j]          | |
|   |   -> If all c_bar[j] >= 0: OPTIMAL (done)          | |
|   |   -> Return entering variable j                    | |
|   +----------------------------------------------------+ |
|                                                          |
|   +----------------------------------------------------+ |
|   | FTRAN: Compute Pivot Column                        | |
|   | cxf_ftran(state, A_j, pivot_col)                   | |
|   |   -> Solve B * pivot_col = A_j                     | |
|   |   -> Apply eta vectors (oldest to newest):         | |
|   |     pivot_col = E_k * ... * E_1 * A_j              | |
|   |   -> Return pivot_col (how entering var affects basis)| |
|   +----------------------------------------------------+ |
|                                                          |
|   +----------------------------------------------------+ |
|   | RATIO TEST: Select Leaving Variable                | |
|   | cxf_ratio_test()                                   | |
|   |   -> For each basic variable i:                    | |
|   |     - Compute step = pivot_col[i]                  | |
|   |     - If step = 0: skip (variable unaffected)      | |
|   |     - Compute bound: how far can we move?          | |
|   |       ratio = (bound - value) / step               | |
|   |     - Track minimum ratio                          | |
|   |   -> If no finite ratio: UNBOUNDED (done)          | |
|   |   -> Return leaving variable i with min ratio      | |
|   +----------------------------------------------------+ |
|                                                          |
|   +----------------------------------------------------+ |
|   | PIVOT: Update Basis                                | |
|   | cxf_pivot_primal()                                 | |
|   |   -> Swap entering j <-> leaving i                 | |
|   |   -> Update basisHeader[row] = j                   | |
|   |   -> Update variableStatus[j] = BASIC              | |
|   |   -> Update variableStatus[i] = AT_BOUND           | |
|   |   -> Create eta vector from pivot column:          | |
|   |     eta[p] = 1/pivot_element                       | |
|   |     eta[k] = -pivot_col[k]/pivot_element (k!=p)    | |
|   |   -> Append eta to list                            | |
|   |   -> Update objective value                        | |
|   |   -> Invalidate pricing cache                      | |
|   +----------------------------------------------------+ |
|                                                          |
|   -> Post-iteration processing                           |
|   -> Check termination conditions                        |
|   -> If basis stable: break                              |
|   -> If eta count high: refactorize basis                |
|                                                          |
|   -> Increment iteration counter                         |
|                                                          |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| SOLUTION REFINEMENT                                      |
+---------------------------------------------------------+
| cxf_simplex_refine()                                     |
|   -> Recompute basic variable values: x_B = B^-1 * b     |
|   -> Apply iterative refinement:                         |
|     1. Compute residual: r = b - B*x_B                   |
|     2. Solve B*dx = r                                    |
|     3. Update: x_B := x_B + dx                           |
|     4. Repeat if residual large                          |
|   -> Improves numerical accuracy                         |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| CLEANUP                                                  |
+---------------------------------------------------------+
| cxf_simplex_cleanup()                                    |
|   -> Restore eliminated variables                        |
|   -> Unperturb bounds (if perturbation was applied)      |
|   -> Unscale solution values                             |
|   -> Verify optimality conditions                        |
+---------------------------------------------------------+
         |
+---------------------------------------------------------+
| FINALIZATION                                             |
+---------------------------------------------------------+
| cxf_simplex_final()                                      |
|   -> Copy solution to model structure                    |
|   -> Deallocate all working arrays                       |
|   -> Free SolverContext                                  |
|   -> Return status code                                  |
+---------------------------------------------------------+
         |
      Return status
```

### 3.2 Data Flow Per Iteration

```
Iteration N:

  basisHeader[m]           (previous state)
  variableStatus[n]        (previous state)
  etaList                  (0 to k eta vectors)
       |
  +-------------------+
  |  BTRAN (Dual)     |
  |  y^T = c_B^T B^-1 |
  +-------------------+
       |
  dualValues[m]            (dual prices)
       |
  +-------------------+
  |  PRICING          |
  |  c_bar = c - A^T*y|
  +-------------------+
       |
  enteringVar j            (most negative reduced cost)
       |
  +-------------------+
  |  FTRAN (Primal)   |
  |  B*x = A_j        |
  +-------------------+
       |
  pivotColumn[m]           (direction of change)
       |
  +-------------------+
  |  RATIO TEST       |
  |  min ratio        |
  +-------------------+
       |
  leavingVar i             (minimum ratio)
       |
  +-------------------+
  |  PIVOT            |
  |  swap i <-> j     |
  +-------------------+
       |
  basisHeader[m]           (updated state)
  variableStatus[n]        (updated state)
  etaList + new eta        (k+1 eta vectors)
  objectiveValue           (improved)
```

## 4. Phase Transitions

### 4.1 Phase I to Phase II Transition

**Phase I Goal:** Achieve primal feasibility

```
Initial State:
  - Artificial variables added for infeasible constraints
  - Objective: minimize sum of artificials
  - Basis may be infeasible

Phase I Iterations:
  -> Run simplex with modified objective
  -> Drive artificial variables toward zero
  -> Check feasibility at each iteration

Transition Check (cxf_simplex_phase_end):
  IF sum_of_artificials < feasibility_tolerance:
    [check] Feasible solution found
    -> Remove artificial variables
    -> Restore original objective
    -> Switch to Phase II
  ELSE:
    [X] Problem is infeasible
    -> Return status = INFEASIBLE

Phase II State:
  - Original objective restored
  - All artificials removed
  - Basis is primal feasible
  - Continue simplex to optimality
```

### 4.2 Phase II Termination

```
Optimality Check (every iteration):

  FOR all nonbasic variables j:
    c_bar[j] = reduced cost

  IF all c_bar[j] >= 0 for variables at lower bound:
    AND all c_bar[j] <= 0 for variables at upper bound:
      [check] Optimal solution found
      -> Set status = CXF_OPTIMAL
      -> Exit iteration loop

Special Cases:

  IF unbounded direction detected:
    -> Set status = UNBOUNDED
    -> Exit immediately

  IF iteration limit reached:
    -> Set status = ITERATION_LIMIT
    -> Return current (possibly suboptimal) solution

  IF time limit reached:
    -> Set status = TIME_LIMIT
    -> Return current solution

  IF user termination:
    -> Set status = INTERRUPTED
    -> Clean exit
```

## 5. Error Propagation

### 5.1 Error Flow Pattern

```
Low-Level Function (e.g., cxf_malloc)
       | (returns error code)
Mid-Level Function (e.g., cxf_simplex_init)
       | (checks code, propagates if error)
High-Level Function (e.g., cxf_solve_lp)
       | (captures first error)
cxf_optimize
       | (logs error via cxf_errorlog)
Environment Error Buffer
       | (stored for retrieval)
User calls cxf_geterrormsg()
       |
Error message returned to user
```

### 5.2 Error Context Preservation

```
Function Stack:
  cxf_optimize()
    -> cxf_solve_lp()
      -> cxf_simplex_init()
        -> cxf_malloc()  [FAILS: OUT_OF_MEMORY]

Error Propagation:
  cxf_malloc:
    [X] malloc() returns NULL
    -> return 1001 (OUT_OF_MEMORY)

  cxf_simplex_init:
    [X] status = 1001
    -> cleanup partial allocations
    -> return 1001

  cxf_solve_lp:
    [X] status = 1001
    -> call cxf_simplex_final (safe, checks for NULL)
    -> return 1001

  cxf_optimize:
    [X] status = 1001
    -> call cxf_errorlog(model, 1001)
    -> set model->lastError = "Out of memory"
    -> release locks
    -> return 1001

User:
    [X] status = cxf_optimize(...)
    -> if (status != 0):
         msg = cxf_geterrormsg(env)
         -> "Out of memory"
```

### 5.3 Guaranteed Cleanup

All functions follow the pattern:
```c
int function(args) {
    int status = 0;
    void *resource = NULL;

    // Allocate
    resource = allocate();
    if (!resource) {
        status = ERROR_CODE;
        goto cleanup;
    }

    // Process
    status = operation();
    if (status) goto cleanup;

cleanup:
    // Always executed
    free(resource);
    return status;
}
```

## 6. State Changes Per Stage

### 6.1 Model State Evolution

```
State 1: CONSTRUCTED
  model->matrix populated
  model->pendingBuffer has changes
  model->solutionData = NULL

        | cxf_updatemodel()

State 2: UPDATED
  model->matrix finalized
  model->pendingBuffer empty
  model->solutionData = NULL

        | cxf_optimize()

State 3: SOLVING
  model->modificationBlocked = 1
  SolverContext allocated
  Basis evolving through iterations

        | Simplex converges

State 4: SOLVED
  model->modificationBlocked = 0
  model->solutionData populated:
    -> solutionData->status = CXF_OPTIMAL
    -> solutionData->objVal = 42.5
    -> solutionData->x[n] = solution values
    -> solutionData->pi[m] = dual values
  SolverContext freed
```

### 6.2 Solver State Evolution

```
Iteration 0:
  basisHeader = [initial basis]
  etaCount = 0
  objectiveValue = ?

Iteration 1:
  Pricing -> entering = 5
  FTRAN -> pivotCol computed
  Ratio test -> leaving = 2
  Pivot -> basis updated

  basisHeader[2] = 5  (change)
  etaCount = 1
  objectiveValue = -10.3

Iteration 2-99:
  [similar updates]
  etaCount grows to 100

Iteration 100:
  Refactorization triggered

  -> cxf_basis_refactor()
  -> New LU factors computed
  -> etaCount reset to 0
  -> objectiveValue = -45.2

Iteration 101-150:
  [continue]
  etaCount grows again

Iteration 150:
  All reduced costs satisfy optimality

  -> Status = CXF_OPTIMAL
  -> Exit iteration loop
```

## 7. Callback Data Flow

### 7.1 Callback Points

```
cxf_optimize() Entry
       |
+------------------------------+
| PRE_OPTIMIZE Callback        |
|   -> User code can query:    |
|     - model dimensions       |
|     - parameter settings     |
|   -> User code can modify:   |
|     - parameters only        |
+------------------------------+
       |
Main Solve Loop
  (No callbacks during simplex)
       |
+------------------------------+
| POST_OPTIMIZE Callback       |
|   -> User code can query:    |
|     - solution status        |
|     - objective value        |
|     - solution values        |
|     - iteration count        |
|     - runtime                |
|   -> User code can modify:   |
|     - nothing (read-only)    |
+------------------------------+
       |
Return to User
```

### 7.2 Termination Signal Flow

```
User Thread:
  cxf_optimize() running
       |
  Simplex iterations
       |
  Periodic check: if (env->terminateFlag) break

Signal Thread:
  User calls cxf_terminate(model)
       |
  Sets: env->terminateFlag = 1
       |
  Returns immediately

Solver Thread:
  Next iteration checks flag
       |
  Detects termination request
       |
  Clean exit from iteration loop
       |
  Return status = INTERRUPTED
```

## 8. Memory Data Flow

### 8.1 Allocation Hierarchy

```
CxfEnv (lifetime: program)
       |
  +-------------------------+
  | Environment pool        |
  | - Total allocated       |
  | - Memory limit          |
  +-------------------------+
       |
CxfModel (lifetime: problem)
       |
  +-------------------------+
  | Model structures        |
  | - Matrix (persistent)   |
  | - Attributes (persistent)|
  +-------------------------+
       |
cxf_optimize() (lifetime: solve)
       |
  +-------------------------+
  | SolverContext (1192 bytes)|
  | - Working arrays        |
  | - Eta list              |
  | - Basis structures      |
  +-------------------------+
       |
Iteration N (lifetime: one pivot)
       |
  +-------------------------+
  | Eta vector (~100 bytes) |
  | - Sparse representation |
  +-------------------------+
```

### 8.2 Allocation Flow

```
Allocation Request:
  cxf_malloc(size)
       |
  Check: env->memUsed + size <= env->memLimit?
       |
  YES: malloc(size)
       |
  Track: env->memUsed += size
       |
  Return: pointer

Deallocation:
  cxf_free(ptr)
       |
  Track: env->memUsed -= size(ptr)
       |
  free(ptr)
```

## 9. Attribute Data Flow

### 9.1 Query Flow

```
User: cxf_getdblattr(model, "ObjVal", &value)
       |
+------------------------------------+
| 1. Lookup attribute by name        |
|    cxf_find_attr("ObjVal")         |
|    -> returns entry index          |
+------------------------------------+
       |
+------------------------------------+
| 2. Validate type                   |
|    entry->type == DOUBLE?          |
|    [check] yes                     |
+------------------------------------+
       |
+------------------------------------+
| 3. Dispatch to getter              |
|    if (entry->directValuePtr):     |
|      value = *directValuePtr       |
|    else if (entry->scalarGetter):  |
|      entry->scalarGetter(model, &value)|
+------------------------------------+
       |
Return value to user
```

### 9.2 Computed Attributes

Some attributes are computed on demand:

```
User: cxf_getintattr(model, "IsMIP", &value)
       |
+------------------------------------+
| scalarGetter = cxf_is_mip_model    |
+------------------------------------+
       |
+------------------------------------+
| cxf_is_mip_model():                |
|   Read model->matrix->vtype array  |
|   FOR i = 0 to numVars:            |
|     IF vtype[i] in {B,I,S}:        |
|       return 1 (is MIP)            |
|   return 0 (is LP)                 |
+------------------------------------+
       |
Return computed value
```

## 10. Timing Data Flow

### 10.1 Performance Measurement

```
+------------------------------------+
| cxf_solve_lp() entry               |
|   timingState->solveStart = now()  |
+------------------------------------+
       |
+------------------------------------+
| cxf_simplex_init()                 |
|   cxf_timing_start(INIT_SECTION)   |
|   [initialization code]            |
|   cxf_timing_end(INIT_SECTION)     |
|   -> timing[INIT_SECTION] += elapsed|
+------------------------------------+
       |
+------------------------------------+
| Iteration loop                     |
|   FOR i = 1 to maxIter:            |
|     cxf_timing_start(PRICING)      |
|     [pricing code]                 |
|     cxf_timing_end(PRICING)        |
|                                    |
|     cxf_timing_start(FTRAN)        |
|     [FTRAN code]                   |
|     cxf_timing_end(FTRAN)          |
|                                    |
|     cxf_timing_start(PIVOT)        |
|     [pivot code]                   |
|     cxf_timing_end(PIVOT)          |
+------------------------------------+
       |
+------------------------------------+
| cxf_solve_lp() exit                |
|   totalTime = now() - solveStart   |
|   model->solutionData->runtime = totalTime|
+------------------------------------+
       |
Timing breakdown available:
  - Total solve time
  - Init time
  - Pricing time
  - FTRAN/BTRAN time
  - Pivot time
  - Refactorization time
```

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
