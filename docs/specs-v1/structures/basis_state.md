# Structure: basis_state

**Spec Version:** 1.0
**Primary Module:** Basis Operations

## 1. Purpose

### 1.1 What It Represents

The basis_state structure represents the current factorization of the simplex basis matrix and the status of all variables in the linear program. In the simplex method, a basis B is an m x m non-singular submatrix of the constraint matrix A corresponding to m basic variables. This structure maintains the basis representation using the Product Form of Inverse (PFI) method, where the basis inverse B^-1 is represented as a product of elementary transformation matrices (eta vectors) rather than being stored explicitly.

The structure tracks which variables are basic (in the basis), which are non-basic (at bounds), and maintains the chain of eta vectors that encode the history of pivot operations since the last full refactorization. This representation enables efficient updates to the basis inverse after each simplex pivot without requiring expensive matrix inversions.

### 1.2 Role in System

The basis_state is central to the simplex solver's operation. It is used by:
- **FTRAN (Forward Transformation):** Solves Bx = b to compute primal basic solution values
- **BTRAN (Backward Transformation):** Solves y^T B = c^T to compute dual prices for pricing
- **Pivot Operations:** Updates the basis representation when variables enter/leave
- **Refactorization:** Rebuilds the basis representation to control numerical error
- **Basis Management:** Tracks variable status and validates basis consistency

Every simplex iteration queries and updates this structure through BTRAN (pricing), FTRAN (ratio test), and pivot operations.

### 1.3 Design Rationale

The PFI representation was chosen over explicit LU factorization for its memory efficiency on sparse problems. Each pivot adds one sparse eta vector rather than modifying dense L/U factors. The tradeoff is that FTRAN/BTRAN cost grows linearly with eta count, requiring periodic refactorization to maintain performance. This design is well-suited to problems with sparse constraint matrices where most pivots create sparse eta vectors.

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| variableStatus | Array[int] | Status of each variable: >0 = basic in row N, -1 = at lower bound, -2 = at upper bound, -3 = superbasic, -4 = fixed | length = numVars |
| basisHeader | Array[int] | Maps each row to the basic variable index occupying that row position | length = numConstrs, values in [0, numVars) or -1 for empty |
| etaListHead | EtaFactors* | Head of linked list of eta vectors representing basis transformations | NULL or valid pointer |
| etaCount | int | Total number of eta vectors in the list | >= 0 |
| etaType2Count | int | Number of Type 2 eta vectors (pivot updates) | >= 0, <= etaCount |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| (none) | | | |

The basis_state contains minimal caching. Derived quantities like basic solution values are computed on-demand via FTRAN rather than stored.

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| workBuffer | double* | Temporary storage for FTRAN/BTRAN | Always allocated | NULL if allocation fails |
| tempIndices | int* | Temporary index arrays for sparse operations | Always allocated | NULL if allocation fails |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| workCounter | double | Tracks cumulative computational work for refactorization timing |
| etaAllocator | MemoryPool* | Pool allocator for efficient eta vector allocation |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| EtaFactors | 0:N | Exclusive - allocated from pool, freed on cleanup |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| ConstraintMatrix | 1:1 | Must outlive basis_state - needed for pivot operations |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| SolverContext | Embedded at various offsets | basis_state fields are part of SolverContext |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] Exactly numConstrs variables have status > 0 (are basic)
- [x] For each basic variable j with status k > 0, basisHeader[k] == j
- [x] For each row i, if basisHeader[i] == j, then variableStatus[j] == i+1
- [x] etaCount >= 0 and matches the length of the eta linked list
- [x] All pointers in eta list are valid or NULL (no dangling pointers)

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] The number of basic variables (status > 0) equals numConstrs exactly
- [x] Each row has at most one basic variable assigned
- [x] etaType2Count <= etaCount (Type 2 etas are a subset)
- [x] If etaCount == 0, then etaListHead == NULL

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After initialization | All arrays allocated, etaCount = 0, basis may be uninitialized |
| After crash/warm start | Valid basis: exactly m basic variables with consistent status/header |
| After refactorization | etaCount reset or reduced, new Type 1 etas for fixed variables |
| After pivot | etaCount incremented, new eta prepended to list |
| Before destruction | All eta memory returned to pool, arrays freed |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_simplex_init | Start of LP solve | Arrays allocated, etaCount = 0, status uninitialized |
| cxf_basis_warm | Warm start from saved basis | Arrays allocated, basis loaded from warm start data |

