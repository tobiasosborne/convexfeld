# Structure: PricingContext

**Spec Version:** 1.0
**Primary Module:** Pricing

## 1. Purpose

### 1.1 What It Represents

The PricingContext structure implements multi-level partial pricing for efficient entering variable selection in the simplex method. Rather than examining all non-basic variables to find the best candidate for entering the basis (full pricing), partial pricing maintains a hierarchy of candidate subsets organized into levels. Each level contains a progressively larger subset of variables, allowing the pricing algorithm to start with a small candidate set and expand only when necessary.

The structure tracks the current pricing level, stores pre-computed candidate lists for each level, caches computed results to avoid redundant work, and manages the neighbor expansion that exploits problem structure to discover improving variables efficiently. This multi-level approach can reduce pricing cost by 10-100x on large sparse problems compared to full pricing.

### 1.2 Role in System

The PricingContext is used during every simplex iteration to select the entering variable:
1. **Initialization:** Created at solve start, candidate lists built based on problem structure
2. **Pricing step:** Queries for candidates at current level
3. **Reduced cost computation:** BTRAN + dot products for candidates
4. **Selection:** Choose best (most negative reduced cost for dual simplex)
5. **Level management:** Escalate to higher level if current level yields no improving variable
6. **Cache invalidation:** After each pivot, cached results are marked stale

The pricing strategy (devex, steepest edge, dantzig) operates on the candidate set provided by this structure.

### 1.3 Design Rationale

The multi-level design balances exploration (finding good entering variables) vs. exploitation (avoiding unnecessary work). Starting with a small candidate set (level 1) is fast but may miss the best entering variable. If no improving variable is found, escalating to level 2 (larger set) increases the chance of success. Level 0 is the fallback containing all non-basic variables, guaranteeing that an improving variable will be found if one exists.

Caching avoids recomputing candidate lists during ratio test retries or multiple pricing attempts within a single iteration. Neighbor expansion leverages problem sparsity: if variable j has good reduced cost and variable k shares constraints with j, then k is also likely to have good reduced cost (clustered improvement).

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| currentLevel | int | Active pricing level (0 = full, 1-N = partial) | 0 to maxLevels-1 |
| maxLevels | int | Number of levels in hierarchy | Typically 3-5 |
| candidateCounts | Array[int] | Number of candidates at each level | length = maxLevels |
| candidateStarts | Array[int64*] | Start position in candidate array for each level | length = maxLevels |
| levelSizes | Array[int] | Total size (capacity) of each level's array | length = maxLevels |
| candidateArrays | Array[int64*] | Variable indices for each level's candidate set | length = maxLevels |
| cachedCounts | Array[int] | Cached result count per level (-1 = invalid) | length = maxLevels |
| outputBuffers | Array[int64*] | Output buffer for computed candidate lists | length = maxLevels |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| cachedCounts[level] | Previous candidate computation | Avoid recomputation within iteration | Pivot invalidates cache |
| outputBuffers[level] | Candidate filtering and expansion | Store filtered candidate list | Computed on cache miss |

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| selectionFlags | Array[bool] | Partial pricing mode | NULL if always using full scan |
| neighborLists | Array[NeighborList*] | Structure-aware pricing | NULL if no neighbor expansion |
| workCounter | double* | Performance tracking | NULL if profiling disabled |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| lastPivotIteration | int | Iteration number of last pivot (for cache invalidation) |
| totalCandidatesScanned | int64 | Cumulative candidates evaluated (statistics) |
| levelEscalations | int | Count of level increases (statistics) |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| candidateArrays | maxLevels | Exclusive - allocated at init, freed at cleanup |
| outputBuffers | maxLevels | Exclusive - allocated at init, freed at cleanup |
| selectionFlags | 0:1 | Exclusive - only if partial pricing enabled |
| neighborLists | 0:1 | Exclusive - only if structure-aware pricing enabled |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| SolverContext | 1:1 | Must outlive PricingContext - needed for variable status |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| SolverContext | pricingState pointer field | Referenced by SolverContext, lazily created |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] currentLevel is in range [0, maxLevels-1]
- [x] For each level i: candidateCounts[i] <= levelSizes[i]
- [x] Level 0 contains all non-basic variables (full candidate set)
- [x] Level i+1 contains a superset of level i (monotonic expansion)
- [x] cachedCounts[level] is either -1 (invalid) or a valid count
- [x] All array pointers are either NULL or valid allocated memory

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] If cachedCounts[level] >= 0, then outputBuffers[level] contains valid data
- [x] candidateCounts[0] equals the number of non-basic variables
- [x] Sum of levelSizes proportional to numVars (typically 1-2x numVars total)
- [x] If selectionFlags is non-NULL, length equals numVars

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After initialization | All levels populated, cachedCounts[*] = -1, currentLevel = 1 |
| After successful pricing | cachedCounts[currentLevel] >= 0, candidate selected |
| After level escalation | currentLevel incremented, cachedCounts[new level] = -1 |
| After pivot | All cachedCounts[*] = -1, selectionFlags cleared |
| Before destruction | All arrays freed, pointers NULL |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_pricing_init | First pricing operation in solve | Levels built, currentLevel=1, caches invalid |

