# Audit Report: Pricing & Crash Basis
**Auditor:** Agent B4
**Date:** 2026-02-16
**Scope:** src/pricing/*.c, src/simplex/crash.c
**Specs:** algorithms/partial_pricing.md, crash_basis.md, data-model/pricing_state.md

## Summary
- Total violations: 25
- Critical: 8 / Major: 11 / Minor: 6

## Executive Summary

The pricing implementation deviates fundamentally from the v2 specification in multiple critical ways:

1. **CRITICAL: Wrong Data Structure** - Implemented `PricingContext` instead of spec's `PricingState`, missing 90% of required fields
2. **CRITICAL: Missing Multi-Level Partial Pricing** - No queue system, no committed/pending split, no dirty marking
3. **CRITICAL: Wrong Strategy Selection** - Implemented section-cycling approach instead of neighbor-expansion partial pricing
4. **MAJOR: Missing Steepest Edge Integration** - No weight update formula, no BTRAN-based weight computation
5. **MAJOR: Crash Basis Missing Triangularity** - All-slack basis only, no structural variable selection

The implementation appears to be a classical "Dantzig pricing with simple partial pricing" approach, which is NOT what the spec describes. The spec requires a sophisticated multi-level neighbor-expansion scheme with dual queue systems, committed/pending splits, and adaptive strategy selection.

---

## Violations

### V-01: Fundamental Data Structure Mismatch
- **Severity:** CRITICAL
- **File:** include/convexfeld/cxf_pricing.h:20-43
- **Spec reference:** data-model/pricing_state.md, entire document
- **Description:** Implemented `PricingContext` structure bears almost no resemblance to spec's `PricingState`
- **Expected:** PricingState with:
  - `currentLevel`, `levelActive[MAX_LEVELS]`
  - Dual queue systems: `constrFlags`, `constrQueueCommitted[MAX_LEVELS]`, `constrQueueTotal[MAX_LEVELS]`, `constrQueue[MAX_LEVELS]`
  - Parallel variable queue system: `varFlags`, `varQueueCommitted`, `varQueueTotal`, `varQueue`
  - Six cache slots per level: `cachedConstrCount[3]`, `cachedConstrCount2[3]`, `cachedConstrCount3[3]` and var equivalents
  - Output buffers: `constrOutputBuffer[MAX_LEVELS]`, `varOutputBuffer[MAX_LEVELS]`
- **Actual:** PricingContext with:
  - `current_level`, `max_levels` (similar)
  - `candidate_counts`, `candidate_arrays`, `candidate_sizes` (single array, not dual queues)
  - `weights` (for steepest edge, spec says this is separate)
  - `cached_counts` (single cache slot, not six)
  - No flag arrays, no committed/pending split, no dual queue systems

**Impact:** This structural mismatch cascades through the entire implementation. The spec's algorithm fundamentally depends on the dual queue architecture.

---

### V-02: Missing Constraint Queue System
- **Severity:** CRITICAL
- **File:** All pricing files
- **Spec reference:** data-model/pricing_state.md:16-42, algorithms/partial_pricing.md:74-108
- **Description:** No constraint queue system exists
- **Expected:**
  - `constrFlags[numConstrs]` - membership flags
  - `constrQueue[MAX_LEVELS]` - per-level queue arrays
  - `constrQueueCommitted[MAX_LEVELS]`, `constrQueueTotal[MAX_LEVELS]` - counts
  - Dirty marking function: `MARK-CONSTRAINTS-DIRTY(pricingState, solverState, enteringVar)`
- **Actual:** None of these exist

**Impact:** Cannot implement neighbor-expansion partial pricing. Spec's algorithm requires tracking which constraints are affected by variable changes.

---

### V-03: Missing Variable Queue System
- **Severity:** CRITICAL
- **File:** All pricing files
- **Spec reference:** data-model/pricing_state.md:43-68, algorithms/partial_pricing.md:94-108
- **Description:** No variable queue system exists
- **Expected:**
  - `varFlags[numVars]` - membership flags
  - `varQueue[MAX_LEVELS]` - per-level queue arrays
  - `varQueueCommitted[MAX_LEVELS]`, `varQueueTotal[MAX_LEVELS]` - counts
  - Dirty marking function: `MARK-VARIABLES-DIRTY(pricingState, solverState, leavingConstr)`
- **Actual:** None of these exist

**Impact:** Cannot implement neighbor-expansion partial pricing. Spec's algorithm requires tracking which variables are affected by constraint changes.

---

### V-04: Missing Committed/Pending Split
- **Severity:** CRITICAL
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:117-166
- **Description:** No committed/pending queue split mechanism
- **Expected:**
  - Queue layout: `[committed entries | pending entries]` with boundary at `committedCount`
  - Flag-based insertion logic checking if level is active
  - Promotion logic in update function moving pending to committed
- **Actual:** Simple candidate array without split mechanism

**Impact:** Cannot safely handle concurrent dirty marking during queue iteration, violating spec's producer-consumer safeguard.

---

### V-05: Missing Flag-Based Duplicate Prevention
- **Severity:** MAJOR
- **File:** All pricing files
- **Spec reference:** data-model/pricing_state.md:70-86, algorithms/partial_pricing.md:119-154
- **Description:** No per-element flag arrays for O(1) duplicate prevention
- **Expected:**
  - `constrFlags[numConstrs]` and `varFlags[numVars]` byte arrays
  - Bit encoding: bit 0 (level-1 committed), bit 1 (level-1 pending), bit 2 (level-2 committed), bit 3 (level-2 pending)
  - Insertion checks: `if flags has neither committed nor pending bit set: add to queue`
- **Actual:** None

**Impact:** Must use linear search for duplicate prevention (O(n) per insertion) or accept duplicates.

---

### V-06: Wrong Partial Pricing Strategy
- **Severity:** CRITICAL
- **File:** src/pricing/candidates.c:98-110
- **Spec reference:** algorithms/partial_pricing.md:235-329
- **Description:** Implemented simple section-cycling instead of neighbor-expansion partial pricing
- **Expected:**
  - Adaptive strategy selection with three threshold checks (EXPANSION_THRESHOLD, COVERAGE_THRESHOLD, EXPANSION_WORK_FACTOR)
  - Full scan or partial expansion based on problem characteristics
  - Partial expansion: seed with dirty set, expand through structural neighbors via CSC/CSR or eta vectors
- **Actual:**
```c
int section_size = num_vars / DEFAULT_NUM_SECTIONS;
int current_section = ctx->last_pivot_iteration % DEFAULT_NUM_SECTIONS;
start_idx = current_section * section_size;
end_idx = start_idx + section_size;
```
This is Dantzig's 1963 sectional pricing (fixed partitions), NOT the spec's neighbor-expansion approach.

**Impact:** Misses spatial locality benefits. Spec's approach focuses on variables structurally adjacent to the pivot; section-cycling scans arbitrary partitions.

---

### V-07: Missing Neighbor Expansion Logic
- **Severity:** CRITICAL
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:290-329
- **Description:** No code for expanding candidate set through structural neighbors
- **Expected:**
  - `PARTIAL-EXPANSION(level)` function
  - Step 1: Seed output with base dirty set, mark in selection flags
  - Step 2: Expand through CSC column (or CSR row) for each dirty entry: `for each neighbor in column/row of dirtyEntry: if neighbor is valid AND selectionFlags[neighbor] = 0: add to output`
  - Step 3: Filter to keep only status-valid candidates, clear flags
- **Actual:** None of this exists

**Impact:** Cannot implement spec's multi-level partial pricing algorithm.

---

### V-08: Missing Queue Processing (Consumer Function)
- **Severity:** CRITICAL
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:167-210
- **Description:** No queue processing/update function as specified
- **Expected:**
  - `PROCESS-QUEUES(pricingState, solverState)` called between iterations
  - Level 0: filter by status validity only
  - Levels 1-2: filter with flag promotion/demotion, invalidate six cache slots
  - Pending-to-committed promotion logic
- **Actual:** `cxf_pricing_update` (update.c:40) exists but only updates SE weights and invalidates simple cache, doesn't process queues

**Impact:** Cannot maintain queue invariants, cannot handle committed/pending split.

---

### V-09: Missing Dirty Marking Producers
- **Severity:** CRITICAL
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:74-115
- **Description:** No dirty marking functions to populate queues after pivots
- **Expected:**
  - `MARK-CONSTRAINTS-DIRTY(pricingState, solverState, enteringVar)` - scans CSC column or eta list
  - `MARK-VARIABLES-DIRTY(pricingState, solverState, leavingConstr)` - scans CSR row or eta list
  - `MARK-ELEMENT-DIRTY(pricingState, elementIdx)` - direct marking
  - `ADD-TO-CONSTRAINT-QUEUES`, `ADD-TO-VARIABLE-QUEUES` helpers
- **Actual:** None of these exist

**Impact:** No way to build the dirty sets that drive the partial pricing algorithm.

---

### V-10: Wrong Cache Structure
- **Severity:** MAJOR
- **File:** include/convexfeld/cxf_pricing.h:37, src/pricing/context.c:61-63
- **Spec reference:** data-model/pricing_state.md:34-41, 61-68
- **Description:** Single cache slot per level instead of six
- **Expected:**
  - Three constraint cache slots: `cachedConstrCount[MAX_LEVELS]`, `cachedConstrCount2[MAX_LEVELS]`, `cachedConstrCount3[MAX_LEVELS]`
  - Three variable cache slots: `cachedVarCount[MAX_LEVELS]`, `cachedVarCount2[MAX_LEVELS]`, `cachedVarCount3[MAX_LEVELS]`
  - Rationale: "support caching of different candidate subsets...when switching between strategies within a single iteration"
- **Actual:** Single `cached_counts[max_levels]` array

**Impact:** Cannot cache multiple pricing strategy views simultaneously.

---

### V-11: Missing Output Buffers
- **Severity:** MAJOR
- **File:** All pricing files
- **Spec reference:** data-model/pricing_state.md:41, 68
- **Description:** No separate output buffers for candidate retrieval results
- **Expected:**
  - `constrOutputBuffer[MAX_LEVELS]` - per-level output buffers for constraint candidates
  - `varOutputBuffer[MAX_LEVELS]` - per-level output buffers for variable candidates
  - "Populated by the candidate retrieval function; valid only when the corresponding cachedCount is not -1"
- **Actual:** `candidate_arrays` serves dual purpose (work arrays and output), but only one per level, not dual (constraint/variable)

**Impact:** Cannot properly separate constraint pricing from variable pricing.

---

### V-12: Missing Level Management Functions
- **Severity:** MAJOR
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:336-353
- **Description:** No explicit level management functions
- **Expected:**
  - `SET-LEVEL(pricingState, level)` - sets currentLevel
  - `END-LEVEL(pricingState, solverState)` - filters queues, invalidates caches
  - Typical sequence: set level 0, retrieve candidates, evaluate; if no candidate, end level 0, set level 1, retrieve, evaluate; etc.
- **Actual:** `current_level` field exists but no proper get/set/end functions

**Impact:** Cannot properly manage multi-level progression as spec requires.

---

### V-13: Wrong Steepest Edge Weight Update
- **Severity:** MAJOR
- **File:** src/pricing/update.c:54-74
- **Spec reference:** algorithms/partial_pricing.md:380, steepest edge references (Goldfarb & Reid 1977)
- **Description:** Incomplete/incorrect steepest edge weight update formula
- **Expected:** Full SE update using formula from Goldfarb & Reid (1977):
  - `tau = gamma_entering / pivot_sq`
  - For each nonbasic variable j: `gamma_j_new = gamma_j - 2*alpha_j*pivot_column[i]*tau + alpha_j^2*tau` where `alpha_j = B^(-1) * a_j` (requires BTRAN)
- **Actual:**
```c
double tau = gamma_entering / pivot_sq; // computed but unused (line 64)
(void)gamma_entering;  /* Suppress warning until full SE update */
// Only resets entering variable's weight to 1.0
```
Comment admits it's incomplete: "Full update requires alpha_j for each variable j, which needs matrix access."

**Impact:** Steepest edge weights become increasingly inaccurate, defeating the purpose of SE pricing.

---

### V-14: Missing Devex Strategy
- **Severity:** MINOR
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:7 (Harris 1973 Devex reference)
- **Description:** Devex strategy mentioned in init.c:22, 157 but never actually implemented
- **Expected:** Devex approximate steepest edge weights (Harris 1973)
- **Actual:** Strategy code 3 (STRATEGY_DEVEX) allocates weights array but update.c only handles strategy 2 (steepest edge)

**Impact:** Selecting Devex strategy would allocate weights but never update them correctly.

---

### V-15: Wrong Variable Status Encoding
- **Severity:** MAJOR
- **File:** src/pricing/candidates.c:18-21, steepest.c:17-21, phase.c:14-18
- **Spec reference:** Implicit in partial_pricing.md:34 "Variable status array: negative values indicate non-basic status"
- **Description:** Inconsistent with spec's encoding and missing fixed/superbasic states
- **Expected:** Per spec partial_pricing.md line 34: "Non-negative values indicate the variable is basic (the value encoding the constraint row in which it is basic); negative values indicate non-basic status"
- **Actual:**
```c
#define VAR_AT_LOWER   -1
#define VAR_AT_UPPER   -2
#define VAR_FREE       -3
```
This is correct for the negative-means-nonbasic convention. However, the enum in cxf_types.h:100-106 uses different encoding:
```c
CXF_BASIC      = 0,  // Variable is basic
CXF_NONBASIC_L = 1,  // At lower bound
CXF_NONBASIC_U = 2,  // At upper bound
CXF_SUPERBASIC = 3,  // Between bounds
CXF_FIXED      = 4   // Fixed (lb == ub)
```

**Impact:** Encoding mismatch between spec's convention (status >= 0 means basic in row `status`, status < 0 means nonbasic) and type system's enum (status = 0 means basic, status > 0 means nonbasic).

---

### V-16: Missing Adaptive Strategy Threshold Checks
- **Severity:** MAJOR
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:235-275
- **Description:** No threshold-based decision logic for choosing full scan vs partial expansion
- **Expected:** Three threshold checks:
  - Threshold 1: `if n <= dirtyCount * EXPANSION_THRESHOLD: FULL-SCAN`
  - Threshold 2: `if n <= queueSize * COVERAGE_THRESHOLD: FULL-SCAN`
  - Threshold 3: Estimate expansion cost, `if n < queueSize * EXPANSION_WORK_FACTOR + expansionEstimate: FULL-SCAN`
  - Else: `PARTIAL-EXPANSION`
  - Typical values: EXPANSION_THRESHOLD=2.0, COVERAGE_THRESHOLD=0.5, EXPANSION_WORK_FACTOR=5e-4
- **Actual:** None of this logic exists

**Impact:** Cannot adaptively choose between full scan and partial expansion based on problem characteristics.

---

### V-17: Missing Selection Flags Temporary Workspace
- **Severity:** MAJOR
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:330-332, 419
- **Description:** No temporary selection flags array for O(1) duplicate prevention during expansion
- **Expected:**
  - `selectionFlags[max(numVars, numConstrs)]` temporary workspace
  - Used during partial expansion: mark as selected (1), check for duplicates, then clear incrementally during filter step
  - "zeroed incrementally (each flag is cleared during the filter step) rather than requiring a full array clear"
- **Actual:** None

**Impact:** Would need O(n) duplicate checking during neighbor expansion if it were implemented.

---

### V-18: Missing Eta Mode Support
- **Severity:** MAJOR
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:82-91, 98-107, 302-318, 439-440
- **Description:** No support for eta vector traversal mode
- **Expected:**
  - Dual traversal modes: matrix mode (CSC/CSR) and eta mode (eta vector linked lists)
  - Dirty marking: "if solverState uses eta representation: for each entry E in etaList[enteringVar]..."
  - Neighbor expansion: "Traverse eta vector linked list for this entry"
- **Actual:** No eta mode logic anywhere

**Impact:** Cannot use eta-based traversal, which spec says "may visit fewer entries when the basis update history is short."

---

### V-19: Missing Work Counter Accumulation
- **Severity:** MINOR
- **File:** All pricing files
- **Spec reference:** algorithms/partial_pricing.md:384-385
- **Description:** No work counter tracking as specified
- **Expected:** "Each operation (queue insertion, status scan, neighbor traversal, filter pass) contributes to a work counter that tracks the computational effort of the pricing subsystem."
- **Actual:** `total_candidates_scanned` in candidates.c:160, steepest.c:112, phase.c:81, but this only counts scanned candidates, not all pricing work

**Impact:** Cannot track full computational effort for basis refactorization trigger.

---

### V-20: Wrong Candidate Retrieval Signature
- **Severity:** MAJOR
- **File:** src/pricing/candidates.c:81-83
- **Spec reference:** algorithms/partial_pricing.md:218-233
- **Description:** Function signature doesn't match spec's algorithm
- **Expected:**
```
GET-CANDIDATES(pricingState, solverState) -> (count, candidateArray):
    level := pricingState.currentLevel
    // Case 1: Level 0 fast path
    if level = 0: return (committedCount[0], queue[0])
    // Case 2: Cache hit
    if cachedCount[level] != -1: return (cachedCount[level], outputBuffer[level])
    // Case 3: Cache miss -- compute candidate list