### 5.2 States

```
UNINITIALIZED
     |
     v
ALLOCATED --> CRASH_BASIS --> FACTORED --> OPTIMAL
     |             |              |            |
     |             v              v            v
     +-------> WARM_START --> ITERATING --> CLEANUP --> DESTROYED
                                 ^  |
                                 +--+
                              (pivot loop)
```

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| UNINITIALIZED | ALLOCATED | cxf_simplex_init | Allocate arrays, zero etaCount |
| ALLOCATED | CRASH_BASIS | cxf_simplex_crash | Create initial basis, set status array |
| ALLOCATED | WARM_START | cxf_basis_warm | Load basis from warm start data |
| CRASH_BASIS | ITERATING | cxf_simplex_step | Begin iteration loop |
| WARM_START | FACTORED | cxf_basis_refactor | Refactorize basis |
| ITERATING | ITERATING | cxf_pivot_with_eta | Add eta vector, increment etaCount |
| ITERATING | FACTORED | cxf_basis_refactor | Clear eta list, rebuild from scratch |
| ITERATING | OPTIMAL | Optimality detected | No basis changes |
| OPTIMAL | CLEANUP | cxf_simplex_final | Prepare for destruction |
| CLEANUP | DESTROYED | Free all memory | All arrays and etas freed |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| cxf_simplex_final | Free variableStatus, basisHeader, all work arrays, return eta pool memory |

### 5.5 Ownership Rules

- **Who creates:** cxf_simplex_init (main state), cxf_basis_warm (from saved basis)
- **Who owns:** SolverContext structure (basis_state is embedded/referenced by SolverContext)
- **Who destroys:** cxf_simplex_final
- **Sharing allowed:** No - single owner per solve

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| Get variable status | Status code for variable j | O(1) |
| Get basic variable for row | Variable index basic in row i | O(1) |
| Check if basis valid | Boolean: all invariants satisfied | O(n + m) |
| Get eta count | Number of eta vectors | O(1) |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| Mark variable basic | Set status to row index, update basisHeader | Previous basis solution |
| Mark variable non-basic | Set status to bound code (-1/-2/-3/-4) | Previous basis solution |
| Add eta vector | Prepend to eta list, increment etaCount | Cached FTRAN/BTRAN results |
| Refactorize | Clear eta list or reset count | All cached results |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| Load warm start basis | Initialize entire status and header arrays | All-or-nothing |
| Crash basis | Create initial basis from scratch | All-or-nothing |
| Validate basis | Check all invariants | Read-only, no atomicity issues |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed overhead | None (embedded in SolverContext) |
| Per-variable | 4 bytes (status) + amortized eta storage |
| Per-constraint | 4 bytes (basisHeader) |
| Eta vectors | Variable: ~48-72 bytes header + sparse data per eta |
| Total | O(n + m + k*d) where k=etaCount, d=avg eta density |

### 7.2 Allocation Strategy

Arrays (variableStatus, basisHeader, work buffers) are allocated as contiguous blocks. Eta vectors are allocated from a memory pool to avoid repeated malloc/free overhead. The pool grows as needed and is freed in bulk at cleanup.

Between refactorizations, eta count typically grows from 0 to 2-5x the number of constraints (m), then refactorization resets the list.

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| variableStatus | 4 bytes (int array) |
| basisHeader | 4 bytes (int array) |
| workBuffer | 8 bytes (double array) |
| EtaFactors | 8 bytes (contains pointers) |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Not thread-safe

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Read | None if no concurrent writes |
| Write | Exclusive access to SolverContext |
| Resize | Exclusive access (not resizable after allocation) |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- None - the simplex algorithm is inherently sequential

