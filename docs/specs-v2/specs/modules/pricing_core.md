# Module: Pricing Core

## Purpose

The Pricing Core module implements the central operations of the multi-level partial pricing subsystem within the simplex solver. In the standard simplex method, every non-basic variable must be evaluated for its reduced cost at each iteration, producing an O(n) per-iteration cost that dominates runtime for large problems. Partial pricing (Dantzig, 1963; Goldfarb and Reid, 1977; Koberstein, 2008) reduces this cost by maintaining a working set of "dirty" candidates -- variables and constraints whose reduced costs may have changed since the last iteration -- and expanding that set through structural neighbors only when needed.

This module provides the five core operations that drive the pricing system: two **producer** functions that populate work queues after basis pivots, one **consumer** function that processes and commits queue entries between iterations, one **retrieval** function that computes and returns candidate lists for pricing evaluation, and one **reset** function that marks individual variables dirty after non-pivot events. Together, these functions implement the producer-consumer-retrieval architecture described in the P2 Multi-Level Partial Pricing algorithm specification.

## Functions

### cxf_pricing_candidates

**Purpose:** Retrieve the candidate variable list for simplex pricing evaluation at the current pricing level. This is the retrieval function of the pricing system -- the consumer endpoint that callers (such as the simplex step function) use to obtain the set of variables eligible for reduced cost evaluation.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state containing work queues, caches, and level information
- Input: `solverState` : pointer-to-SolverState - The solver state providing matrix structure, status arrays, and work counter
- Output (via parameter): `count` : pointer-to-int - Number of candidate variables returned
- Output (via parameter): `candidates` : pointer-to-pointer-to-int-array - Pointer to the array of candidate variable indices
- Return: void

**Preconditions:**
- PricingState has been initialized (all counts zero, caches set to -1, current level set to 0)
- SolverState contains a valid sparse matrix representation (either CSC or eta vector linked lists)
- The variable status array in SolverState is populated and consistent with the current basis
- The output parameter pointers are non-null

**Postconditions:**
- The count output contains the number of candidate variables (may be zero)
- The candidates output points to an array of valid variable indices, each with non-negative status in the status array
- No index appears more than once in the returned array
- At level 0, the returned set is exactly the base dirty list (the committed variable queue at level 0)
- At levels 1 and above with a cache hit, the previously computed result is returned
- At levels 1 and above with a cache miss, the result has been computed, cached, and returned
- The selection flags array (a temporary workspace in SolverState) has been restored to its initial zeroed state for all indices that were temporarily marked

**Side Effects:**
- Populates the output buffer and cached count for the current level when the cache is invalid
- Increments the work counter (if non-null) proportionally to the number of elements scanned or expanded
- Temporarily marks entries in the selection flags array during partial expansion (cleared during the filter step)

**Error Conditions:**
- None explicitly handled. Behavior is undefined if preconditions are violated.

**Behavioral Description:**
This function implements Phase 4 (Candidate Retrieval with Adaptive Strategy) of the P2 Multi-Level Partial Pricing algorithm specification.

**Level 0 fast path:** When the current pricing level is 0, the function returns the base dirty list directly. It sets the output count to the committed count of the level-0 variable queue and the output pointer to the level-0 queue array. This is an O(1) operation with no computation, copying, or caching.

**Cache check:** At levels 1 and 2, the function first checks whether the cached candidate count for the current level is valid (not equal to -1). If valid, the cached count and output buffer pointer are returned immediately. Caches are invalidated by cxf_pricing_update whenever it processes queues at that level, ensuring stale results are never returned.

**Cache miss -- strategy selection:** When the cache is invalid, the function must compute the candidate list. It adaptively selects between two strategies using three threshold checks, as described in the P2 specification. The three checks implement a cost-benefit analysis: partial expansion is only worthwhile when the expected neighborhood is a small fraction of the problem dimension, so that the bookkeeping overhead (flag marking, duplicate prevention) is repaid by scanning fewer variables overall. This adaptive approach follows the principle articulated by Koberstein (2008, Section 4) that neighbor-based expansion should fall back to full pricing when the dirty set is large relative to the problem.

