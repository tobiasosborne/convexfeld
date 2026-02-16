# Multi-Level Partial Pricing

## Published Reference

- **Primary**: Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press. Chapter 22: Partial pricing as a technique for reducing per-iteration work in the simplex method by scanning only a subset of non-basic variables.
- **Steepest edge integration**: Goldfarb, D. and Reid, J.K. (1977). "A practicable steepest-edge simplex algorithm." *Mathematical Programming*, 12(1):361-371. Provides the steepest edge pricing rule that multi-level partial pricing is designed to accelerate.
- **Devex approximation**: Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28. Defines approximate steepest edge weights used as an alternative pricing strategy within the partial pricing framework.
- **Extended steepest edge**: Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341-374. Extends steepest edge to dual simplex and discusses practical considerations for partial pricing in production solvers.
- **Computational techniques**: Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 9-10: Systematic treatment of pricing strategies and partial pricing, including sectional pricing (partitioning variables into clusters) and multiple pricing (selecting more than one candidate per scan).
- **Partial pricing in dual simplex**: Koberstein, A. (2008). "Progress in the dual simplex algorithm for solving large scale LP problems." *Computational Optimization and Applications*, 41(2):185-204. Section 4: Practical partial pricing techniques with neighbor-based expansion.

The algorithm described here is a **multi-level neighbor-expansion partial pricing scheme**. This is a refinement of Dantzig's sectional pricing (1963), which partitions variables into fixed groups and cycles through them. Instead of fixed partitions, this approach dynamically constructs candidate sets based on structural adjacency in the constraint matrix: when a variable is marked dirty, its constraint-neighbors (other variables sharing a constraint) form the natural expansion set. The multi-level structure generalizes this by allowing progressively wider neighborhoods (direct dirty set at level 0, one-hop neighbors at level 1, two-hop neighbors at level 2), echoing the spatial locality principles discussed by Koberstein (2008) for large-scale LP solvers.

## Purpose

Multi-level partial pricing reduces the per-iteration cost of the simplex pricing step. In the standard simplex method, pricing requires evaluating the reduced cost of every non-basic variable to find the best pivot candidate, costing O(n) per iteration where n is the number of variables. For large problems with tens of thousands of variables, this dominates the per-iteration cost.

Partial pricing exploits two observations:

1. **Spatial locality**: After a pivot, only variables structurally adjacent to the pivoting variable or constraint are likely to have significantly changed reduced costs. Variables in distant parts of the constraint matrix are unlikely to become attractive pivot candidates.

2. **Temporal persistence**: A variable that was a good pivot candidate in the previous iteration often remains competitive after a single basis change.

The multi-level scheme organizes this exploitation into a hierarchy: the solver first checks the smallest, cheapest candidate set (level 0: directly dirty variables), and only if that fails to produce a good pivot does it expand to wider neighborhoods (levels 1 and 2). This adaptive approach avoids paying the full O(n) pricing cost on iterations where a nearby candidate suffices, while still guaranteeing access to the full variable set when needed.

## Inputs

The algorithm operates on the following inputs, all accessed through the SolverState:

1. **Constraint matrix in dual sparse representation**: Both CSC (column-major) and CSR (row-major) formats, enabling efficient column scans (for finding constraints affected by a variable change) and row scans (for finding variables affected by a constraint change). See Maros (2003, Section 2.4) for dual representation rationale.

2. **Eta vector linked lists**: When the basis inverse is maintained via the Product Form of Inverse (Dantzig, 1963), each pivot appends an eta vector that records the affected rows and columns. The linked list structure supports the same neighbor-traversal operations as the sparse matrix, but over the basis update history rather than the original matrix.

3. **Variable status array**: An integer array with one entry per variable. Non-negative values indicate the variable is basic (the value encoding the constraint row in which it is basic); negative values indicate non-basic status at lower bound, upper bound, superbasic, or fixed. See Maros (2003, Section 3.2) for status encoding.

4. **Constraint status array**: An analogous integer array for constraints, used to filter out invalid entries during queue processing.

5. **PricingState**: The stateful data structure holding all queue and cache data for the pricing subsystem. See the PricingState data model specification for field definitions.

