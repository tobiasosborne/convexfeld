# PricingState

## Purpose

PricingState manages the multi-level partial pricing subsystem for the simplex solver. In large-scale linear programming, evaluating every variable for pricing at each simplex iteration is prohibitively expensive. Partial pricing addresses this by maintaining a working set of "dirty" candidates -- variables and constraints whose reduced costs may have changed since the last iteration -- and expanding that set through structural neighbors only when needed. PricingState organizes these candidates into two parallel queue systems (one for constraints, one for variables), with each queue supporting multiple pricing levels and a committed/pending split for safe batch processing during active iterations. A single PricingState instance is owned by the SolverState and exists for the duration of one LP solve.

## Fields

### Level Management

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| currentLevel | int | The active pricing level; level 0 is the base level containing directly-affected candidates, and higher levels represent progressively wider neighborhoods | 0, 1, or 2 | Set via the level-setting function; read by all queue operations |
| levelActive | array-of-bool [MAX_LEVELS] | Per-level flag indicating whether the level's phase has been activated since the last update cycle; controls whether new entries are committed immediately or held as pending | true or false | Set to true when the update function processes a level; reset when the level is cleaned up |

### Constraint Queue System

These fields track constraints that need pricing re-evaluation after basis changes. The constraint queue is populated when a variable changes status (entering or leaving the basis), causing constraints that share nonzeros with that variable to become potentially stale.

#### Flag Array

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| constrFlags | pointer-to-array-of-byte [numConstrs] | Per-constraint membership flags tracking which queue(s) and level(s) each constraint belongs to, and whether it is in the committed or pending section | See Membership Flag Encoding below | Exactly those constraints appearing in a queue have the corresponding flag bits set |

#### Per-Level Queue Storage

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| constrQueueCommitted | array-of-int [MAX_LEVELS] | Per-level count of committed (ready-to-process) entries in the constraint queue | >= 0 | constrQueueCommitted[L] <= constrQueueTotal[L] for all levels L |
| constrQueueTotal | array-of-int [MAX_LEVELS] | Per-level total count (committed + pending) of entries in the constraint queue | >= 0 | Entries at indices [0, constrQueueCommitted) are committed; entries at [constrQueueCommitted, constrQueueTotal) are pending |
| constrQueue | array-of-pointer-to-array-of-int [MAX_LEVELS] | Per-level arrays holding the indices of dirty constraints; each array is logically split into a committed prefix and a pending suffix | Non-null during solve | Queue arrays are pre-allocated to hold up to numConstrs entries |

#### Per-Level Candidate Cache (Constraints)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| cachedConstrCount | array-of-int [MAX_LEVELS] | Per-level cached count of constraint candidates computed during the most recent candidate retrieval; a sentinel value of -1 means the cache is invalid and must be recomputed | >= 0 or -1 (uncached) | Set to -1 when the update function invalidates caches at levels 1 and above |
| cachedConstrCount2 | array-of-int [MAX_LEVELS] | Secondary per-level constraint candidate cache slot; supports caching of different candidate subsets (e.g., for different pricing strategies) | >= 0 or -1 (uncached) | Invalidated in tandem with the primary cache |
| cachedConstrCount3 | array-of-int [MAX_LEVELS] | Tertiary per-level constraint candidate cache slot | >= 0 or -1 (uncached) | Invalidated in tandem with the primary cache |
| constrOutputBuffer | array-of-pointer-to-array-of-int [MAX_LEVELS] | Per-level output buffers for materialized constraint candidate lists | Non-null during solve | Populated by the candidate retrieval function; valid only when the corresponding cachedConstrCount is not -1 |

### Variable Queue System

These fields track variables that need pricing re-evaluation after basis changes. The variable queue is populated when a constraint changes status (a pivot row leaves the basis), causing variables that share nonzeros with that constraint to become potentially stale. The structure mirrors the constraint queue system exactly.

#### Flag Array

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| varFlags | pointer-to-array-of-byte [numVars] | Per-variable membership flags tracking which queue(s) and level(s) each variable belongs to, and whether it is in the committed or pending section | See Membership Flag Encoding below | Exactly those variables appearing in a queue have the corresponding flag bits set |

#### Per-Level Queue Storage

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| varQueueCommitted | array-of-int [MAX_LEVELS] | Per-level count of committed entries in the variable queue | >= 0 | varQueueCommitted[L] <= varQueueTotal[L] for all levels L |
| varQueueTotal | array-of-int [MAX_LEVELS] | Per-level total count (committed + pending) of entries in the variable queue | >= 0 | Entries at indices [0, varQueueCommitted) are committed; entries at [varQueueCommitted, varQueueTotal) are pending |
| varQueue | array-of-pointer-to-array-of-int [MAX_LEVELS] | Per-level arrays holding the indices of dirty variables; each array is logically split into a committed prefix and a pending suffix | Non-null during solve | Queue arrays are pre-allocated to hold up to numVars entries |