1. **Cross-queue size threshold (expansion multiplier):** If the problem dimension (number of variables or constraints) is no larger than the committed count of the cross-queue multiplied by the expansion multiplier, the candidate set from expansion would cover most of the problem anyway, so a full scan is cheaper. The expansion multiplier is a small constant, typically in the range [1.5, 3.0]. A value near 2.0 is a natural starting point: it means that if the cross-queue contains at least half the problem dimension (accounting for the multiplier), neighbor expansion would fan out to essentially the entire problem. Smaller values (toward 1.5) make the algorithm more aggressive about using expansion; larger values (toward 3.0) make it more willing to fall back to full scans. This parameter is analogous to the cluster-count parameter K in Maros's SIMPRI framework (Maros, 2003, Section 9.3): both control how much of the problem the pricing scan touches before declaring the search complete.

2. **Coverage fraction threshold:** If the problem dimension is no larger than the total variable queue size at this level multiplied by the coverage fraction, the queue already covers enough of the problem that full scanning is more efficient than the bookkeeping overhead of partial expansion. The coverage fraction is typically in the range [0.3, 0.7], with a value near 0.5 being a natural midpoint. At 0.5, the check triggers a full scan when the queue already contains at least half the eligible variables (since n <= queueSize * 0.5 means queueSize >= 2n, which in practice with status filtering means the queue covers a large portion of the problem). Lower values (toward 0.3) cause the algorithm to prefer expansion even when queues are moderately large; higher values (toward 0.7) cause earlier fallback to full scans. The intuition is that once the dirty set covers a substantial fraction of the problem, the per-element overhead of flag-based duplicate prevention in partial expansion exceeds the cost of a simple linear scan. This mirrors the observation by Maros (2003, Chapter 10) that partial pricing loses its advantage when the active subset grows to encompass most of the variable space.

3. **Expansion cost estimate (work factor):** If neither of the above triggers, the function estimates the expansion cost by summing the nonzero counts (from the basis header or column lengths) across all entries in the cross-queue at this level. It then checks whether the estimated total number of neighbors to visit, combined with the queue size scaled by a work factor, exceeds the problem dimension. If so, expansion would be more expensive than a full scan. The work factor is a small positive constant, typically in the range [1e-4, 1e-3]. A value near 5e-4 reflects the amortized overhead per queue element of the expansion bookkeeping (flag tests, conditional insertions, and linked list traversals for eta mode) relative to the per-element cost of a simple status check in full scan mode. The work factor is intentionally small because the queue-size contribution is a secondary correction: the dominant term in the cost estimate is the sum of neighbor counts (the expansion fan-out), while the work factor adjusts for the fixed per-element overhead. If the expansion fan-out alone already exceeds the problem dimension, partial expansion is clearly not worthwhile regardless of the work factor; the work factor catches borderline cases where the fan-out is just below the problem dimension but the combined cost tips the balance toward full scan.

**Threshold tuning guidance:** These three parameters interact to form a conservative decision rule: partial expansion is used only when all three checks pass (i.e., the cross-queue is small, the variable queue coverage is low, and the estimated expansion cost is below the full-scan cost). An implementor should start with the midpoint values (expansion multiplier near 2.0, coverage fraction near 0.5, work factor near 5e-4) and tune based on benchmark performance. For very sparse problems (average column density below 10), the expansion fan-out tends to be small, so partial expansion is more often worthwhile and the expansion multiplier can be set higher (toward 3.0). For denser problems (average column density above 50), expansion fans out rapidly, and lowering the expansion multiplier (toward 1.5) helps the algorithm detect this and fall back to full scans earlier. The coverage fraction is less sensitive to problem structure and is mainly a safeguard against degenerate cases where the queue accumulates a large fraction of the problem. The work factor rarely needs adjustment from its default range. These parameter ranges are consistent with the practical partial pricing guidance in Koberstein (2008) and the SIMPRI framework analysis in Maros (2003).

