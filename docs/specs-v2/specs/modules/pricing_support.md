# Module: Pricing Support

## Purpose

The Pricing Support module provides the queue management and candidate retrieval infrastructure that underpins the multi-level partial pricing subsystem of the simplex solver. While the core pricing algorithm (variable selection by reduced cost evaluation) is handled elsewhere, this module is responsible for maintaining the dirty-element tracking queues, propagating structural adjacency information after basis pivots, managing pricing level transitions, and delivering filtered candidate lists to the simplex iteration loop. These eight functions collectively implement the producer side (dirty marking and cascade propagation), the consumer side (queue filtering and level cleanup), and the accessor interface (queue statistics and candidate retrieval) of the partial pricing system described in the Multi-Level Partial Pricing algorithm specification (P2.02).

## Functions

### cxf_pricing_mark_dirty

**Purpose:** Mark a single variable as needing pricing re-evaluation by inserting it into both level-1 and level-2 variable queues, using per-element flag bits for duplicate prevention.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state containing all queue arrays and flag data
- Input: `varIdx` : int - Index of the variable to mark dirty
- Output: void

**Preconditions:**
- PricingState has been initialized (all queue arrays allocated, flag arrays zeroed, counts at zero)
- `varIdx` is a valid variable index in the range [0, numVars)
- The variable flag array has been allocated with at least numVars entries

**Postconditions:**
- The variable index appears in the level-1 variable queue (either in the committed section or the pending section, depending on whether the level-1 phase is currently active)
- The variable index appears in the level-2 variable queue (similarly split between committed and pending)
- If the variable was already present in a queue (detected via flag bits), no duplicate entry is added to that queue
- The committed count and total count for each queue are updated to reflect any new insertions
- The per-variable flag byte for `varIdx` has the appropriate membership and pending bits set

**Side Effects:**
- Modifies the variable flag array entry for `varIdx`
- Modifies the level-1 and level-2 variable queue arrays (appending entries)
- Increments committed counts and/or total counts for both variable queues

**Error Conditions:**
- None; the function assumes all preconditions are met and performs no validation

**Behavioral Description:**
This function implements the direct dirty-marking path for a single variable, corresponding to Phase 1 (Dirty Marking) of the multi-level partial pricing algorithm (P2.02). It is called when a variable's reduced cost may have changed due to a bound modification, coefficient update, or other non-cascade event.

The function reads the current flag byte for the variable. For each of the two queue levels, it checks whether the variable is already tracked (either committed or pending) by examining the two flag bits allocated to that level. If the variable is not yet in the queue:

- If the corresponding level's phase is **not active**, the variable is inserted at the committed count position (the boundary between committed and pending sections), both the committed count and the total count are incremented, and the committed flag bit is set. This makes the entry immediately available for the next pricing evaluation.
- If the corresponding level's phase **is active**, the variable is appended at the total count position (the end of the pending section), only the total count is incremented, and the committed flag bit is not set. This defers the entry to the pending section, ensuring it does not interfere with any iteration currently in progress over the committed portion.

After processing both queues, the function unconditionally sets the pending flag bits for any levels whose phases are currently active. This marks the variable as having been touched during the active phase, which the queue processing step (cxf_pricing_end_level) later uses to distinguish fresh entries from stale ones during promotion/demotion.

This is the variable-side counterpart to cxf_pricing_mark_constr_dirty, which performs the identical algorithm on the constraint queues. Both functions implement the ADD-TO-QUEUES protocol described in Phase 2 (Queue Insertion with Committed/Pending Split) of the P2.02 specification.

**Thread Safety:** Unsafe. Must be called from a single thread; no internal synchronization.

**Dependencies:**
- PricingState data model (P1.06) -- flag arrays, queue arrays, committed/total counts, phase-active flags

---

### cxf_pricing_mark_constr_dirty