```
- **Actual:**
```c
int cxf_pricing_candidates(PricingContext *ctx, const double *reduced_costs,
                           const int *var_status, int num_vars, double tolerance,
                           int *candidates, int max_candidates)
```
Takes reduced_costs and tolerance, performs filtering inline. Spec's GET-CANDIDATES returns indices for later evaluation.

**Impact:** Mixes candidate selection with reduced cost evaluation, preventing proper separation of concerns.

---

### V-21: Crash Basis - Missing Triangularity Heuristic
- **Severity:** MAJOR
- **File:** src/simplex/crash.c:113-133
- **Spec reference:** crash_basis.md:74-155, specifically lines 82-84, 125-140
- **Description:** Crash uses all-slack basis instead of structural variable selection
- **Expected:** Per spec lines 82-83: "The pre-assignment of positive status values to candidate rows is performed by a separate initialization step...which may use sparsity-based heuristics or bound analysis to identify rows that are unlikely to be active at the optimum."
And lines 125-140: "Candidate rows" with removal logic for sparse rows.
- **Actual:**
```c
/* Select slack variable as basic for this row */
int slack_idx = n + i;
basis_header[i] = slack_idx;
var_status[slack_idx] = i;  /* Status = row index means basic */
```
Comment in crash.c:115-126 admits: "Future enhancement: For equality constraints or to improve starting point, score structural variables based on..."

**Impact:** Starting basis is trivial all-slack, defeating the purpose of a crash procedure. Spec says crash should provide "better than...the trivial all-slack basis."

---

### V-22: Crash Basis - Missing Row Pre-Classification
- **Severity:** MAJOR
- **File:** src/simplex/crash.c
- **Spec reference:** crash_basis.md:82-84, 178-179
- **Description:** No pre-classification step to identify candidate rows for removal
- **Expected:**
  - "The pre-assignment of positive status values to candidate rows is performed by a separate initialization step (typically during simplex setup), which may use sparsity-based heuristics or bound analysis"
  - "The determination of which rows are candidates for removal...is not part of the crash procedure itself. This separation of concerns allows different initialization strategies"
- **Actual:** All rows initialized uniformly (crash.c:104-106: `var_status[n + i] = -1;`), no pre-classification

**Impact:** No row removal, no sparsity exploitation.

---

### V-23: Crash Basis - Wrong Feasibility Check
- **Severity:** MINOR
- **File:** src/simplex/crash.c
- **Spec reference:** crash_basis.md:90-166
- **Description:** Crash doesn't perform feasibility checks as specified
- **Expected:** Per spec lines 98-118:
```
IF S.rowStatus[i] = 0 THEN
    // Unassigned row: feasibility check
    IF sense = '=' THEN
        IF |rhs| >= epsilon_feas THEN
            RETURN INFEASIBLE
    ELSE
        IF rhs < -epsilon_feas THEN
            RETURN INFEASIBLE