**Unsafe patterns:**
- Any concurrent modification of variableStatus or basisHeader
- Simultaneous FTRAN/BTRAN using shared work buffers
- Adding eta vectors from multiple threads

## 9. Serialization

### 9.1 Persistent Format

The basis can be saved/loaded via cxf_getbasis/cxf_setbasis in the public API. The persistent format contains:
- Variable status codes for each variable
- Constraint status codes for each constraint (dual information)

The eta vector list is NOT saved - only the final basis status. On load, the basis is either used as-is (warm start) or refactorized to build the eta representation.

### 9.2 Version Compatibility

| Version | Compatible | Migration |
|---------|------------|-----------|
| All versions | Yes | Basis format is stable across versions |

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Sequential status check | Yes | Status array is cache-friendly |
| Random basis lookup | Yes | O(1) access via basisHeader |
| Eta list traversal | Moderate | Linked list has poor cache locality |
| FTRAN/BTRAN | Sparse operations | Performance degrades as etaCount grows |

### 10.2 Cache Behavior

Variable status and basisHeader arrays have good spatial locality for sequential access. The eta list linked structure has poor cache behavior, but sparse data within each eta can exploit cache if well-clustered.

### 10.3 Memory Bandwidth

FTRAN/BTRAN are the primary consumers of memory bandwidth, performing sparse vector operations through the eta list. Bandwidth requirements grow linearly with eta count and density. Refactorization (every 100-1000 iterations) prevents unbounded growth.

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| Singular basis | Refactorization fails (zero pivot) | Perturb bounds, retry |
| Inconsistent status/header | Validation check fails | Rebuild basis from scratch |
| Corrupted eta list | NULL pointer or invalid type | Trigger refactorization |
| Too many etas | etaCount exceeds threshold | Automatic refactorization |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| Count basic variables | After pivot | O(n) |
| Verify status/header consistency | On demand or debug builds | O(n + m) |
| Eta list integrity | On demand | O(k) where k = etaCount |

## 12. Examples

### 12.1 Typical Instance

For a problem with 10,000 variables and 5,000 constraints:
- variableStatus: 10,000 ints (40 KB)
- basisHeader: 5,000 ints (20 KB)
- etaCount: 0 initially, grows to ~10,000-25,000 between refactorizations
- Eta storage: ~500 KB - 5 MB depending on sparsity and iteration count

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| Empty basis | No problem or m=0 | Empty arrays, etaCount=0 |
| Identity basis | All slack variables basic | Status array: first m variables basic, rest non-basic |
| Dense problem | All etas are dense | Large memory footprint, frequent refactorization needed |
| Highly sparse | Most etas have 1-5 entries | Minimal memory, long refactorization intervals |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| SolverContext | Parent structure containing basis_state fields |
| EtaFactors | Owned by basis_state, forms linked list |
| ConstraintMatrix | Referenced for pivot operations and FTRAN/BTRAN |
| PricingContext | Uses basis_state (via BTRAN) to compute reduced costs |

## 14. References

- Dantzig, G. B. (1963). *Linear Programming and Extensions*, Chapter 8: The Product Form of Inverse. Princeton University Press.
- Forrest, J. J. and Tomlin, J. A. (1972). "Updated Triangular Factors of the Basis to Maintain Sparsity in the Product Form Simplex Method." *Mathematical Programming*, 2(1):263-278.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*, Chapter 5: Basis Handling. Springer.
- Reid, J. K. (1982). "A Sparsity-Exploiting Variant of the Bartels-Golub Decomposition for Linear Programming Bases." *Mathematical Programming*, 24(1):55-69.

## 15. Validation Checklist

Before finalizing:

- [x] All fields documented with logical meaning, not byte offsets
- [x] All invariants identified (structural, consistency, temporal)
- [x] Lifecycle complete with state diagram
- [x] Thread safety analyzed (not thread-safe)
- [x] No implementation details leaked (no magic offsets)
- [x] Could implement structure from this spec (using any reasonable data structures)
- [x] Standard optimization terminology used (basic variables, eta vectors, PFI)
- [x] Relationships to other structures documented
- [x] Performance characteristics explained
- [x] References to linear programming literature included

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
