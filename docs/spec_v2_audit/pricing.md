# Spec V2 Audit: Pricing

## Files Reviewed

### Spec Files
- `docs/specs-v2/specs/modules/pricing_core.md` (5 functions)
- `docs/specs-v2/specs/modules/pricing_support.md` (8 functions)
- `docs/specs-v2/specs/algorithms/partial_pricing.md` (algorithm description)
- `docs/specs-v2/specs/data-model/pricing_state.md` (PricingState data model)

### Implementation Files
- `src/pricing/init.c` — `cxf_pricing_init`
- `src/pricing/candidates.c` — `cxf_pricing_candidates_v2`
- `src/pricing/constr_candidates.c` — `cxf_pricing_constr_candidates_v2`, `cxf_pricing_get_constr_stats`
- `src/pricing/constr_init.c` — `cxf_pricing_init_constrs`
- `src/pricing/context.c` — `cxf_pricing_create`, `cxf_pricing_free`
- `src/pricing/phase.c` — `cxf_pricing_step2`
- `src/pricing/queue.c` — `cxf_pricing_mark_dirty`, `cxf_pricing_mark_constr_dirty`, `cxf_pricing_cascade_update`, `cxf_pricing_end_level`, `cxf_pricing_set_level`, `cxf_pricing_get_constr_candidates`
- `src/pricing/queue_insert.c` — `v2_insert_var`, `v2_insert_constr`
- `src/pricing/steepest.c` — `cxf_pricing_steepest`, `cxf_pricing_compute_weight`
- `src/pricing/update.c` — `cxf_pricing_update_queues`
- `src/pricing/update_constr.c` — `cxf_pricing_update_constr`
- `src/pricing/update_var.c` — `cxf_pricing_update_var`
- `src/pricing/weight_update.c` — `cxf_pricing_update_weights`, `cxf_pricing_recompute_weights`
- `src/pricing/invalidate.c` — `cxf_pricing_invalidate`
- `src/pricing/pricing_stub.c` — empty (all stubs replaced)
- `src/pricing/pricing_internal.h` — internal shared header
- `include/convexfeld/cxf_pricing.h` — public header / PricingState definition

---

## Compliant Functions

### v2_insert_var / v2_insert_constr (queue_insert.c)
Flag bit encoding matches spec exactly: L1_COMMITTED=0x01, L1_PENDING=0x02, L2_COMMITTED=0x04, L2_PENDING=0x08. Committed/pending insertion protocol follows spec Phase 2 correctly. Pending bit marking after queue insertion for active levels is correct.

### cxf_pricing_update_var (update_var.c)
CSC column traversal inserts constraints into V2 queues via `v2_insert_constr`. Work counter integration present. Handles auxiliary/slack variables correctly.

### cxf_pricing_update_constr (update_constr.c)
CSR row traversal inserts variables into V2 queues via `v2_insert_var`. Work counter integration present. Has a fallback CSC scan when CSR is unavailable (not in spec, but not a violation).

### cxf_pricing_set_level (queue.c)
Simple setter matching spec. Validates level range.

### cxf_pricing_get_constr_stats (constr_candidates.c)
Returns committed count and queue pointer at current level. Matches spec for `cxf_pricing_get_constr_stats`.

### PricingState Structure (cxf_pricing.h)
All V2 fields present: level_active, var_flags/constr_flags, per-level queue arrays with committed/total counts, 3-slot caches per queue per level, output buffers. CXF_MAX_PRICING_LEVELS=3. Matches pricing_state.md.

---

## VIOLATIONS