```
- **Actual:** No feasibility checks in crash.c. Just assigns all slacks as basic.

**Impact:** Cannot detect trivial infeasibility before simplex iterations begin.

---

### V-24: Crash Basis - Missing Row Removal Logic
- **Severity:** MAJOR
- **File:** src/simplex/crash.c
- **Spec reference:** crash_basis.md:124-152
- **Description:** No logic to remove candidate rows and decrement column counts
- **Expected:** Per spec lines 124-152:
```
ELSE IF S.rowStatus[i] > 0 THEN
    // Candidate row: conditional removal
    IF S.constraintSense[i] != '=' AND S.constraintRHS[i] >= epsilon_tiny THEN
        // Remove all column entries in this row
        FOR k := start TO start + count - 1 DO
            LET col := S.rowColIndices[k]
            IF col >= 0 THEN
                S.colNonzeroCounts[col] := S.colNonzeroCounts[col] - 1
                S.rowColIndices[k] := -1    // mark as inactive
```
- **Actual:** None of this exists

**Impact:** Cannot remove sparse non-binding rows, missing key benefit of crash procedure.

---

### V-25: Missing MAX_LEVELS Constant Definition
- **Severity:** MINOR
- **File:** All pricing files
- **Spec reference:** data-model/pricing_state.md:13-14, algorithms/partial_pricing.md:364
- **Description:** Spec uses MAX_LEVELS=3 (levels 0, 1, 2) but code uses max_levels as variable
- **Expected:** `#define MAX_LEVELS 3` or similar constant
- **Actual:** `max_levels` is a field in PricingContext, passed to cxf_pricing_create