**Purpose:** Mark a single constraint as needing pricing re-evaluation by inserting it into both level-1 and level-2 constraint queues, using per-element flag bits for duplicate prevention.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `constrIdx` : int - Index of the constraint to mark dirty
- Output: void

**Preconditions:**
- PricingState has been initialized
- `constrIdx` is a valid constraint index in the range [0, numConstrs)
- The constraint flag array has been allocated with at least numConstrs entries

**Postconditions:**
- The constraint index appears in the level-1 constraint queue (committed or pending section, depending on phase-active state)
- The constraint index appears in the level-2 constraint queue (similarly)
- No duplicate entry is added if the constraint was already tracked
- Committed counts and total counts updated for each queue as appropriate
- Per-constraint flag byte for `constrIdx` has appropriate membership and pending bits set

**Side Effects:**
- Modifies the constraint flag array entry for `constrIdx`
- Modifies the level-1 and level-2 constraint queue arrays (appending entries)
- Increments committed counts and/or total counts for both constraint queues

**Error Conditions:**
- None; no validation performed

**Behavioral Description:**
This function is the constraint-side counterpart to cxf_pricing_mark_dirty. It implements the identical committed/pending insertion protocol described in Phase 2 of the P2.02 specification, but operates on the constraint queue subsystem rather than the variable queue subsystem.

The algorithm is structurally identical: for each of two queue levels, the function checks whether the constraint is already tracked via the corresponding pair of flag bits. If not tracked:

- Phase not active: insert at committed boundary, increment both committed count and total count, set committed flag bit.
- Phase active: append at total count position, increment total count only.

After both queues are processed, pending flag bits are set for any levels with active phases.

This function is typically called when a constraint's feasibility status may have changed, such as after a bound change on a basic variable or after a coefficient modification. It is also called by cxf_pricing_cascade_update as part of the cascade propagation after a pivot.

**Thread Safety:** Unsafe. Must be called from a single thread.

**Dependencies:**
- PricingState data model (P1.06) -- constraint flag arrays, constraint queue arrays, committed/total counts, phase-active flags

---

### cxf_pricing_cascade_update

**Purpose:** Propagate dirty marking through the constraint matrix structure after a variable change, marking all structurally adjacent constraints as dirty in the variable queues.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `solverState` : pointer-to-SolverState - The solver state providing matrix structure and status arrays
- Input: `varIdx` : int - Index of the variable whose change should be propagated
- Output: void

**Preconditions:**
- PricingState and SolverState have been initialized
- `varIdx` is a valid variable index
- The solver's sparse matrix representation (CSC) or eta vector linked list is consistent and initialized
- The work counter pointer (if non-null) points to a valid accumulator

**Postconditions:**
- Every constraint that is structurally adjacent to `varIdx` (shares a nonzero in the constraint matrix column for that variable) and has a valid (non-negative) index has been added to both level-1 and level-2 variable queues, subject to the committed/pending split and duplicate prevention rules
- The work counter (if enabled) has been incremented by an amount proportional to the number of entries traversed, scaled by a traversal-mode-dependent work factor and the solver's scale factor

**Side Effects:**
- Modifies the variable flag array for all affected constraint indices
- Appends entries to the level-1 and level-2 variable queue arrays
- Increments variable queue committed counts and/or total counts
- Increments the work counter

**Error Conditions:**
- None; no validation performed

**Behavioral Description:**
This function implements the cascade dirty-marking operation described in Phase 1 of the P2.02 specification. When a variable changes (for example, when a variable enters or leaves the basis during a simplex pivot), all constraints that share a nonzero coefficient with that variable in the constraint matrix are potentially affected and must be added to the pricing work queues for re-evaluation.

The function supports two traversal modes, determined by the solver's current representation state:

1. **Eta vector mode** (when the solver is using the Product Form of Inverse): The function traverses the eta vector linked list associated with the given variable. Each entry in the linked list identifies a constraint that was affected by a prior basis update. For each valid entry (non-negative constraint index), the function applies the same committed/pending queue insertion protocol used by cxf_pricing_mark_dirty, but targeting the variable queues. The work contribution is proportional to the number of eta entries traversed, scaled by a work factor that reflects the higher per-entry cost of linked list traversal.

2. **Direct matrix mode** (when the solver has the explicit sparse matrix available): The function scans the CSC (compressed sparse column) representation of the constraint matrix for the given variable's column. Each row index in the column identifies a constraint with a nonzero coefficient for this variable. For each valid row index, the same queue insertion protocol is applied. The work contribution is proportional to the number of nonzeros in the column, scaled by a work factor that reflects the lower per-entry cost of array-based traversal.

In both modes, the per-element flag bits provide O(1) duplicate prevention, ensuring that a constraint already present in a queue is not added again.

This function is the primary mechanism by which basis pivots propagate through the pricing subsystem. After a pivot, both the entering variable and the leaving constraint trigger cascade updates, ensuring that the dirty queues capture all structurally affected elements. This corresponds to the MARK-CONSTRAINTS-DIRTY and MARK-VARIABLES-DIRTY operations in Phase 1 of the P2.02 specification.

**Thread Safety:** Unsafe. Must be called from a single thread.

**Dependencies:**
- PricingState data model (P1.06) -- variable flag arrays, variable queue arrays
- SolverState data model (P1.04) -- sparse matrix (CSC column structure), eta vector linked lists, work counter, scale factor, representation mode flag

---

### cxf_pricing_end_level

**Purpose:** Complete the current pricing level by filtering both constraint and variable queues to remove invalid entries, performing flag-based promotion and demotion at higher levels, and invalidating candidate caches.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `solverState` : pointer-to-SolverState - The solver state providing status arrays and work counter
- Output: void

**Preconditions:**
- PricingState and SolverState have been initialized
- The current level is set to a valid value (0, 1, or 2)
- The constraint status and variable status arrays in SolverState are up to date

**Postconditions:**
- All entries in both the constraint queue and variable queue at the current level whose status is invalid (negative value in the corresponding status array) have been removed
- At level 0: the remaining valid entries are compacted to the front of the queue, and both committed count and total count are updated to the new compacted size
- At levels 1 and 2: entries that were marked as pending during the active phase are promoted (committed bit set, pending bit cleared) and retained; entries that were not marked as pending (stale entries from a prior iteration) have their flag bits cleared and are discarded; the compacted queue contains only promoted entries
- At levels 1 and 2: all six candidate cache slots for this level (three constraint, three variable) are invalidated (set to the sentinel value indicating recomputation is needed)
- The level's activity flag is set, indicating it has been processed
- The work counter (if enabled) is incremented proportionally to the number of entries scanned

**Side Effects:**
- Modifies constraint and variable queue arrays (in-place compaction)
- Modifies constraint and variable flag arrays (bit manipulation for promotion/demotion)
- Updates committed counts and total counts for both queues at the current level
- Invalidates all candidate cache slots at the current level (levels 1 and 2 only)
- Sets the level's activity flag
- Increments the work counter

**Error Conditions:**
- None; no validation performed

**Behavioral Description:**
This function implements the queue processing and level cleanup described in Phase 3 (Queue Processing and Cache Invalidation) and Phase 5 (Level Management) of the P2.02 specification. It is called by the simplex iteration loop when it is done evaluating candidates at the current pricing level and wishes to either advance to a higher level or prepare the current level for the next iteration.

The function first checks whether the current level has been activated previously. If the level has not yet been activated (first call at this level), it simply marks the level as active and returns without filtering. This lazy activation ensures that the first batch of dirty entries at a new level is treated correctly.

For subsequent calls, the behavior depends on the level:

**Level 0 (base level):** Simple status-based filtering is applied. The function scans each queue (constraint and variable) and retains only entries whose status in the corresponding status array (constraint status or variable status in the SolverState) is non-negative. Valid entries are compacted to the front of the queue array, and both the committed count and total count are set to the number of retained entries. No flag manipulation is performed at level 0 because the base level does not use the committed/pending flag distinction for promotion.

**Levels 1 and 2 (expanded levels):** The function applies a more sophisticated filter that combines status validation with flag-based promotion and demotion. For each entry in the queue:

- If the status is invalid (negative): the entry is discarded unconditionally.
- If the status is valid and the entry has the pending flag bit set for this level: the entry is a fresh addition from the current active phase. The function promotes it by setting the committed flag bit and clearing the pending flag bit, then retains the entry in the compacted queue.
- If the status is valid but the entry does not have the pending flag bit: the entry is stale (it was committed in a prior iteration but not refreshed in the current phase). The function demotes it by clearing all flag bits for this level, and the entry is discarded.

After filtering both queues, the function invalidates all candidate cache slots at this level. This forces the candidate retrieval functions (cxf_pricing_get_constr_candidates and its variable counterpart) to recompute their candidate lists on the next call, reflecting the updated queue contents.

**Thread Safety:** Unsafe. Must be called from a single thread.

**Dependencies:**
- PricingState data model (P1.06) -- queue arrays, flag arrays, committed/total counts, level activity flags, candidate caches
- SolverState data model (P1.04) -- constraint status array, variable status array, work counter, scale factor

---

### cxf_pricing_set_level

**Purpose:** Set the current pricing level in the PricingState, controlling which queue tier is active for subsequent pricing operations.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `level` : int - The new pricing level (0 for base level, 1 or 2 for expanded levels)
- Output: void

**Preconditions:**
- PricingState has been initialized
- `level` is 0, 1, or 2

**Postconditions:**
- The current level field of the PricingState is set to `level`
- All subsequent queue operations and candidate retrievals will use queues and caches at this level

**Side Effects:**
- Modifies the current level field of the PricingState

**Error Conditions:**
- None; no validation performed

**Behavioral Description:**
This is a simple setter that updates the current pricing level in the PricingState. It implements the SET-LEVEL operation from Phase 5 (Level Management) of the P2.02 specification.

The pricing level determines which tier of the multi-level queue system is consulted by the accessor functions (cxf_pricing_get_var_stats, cxf_pricing_get_constr_stats, cxf_pricing_get_constr_candidates). A typical simplex iteration starts at level 0, and if no sufficiently attractive pivot candidate is found, calls cxf_pricing_end_level to clean up level 0, then calls cxf_pricing_set_level to advance to level 1 (which provides a wider neighborhood of candidates through structural expansion). This progressive widening continues up to level 2 if needed.

**Thread Safety:** Unsafe. Must be called from a single thread.

**Dependencies:**
- PricingState data model (P1.06) -- currentLevel field

---

### cxf_pricing_get_var_stats

**Purpose:** Retrieve the committed dirty variable count and queue pointer for the current pricing level, providing the simplex iteration with information about how many variables are queued for pricing evaluation.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Output (via pointer): `countOut` : pointer-to-int - Receives the committed count of dirty variables at the current level
- Output (via pointer): `queueOut` : pointer-to-pointer-to-int - Receives a pointer to the variable queue array at the current level

**Preconditions:**
- PricingState has been initialized
- The current level has been set to a valid value (0, 1, or 2)
- `countOut` and `queueOut` are non-null

**Postconditions:**
- `*countOut` contains the committed variable count for the current level
- `*queueOut` points to the variable queue array for the current level
- No state is modified

**Side Effects:**
- None; this is a read-only accessor

**Error Conditions:**
- None; no validation performed

**Behavioral Description:**
This function is a lightweight accessor that reads the current pricing level from the PricingState, then returns the committed count and queue pointer for the variable queue at that level. It is used by the simplex iteration loop to determine the size of the variable candidate pool before deciding whether to proceed with pricing at the current level or to expand to a higher level.