### Preconditions

- The basis is valid: exactly numConstrs variables have basic status.
- The constraint matrix (CSC and CSR) or eta vector linked list is initialized and consistent.
- PricingState has been allocated and initialized (all counts zero, all caches set to -1, current level set to 0).
- Problem dimensions (numVars, numConstrs) are positive.

## Outputs

The algorithm produces, for each call to the candidate retrieval function:

1. **Candidate count**: An integer indicating how many constraints or variables are in the candidate set for pricing evaluation.
2. **Candidate array**: A pointer to an array of constraint or variable indices eligible for reduced cost evaluation.

### Postconditions

- Every returned index has a valid (non-negative) status in the corresponding status array.
- No index appears more than once in the returned array.
- At level 0, the returned set is exactly the base dirty list (all directly-affected elements).
- At levels 1 and above, the returned set is a superset of the level-0 dirty list, expanded through structural neighbors.
- If the cache is populated, subsequent calls at the same level with no intervening basis change return the same result.

## Algorithm Description

### Overview

The multi-level partial pricing algorithm is organized as a producer-consumer system with three tiers. **Producers** detect which constraints and variables are affected by each basis pivot and add them to work queues. The **consumer** (the update function) processes these queues, filters out invalidated entries, manages the committed/pending split, and invalidates candidate caches. **Candidate retrieval** functions then compute and cache the actual candidate lists for the pricing evaluation step, adaptively choosing between a full scan and a partial neighbor-expansion strategy.

The system maintains two parallel queue subsystems -- one for constraints and one for variables -- reflecting the duality in simplex pricing: after a pivot, the entering variable affects constraints (which share nonzeros with it in the matrix column), while the leaving constraint affects variables (which share nonzeros with it in the matrix row).

### Detailed Steps

The algorithm is organized into five phases that repeat each simplex iteration.

#### Phase 1: Dirty Marking (Producers)

After each basis pivot, two marking operations propagate the change through the constraint matrix.

**Marking constraints dirty after a variable change**:

```
MARK-CONSTRAINTS-DIRTY(pricingState, solverState, enteringVar):
    if solverState uses eta representation:
        for each entry E in etaList[enteringVar]:
            if E is valid:
                constrIdx := constraint index from E
                ADD-TO-CONSTRAINT-QUEUES(pricingState, constrIdx)
    else:
        for each nonzero in column enteringVar of the CSC matrix:
            constrIdx := row index of the nonzero
            if constrIdx is valid:
                ADD-TO-CONSTRAINT-QUEUES(pricingState, constrIdx)
```

**Marking variables dirty after a constraint change**:

```
MARK-VARIABLES-DIRTY(pricingState, solverState, leavingConstr):
    if solverState uses eta representation:
        for each entry E in etaList[leavingConstr]:
            if E is valid:
                varIdx := variable index from E
                ADD-TO-VARIABLE-QUEUES(pricingState, varIdx)
    else:
        for each nonzero in row leavingConstr of the CSR matrix:
            varIdx := column index of the nonzero
            if varIdx is valid:
                ADD-TO-VARIABLE-QUEUES(pricingState, varIdx)
```

**Direct dirty marking** (for individual elements affected by bound changes, coefficient modifications, or other non-pivot events):

```
MARK-ELEMENT-DIRTY(pricingState, elementIdx):
    ADD-TO-QUEUES(pricingState, elementIdx)
```

#### Phase 2: Queue Insertion with Committed/Pending Split

Each queue insertion follows the same protocol, parameterized by the target queue (level 1 or level 2) and whether the corresponding level's phase is currently active. The protocol uses per-element flag bits for O(1) duplicate prevention (Maros, 2003, Section 9.3):