### [V1] cxf_pricing_candidates_v2 — Status filter inverted (keeps nonbasic instead of non-negative status)
- **Spec says**: Full scan keeps variables "with non-negative status" (pricing_core.md). The spec states "keeping those with non-negative status" for the full scan, and in the filter step of partial expansion "keeping only entries with non-negative status in the variable status array."
- **Code does**: Full scan at line 110 keeps `var_status[j] < 0` (nonbasic variables). Partial expansion filter at line 158 keeps `var_status[vi] < 0`.
- **File**: `src/pricing/candidates.c:110,158`
- **Analysis**: This is a **semantic mismatch** between the spec's abstract description and the implementation's practical interpretation. In the spec, "non-negative status" means the variable is a valid candidate (basic status encodes row position >= 0). In the implementation, nonbasic variables have status < 0 (at-lower=-1, at-upper=-2, free=-3) and these are the ones that need pricing. The implementation is **correct for the simplex algorithm** but uses opposite polarity from the spec's literal wording. The spec says "non-negative status" to mean "valid" entries, but the actual semantics for variable pricing require selecting nonbasic (negative status) variables. The code is functionally correct, but the spec language could be clearer. **Borderline violation** -- the spec's wording about "non-negative status" for variable candidates appears to be a spec drafting inconsistency (constraint status non-negative = valid constraint, but variable pricing needs nonbasic = negative status).

### [V2] cxf_pricing_candidates_v2 — Uses var_flags instead of SolverState selection flags workspace
- **Spec says**: Partial expansion uses "the selection flags array (a temporary workspace in SolverState)" for duplicate prevention during expansion. "The selection flags workspace (if used) is restored to its initial state (all zeros) by the end of the function."
- **Code does**: Uses bit 0x10 of `ctx->var_flags` (PricingState member) as the selection marker, not a separate SolverState workspace.
- **File**: `src/pricing/candidates.c:118,127,144,157`
- **Analysis**: The spec explicitly requires a dedicated selection flags array in SolverState. The implementation reuses bits in the PricingState's own flag array. While functionally equivalent (bit 4 is unused by the queue system), this deviates from the spec's data flow. Risk: if future code uses bit 4 of var_flags, this will collide.

### [V3] cxf_pricing_constr_candidates_v2 — Full scan does not filter by constraint status
- **Spec says**: "Iterates over all variables from index 0 to numVars-1, keeping those with non-negative status" (pricing_core.md). For the constraint-side, pricing_support.md says "retaining only entries whose constraint status is valid (non-negative)."
- **Code does**: Full scan at line 130-131 adds ALL constraint indices unconditionally (`out_buf[result_count++] = i`). No status check whatsoever.
- **File**: `src/pricing/constr_candidates.c:130-131`
- **Analysis**: Clear violation. The spec requires status-based filtering in the full scan path. The constraint full scan produces an unfiltered list.

### [V4] cxf_pricing_constr_candidates_v2 — Partial expansion filter does not check constraint status
- **Spec says**: "Retaining only entries whose constraint status is valid (non-negative)" in the filter step.
- **Code does**: Filter at line 176 checks only `ci >= 0 && ci < m` (bounds check), not status validity.
- **File**: `src/pricing/constr_candidates.c:176-177`
- **Analysis**: Clear violation. The filter step should discard constraints with invalid (negative) status.

### [V5] cxf_pricing_constr_candidates_v2 — Uses constr_flags instead of SolverState selection flags
- **Spec says**: Same as V2 -- selection flags array in SolverState for temporary workspace.
- **Code does**: Uses bit 0x10 of `ctx->constr_flags` (PricingState member).
- **File**: `src/pricing/constr_candidates.c:136,145,162,174`

### [V6] cxf_pricing_update_queues — Level 0 constraint queue uses wrong status check
- **Spec says**: Level 0 filtering: "keeping only entries whose status in the corresponding status array (constraint status or variable status) is non-negative" (pricing_support.md).
- **Code does**: `filter_queue_l0` for constraint queues (is_var_queue=0) keeps ALL valid-index entries without any status check (line 49: `queue[write++] = idx`).
- **File**: `src/pricing/update.c:48-50`
- **Analysis**: The spec requires status-based filtering for constraint queues. The implementation only validates the index bounds.