#### Per-Level Candidate Cache (Variables)

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| cachedVarCount | array-of-int [MAX_LEVELS] | Per-level cached count of variable candidates computed during the most recent candidate retrieval; -1 means uncached | >= 0 or -1 (uncached) | Set to -1 when the update function invalidates caches at levels 1 and above |
| cachedVarCount2 | array-of-int [MAX_LEVELS] | Secondary per-level variable candidate cache slot | >= 0 or -1 (uncached) | Invalidated in tandem with the primary cache |
| cachedVarCount3 | array-of-int [MAX_LEVELS] | Tertiary per-level variable candidate cache slot | >= 0 or -1 (uncached) | Invalidated in tandem with the primary cache |
| varOutputBuffer | array-of-pointer-to-array-of-int [MAX_LEVELS] | Per-level output buffers for materialized variable candidate lists | Non-null during solve | Populated by the candidate retrieval function; valid only when the corresponding cachedVarCount is not -1 |

## Membership Flag Encoding

Both `constrFlags` and `varFlags` use a per-element byte with the following bit semantics. Two bits are allocated per pricing level (levels 1 and 2 use queue-pair 1 and queue-pair 2 respectively), for a total of four active bits:

| Bit Position | Meaning |
|--------------|---------|
| Bit 0 | Element is in level-1 queue, committed section |
| Bit 1 | Element is in level-1 queue, pending section (added while level was active) |
| Bit 2 | Element is in level-2 queue, committed section |
| Bit 3 | Element is in level-2 queue, pending section (added while level was active) |

**Duplicate prevention**: Before adding an element to a queue at a given level, the producer checks whether either the committed or pending bit for that level is already set. If so, the element is already tracked and is not added again.

**Phase-active marking**: When a level's phase is active, newly inserted elements receive the pending bit but not the committed bit. This allows the update function to distinguish fresh entries from stale ones during queue processing.

**Promotion semantics**: When the update function processes a level, entries with the pending bit set are promoted: the committed bit is set and the pending bit is cleared. Entries without the pending bit are discarded (their committed bits are cleared), as they are considered stale from a prior iteration.

## Relationships

- **Owned by** the SolverState. The SolverState allocates PricingState during simplex initialization and frees it during cleanup.
- **Borrows** the SolverState for read access to constraint/variable status arrays, the sparse matrix (CSC and CSR representations), eta vector linked lists, and the work counter.
- **References** the constraint matrix structure (via the SolverState) for neighbor expansion during candidate retrieval. The PricingState does not own or modify the matrix.
- **References** the basis status arrays (via the SolverState) to filter out invalid entries -- constraints or variables that have been removed from consideration are discarded during queue processing.
- The constraint queue and variable queue systems are **structurally parallel**: they use identical logic with different data arrays. The constraint queues track which constraints need re-evaluation after variable changes, while the variable queues track which variables need re-evaluation after constraint changes.

## Lifecycle

### Creation

1. PricingState is allocated as a zero-initialized block during simplex initialization.
2. The flag arrays (constrFlags, varFlags) are allocated with one byte per constraint and one byte per variable, respectively, and initialized to zero.
3. Per-level queue arrays are allocated with capacity equal to the corresponding problem dimension (numConstrs for constraint queues, numVars for variable queues).
4. Per-level output buffers for candidate caching are allocated similarly.
5. All queue counts (committed and total) are initialized to zero.
6. All cached candidate counts are initialized to -1 (uncached).
7. The current level is set to 0.
8. All level-active flags are initialized to false.

### Mutation

The PricingState undergoes the following mutations during simplex iterations:

- **Marking dirty (producers)**: When a basis pivot occurs, the entering variable triggers constraint-side marking (adding affected constraints to the constraint queues), and the leaving row triggers variable-side marking (adding affected variables to the variable queues). Two traversal modes are used depending on the solver's current representation: column/row scan through the sparse matrix, or traversal of eta vector linked lists.

- **Queue update/commit (consumer)**: After marking, the update function processes both queues at the current level. It filters out entries whose basis status is now invalid (the constraint or variable has been removed or is no longer active), promotes pending entries to committed status, discards stale committed entries, and invalidates candidate caches at levels 1 and above.

- **Level transitions**: The simplex algorithm may advance through pricing levels during an iteration. Setting a new level changes which queue tier is active. Ending a level triggers filtering of that level's queues and cache invalidation.

- **Cache population**: When the candidate retrieval function is called and the cache is invalid (-1), it computes the candidate list using one of two strategies (full scan or partial expansion), stores the result in the output buffer, and sets the cached count to a non-negative value.

- **Cache invalidation**: The update function sets all six cached counts (three constraint, three variable) to -1 at levels 1 and above, forcing recomputation on the next candidate retrieval call.

### Destruction

1. All per-level queue arrays (constraint and variable) are freed.
2. All per-level output buffer arrays are freed.
3. The constrFlags and varFlags arrays are freed.
4. The PricingState structure itself is freed.
5. The SolverState's pointer to PricingState is set to null.

## Invariants

1. **Queue count ordering**: For every level L, `constrQueueCommitted[L] <= constrQueueTotal[L]` and `varQueueCommitted[L] <= varQueueTotal[L]`.