### 5.2 States

```
UNINITIALIZED
     |
     v
INITIALIZED --> LEVEL_1_ACTIVE --> LEVEL_2_ACTIVE --> LEVEL_0_FALLBACK
     |              |  ^               |  ^                 |
     |              v  |               v  |                 v
     |           CACHED (reuse)     CACHED (reuse)      CACHED
     |              |                   |                   |
     |              v                   v                   v
     +--------> INVALIDATED -------> DESTROYED
                (after pivot)
```

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| UNINITIALIZED | INITIALIZED | cxf_pricing_init | Allocate arrays, build candidate lists |
| INITIALIZED | LEVEL_1_ACTIVE | First pricing call | Set currentLevel=1 |
| LEVEL_1_ACTIVE | CACHED | Compute candidates | Set cachedCounts[1], populate outputBuffers[1] |
| CACHED | CACHED | Reuse within iteration | Return cached result (no computation) |
| LEVEL_1_ACTIVE | LEVEL_2_ACTIVE | No improving variable | Increment currentLevel |
| LEVEL_2_ACTIVE | LEVEL_0_FALLBACK | No improving variable | Set currentLevel=0 (full pricing) |
| CACHED | INVALIDATED | Pivot occurs | Set all cachedCounts[*] = -1 |
| INVALIDATED | LEVEL_1_ACTIVE | Next iteration starts | Reset currentLevel=1 |
| Any | DESTROYED | Solve cleanup | Free all arrays |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| cxf_pricing_cleanup | Free candidateArrays, outputBuffers, selectionFlags, neighborLists |

### 5.5 Ownership Rules

- **Who creates:** cxf_pricing_init (lazy creation on first pricing operation)
- **Who owns:** SolverContext (via pricingState pointer)
- **Who destroys:** cxf_simplex_final (as part of SolverContext cleanup)
- **Sharing allowed:** No - single owner per solve

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| Get current level | currentLevel | O(1) |
| Get candidate count at level | candidateCounts[level] | O(1) |
| Get candidate list at level | candidateArrays[level] | O(1) pointer return |
| Check cache validity | cachedCounts[level] >= 0 | O(1) |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| Escalate level | Increment currentLevel | Current level cache (if escalating) |
| Compute candidates | Fill outputBuffers, set cachedCounts | Nothing (creates cache entry) |
| Invalidate cache | Set all cachedCounts to -1 | All cached results |
| Reset level | Set currentLevel back to 1 | Nothing (cache already invalid) |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| Build candidate hierarchy | Initialize all levels from problem structure | All-or-nothing (init phase) |
| Rebuild after major change | Reconstruct neighbor lists after basis change | Not atomic (rare operation) |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed overhead | ~100-200 bytes (structure + level metadata arrays) |
| Per-level candidate arrays | levelSizes[i] x 8 bytes per level |
| Per-level output buffers | levelSizes[i] x 8 bytes per level |
| Selection flags | numVars x 1 byte (if enabled) |
| Neighbor lists | numVars x avgDegree x 8 bytes (if enabled) |
| Total | ~200 bytes + 2 x Sum(levelSizes[i]) x 8 + optional fields |

**Example:** 10,000 vars, 3 levels (sizes: 10000, 2000, 500)
- Candidate arrays: (10000 + 2000 + 500) x 8 = 100 KB
- Output buffers: (10000 + 2000 + 500) x 8 = 100 KB
- Selection flags: 10000 bytes = 10 KB
- **Total:** ~210 KB + neighbor lists

### 7.2 Allocation Strategy

Arrays are allocated once during initialization and persist for the entire solve. Level 0 (full pricing) is pre-populated with all non-basic variable indices. Levels 1-N are built using heuristics (random sampling, structural clustering, or frequency-based selection from previous solves).

Output buffers are working space for filtered candidate lists. Selection flags are reused across calls to avoid repeated allocation.

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| Structure itself | 8 bytes (contains pointers) |
| candidateArrays | 8 bytes (int64 arrays) |
| outputBuffers | 8 bytes (int64 arrays) |
| selectionFlags | 1 byte (bool/char array) |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Not thread-safe

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Read | None if no concurrent writes |
| Write (cache update) | Exclusive access to PricingContext |
| Level escalation | Exclusive access |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- None - pricing is inherently sequential in simplex