### [V7] cxf_pricing_update_queues — Variable queue status filtering uses wrong semantics
- **Spec says**: Process queues filters entries whose status is "non-negative" to keep them.
- **Code does**: `filter_queue_l0` for variable queues keeps entries where `status[idx] < 0` (line 47).
- **File**: `src/pricing/update.c:44-47`
- **Analysis**: Same semantic mismatch as V1 -- the code keeps nonbasic (status < 0) which is correct for variable pricing but opposite of spec's literal "non-negative" wording. This parallels V1.

### [V8] cxf_pricing_update_queues — Level 0 does not invalidate caches
- **Spec says**: pricing_core.md: "No cache invalidation occurs at level 0, because level 0 returns the base dirty list directly without caching." This is actually CORRECT behavior.
- **Note**: This is NOT a violation. Confirmed compliant.

### [V9] cxf_pricing_end_level — Does not perform queue filtering
- **Spec says**: `cxf_pricing_end_level` (pricing_support.md) should "Complete the current pricing level by filtering both constraint and variable queues to remove invalid entries, performing flag-based promotion and demotion at higher levels."
- **Code does**: The function only invalidates V1 and V2 caches and clears V1 dirty flags. It does NOT perform any V2 queue filtering. A comment says "Queue filtering is handled by cxf_pricing_update_queues (update.c) which has access to SolverState."
- **File**: `src/pricing/queue.c:108-117`
- **Analysis**: The spec says `cxf_pricing_end_level` IS the queue processing function. The implementation splits this into two functions: `cxf_pricing_end_level` (cache invalidation only) and `cxf_pricing_update_queues` (actual filtering). This architectural split means the spec's `cxf_pricing_end_level` is only partially implemented. The filtering logic exists in `cxf_pricing_update_queues` but the spec expects it in `cxf_pricing_end_level`. The signature deviation is notable: the spec says `cxf_pricing_end_level` takes `(pricingState, solverState)` but the implementation takes only `(pricingState)`.

### [V10] cxf_pricing_end_level — Missing SolverState parameter
- **Spec says**: Signature is `Input: pricingState : pointer-to-PricingState`, `Input: solverState : pointer-to-SolverState`.
- **Code does**: `void cxf_pricing_end_level(PricingState *ctx)` -- no SolverState parameter.
- **File**: `src/pricing/queue.c:82`
- **Analysis**: Direct signature mismatch. Without SolverState, end_level cannot perform status-based filtering, which is why the code delegates to a separate function.

### [V11] cxf_pricing_end_level — Invalidates caches at ALL levels including level 0
- **Spec says**: "At levels 1 and 2: all six candidate cache slots at the current level have been set to -1" and "No cache invalidation occurs at level 0."
- **Code does**: Lines 112-117 unconditionally invalidate V2 caches at the current level, including level 0.
- **File**: `src/pricing/queue.c:112-117`
- **Analysis**: At level 0, caches should NOT be invalidated. The code invalidates them anyway.

### [V12] cxf_pricing_update_queues — Work counter measured after compaction, not during scan
- **Spec says**: "The work counter is incremented proportionally to the number of queue entries scanned (both constraint and variable)."
- **Code does**: Work counter at line 161 adds `constr_q_total[level] + var_q_total[level]` AFTER compaction, so these are the post-filter counts, not the pre-filter scan counts.
- **File**: `src/pricing/update.c:161-163`
- **Analysis**: The counts used are the TOTAL counts, but they are read after compaction has already modified them (at level 0, both committed and total are set to the compacted count). The work counter should reflect the number of entries scanned (pre-compaction), not the number surviving.