**Impact:** Minor - allows flexibility but deviates from spec's fixed three-level design.

---

## Spec Sections Not Implemented

### From algorithms/partial_pricing.md:

1. **Phase 1: Dirty Marking (Producers)** (lines 74-115) - Completely missing
   - MARK-CONSTRAINTS-DIRTY
   - MARK-VARIABLES-DIRTY
   - MARK-ELEMENT-DIRTY

2. **Phase 2: Queue Insertion with Committed/Pending Split** (lines 117-166) - Completely missing
   - ADD-TO-QUEUES logic
   - Flag-based duplicate prevention
   - Committed/pending boundary management

3. **Phase 3: Queue Processing and Cache Invalidation** (lines 167-210) - Completely missing
   - PROCESS-QUEUES function
   - FILTER-BY-STATUS
   - Pending-to-committed promotion
   - Six-cache invalidation

4. **Phase 4: Candidate Retrieval - Adaptive Strategy** (lines 218-333) - Partially implemented (wrong approach)
   - Three threshold checks (lines 243-266) - Missing
   - FULL-SCAN strategy (lines 280-288) - Exists in phase.c but not selected adaptively
   - PARTIAL-EXPANSION strategy (lines 290-329) - Completely missing

5. **Phase 5: Level Management** (lines 336-361) - Partially implemented
   - SET-LEVEL - Missing as explicit function
   - END-LEVEL - Missing