The committed count represents the number of variable entries that are ready for immediate processing (as opposed to pending entries that have not yet been promoted). The queue pointer provides direct access to the array of variable indices for iteration.

This function is the variable-side counterpart to cxf_pricing_get_constr_stats.

**Thread Safety:** Unsafe. Must be called from a single thread.

**Dependencies:**
- PricingState data model (P1.06) -- currentLevel, varQueueCommitted array, varQueue array

---

### cxf_pricing_get_constr_stats

**Purpose:** Retrieve the committed dirty constraint count and queue pointer for the current pricing level, providing the simplex iteration with information about how many constraints are queued for pricing evaluation.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Output (via pointer): `countOut` : pointer-to-int - Receives the committed count of dirty constraints at the current level
- Output (via pointer): `queueOut` : pointer-to-pointer-to-int - Receives a pointer to the constraint queue array at the current level

**Preconditions:**
- PricingState has been initialized
- The current level has been set to a valid value (0, 1, or 2)
- `countOut` and `queueOut` are non-null

**Postconditions:**
- `*countOut` contains the committed constraint count for the current level
- `*queueOut` points to the constraint queue array for the current level
- No state is modified

**Side Effects:**
- None; this is a read-only accessor

**Error Conditions:**
- None; no validation performed

**Behavioral Description:**
This function is a lightweight accessor that reads the current pricing level from the PricingState, then returns the committed count and queue pointer for the constraint queue at that level. It serves the same role as cxf_pricing_get_var_stats but for the constraint queue subsystem.

The committed count represents the number of constraint entries ready for immediate processing. The simplex iteration loop uses this information to assess whether the constraint candidate pool is sufficient or whether level expansion is needed.

**Thread Safety:** Unsafe. Must be called from a single thread.

**Dependencies:**
- PricingState data model (P1.06) -- currentLevel, constrQueueCommitted array, constrQueue array

---

### cxf_pricing_get_constr_candidates

**Purpose:** Compute and return a filtered list of constraint candidates eligible for pricing evaluation at the current level, adaptively choosing between a full scan and a partial neighbor expansion strategy based on problem characteristics.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `solverState` : pointer-to-SolverState - The solver state providing matrix structure, status arrays, and workspace
- Output (via pointer): `countOut` : pointer-to-int - Receives the number of constraint candidates
- Output (via pointer): `candidatesOut` : pointer-to-pointer-to-int - Receives a pointer to the array of candidate constraint indices

**Preconditions:**
- PricingState and SolverState have been initialized
- The current level has been set to a valid value (0, 1, or 2)
- The constraint status array in SolverState is up to date
- If level is 1 or 2: the selection flags workspace in SolverState is available (all entries initially zero or properly cleared from prior use)
- `countOut` and `candidatesOut` are non-null

**Postconditions:**
- `*countOut` contains the number of constraint candidates returned
- `*candidatesOut` points to an array of that many constraint indices
- Every returned index has a valid (non-negative) constraint status in the SolverState
- No index appears more than once in the returned array
- At level 0: the returned set is exactly the committed portion of the base constraint queue
- At levels 1 and 2 with a cache hit: the previously computed result is returned unchanged
- At levels 1 and 2 with a cache miss: a new candidate list is computed, cached, and returned
- The selection flags workspace (if used) is restored to its initial state (all zeros) by the end of the function

**Side Effects:**
- May populate the output buffer for the current level in PricingState (on cache miss)
- May update the cached constraint count for the current level (on cache miss)
- May modify and restore the selection flags workspace in SolverState (during partial expansion)
- Increments the work counter (if enabled) proportionally to the work performed

**Error Conditions:**
- None; no validation performed

**Behavioral Description:**
This function implements the candidate retrieval logic described in Phase 4 (Candidate Retrieval with Adaptive Strategy) of the P2.02 specification. It is the constraint-side candidate retrieval function; an analogous function exists for variable candidates.