### [V13] cxf_pricing_candidates_v2 — Threshold check 3 uses constr_queue cross-queue but estimates via CSR
- **Spec says**: "The function estimates the expansion cost by summing the nonzero counts (from the basis header or column lengths) across all entries in the cross-queue at this level."
- **Code does**: Cross-queue is `constr_queue[level]` (correct), but the expansion estimate at lines 92-99 uses CSR row lengths. For variable candidate retrieval, the cross-queue is constraints, and expanding them means scanning their rows to find variable neighbors. Using CSR row lengths is actually the correct approach for this direction.
- **Analysis**: Actually compliant -- CSR row lengths give the correct neighbor count for constraint-to-variable expansion.

### [V14] cxf_pricing_cascade_update — Marks variable dirty then cascades to constraints
- **Spec says**: `cxf_pricing_cascade_update` (pricing_support.md) describes it as marking "all structurally adjacent constraints as dirty in the **variable queues**" after a variable change. The spec says the function "adds it to both level-1 and level-2 variable queues" for each affected constraint.
- **Code does**: Calls `cxf_pricing_mark_dirty(ctx, var_idx)` (marks the variable itself in variable queues), then calls `cxf_pricing_mark_constr_dirty(ctx, ...)` for each column neighbor (marks constraints in constraint queues).
- **File**: `src/pricing/queue.c:62-72`
- **Analysis**: The spec says cascade_update should mark structurally adjacent constraints as dirty in the VARIABLE queues, but the code marks them in the CONSTRAINT queues via `cxf_pricing_mark_constr_dirty`. The spec's description seems confused here -- actually, re-reading: the spec describes `cxf_pricing_cascade_update` as "marking all structurally adjacent constraints as dirty in the variable queues." But this doesn't make sense -- constraints go in constraint queues. Looking at pricing_core.md `cxf_pricing_update_var`: "Every constraint that shares a nonzero coefficient with the specified variable has been added to both level-1 and level-2 **constraint queues**." The cascade_update in pricing_support.md has a drafting error (says "variable queues" when it means "constraint queues"). The implementation correctly inserts into constraint queues.

### [V15] cxf_pricing_cascade_update — No eta vector mode
- **Spec says**: Both `cxf_pricing_update_var` and `cxf_pricing_cascade_update` must support two traversal modes: "Eta vector mode" (traversing eta linked lists) and "Matrix mode" (CSC/CSR scan).
- **Code does**: `cxf_pricing_cascade_update` only supports CSC matrix mode. No eta vector linked list traversal.
- **File**: `src/pricing/queue.c:67-72`
- **Analysis**: The code delegates to individual `mark_constr_dirty` calls which also lack eta mode. The spec requires eta vector support as an alternative traversal path.

### [V16] cxf_pricing_update_var — No eta vector mode
- **Spec says**: "Eta vector mode: When the solver is using the Product Form of Inverse representation, the function traverses the eta vector linked list associated with the specified variable."
- **Code does**: Only CSC matrix traversal (and special-case for auxiliary/slack variables). No eta vector mode.
- **File**: `src/pricing/update_var.c:40-63`

### [V17] cxf_pricing_update_constr — No eta vector mode
- **Spec says**: "Eta vector mode: When the solver is using the Product Form of Inverse representation, the function traverses the eta vector linked list associated with the specified constraint."
- **Code does**: Only CSR matrix mode (with CSC fallback). No eta vector mode.
- **File**: `src/pricing/update_constr.c:38-67`

### [V18] cxf_pricing_invalidate — Spec signature mismatch
- **Spec says**: `cxf_pricing_invalidate` in pricing_core.md takes `(pricingState, varIndex)` and marks a SINGLE variable as dirty by adding it to both level-1 and level-2 variable queues. This is the "direct dirty marking" producer.
- **Code does**: `cxf_pricing_invalidate(PricingState *ctx, int flags)` takes a bitmask of invalidation flags and resets cached candidates/weights. It is a cache invalidation function, NOT a single-variable dirty marker.
- **File**: `src/pricing/invalidate.c:25`
- **Analysis**: Complete semantic mismatch. The spec's `cxf_pricing_invalidate` is a producer function that marks a single variable dirty. The implementation is a cache-management function. The spec's intended behavior is partially covered by `cxf_pricing_mark_dirty` (queue.c), but the function name/signature/semantics do not match.