6. **Steepest Edge Weight Update** (lines 380, references to Goldfarb & Reid 1977) - Incomplete
   - Full weight update formula missing

### From crash_basis.md:

1. **Pre-Classification of Candidate Rows** (lines 82-84, 178-179) - Missing

2. **Feasibility Check** (lines 98-118) - Missing

3. **Row Removal Logic** (lines 124-152) - Missing

4. **Column Nonzero Count Maintenance** (lines 134-139, 174) - Missing

5. **Work Counter Tracking** (lines 143-145, 159-162) - Missing

---

## Code Sections Not In Spec

### In src/pricing/candidates.c:

1. **Section-cycling partial pricing** (lines 98-110) - Spec describes neighbor-expansion, not section-cycling
   - This is Dantzig's 1963 sectional pricing approach
   - Spec cites Dantzig (1963) but describes a "multi-level neighbor-expansion" refinement (partial_pricing.md:12)

2. **Inline reduced cost filtering** (lines 116-157) - Spec's GET-CANDIDATES returns indices only, doesn't evaluate reduced costs
   - Spec separates candidate selection from reduced cost evaluation

3. **qsort_r for sorting candidates** (lines 163-166) - Not mentioned in spec
   - Spec doesn't require sorting, though it's reasonable

### In src/pricing/steepest.c:

1. **Full scan steepest edge** (lines 50-117) - Spec says steepest edge is orthogonal to partial pricing (partial_pricing.md:380)
   - "When combined with steepest edge pricing...the candidate list produced by partial pricing is passed to the steepest edge evaluator"
   - Code does full scan of all variables, not just candidates from partial pricing

### In src/pricing/init.c:

1. **Auto-strategy selection** (lines 98-108) - Reasonable but not in spec
   - Spec describes adaptive strategy for full scan vs partial expansion, not for choosing pricing rule

2. **sqrt(n) candidate list sizing** (line 53) - Not in spec
   - Spec's queue arrays are sized to full problem dimension (numConstrs or numVars)

---

## Recommendations

### Critical Path to Spec Compliance:

1. **Replace PricingContext with PricingState** - Implement full spec structure with dual queue systems, flag arrays, six cache slots per level

2. **Implement dirty marking producers** - MARK-CONSTRAINTS-DIRTY, MARK-VARIABLES-DIRTY based on CSC/CSR or eta vectors

3. **Implement queue insertion with committed/pending split** - ADD-TO-QUEUES with flag-based duplicate prevention

4. **Implement queue processing consumer** - PROCESS-QUEUES with filtering, promotion, cache invalidation

5. **Implement neighbor-expansion partial pricing** - Replace section-cycling with PARTIAL-EXPANSION algorithm