**Full scan strategy:** Iterates over all variables from index 0 to numVars-1, keeping those with non-negative status. The result is written to the output buffer and the count is cached. The work counter is incremented proportionally to the number of variables scanned.

**Partial expansion strategy:** Proceeds in three steps:

- *Step 1 (Seed):* Copies the base dirty variable list from the current level's expanded queue into the output buffer. Each copied variable index is marked in the selection flags array (set to 1) to prevent duplicate additions.

- *Step 2 (Expand):* For each entry in the cross-queue (committed dirty entries at this level), the function expands to structural neighbors. If the solver is using the direct matrix representation, it scans the corresponding column in CSC format and adds each valid neighbor (non-negative row index) that is not already marked in the selection flags. If the solver is using the eta vector representation, it traverses the linked list of eta entries for that index, adding valid neighbors (entries with non-negative status). Each newly added neighbor is marked in the selection flags to prevent duplicates.

- *Step 3 (Filter):* Iterates over all entries in the output buffer, clearing the selection flag for each entry (restoring the flags array to its zeroed state) and keeping only entries with non-negative status in the variable status array. The filtered count is cached and the output buffer pointer is returned.

The work counter is incremented after each phase (estimation, seed copy, expansion, and filtering) proportionally to the work performed, scaled by a problem-dependent factor from the solver state.

**Thread Safety:** Unsafe. This function reads and writes shared pricing state without synchronization. Each concurrent simplex solve must use its own independent PricingState.

**Dependencies:**
- P1 PricingState: currentLevel, per-level queue counts, per-level queue arrays, per-level cached counts, per-level output buffers
- P1 SolverState: numVars, numConstrs, variable status array, CSC matrix (colStart, colRowCount, colRowIndices), eta vector linked lists, selection flags array, work counter, scale factor
- P2 Multi-Level Partial Pricing: Phase 4 (Candidate Retrieval with Adaptive Strategy)

---

### cxf_pricing_update

**Purpose:** Process both constraint and variable work queues at the current pricing level, filtering out invalidated entries, promoting pending entries to committed status, and invalidating candidate caches. This is the consumer function that finalizes queue state between simplex iterations.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `solverState` : pointer-to-SolverState - The solver state providing status arrays and work counter
- Return: void

**Preconditions:**
- PricingState has been initialized
- SolverState contains valid constraint and variable status arrays
- The current pricing level is 0, 1, or 2

**Postconditions:**
- If the current level's phase was not previously active, it is now marked as active, and no other state changes occur
- If the current level's phase was previously active:
  - Both constraint and variable queues at the current level have been filtered: entries with negative status have been removed
  - At level 0: queue committed counts and total counts are both set to the number of surviving entries (simple compaction)
  - At levels 1-2: entries with the pending flag bit set have been promoted to committed status (committed bit set, pending bit cleared, entry retained); entries without the pending flag bit have been demoted (all level-specific bits cleared, entry discarded)
  - At levels 1-2: all six cache slots at the current level have been set to -1 (invalidated), forcing cxf_pricing_candidates to recompute on the next call
- The current level's phase-active flag is set to true (value 1)

**Side Effects:**
- Modifies per-element flag bytes in both the constraint and variable flag arrays
- Compacts queue arrays in place (surviving entries shifted to the front)
- Updates queue committed counts and total counts
- Invalidates candidate caches at levels 1 and 2 (sets cached counts to -1)
- Increments the work counter (if non-null) proportionally to the number of entries scanned

**Error Conditions:**
- None explicitly handled. Behavior is undefined if preconditions are violated.

**Behavioral Description:**
This function implements Phase 3 (Queue Processing and Cache Invalidation) of the P2 Multi-Level Partial Pricing algorithm specification.

**Phase activation guard:** The function first checks whether the current level's phase has been previously activated. If not, it marks the phase as active and returns immediately. This ensures that the first call at a level merely activates it (enabling the committed/pending distinction for subsequent producer insertions), while subsequent calls perform the actual processing.