### [V19] cxf_pricing_candidates (V1) — Spec signature mismatch
- **Spec says**: `cxf_pricing_candidates(pricingState, solverState, &count, &candidates)` returning void, producing count + pointer via output parameters.
- **Code does**: V1 version `cxf_pricing_candidates(ctx, rc, vs, nv, tol, out, max_out)` in pricing_internal.h takes reduced costs, status, tolerance and returns int count. This is the V1 API. The V2 version `cxf_pricing_candidates_v2` matches the spec's output-parameter style.
- **File**: `src/pricing/pricing_internal.h:20-22`
- **Analysis**: The V1 function retains the old signature. The V2 function `cxf_pricing_candidates_v2` mostly matches, but the name differs from the spec (`cxf_pricing_candidates`). The spec function name is occupied by the V1 implementation.

### [V20] cxf_pricing_update (spec name) — Named cxf_pricing_update_queues in code
- **Spec says**: The consumer function is named `cxf_pricing_update`.
- **Code does**: Named `cxf_pricing_update_queues`.
- **File**: `src/pricing/update.c:91`
- **Analysis**: Function name deviation. The spec calls it `cxf_pricing_update`, the implementation calls it `cxf_pricing_update_queues`.

### [V21] cxf_pricing_update_queues — Level 0 does not use separate constraint status array
- **Spec says**: Level 0 filters constraint queue "keeping only entries whose constraint status is non-negative" using the constraint status array, and variable queue using the variable status array.
- **Code does**: Uses a single `var_status` array from `state->basis->var_status` for both queues. For constraint queues, there is no separate constraint status array -- it just keeps all valid-index entries.
- **File**: `src/pricing/update.c:104,111-115`
- **Analysis**: The spec envisions separate constraint and variable status arrays. The implementation uses only the variable status array, and the constraint queue filtering has no status check at all (see V6).

---

## Missing Functions (in spec but not implemented)

### cxf_pricing_get_var_stats
- **Spec**: pricing_support.md defines `cxf_pricing_get_var_stats(pricingState, &countOut, &queueOut)` -- lightweight accessor returning committed variable count and queue pointer at current level.
- **Status**: NOT IMPLEMENTED. No function with this name exists anywhere in the codebase. The constraint-side counterpart `cxf_pricing_get_constr_stats` IS implemented.

---

## Extra Functions (in code but not in spec)

### cxf_pricing_create (context.c)
Not in V2 spec. Lifecycle management (allocation).

### cxf_pricing_free (context.c)
Not in V2 spec. Lifecycle management (deallocation). Spec mentions destruction in pricing_state.md lifecycle section but not as a named function.

### cxf_pricing_init (init.c)
Not in V2 spec as a named function. Spec describes initialization in pricing_state.md lifecycle section.

### cxf_pricing_init_constrs (constr_init.c)
Not in V2 spec. Separate constraint-side initialization.

### cxf_pricing_step2 (phase.c)
Not in V2 spec. Fallback full-scan pricing with Dantzig-like variable selection.

### cxf_pricing_steepest (steepest.c)
Not in V2 spec (no steepest edge module spec exists in V2). Steepest edge pricing with SE ratio.

### cxf_pricing_compute_weight (steepest.c)
Not in V2 spec. Helper for computing SE weight from BTRAN result.

### cxf_pricing_update_weights (weight_update.c)
Not in V2 spec. SE/Devex weight update after pivot.

### cxf_pricing_recompute_weights (weight_update.c)
Not in V2 spec. Full weight recomputation at refactorization.

### cxf_pricing_get_constr_candidates (queue.c, V1 version)
V1 function returning dirty constraints via array copy. Not in V2 spec.