2. **Flag-queue consistency**: An element appears in a level's queue if and only if at least one of the two flag bits for that level is set in the corresponding flags array. Conversely, if neither bit is set, the element must not appear in that level's queue.

3. **No duplicates**: Each constraint index appears at most once in any single constraint queue (across both committed and pending sections). Likewise for variable indices in variable queues. The flag bits enforce this by guarding insertion.

4. **Queue layout**: Within each queue array, committed entries occupy indices [0, committedCount) and pending entries occupy indices [committedCount, totalCount). There is no gap between sections.

5. **Cache validity**: If `cachedConstrCount[L]` is not -1, then `constrOutputBuffer[L]` contains exactly that many valid constraint indices. The same applies to all variable cache fields and secondary/tertiary cache slots.

6. **Level range**: `currentLevel` is always 0, 1, or 2. Queue operations on a level outside this range are undefined.

7. **Status filtering**: After the update function processes a queue, every remaining entry has a valid (non-negative) basis status in the SolverState. Entries with negative status are removed during filtering.

8. **Symmetric queue structure**: The constraint queue subsystem and the variable queue subsystem have identical structure and invariants, differing only in which problem dimension they index (numConstrs vs numVars).

## Thread Safety

PricingState is **not thread-safe**. It is designed as a single-threaded subsystem within the simplex solver.

- All fields are read and written without synchronization.
- Each concurrent simplex solve must have its own independent SolverState, which in turn owns its own PricingState.
- The flag arrays and queue arrays must not be accessed from multiple threads simultaneously.

## Design Rationale

**Multi-level partial pricing**: Evaluating every variable's reduced cost at each simplex iteration has O(mn) cost per iteration, which is prohibitive for large problems. Partial pricing, introduced by Goldfarb and Reid (1977), reduces this by maintaining a working set of candidates and scanning only a subset of variables. The multi-level extension allows the solver to start with a narrow set of directly-affected candidates (level 0) and progressively widen to structural neighbors (levels 1 and 2) only when the narrow set fails to produce a good pivot. This balances the cost of pricing against the quality of the selected pivot.

**Two parallel queue systems**: After a basis pivot, both constraints and variables may need re-evaluation. Constraints sharing nonzeros with the entering variable may have changed feasibility status, and variables sharing nonzeros with the leaving constraint may have changed optimality status. Maintaining separate queue systems for each allows the pricing module to answer both "which constraints are affected?" and "which variables are affected?" efficiently, which is necessary for both primal and dual simplex variants.

**Committed/pending split**: During an active pricing phase, the solver is iterating over the committed portion of a queue. If a new dirty entry is discovered during this iteration (for example, through cascade effects), it must not corrupt the iteration in progress. The pending section provides a staging area: new entries are appended after the committed boundary and are only promoted to committed status when the update function runs between iterations. This pattern is a standard producer-consumer safeguard for batch processing.

**Per-element flag bits for duplicate prevention**: Rather than searching the queue array linearly to check for membership (O(n) per insertion), the flag byte provides O(1) membership testing. Each element can be in at most two queues (one per level), requiring only four bits per element. This is a space-efficient alternative to a hash set, appropriate because the flag array is already indexed by element identity.

**Adaptive candidate retrieval strategy**: The candidate retrieval function dynamically chooses between two strategies: a full scan of all constraints or variables (when the problem is small or the queue coverage is high), and a partial expansion from the dirty set through the constraint matrix structure (when the problem is large and the dirty set is small). The decision is based on comparing the problem dimensions against the queue sizes and estimated expansion costs. This adaptive approach avoids both the overhead of full scans on large problems and the overhead of partial expansion when it would visit most of the problem anyway.

**Cache invalidation with sentinel values**: Candidate lists computed by the retrieval function are cached to avoid redundant recomputation within a single pricing phase. The sentinel value -1 signals that the cache is stale and must be recomputed. Caches are invalidated when the update function processes queues (because the underlying dirty set has changed), ensuring that stale candidate lists are never returned. Level 0 does not use caching because it returns the base dirty list directly without transformation.

**Multiple cache slots**: The three cache slots per queue per level (primary, secondary, tertiary) support caching of different candidate subsets. This accommodates pricing strategies that may need different views of the candidate set (for example, separate subsets for steepest edge pricing and Devex pricing), without requiring recomputation when switching between strategies within a single iteration.

## References

- Goldfarb, D. and Reid, J.K. (1977). "A practicable steepest-edge simplex algorithm." *Mathematical Programming*, 12(1):361-371.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. (Chapters 9-10: Pricing strategies and partial pricing.)
- Harris, P.M.J. (1973). "Pivot selection methods of the Devex LP code." *Mathematical Programming*, 5(1):1-28.
- Forrest, J.J.H. and Goldfarb, D. (1992). "Steepest-edge simplex algorithms for linear programming." *Mathematical Programming*, 57(1):341-374.
- Koberstein, A. (2008). "Progress in the dual simplex algorithm for solving large scale LP problems: techniques for a fast and stable implementation." *Computational Optimization and Applications*, 41(2):185-204. (Section 4: Partial pricing techniques.)

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