```
ADD-TO-QUEUES(pricingState, elementIdx):
    flags := flagArray[elementIdx]

    -- Queue at level 1 (bits 0-1)
    if flags has neither committed-1 nor pending-1 bit set:
        if level-1 phase is NOT active:
            -- Commit immediately: insert at committedCount position
            queue1[committedCount1] := elementIdx
            committedCount1 := committedCount1 + 1
            totalCount1 := totalCount1 + 1
            set committed-1 bit in flagArray[elementIdx]
        else:
            -- Defer to pending section: insert at totalCount position
            queue1[totalCount1] := elementIdx
            totalCount1 := totalCount1 + 1
            -- Do NOT set committed-1 bit yet

    -- Queue at level 2 (bits 2-3)
    if flags has neither committed-2 nor pending-2 bit set:
        if level-2 phase is NOT active:
            queue2[committedCount2] := elementIdx
            committedCount2 := committedCount2 + 1
            totalCount2 := totalCount2 + 1
            set committed-2 bit in flagArray[elementIdx]
        else:
            queue2[totalCount2] := elementIdx
            totalCount2 := totalCount2 + 1

    -- Mark pending bits for active phases
    if level-1 phase is active:
        set pending-1 bit in flagArray[elementIdx]
    if level-2 phase is active:
        set pending-2 bit in flagArray[elementIdx]
```

The queue memory layout is:

```
[committed entries | pending entries]
 ^                  ^                ^
 index 0      committedCount    totalCount
```

This design ensures that iteration over committed entries is never disrupted by concurrent insertions into the pending section.

#### Phase 3: Queue Processing and Cache Invalidation (Consumer)

Between simplex iterations, the update function processes queues at the current level. It performs three tasks: status-based filtering, pending-to-committed promotion, and cache invalidation.

```
PROCESS-QUEUES(pricingState, solverState):
    level := pricingState.currentLevel

    if level's phase has not yet been activated:
        -- First call at this level: just mark it as active
        mark level as active
        return

    if level = 0:
        -- Level 0: simple filtering by status validity
        FILTER-BY-STATUS(constrQueue[0], constrStatus, constrQueueCommitted[0])
        FILTER-BY-STATUS(varQueue[0], varStatus, varQueueCommitted[0])
        -- Set totalCount := committedCount for each queue (no pending at level 0)

    else:  -- level = 1 or 2
        -- Higher levels: filter with flag manipulation
        for each entry in constrQueue[level]:
            if constrStatus[entry] < 0:
                discard entry (status invalid)
            else if entry has pending bit for this level:
                -- Fresh entry: promote to committed
                set committed bits, clear pending bit in constrFlags[entry]
                keep entry in queue
            else:
                -- Stale entry: discard
                clear all bits for this level in constrFlags[entry]

        -- Repeat symmetric processing for varQueue[level]

        -- Invalidate all six cache slots at this level
        cachedConstrCount[level] := -1
        cachedConstrCount2[level] := -1
        cachedConstrCount3[level] := -1
        cachedVarCount[level] := -1
        cachedVarCount2[level] := -1
        cachedVarCount3[level] := -1

    mark level as active for subsequent insertions
```

**Filter-by-status** is a simple compaction: scan the queue, keeping only entries whose status in the status array is non-negative, and compact the kept entries to the front of the array.

#### Phase 4: Candidate Retrieval with Adaptive Strategy

When the simplex iteration needs candidates for pricing evaluation, it calls the candidate retrieval function. This function handles three cases:

```
GET-CANDIDATES(pricingState, solverState) -> (count, candidateArray):
    level := pricingState.currentLevel

    -- Case 1: Level 0 fast path
    if level = 0:
        return (committedCount[0], queue[0])

    -- Case 2: Cache hit
    if cachedCount[level] != -1:
        return (cachedCount[level], outputBuffer[level])

    -- Case 3: Cache miss -- compute candidate list
    SELECT-AND-COMPUTE-CANDIDATES(pricingState, solverState, level)
    return (cachedCount[level], outputBuffer[level])
```

The strategy selection for Case 3 uses three threshold checks to choose between a **full scan** and **partial neighbor expansion**:

```
SELECT-AND-COMPUTE-CANDIDATES(pricingState, solverState, level):
    n := problem dimension (numVars or numConstrs)
    dirtyCount := committedCount at this level for the cross-queue
    queueSize := totalCount at this level for the primary queue

    -- Threshold 1: Is the cross-queue already large relative to n?
    if n <= dirtyCount * EXPANSION_THRESHOLD:
        FULL-SCAN(level)
        return

    -- Threshold 2: Is the problem small relative to the queue?
    if n <= queueSize * COVERAGE_THRESHOLD:
        FULL-SCAN(level)
        return

    -- Threshold 3: Estimate expansion cost
    expansionEstimate := 0
    for each entry in cross-queue:
        nnz := number of nonzeros in the entry's row/column
        expansionEstimate := expansionEstimate + nnz

    if n < queueSize * EXPANSION_WORK_FACTOR + expansionEstimate:
        -- Expansion would visit most of n anyway
        FULL-SCAN(level)
        return

    -- All thresholds passed: use partial expansion
    PARTIAL-EXPANSION(level)
```

Where the threshold parameters are:

| Parameter | Role | Typical Value |
|-----------|------|---------------|
| EXPANSION_THRESHOLD | Controls when the cross-queue is large enough that expansion would be wasteful | A small multiplier (e.g., 2.0) |
| COVERAGE_THRESHOLD | Controls when the queue already covers enough of the problem | A fraction near 0.5 |
| EXPANSION_WORK_FACTOR | Amortization factor for expansion overhead | A small value (e.g., 5 x 10^-4) |

These thresholds ensure that partial expansion is only used when it is genuinely cheaper than a full scan. The exact values are implementation-tunable parameters that trade off between the cost of expansion bookkeeping and the savings from scanning fewer elements.

**Full scan strategy**:

```
FULL-SCAN(level):
    outputCount := 0
    for i := 0 to n - 1:
        if status[i] >= 0:
            outputBuffer[level][outputCount] := i
            outputCount := outputCount + 1
    cachedCount[level] := outputCount
```

**Partial expansion strategy**:

```
PARTIAL-EXPANSION(level):
    -- Step 1: Seed output with the base dirty set, mark in selection flags
    outputCount := 0
    for each entry in primaryQueue[level]:
        selectionFlags[entry] := 1       -- mark as selected
        outputBuffer[level][outputCount] := entry
        outputCount := outputCount + 1

    -- Step 2: Expand through structural neighbors
    for each dirtyEntry in crossQueue[level]:
        if solverState uses matrix representation:
            -- Scan the CSC column (or CSR row) for this entry
            for each neighbor in column/row of dirtyEntry:
                if neighbor is valid AND selectionFlags[neighbor] = 0:
                    selectionFlags[neighbor] := 1
                    outputBuffer[level][outputCount] := neighbor
                    outputCount := outputCount + 1
        else:
            -- Traverse eta vector linked list for this entry
            for each etaEntry linked from dirtyEntry:
                neighbor := index from etaEntry
                if etaEntry is valid AND selectionFlags[neighbor] = 0:
                    selectionFlags[neighbor] := 1
                    outputBuffer[level][outputCount] := neighbor
                    outputCount := outputCount + 1

    -- Step 3: Filter to keep only status-valid candidates, clear flags
    filteredCount := 0
    for i := 0 to outputCount - 1:
        entry := outputBuffer[level][i]
        selectionFlags[entry] := 0       -- clear for next use
        if status[entry] >= 0:
            outputBuffer[level][filteredCount] := entry
            filteredCount := filteredCount + 1

    cachedCount[level] := filteredCount
```

The selection flags array is a temporary workspace of size max(numVars, numConstrs) that provides O(1) duplicate prevention during expansion. It is zeroed incrementally (each flag is cleared during the filter step) rather than requiring a full array clear.

#### Phase 5: Level Management

The simplex iteration controls level transitions explicitly:

```
SET-LEVEL(pricingState, level):
    pricingState.currentLevel := level

END-LEVEL(pricingState, solverState):
    level := pricingState.currentLevel

    if level has not been activated:
        mark level as active
        return

    -- Filter both queues at this level (same logic as PROCESS-QUEUES)
    -- At level 0: filter by status only
    -- At levels 1-2: filter with flag promotion/demotion

    -- Invalidate all cache slots at this level (levels 1-2 only)
```

A typical simplex iteration uses the following level sequence:

1. Set level to 0; retrieve candidates; if a sufficiently attractive pivot is found, use it.
2. If level 0 yields no acceptable candidate, end level 0 and set level to 1; retrieve candidates; evaluate.
3. If level 1 also fails, end level 1 and set level to 2; retrieve candidates. Level 2 may trigger a full scan, ensuring that all variables are eventually considered.
4. After the pivot, mark the affected elements dirty (Phase 1), then process queues (Phase 3) at the active level.

### Key Design Choices

- **Three pricing levels (0, 1, 2)**: This provides a balance between granularity and overhead. Level 0 is essentially free (it returns the existing dirty list). Levels 1 and 2 progressively widen the search at increasing cost. More than three levels would add complexity without significant benefit, because two hops of structural expansion typically reach a large fraction of the problem. This is consistent with the observation by Koberstein (2008) that two expansion passes are generally sufficient for large-scale LP problems.

- **Dual queue systems (constraint + variable)**: Both primal and dual simplex require pricing from different perspectives. In primal simplex, the entering variable is selected by scanning reduced costs of non-basic variables (variable pricing). In dual simplex, the leaving variable is selected by scanning infeasibility of constraints (constraint pricing). Maintaining both queue systems allows the same partial pricing infrastructure to serve both variants without reinitialization.

- **Committed/pending split rather than double buffering**: The single-array design with a committed/pending boundary is more cache-friendly and simpler to manage than maintaining two separate arrays. It requires only two integer counters per queue per level, rather than array-swap logic.

- **Adaptive strategy selection rather than fixed strategy**: The choice between full scan and partial expansion is made dynamically based on problem characteristics, not statically at initialization. This is important because the optimal strategy depends on the density of the dirty set relative to the problem size, which changes as the solve progresses. Early in the solve, many variables may be dirty, favoring full scans; later, pivots affect fewer variables, making partial expansion more efficient.

- **Incremental flag clearing during filter step**: Rather than clearing the selection flags array with a memset before each use, flags are cleared one-by-one during the filter pass. This avoids an O(n) operation and keeps the expansion cost proportional to the actual number of candidates visited.

- **Cache invalidation embedded in the update function**: Rather than providing a separate invalidation function, cache invalidation is performed as part of queue processing. This ensures that caches are never stale when the candidate retrieval function is called, without requiring the caller to remember to invalidate manually. Level 0 does not invalidate caches because it returns the base list directly and does not use caching.

## Numerical Considerations

- **Status validity filtering**: After each basis pivot, some constraints or variables may become invalid (e.g., removed during presolve restoration, or assigned a negative status). The filtering step in queue processing removes these entries, preventing the pricing evaluator from computing reduced costs for irrelevant elements. A status value of -1 or lower indicates the element should be excluded.

- **Interaction with steepest edge pricing**: The multi-level partial pricing scheme is orthogonal to the choice of pricing rule (Dantzig, steepest edge, or Devex). It determines *which* variables to evaluate, not *how* to score them. When combined with steepest edge pricing (Goldfarb and Reid, 1977), the candidate list produced by partial pricing is passed to the steepest edge evaluator, which computes weighted reduced costs only for the candidates. This multiplicative savings -- fewer candidates times cheaper per-candidate evaluation -- is the primary performance benefit for large-scale problems.

- **Degeneracy handling**: Partial pricing does not directly address degeneracy, but the multi-level expansion helps avoid cycling that might occur if the same small set of variables is repeatedly selected. By expanding to neighbors, the algorithm introduces new candidates that may break degenerate cycling patterns.

- **Work counter accumulation**: Each operation (queue insertion, status scan, neighbor traversal, filter pass) contributes to a work counter that tracks the computational effort of the pricing subsystem. The work counter is scaled by a problem-dependent factor and is used elsewhere in the solver to trigger basis refactorization (when accumulated eta vector overhead becomes too large) or to adjust time reporting. The counter is incremented proportionally to the number of elements processed, not to the computational time, making it architecture-independent.

## Termination

- **Queue processing**: The update function always terminates because it iterates over a finite queue (bounded by numConstrs or numVars) exactly once, compacting entries in place.

- **Candidate retrieval**: The full scan strategy terminates after examining every element once. The partial expansion strategy terminates because (a) the dirty set is finite, (b) each dirty entry is expanded through a finite number of nonzeros in the sparse matrix or eta list, and (c) the selection flags prevent any element from being added more than once.