### cxf_pricing_cascade_update (queue.c)
Spec equivalent: `cxf_pricing_cascade_update` in pricing_support.md. Name matches but behavior differs (see V14, V15).

### v2_insert_var / v2_insert_constr (queue_insert.c)
Internal helpers. Not spec functions -- they implement the shared queue insertion protocol.

---

## Notes

### 1. V1/V2 Dual System
The codebase maintains both V1 and V2 pricing systems in parallel. V1 uses simple boolean dirty flags and array-based candidate lists. V2 uses the spec-compliant 4-bit flag queues with committed/pending split. Many functions (mark_dirty, update_var, update_constr) maintain both V1 and V2 state simultaneously for backward compatibility. This dual maintenance adds complexity and potential inconsistency risk.

### 2. Naming Conventions
Several spec functions have different names in the implementation:
| Spec Name | Implementation Name |
|-----------|-------------------|
| `cxf_pricing_candidates` | `cxf_pricing_candidates_v2` |
| `cxf_pricing_update` | `cxf_pricing_update_queues` |
| `cxf_pricing_invalidate` (single-var dirty) | `cxf_pricing_mark_dirty` (closest equivalent) |
| `cxf_pricing_end_level` | `cxf_pricing_end_level` (name matches but split semantics) |
| `cxf_pricing_get_constr_candidates` (V2) | `cxf_pricing_constr_candidates_v2` |

### 3. Eta Vector Mode Entirely Missing
All three spec functions that require dual traversal mode (eta + matrix) only implement matrix mode. This is the largest systematic gap. The spec explicitly describes eta vector traversal as an alternative path for when the solver uses the Product Form of Inverse. If the solver refactors using LU (which it does per MEMORY.md), eta mode may be unnecessary in practice, but the spec still requires it.

### 4. Threshold Constants Match Spec
The adaptive strategy thresholds are correctly set to the spec's recommended starting values:
- EXPANSION_MULTIPLIER = 2.0 (spec: "near 2.0")
- COVERAGE_FRACTION = 0.5 (spec: "near 0.5")
- EXPANSION_WORK_FACTOR = 5e-4 (spec: "near 5e-4")

### 5. Flag Bit Encoding Matches Spec
The 4-bit flag encoding in pricing_internal.h matches pricing_state.md exactly:
- Bit 0 (0x01): Level 1 committed
- Bit 1 (0x02): Level 1 pending
- Bit 2 (0x04): Level 2 committed
- Bit 3 (0x08): Level 2 pending

### 6. Queue Insertion Protocol Correct
The committed/pending insertion protocol in queue_insert.c correctly implements Phase 2 of the partial_pricing algorithm spec, including the phase-active guard and pending bit marking.

### 7. Status Semantics Confusion
The spec consistently uses "non-negative status" to mean "valid/eligible" for both constraints and variables. However, in the simplex context, nonbasic variables (the ones that need pricing) have NEGATIVE status (-1, -2, -3), while basic variables have non-negative status (encoding their basis row). The implementation correctly selects nonbasic variables for pricing, but this inverts the spec's literal language. This confusion pervades multiple functions (V1, V7) and suggests the spec's "non-negative status" language was written from the constraint perspective (where non-negative means "active constraint") but applied uniformly to variables where the semantics differ.

### 8. Missing Separate Constraint Status Array
The spec envisions separate constraint and variable status arrays in SolverState. The implementation only uses `basis->var_status` which covers both variables and constraints in a unified array. This causes constraint queue filtering to lack proper status checks (V6, V21).

### 9. Cache Slot Usage
The spec describes three cache slots per queue per level (primary, secondary, tertiary). The implementation has all three slots (`cached_var_count`, `cached_var_count2`, `cached_var_count3` and constraint equivalents) but only uses the primary slot. The secondary and tertiary slots are initialized to -1 and invalidated but never populated.