**Unsafe patterns:**
- Concurrent cache reads/writes during parallel pricing attempts
- Multiple threads escalating levels simultaneously
- Reading candidates while another thread invalidates cache

## 9. Serialization

### 9.1 Persistent Format

The PricingContext is not serialized. It is ephemeral and rebuilt on each solve. The candidate hierarchy is problem-dependent and changes as the basis changes, making serialization of limited value.

### 9.2 Version Compatibility

| Version | Compatible | Migration |
|---------|------------|-----------|
| N/A | N/A | Not applicable (ephemeral structure) |

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Level 0 (full) access | Sequential scan | Cache-friendly, pre-built list |
| Level 1-N access | Cache lookup then scan | O(1) cache hit, O(k) scan on miss |
| Neighbor expansion | Linked list traversal | Poor cache locality but exploits sparsity |
| Selection flags | Random access | Boolean array, very cache-friendly |

### 10.2 Cache Behavior

Candidate arrays have good spatial locality (sequential int64 arrays). The caching mechanism (cachedCounts) is very effective within an iteration--typical hit rate 50-90% for ratio test retries. Selection flags fit in cache for problems up to ~100K variables (100 KB).

### 10.3 Memory Bandwidth

Candidate list generation is memory-bound when scanning large level 0 lists. Partial pricing (levels 1-N) reduces bandwidth by examining only a subset. The speedup is proportional to the size ratio: level 1 with 1000 candidates vs. level 0 with 50,000 candidates gives 50x bandwidth reduction (and similar speedup if candidates uniformly distributed).

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| Level out of range | currentLevel >= maxLevels | Clamp to maxLevels-1 (level 0) |
| Negative cached count | cachedCounts[level] < -1 | Reset to -1 (invalid) |
| NULL required array | Array pointer is NULL | Allocation failure, return error |
| Empty candidate list | candidateCounts[0] = 0 | Problem has no non-basic variables (should not happen) |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| Level bounds | On escalation | O(1) |
| Cache validity | Before using cached result | O(1) |
| Array allocation | At initialization | O(1) per allocation |
| Candidate count consistency | Debug builds | O(maxLevels) |

## 12. Examples

### 12.1 Typical Instance

**Problem:** 10,000 variables, 5,000 constraints, sparse

**Configuration:**
- maxLevels = 3
- Level 0 (fallback): 10,000 candidates (all non-basic)
- Level 1 (primary): 500 candidates (5% sample)
- Level 2 (secondary): 2,000 candidates (20% sample)

**Iteration profile:**
- 80% of iterations: Level 1 sufficient (find improving variable)
- 15% of iterations: Escalate to Level 2
- 5% of iterations: Fallback to Level 0

**Speedup:** Effective candidate evaluation reduced from 10,000 to ~1,000 average (10x speedup)

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| Small problem | n < 1000 | Use only level 0 (no partial pricing benefit) |
| Dense problem | All variables connected | Neighbor expansion ineffective, level 1 ~ level 0 |
| Single candidate | Only one non-basic variable | All levels contain same variable |
| All basic | No non-basic variables | Empty candidate lists (should not occur in valid state) |
| Cold start | First iteration | All caches invalid, level 1 active |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| SolverContext | Parent, PricingContext is referenced by SolverContext |
| BasisState | Provides variable status used for candidate filtering |
| WorkingArrays | Reduced costs computed for candidates returned by PricingContext |

## 14. References

- Goldfarb, D. and Reid, J. (1977). "A Practicable Steepest-Edge Simplex Algorithm." *Mathematical Programming*, 12(1):361-371.
- Forrest, J. J. and Goldfarb, D. (1992). "Steepest-Edge Simplex Algorithms for Linear Programming." *Mathematical Programming*, 57(1):341-374.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*, Chapter 7: Pricing Strategies. Springer.
- Koberstein, A. (2005). "The Dual Simplex Method, Techniques for a Fast and Stable Implementation." Ph.D. thesis. Sections on partial pricing.
- Bixby, R. E. (1992). "Implementing the Simplex Method: The Initial Basis." *ORSA Journal on Computing*, 4(3):267-284.

## 15. Validation Checklist

Before finalizing:

- [x] All fields documented with logical meaning, not byte offsets
- [x] All invariants identified (structural, consistency, temporal)
- [x] Lifecycle complete with state diagram
- [x] Thread safety analyzed (not thread-safe)
- [x] No implementation details leaked (no magic offsets)
- [x] Could implement structure from this spec (using standard data structures)
- [x] Standard optimization terminology used (pricing, entering variable, candidate sets)
- [x] Relationships to other structures documented
- [x] Performance characteristics explained (cache hit rates, speedup factors)
- [x] References to optimization literature included

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