**Level 0 processing:** At the base level, the function performs simple status-based filtering. It scans the constraint queue, keeping only entries whose constraint status is non-negative, and compacts the surviving entries to the front of the array. Both the committed count and total count are set to the number of survivors (there is no committed/pending distinction at level 0). The same filtering is then applied to the variable queue. No cache invalidation occurs at level 0, because level 0 returns the base dirty list directly without caching.

**Levels 1-2 processing:** At higher levels, the function uses the membership flag bits described in the P1 PricingState specification to distinguish fresh entries from stale ones. The flag bit assignments depend on the level:

- Level 1 uses bits 0 (committed) and 1 (pending), with a combined mask for both bits.
- Level 2 uses bits 2 (committed) and 3 (pending), with a combined mask for both bits.

For each entry in the constraint queue at the current level:
1. If the entry's constraint status is negative, the entry is discarded (status-invalid filtering).
2. If the entry has the pending bit set for this level, the entry is fresh (added during the active phase). The committed bits are set, the pending bit is cleared, and the entry is retained in the queue.
3. If the entry does not have the pending bit set, the entry is stale (it was present before the current phase). All bits for this level are cleared in the flag array, and the entry is discarded.

The same logic is applied symmetrically to the variable queue.

After processing both queues, all six cache slots at the current level are set to -1, forcing the candidate retrieval functions to recompute their results. The six slots cover the primary, secondary, and tertiary cache for both constraints and variables.

**Work counter:** The work counter is incremented proportionally to the number of queue entries scanned (both constraint and variable), scaled by a factor from the solver state.

**Thread Safety:** Unsafe. This function modifies shared pricing state without synchronization.

**Dependencies:**
- P1 PricingState: currentLevel, levelActive flags, per-level queue arrays and counts, constraint and variable flag arrays, all six cache slot arrays
- P1 SolverState: constraint status array, variable status array, work counter, scale factor
- P2 Multi-Level Partial Pricing: Phase 3 (Queue Processing and Cache Invalidation)

---

### cxf_pricing_update_var

**Purpose:** After a variable enters the basis (or otherwise changes status), propagate the change by marking all structurally affected constraints as dirty in the pricing work queues. This is one of the two producer functions in the pricing system.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `solverState` : pointer-to-SolverState - The solver state providing matrix structure and eta vectors
- Input: `varIndex` : int - The index of the variable that changed
- Return: void

**Preconditions:**
- PricingState has been initialized with valid constraint flag and queue arrays
- SolverState contains either a valid CSC matrix representation or valid eta vector linked lists, depending on the current representation mode
- The variable index is within the valid range (0 to numVars-1)

**Postconditions:**
- Every constraint that shares a nonzero coefficient with the specified variable (in either the matrix column or eta vector chain) and has a valid status has been added to both level-1 and level-2 constraint queues, subject to duplicate prevention
- Constraint flag bits have been updated to reflect queue membership and pending/committed status
- The work counter (if non-null) has been incremented proportionally to the number of entries traversed

**Side Effects:**
- Appends constraint indices to the level-1 and level-2 constraint queue arrays
- Increments constraint queue committed counts and/or total counts
- Sets flag bits in the constraint flag array for each newly added constraint
- Increments the work counter

**Error Conditions:**
- None explicitly handled. Behavior is undefined if the variable index is out of range or the matrix/eta structures are invalid.

**Behavioral Description:**
This function implements the "Marking constraints dirty after a variable change" operation in Phase 1 (Dirty Marking) of the P2 Multi-Level Partial Pricing algorithm specification.

The function operates in one of two modes based on the solver's current representation state:

**Eta vector mode:** When the solver is using the Product Form of Inverse representation, the function traverses the eta vector linked list associated with the specified variable. For each entry in the linked list with a valid status (non-negative), the function extracts the affected constraint index and adds it to both constraint queues using the queue insertion protocol.

**Matrix mode:** When the solver is using the direct matrix representation, the function scans the CSC column for the specified variable. For each nonzero entry in the column (identified by a non-negative row index), the function adds the corresponding constraint index to both constraint queues using the queue insertion protocol.