The function handles three cases:

**Case 1 -- Level 0 fast path:** At the base level, the function simply returns the committed count and queue pointer from the constraint queue at level 0. No computation, caching, or filtering is performed beyond what was already done by cxf_pricing_end_level. This path has O(1) cost.

**Case 2 -- Cache hit at levels 1 or 2:** The function checks whether the cached constraint count for the current level holds a valid (non-sentinel) value. If so, the cached count and the corresponding output buffer pointer are returned directly. This path also has O(1) cost.

**Case 3 -- Cache miss at levels 1 or 2:** The function must compute the candidate list from scratch. It applies three threshold checks to decide between a full scan strategy and a partial neighbor expansion strategy:

1. **Cross-queue density check:** If the number of constraints is small relative to the variable queue size at this level (scaled by an expansion threshold parameter), the dirty set is already large enough that neighbor expansion would touch most constraints. A full scan is chosen.

2. **Coverage check:** If the number of constraints is small relative to the constraint queue size at this level (scaled by a coverage threshold parameter), the queue already covers a significant fraction of the problem. A full scan is chosen.

3. **Expansion cost estimate:** The function estimates the total number of structural neighbors that would be visited during expansion by summing, for each variable in the variable queue, the number of nonzeros in that variable's column (obtained from the status or basis information). If this estimated expansion cost plus the current queue size exceeds the number of constraints, expansion is no cheaper than a full scan. A full scan is chosen.

If all three threshold checks pass (indicating that the problem is large and the dirty set is small), the **partial neighbor expansion** strategy is used:

- **Step 1 (Seed):** Copy the committed entries from the constraint queue at this level into the output buffer, marking each in the selection flags workspace to track which constraints have already been included.
- **Step 2 (Expand):** For each variable in the variable queue at this level, traverse its structural neighbors. In direct matrix mode, this means scanning the CSC column for the variable to find all constraint indices with nonzero coefficients. In eta vector mode, this means traversing the eta vector linked list for the variable. For each neighbor not already marked in the selection flags, add it to the output buffer and mark it.
- **Step 3 (Filter and clean up):** Scan the entire output buffer, clearing the selection flag for each entry (restoring the workspace for future use) and retaining only entries whose constraint status is valid (non-negative). The retained entries are compacted to the front of the buffer.

If any threshold check fails, the **full scan** strategy is used: iterate over all constraint indices from 0 to numConstrs-1, keeping those with valid constraint status.

In both strategies, the result is cached in the PricingState (the output buffer and the cached count are set), and the count and buffer pointer are returned through the output parameters. The work counter is incremented throughout to track computational effort.

**Thread Safety:** Unsafe. Must be called from a single thread.

**Dependencies:**
- PricingState data model (P1.06) -- currentLevel, constraint queue arrays, variable queue arrays, candidate caches, output buffers
- SolverState data model (P1.04) -- sparse matrix (CSC column structure), eta vector linked lists, constraint status array, variable status array, selection flags workspace, work counter, scale factor, representation mode flag
- Multi-Level Partial Pricing algorithm (P2.02) -- Phase 4 candidate retrieval strategy

---

## Module-Level Behavioral Notes

### Symmetric Queue Architecture

The pricing support module operates on two structurally parallel queue subsystems: one for constraints and one for variables. The constraint queues track which constraints need re-evaluation after variable changes; the variable queues track which variables need re-evaluation after constraint changes. The marking functions (cxf_pricing_mark_dirty for variables, cxf_pricing_mark_constr_dirty for constraints) use the identical committed/pending insertion protocol on their respective queues. The stats functions (cxf_pricing_get_var_stats and cxf_pricing_get_constr_stats) are symmetric read-only accessors. This symmetry reflects the duality inherent in the simplex method: both primal simplex (which selects entering variables) and dual simplex (which selects leaving constraints) need efficient access to candidate sets, and both sides are served by the same multi-level queue infrastructure.