6. **Implement adaptive strategy selection** - Three threshold checks to choose full scan vs partial expansion

7. **Complete steepest edge weight updates** - Full formula with BTRAN-based weight computation

8. **Enhance crash basis** - Add pre-classification, feasibility checks, row removal logic

### Design Questions for User:

1. **Partial Pricing Approach** - The implemented section-cycling (Dantzig 1963) is simpler than spec's neighbor-expansion. Is the simpler approach acceptable, or do you want full spec compliance?

2. **Steepest Edge Completeness** - Current SE implementation is incomplete. Do you want full BTRAN-based weight updates, or is approximate Devex sufficient?

3. **Crash Basis Sophistication** - Current all-slack crash is trivial. Do you want structural variable selection and triangularity heuristics?

4. **Data Structure Migration** - Migrating from PricingContext to PricingState is a major refactor. Is backward compatibility required?

---

## Conclusion

The pricing implementation is a classical Dantzig-style pricing system with simple sectional partial pricing. This is NOT what the v2 specification describes. The spec requires a sophisticated multi-level neighbor-expansion partial pricing system with:

- Dual queue architecture (constraints + variables)
- Committed/pending split for safe concurrent marking
- Flag-based O(1) duplicate prevention
- Adaptive strategy selection (three threshold checks)
- Neighbor expansion through sparse matrix structure
- Six cache slots per level for multiple strategy views

The gap between implementation and specification is substantial. This appears to be either:
1. An earlier/simpler design that predates the v2 spec, OR
2. A partial implementation that was never completed

Recommend clarifying with stakeholders whether the v2 spec is the ground truth, or if the simpler implemented approach is acceptable.