**Queue insertion protocol (applied identically to both levels):**

For each constraint queue (level 1 and level 2), the function checks the constraint's flag bits to determine if it is already present in that queue. Two bits per level encode membership: one for the committed section and one for the pending section. If neither bit is set (the constraint is not yet in the queue), the insertion proceeds:

- If the level's phase is *not* currently active, the constraint is inserted into the committed section of the queue. Both the committed count and total count are incremented, and the committed flag bit is set.
- If the level's phase *is* currently active, the constraint is inserted into the pending section (after the committed entries). Only the total count is incremented. The committed flag bit is not set.

After the queue insertion checks, if either level's phase is active, the corresponding pending flag bit is set on the constraint. This allows cxf_pricing_update to later distinguish fresh entries (added during the active phase) from stale entries.

This per-element flag-based duplicate prevention ensures O(1) membership testing, avoiding linear queue searches (Maros, 2003, Section 9.3).

**Thread Safety:** Unsafe. This function modifies shared constraint queues and flag arrays without synchronization.

**Dependencies:**
- P1 PricingState: constraint flag array, level-1 and level-2 constraint queue arrays, constraint queue committed and total counts, phase-active flags for both levels
- P1 SolverState: CSC matrix (colStart, colRowCount, colRowIndices) or eta vector linked lists, representation mode flag, work counter, scale factor
- P2 Multi-Level Partial Pricing: Phase 1 (Dirty Marking) and Phase 2 (Queue Insertion with Committed/Pending Split)

---

### cxf_pricing_update_constr

**Purpose:** After a constraint leaves the basis (or otherwise changes status), propagate the change by marking all structurally affected variables as dirty in the pricing work queues. This is the symmetric counterpart to cxf_pricing_update_var, operating on variable queues instead of constraint queues.

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `solverState` : pointer-to-SolverState - The solver state providing matrix structure and eta vectors
- Input: `constrIndex` : int - The index of the constraint that changed
- Return: void

**Preconditions:**
- PricingState has been initialized with valid variable flag and queue arrays
- SolverState contains either a valid CSR matrix representation or valid eta vector linked lists, depending on the current representation mode
- The constraint index is within the valid range (0 to numConstrs-1)

**Postconditions:**
- Every variable that shares a nonzero coefficient with the specified constraint (in either the matrix row or eta vector chain) and has a valid status has been added to both level-1 and level-2 variable queues, subject to duplicate prevention
- Variable flag bits have been updated to reflect queue membership and pending/committed status
- The work counter (if non-null) has been incremented proportionally to the number of entries traversed

**Side Effects:**
- Appends variable indices to the level-1 and level-2 variable queue arrays
- Increments variable queue committed counts and/or total counts
- Sets flag bits in the variable flag array for each newly added variable
- Increments the work counter

**Error Conditions:**
- None explicitly handled. Behavior is undefined if the constraint index is out of range or the matrix/eta structures are invalid.

**Behavioral Description:**
This function implements the "Marking variables dirty after a constraint change" operation in Phase 1 (Dirty Marking) of the P2 Multi-Level Partial Pricing algorithm specification.

The function is structurally identical to cxf_pricing_update_var but operates in the transpose direction: where cxf_pricing_update_var scans a variable's column to find affected constraints, this function scans a constraint's row to find affected variables.

**Eta vector mode:** When the solver is using the Product Form of Inverse representation, the function traverses the eta vector linked list associated with the specified constraint. For each entry with a valid status, the function extracts the affected variable index and adds it to both variable queues using the queue insertion protocol.

**Matrix mode:** When the solver is using the direct matrix representation, the function scans the CSR row for the specified constraint. For each nonzero entry in the row (identified by a non-negative column index), the function adds the corresponding variable index to both variable queues using the queue insertion protocol.