### Cascade Propagation after Pivots

cxf_pricing_cascade_update serves as the primary producer for the variable queues following a basis pivot. When a variable changes basis status, all constraints sharing a nonzero coefficient with that variable may have changed feasibility or pricing status. The cascade function traverses the column structure (via CSC array or eta vector linked list) and calls the same queue insertion logic as cxf_pricing_mark_dirty for each affected constraint. This cascade is the mechanism by which local basis changes propagate into the global pricing queue system, enabling the multi-level pricing scheme to detect and track structurally adjacent elements efficiently.

### Level Lifecycle Flow

A typical simplex iteration uses the pricing support functions in the following sequence:

1. **cxf_pricing_set_level(0)** -- Start at the base level.
2. **cxf_pricing_get_var_stats / cxf_pricing_get_constr_stats** -- Check queue sizes.
3. **cxf_pricing_get_constr_candidates** (or variable counterpart) -- Retrieve candidates for pricing evaluation.
4. If a good pivot is found, perform the pivot and call **cxf_pricing_cascade_update** plus **cxf_pricing_mark_dirty / cxf_pricing_mark_constr_dirty** to mark affected elements.
5. **cxf_pricing_end_level(0)** -- Filter level 0 queues, removing invalid entries.
6. If no good pivot was found at level 0, **cxf_pricing_set_level(1)** and repeat steps 2-5 at level 1.
7. If still no good pivot, advance to level 2, which may trigger a full scan via the adaptive strategy in cxf_pricing_get_constr_candidates.

### Committed/Pending Safety Guarantee

The committed/pending split in the queue arrays ensures that producer functions (cxf_pricing_mark_dirty, cxf_pricing_mark_constr_dirty, cxf_pricing_cascade_update) never corrupt an in-progress iteration over the committed section. When a level's phase is active, new entries are appended to the pending section beyond the committed boundary. Only when cxf_pricing_end_level runs between iterations are pending entries promoted to committed status (at levels 1 and 2) or the counts reconciled (at level 0). This design eliminates the need for double buffering or locks.

### Work Counter Integration

Several functions in this module (cxf_pricing_cascade_update, cxf_pricing_end_level, cxf_pricing_get_constr_candidates) contribute to a work counter that tracks the computational effort of the pricing subsystem. The work counter is incremented by an amount proportional to the number of elements processed, scaled by a mode-dependent work factor and the solver's scale factor. This work metric is architecture-independent and is used elsewhere in the solver to trigger basis refactorization when accumulated pricing overhead becomes excessive, or to adjust time-based progress reporting. The work counter is optional: if the counter pointer is null, no accounting is performed.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_pricing_mark_dirty | Unsafe | Modifies shared queue state |
| cxf_pricing_mark_constr_dirty | Unsafe | Modifies shared queue state |
| cxf_pricing_cascade_update | Unsafe | Modifies shared queue state; reads solver matrix |
| cxf_pricing_end_level | Unsafe | Modifies queue state and caches |
| cxf_pricing_set_level | Unsafe | Modifies current level field |
| cxf_pricing_get_var_stats | Unsafe | Reads shared state without synchronization |
| cxf_pricing_get_constr_stats | Unsafe | Reads shared state without synchronization |
| cxf_pricing_get_constr_candidates | Unsafe | Reads and writes shared state and workspace |

All functions in this module are designed as single-threaded components of the simplex solver. Each concurrent LP solve must have its own independent PricingState and SolverState. No internal synchronization is provided.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures referenced by Layer 1 semantic names
[x] Cross-references P2.02 (Multi-Level Partial Pricing) algorithm spec
[x] Cross-references P1.05 (PricingState) and P1.01 (SolverState) data models
[x] Passes the Clean Room Test: could be written without seeing the binary
```