- **Level progression**: The simplex algorithm is responsible for advancing through levels. The pricing system itself does not autonomously change levels. At most three levels are used (0, 1, 2), so level progression always terminates.

- **Overall convergence**: Multi-level partial pricing does not affect the convergence guarantee of the simplex method. At the highest level (or via fallback to full scan), all variables are considered, ensuring that the simplex method can always find an improving direction if one exists. The pricing scheme only affects the order and efficiency of variable examination, not the mathematical properties of the simplex algorithm (Maros, 2003, Chapter 10).

## Complexity

### Time Complexity

| Operation | Best Case | Typical Case | Worst Case |
|-----------|-----------|--------------|------------|
| Mark dirty (single element) | O(1) | O(1) | O(1) |
| Mark dirty (cascade via column/row) | O(1) | O(nnz_col) | O(n) |
| Queue update/filter | O(1) | O(q) | O(n) |
| Candidate retrieval (level 0) | O(1) | O(1) | O(1) |
| Candidate retrieval (cache hit) | O(1) | O(1) | O(1) |
| Candidate retrieval (full scan) | O(n) | O(n) | O(n) |
| Candidate retrieval (partial expansion) | O(q) | O(q + sum of neighbor counts) | O(n) |

Where:
- n = max(numVars, numConstrs) (problem dimension)
- q = queue size at the current level (typically much smaller than n)
- nnz_col = number of nonzeros in a single column/row (depends on problem sparsity)

The key insight is that in the typical case, the cost per iteration is O(q + neighborhood size) rather than O(n), and q is usually a small fraction of n for large problems.

### Space Complexity

- Flag arrays: O(numConstrs + numVars) bytes
- Queue arrays: O(MAX_LEVELS * (numConstrs + numVars)) integers
- Output buffers: O(MAX_LEVELS * (numConstrs + numVars)) integers
- Cache counters: O(MAX_LEVELS) integers
- Selection flags (temporary): O(max(numVars, numConstrs)) integers

Total: O(MAX_LEVELS * (numConstrs + numVars)), which is O(m + n) since MAX_LEVELS is a small constant (3).

## Edge Cases

1. **Empty dirty set**: If no variables or constraints are marked dirty (e.g., at the very start of the solve before the first pivot), the candidate retrieval at level 0 returns a count of zero and a pointer to an empty array. The simplex algorithm must handle the case of no candidates by either expanding to a higher level or performing a full pricing scan outside the partial pricing framework.

2. **Single-variable or single-constraint problem**: The algorithm degenerates gracefully. With one variable, the queue can hold at most one entry, and all threshold checks will favor full scan. The overhead of multi-level management is negligible.

3. **Dense constraint matrix**: When every column has O(m) nonzeros, neighbor expansion from a single dirty variable touches O(m) constraints, making partial expansion no cheaper than full scan. The adaptive threshold check detects this situation (the expansion estimate exceeds n) and falls back to full scan.

4. **All variables dirty**: If a large fraction of variables are dirty (e.g., after a bound change that affects the entire problem), the coverage threshold check triggers a full scan, avoiding the overhead of partial expansion bookkeeping that would ultimately visit most variables anyway.

5. **Cache invalidation during iteration**: If a pivot occurs while level 1 or 2 is active, the update function invalidates all caches at the current level. Any previously cached candidate list is discarded, forcing recomputation on the next retrieval call. This ensures correctness but may temporarily increase per-iteration cost.

6. **Eta mode vs matrix mode**: The neighbor-expansion step supports two traversal modes: direct sparse matrix scanning (CSC or CSR) and eta vector linked list traversal. The mode is determined by the solver's current representation state. Both modes produce the same logical result (the set of structural neighbors), but differ in performance characteristics. Eta mode may visit fewer entries when the basis update history is short, but becomes slower as the eta list grows between refactorizations.

7. **Invalid entries in queues after basis refactorization**: After a basis refactorization, some constraint or variable indices may become invalid (status set to negative). The filtering step in queue processing removes these entries, preventing stale data from persisting across refactorization boundaries.

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