**Queue insertion protocol:** The insertion logic is identical to that described for cxf_pricing_update_var, but applied to the variable queue subsystem instead of the constraint queue subsystem. Per-variable flag bits (using the same four-bit encoding: committed and pending for each of two levels) provide O(1) duplicate prevention. The committed/pending split follows the same phase-active rules: direct insertion to the committed section when the phase is inactive, deferred insertion to the pending section when the phase is active.

The symmetry between this function and cxf_pricing_update_var reflects the duality in simplex pricing: after a pivot, the entering variable affects constraints (via column nonzeros), while the leaving constraint affects variables (via row nonzeros). Both directions must be propagated to keep the pricing candidate sets accurate.

**Thread Safety:** Unsafe. This function modifies shared variable queues and flag arrays without synchronization.

**Dependencies:**
- P1 PricingState: variable flag array, level-1 and level-2 variable queue arrays, variable queue committed and total counts, phase-active flags for both levels
- P1 SolverState: CSR matrix (rowStart, rowColCount, rowColIndices) or eta vector linked lists, representation mode flag, work counter, scale factor
- P2 Multi-Level Partial Pricing: Phase 1 (Dirty Marking) and Phase 2 (Queue Insertion with Committed/Pending Split)

---

### cxf_pricing_invalidate

**Purpose:** Mark a single variable as needing pricing recalculation by adding it to the variable work queues at both pricing levels. This is the direct-marking producer function, used when an individual variable's pricing-relevant data changes due to a non-pivot event (bound change, coefficient modification, or presolve adjustment).

**Signature:**
- Input: `pricingState` : pointer-to-PricingState - The pricing subsystem state
- Input: `varIndex` : int - The index of the variable to mark as dirty
- Return: void

**Preconditions:**
- PricingState has been initialized with valid variable flag and queue arrays
- The variable index is within the valid range (0 to numVars-1)

**Postconditions:**
- The specified variable has been added to both the level-1 and level-2 variable queues, unless it was already present in a queue (in which case that queue is unchanged)
- Variable flag bits have been updated to reflect queue membership and pending/committed status
- No matrix traversal or neighbor expansion occurs -- only the single specified variable is affected

**Side Effects:**
- Appends the variable index to the level-1 and/or level-2 variable queue arrays (if not already present)
- Increments variable queue committed counts and/or total counts
- Sets flag bits in the variable flag array

**Error Conditions:**
- None explicitly handled. Behavior is undefined if the variable index is out of range.

**Behavioral Description:**
This function implements the "Direct dirty marking" operation in Phase 1 (Dirty Marking) of the P2 Multi-Level Partial Pricing algorithm specification. Unlike cxf_pricing_update_var and cxf_pricing_update_constr, which cascade through the constraint matrix to find structural neighbors, this function directly marks a single element without any traversal.

The function reads the current flag byte for the specified variable and applies the queue insertion protocol to both the level-1 and level-2 variable queues:

**Level-1 queue insertion:** The function checks whether bits 0 and 1 of the variable's flag byte are both clear (the variable is not currently in the level-1 queue in either committed or pending status). If both bits are clear:

- If the level-1 phase is not currently active, the variable is inserted at the committed position. The committed count and total count are both incremented, and the committed flag bit (bit 0) is set.
- If the level-1 phase is currently active, the variable is inserted at the total count position (the pending section). Only the total count is incremented. The committed flag bit is not set.

**Level-2 queue insertion:** The same logic is applied for the level-2 queue, checking bits 2 and 3 of the flag byte and using the corresponding queue arrays and counters.

**Pending flag marking:** After the queue insertions, if the level-1 phase is active, the pending bit (bit 1) is set on the variable's flag byte. If the level-2 phase is active, the pending bit (bit 3) is set. These bits allow cxf_pricing_update to distinguish entries added during the current active phase from entries that pre-date the phase.

The O(1) duplicate prevention via flag bits ensures this function can be called repeatedly for the same variable without producing duplicate queue entries.

**Thread Safety:** Unsafe. This function modifies shared variable queues and flag arrays without synchronization.

**Dependencies:**
- P1 PricingState: variable flag array, level-1 and level-2 variable queue arrays, variable queue committed and total counts, phase-active flags for both levels
- P2 Multi-Level Partial Pricing: Phase 1 (Dirty Marking) and Phase 2 (Queue Insertion with Committed/Pending Split)

---

## Module-Level Behavioral Notes

### Producer-Consumer-Retrieval Architecture

The five functions in this module form a three-tier architecture that operates on every simplex iteration:

| Tier | Functions | Role | P2 Phase |
|------|-----------|------|----------|
| Producer | cxf_pricing_update_var, cxf_pricing_update_constr, cxf_pricing_invalidate | Populate work queues after basis changes | Phase 1 (Dirty Marking), Phase 2 (Queue Insertion) |
| Consumer | cxf_pricing_update | Process queues, filter, promote/demote, invalidate caches | Phase 3 (Queue Processing) |
| Retrieval | cxf_pricing_candidates | Compute and return candidate lists for pricing evaluation | Phase 4 (Candidate Retrieval) |

A typical simplex iteration uses these functions in the following order:

1. After a pivot, the entering variable triggers cxf_pricing_update_var to mark affected constraints in the constraint queues.
2. The leaving constraint triggers cxf_pricing_update_constr to mark affected variables in the variable queues.
3. Any individual variable whose bounds or coefficients changed during the pivot is marked via cxf_pricing_invalidate.
4. cxf_pricing_update is called to process and commit the queues at the current level, filter out invalidated entries, and invalidate candidate caches.
5. cxf_pricing_candidates is called to retrieve the candidate list for the pricing evaluation step.

### Symmetric Queue Subsystems

The pricing system maintains two parallel queue subsystems -- one for constraints and one for variables -- reflecting the duality in simplex pricing. cxf_pricing_update_var populates the constraint queues, while cxf_pricing_update_constr and cxf_pricing_invalidate populate the variable queues. cxf_pricing_update processes both subsystems. cxf_pricing_candidates retrieves from the variable queue subsystem (the constraint-side retrieval function is in the Pricing Support module, P3.18).

### Flag Bit Encoding

Both constraint and variable flag arrays use a per-element byte with four active bits encoding queue membership across two levels:

| Bits | Level | Meaning |
|------|-------|---------|
| Bit 0 | Level 1 | Element is in the committed section of the level-1 queue |
| Bit 1 | Level 1 | Element was added to the level-1 queue during an active phase (pending) |
| Bit 2 | Level 2 | Element is in the committed section of the level-2 queue |
| Bit 3 | Level 2 | Element was added to the level-2 queue during an active phase (pending) |

This encoding supports O(1) duplicate prevention (check two bits before insertion), O(1) phase-status tagging (set pending bit when phase is active), and efficient promotion/demotion during queue processing (bitwise operations to set committed and clear pending or vice versa).

### Dual Traversal Modes

The producer functions (cxf_pricing_update_var and cxf_pricing_update_constr) and the retrieval function (cxf_pricing_candidates) all support two traversal modes for finding structural neighbors:

- **Matrix mode:** Directly scans the CSC column (for variable-to-constraint traversal) or CSR row (for constraint-to-variable traversal) in the sparse matrix representation. Preferred when the matrix representation is current.
- **Eta vector mode:** Traverses the linked list of eta vector entries associated with the element. Used when the solver is maintaining the basis inverse via the Product Form of Inverse. Each eta entry contains the neighbor index and a validity status flag.

The mode is selected by a representation-mode flag on the SolverState. Both modes produce the same logical result (the set of structural neighbors) but differ in performance characteristics: matrix mode has predictable cost proportional to column/row density, while eta mode cost depends on the length of the eta vector chain, which grows between basis refactorizations.

### Caching Behavior

Candidate lists at levels 1 and 2 are cached to avoid redundant recomputation within a single pricing phase. The caching policy is:

- A cached count of -1 signals an invalid cache (recomputation required).
- cxf_pricing_update invalidates all six cache slots (three per queue subsystem) at levels 1 and 2 whenever it processes queues.
- Level 0 does not use caching because it returns the base dirty list by reference with no transformation.
- Each level maintains three cache slots per queue subsystem (primary, secondary, tertiary) to support caching of different candidate subsets for different pricing strategies.

### Adaptive Strategy Threshold Parameters

The candidate retrieval function (cxf_pricing_candidates) uses three tunable parameters to decide between full scan and partial neighbor expansion. These parameters are standard algorithmic design choices for neighbor-based partial pricing, grounded in the literature on pricing strategies for the simplex method (Maros, 2003; Koberstein, 2008).

| Parameter | Role | Typical Range | Recommended Starting Value |
|-----------|------|---------------|---------------------------|
| Expansion multiplier | Controls when the cross-queue is large enough that neighbor expansion would cover most of the problem | [1.5, 3.0] | 2.0 |
| Coverage fraction | Controls when the variable queue already covers enough of the problem to make full scan cheaper than expansion bookkeeping | [0.3, 0.7] | 0.5 |
| Work factor | Amortization factor for per-element expansion overhead relative to per-element full-scan cost | [1e-4, 1e-3] | 5e-4 |

**Decision logic:** Partial expansion is used only when all three checks pass. If any check fails, the function falls back to a full scan. This conservative design ensures that partial expansion is never used when it would be more expensive than full pricing. The three checks are ordered from cheapest to most expensive to evaluate: the first two are simple arithmetic comparisons (O(1)), while the third requires iterating over the cross-queue to sum nonzero counts (O(cross-queue size)). This ordering ensures that the expensive estimation step is skipped whenever a cheaper check already determines that full scan is preferable.

**Sensitivity analysis:** The expansion multiplier has the largest impact on solver performance for sparse problems, where it determines how aggressively the algorithm exploits structural locality. The coverage fraction primarily affects behavior during the early iterations of a solve (when many variables are dirty) and during degenerate cycling (when the dirty set grows). The work factor has the least impact and mainly affects borderline cases where the expansion fan-out is close to the problem dimension.

**Relationship to published frameworks:** These three parameters serve a role analogous to the (K, P, R) parameters in Maros's SIMPRI framework (Maros, 2003, Section 9.3), which control how many clusters to scan, when to stop scanning, and how many candidates to retain. The key difference is that SIMPRI uses fixed partitions of the variable space, while the neighbor-expansion approach dynamically constructs the candidate set based on structural adjacency. The threshold parameters here replace the partition-count parameter with a cost-based decision that adapts to the sparsity structure of the current problem. The neighbor-based approach follows the design philosophy described by Koberstein (2008, Section 4), where structural proximity in the constraint matrix serves as a proxy for pricing relevance after a basis change.

### Work Counter Integration

All five functions increment a shared work counter when it is available (non-null). The counter tracks computational effort proportionally to the number of elements processed, scaled by a problem-dependent factor. The counter is used elsewhere in the solver for triggering basis refactorization, adjusting time estimates, and performance profiling. The counter accumulation is architecture-independent (it counts operations, not elapsed time).

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_pricing_candidates | Unsafe | Reads and writes queue arrays, caches, and selection flags |
| cxf_pricing_update | Unsafe | Modifies flag arrays, queue counts, and cache slots |
| cxf_pricing_update_var | Unsafe | Modifies constraint flag arrays and queue arrays |
| cxf_pricing_update_constr | Unsafe | Modifies variable flag arrays and queue arrays |
| cxf_pricing_invalidate | Unsafe | Modifies variable flag arrays and queue arrays |

All functions require exclusive access to the PricingState. Each concurrent simplex solve must use its own independent SolverState, which in turn owns its own PricingState. No internal synchronization is provided.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments from analyzed source
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] References P2 Multi-Level Partial Pricing algorithm specification
[x] References P1 PricingState and P1 SolverState data model specifications
[x] Producer-consumer-retrieval architecture documented in module-level notes
[x] Thread safety summary table included
[x] Adaptive strategy threshold parameters documented with quantitative ranges
[x] Threshold ranges grounded in published references (Maros 2003, Koberstein 2008)
[x] Passes the Clean Room Test
```
