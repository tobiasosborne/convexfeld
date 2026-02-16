# Structure: EtaFactors

**Spec Version:** 1.0
**Primary Module:** Basis Operations

## 1. Purpose

### 1.1 What It Represents

The EtaFactors structure (also called EtaVector) represents a single elementary transformation matrix in the Product Form of Inverse (PFI) representation of the simplex basis inverse. Each eta matrix corresponds to one of three operations: (1) a basis refactorization that fixes variables at bounds (Type 1), (2) a pivot operation that exchanges entering and leaving variables (Type 2), or (3) other specialized transformations. The structure stores this transformation in a compact sparse format, encoding only the non-zero coefficients and their positions.

Mathematically, an eta matrix E is close to the identity matrix I, differing in only one column or row. The EtaFactors structure stores this single column/row of differences as a sparse vector, enabling efficient application of the transformation during FTRAN (forward transformation) and BTRAN (backward transformation) operations that solve linear systems involving the basis.

### 1.2 Role in System

Eta matrices form a linked list representing the cumulative history of basis changes since the last full refactorization. The basis inverse is maintained as B^-1 = E_k x E_{k-1} x ... x E_1, where each E_i is stored as an EtaFactors structure.

**Usage in algorithms:**
- **FTRAN:** Applies eta matrices in forward order (E_1, E_2, ..., E_k) to solve Bx = b
- **BTRAN:** Applies eta matrices in reverse order (E_k, ..., E_2, E_1) with transposes to solve y^T B = c^T
- **Pivot:** Creates new Type 2 eta matrix representing the basis change
- **Refactorization:** Creates new Type 1 eta matrices for variables fixed at bounds
- **Solution extraction:** Uses eta list to compute basic variable values

### 1.3 Design Rationale

The PFI approach with eta matrices is chosen for its:
1. **Sparsity preservation:** Each pivot adds one sparse eta, not a dense matrix update
2. **Memory efficiency:** Typical eta has 5-50 non-zeros, far less than m^2 for dense inverse
3. **Update speed:** Adding an eta is O(d) where d = density, vs. O(m^2) for dense update
4. **Numerical stability:** Fresh refactorization periodically resets accumulated error

The tradeoff is that FTRAN/BTRAN cost grows linearly with eta count, requiring periodic refactorization when eta count reaches 2-5x the number of constraints. This amortizes well: refactorization every 100-1000 iterations costs less than maintaining dense factors.

## 2. Logical Fields

### 2.1 Core Fields

| Field | Logical Type | Description | Valid Range |
|-------|-------------|-------------|-------------|
| type | int | Eta type: 1=refactorization, 2=pivot, others for special cases | 1, 2, or other small int |
| next | EtaFactors* | Link to next (newer) eta in chronological order | NULL or valid pointer |
| pivotVar | int | Variable index involved in this transformation | [0, numVars) |
| pivotValue | double | Value at which variable was fixed or pivoted | Any finite double |
| objCoeff | double | Objective coefficient of pivotVar (for objective updates) | Any double |
| status | int | New status of pivotVar: -1=at lower, -2=at upper, -3=superbasic, -4=fixed, >0=basic | Status code |
| nnz | int | Number of non-zeros in sparse representation | >= 0 |
| indices | Array[int] | Row or column indices of non-zero entries | length = nnz |
| values | Array[double] | Non-zero coefficient values | length = nnz |

### 2.2 Derived/Cached Fields

| Field | Derived From | Purpose | Update Trigger |
|-------|--------------|---------|----------------|
| (none) | | | |

Eta matrices are immutable once created. No derived fields are needed.

### 2.3 Optional Fields

| Field | Type | Present When | Default |
|-------|------|--------------|---------|
| leavingRow | int | Type 2 eta (pivot) | -1 or not present for Type 1 |
| modeFlag | int | Type 2 eta (pivot) | 0 for Type 1 |
| colIndices | Array[int] | Type 2 eta with column data | NULL for Type 1 or row-only Type 2 |
| colValues | Array[double] | Type 2 eta with column data | NULL for Type 1 or row-only Type 2 |
| colCount | int | Type 2 eta with column data | 0 for Type 1 or row-only Type 2 |

### 2.4 Internal/Bookkeeping Fields

| Field | Type | Purpose |
|-------|------|---------|
| allocSize | size_t | Total bytes allocated for this eta (for pool management) |
| timestamp | int | Creation order/timestamp (implicit via list position) |

## 3. Relationships

### 3.1 Owned Structures

Structures that this structure owns (creates/destroys):

| Structure | Cardinality | Ownership |
|-----------|-------------|-----------|
| (none) | | Eta is a leaf structure with inline arrays |

### 3.2 Referenced Structures

Structures that this structure references but doesn't own:

| Structure | Cardinality | Lifetime Dependency |
|-----------|-------------|---------------------|
| (none) | | Eta stores only primitive data (ints, doubles) |

### 3.3 Parent Structure

| Parent | Field in Parent | Relationship |
|--------|-----------------|--------------|
| BasisState | etaListHead | Eta is node in linked list owned by BasisState |

## 4. Invariants

### 4.1 Structural Invariants

What must ALWAYS be true about this structure:

- [x] type is a valid type code (typically 1 or 2)
- [x] nnz >= 0 and matches the actual length of indices/values arrays
- [x] pivotVar is a valid variable index
- [x] indices array contains valid row/column indices in range [0, numConstrs) or [0, numVars)
- [x] values array contains finite floating-point numbers (no NaN, no Inf)
- [x] If type=1, only row data (indices/values) is present
- [x] If type=2 and colCount>0, column arrays (colIndices/colValues) are present

### 4.2 Consistency Invariants

Relationships between fields that must hold:

- [x] nnz matches the number of entries stored in indices/values
- [x] If colCount>0, colIndices/colValues have length colCount
- [x] indices array has no duplicates (each index appears at most once)
- [x] For Type 1: indices represent rows/columns of basic variables in the old basis
- [x] For Type 2: leavingRow is a valid row index and is NOT in indices array (pivot row handled separately)

### 4.3 Temporal Invariants

What must be true at specific points:

| State | Invariant |
|-------|-----------|
| After creation | All fields initialized, linked to eta list, immutable thereafter |
| During FTRAN/BTRAN | Values are read-only, no modifications |
| Before refactorization | May have accumulated 100s-1000s of etas in list |
| After refactorization | Eta list reset or shortened, old etas discarded |
| Before destruction | Eta is unlinked from list or entire list is being freed |

## 5. Lifecycle

### 5.1 Creation

| Creation Method | When Used | Initial State |
|-----------------|-----------|---------------|
| cxf_basis_refactor | Basis refactorization, Type 1 | All fields set, type=1, linked to list |
| cxf_pivot_with_eta | Simplex pivot, Type 2 | All fields set, type=2, both row and column data |

### 5.2 States

```
UNALLOCATED
     |
     v
ALLOCATED --> INITIALIZED --> LINKED --> ACTIVE
                                            |
                                            v
                                       DISCARDED --> FREED
                                     (refactorization)
```

Eta matrices are immutable after creation, so there are no intermediate states between LINKED and DISCARDED.

### 5.3 State Transitions

| From State | To State | Trigger | Side Effects |
|------------|----------|---------|--------------|
| UNALLOCATED | ALLOCATED | Memory pool allocates block | Memory reserved |
| ALLOCATED | INITIALIZED | Fill in fields (type, pivotVar, etc.) | Data populated |
| INITIALIZED | LINKED | Prepend to eta list | next pointer set, list head updated |
| LINKED | ACTIVE | Normal use in FTRAN/BTRAN | Read-only access |
| ACTIVE | DISCARDED | Refactorization clears list | Unlinked from list |
| DISCARDED | FREED | Memory pool cleanup | Memory returned to pool |

### 5.4 Destruction

| Destruction Method | Cleanup Required |
|-------------------|------------------|
| Pool reset | Return entire pool to allocator (bulk operation) |
| Individual free | Rare - typically etas are freed in bulk |

### 5.5 Ownership Rules

- **Who creates:** cxf_basis_refactor (Type 1), cxf_pivot_with_eta (Type 2)
- **Who owns:** BasisState structure (via etaListHead and list links)
- **Who destroys:** cxf_simplex_final or cxf_basis_refactor (pool reset)
- **Sharing allowed:** No - exclusive ownership by one BasisState

## 6. Operations

### 6.1 Query Operations (Read-Only)

| Operation | Returns | Complexity |
|-----------|---------|------------|
| Get type | type field | O(1) |
| Get non-zero count | nnz | O(1) |
| Get coefficient at index i | values[i] | O(1) |
| Find index in sparse array | Linear search in indices | O(nnz) |
| Traverse to next eta | next pointer | O(1) |

### 6.2 Mutation Operations

| Operation | Effect | Invalidates |
|-----------|--------|-------------|
| (none) | Eta matrices are immutable | N/A |

### 6.3 Bulk Operations

| Operation | Purpose | Atomicity |
|-----------|---------|-----------|
| Apply to vector (FTRAN) | Multiply vector by eta transformation | Atomic (read-only access) |
| Apply transpose to vector (BTRAN) | Multiply vector by eta transpose | Atomic (read-only access) |

## 7. Memory Layout

### 7.1 Size Characteristics

| Component | Size Formula |
|-----------|-------------|
| Fixed header | ~48-72 bytes (type, pointers, pivotVar, status, etc.) |
| Row indices | nnz x 4 bytes (aligned to 8) |
| Row values | nnz x 8 bytes |
| Column indices (Type 2) | colCount x 4 bytes (aligned to 8) |
| Column values (Type 2) | colCount x 8 bytes |
| Total Type 1 | ~72 + align8(nnz*4) + nnz*8 |
| Total Type 2 | Type 1 size + align8(colCount*4) + colCount*8 |

**Typical sizes:**
- Type 1, nnz=10: ~200 bytes
- Type 2, nnz=20, colCount=15: ~400 bytes
- Dense eta, nnz=5000: ~60 KB (rare, triggers refactorization)

### 7.2 Allocation Strategy

Etas are allocated from a memory pool in a single contiguous block per eta. The header and data arrays are inline (no separate allocations), improving cache locality. The pool allocator amortizes allocation cost across many etas.

Size is computed at creation time based on nnz and colCount, then a single allocation provides space for the entire eta. This reduces fragmentation and avoids pointer chasing.

### 7.3 Alignment Requirements

| Field/Array | Alignment |
|-------------|-----------|
| Header | 8 bytes (contains pointers) |
| indices arrays | 4 bytes (int32) |
| values arrays | 8 bytes (double) |
| Overall structure | 8 bytes |

## 8. Thread Safety

### 8.1 Thread Safety Level

**Level:** Read-only safe (after creation)

### 8.2 Synchronization Requirements

| Operation Type | Required Lock |
|----------------|---------------|
| Read | None (immutable after creation) |
| Write | N/A (immutable) |
| Create | Exclusive access to BasisState (for list insertion) |

### 8.3 Concurrent Access Patterns

**Safe patterns:**
- Multiple threads reading the same eta list (for parallel FTRAN/BTRAN on different vectors)

**Unsafe patterns:**
- Creating new eta while another thread traverses the list
- Freeing eta list while another thread accesses it
- Modifying eta fields (would violate immutability)

## 9. Serialization

### 9.1 Persistent Format

Eta matrices are not directly serialized. The basis is saved as variable/constraint status codes (via cxf_getbasis), not as an eta list. On reload, the basis is refactorized from scratch, generating a new eta list.

This is appropriate because:
1. Eta list is large and complex (linked list of variable-sized structures)
2. Eta list is solver-internal (not part of the problem definition)
3. Refactorization from status is fast and guaranteed to produce a valid eta list

### 9.2 Version Compatibility

| Version | Compatible | Migration |
|---------|------------|-----------|
| N/A | N/A | Not serialized |

## 10. Performance Considerations

### 10.1 Access Patterns

| Pattern | Optimized For | Notes |
|---------|---------------|-------|
| Sequential list traversal | Moderate | Linked list has poor cache locality |
| Sparse vector application | Yes | Only touches nnz entries, exploits sparsity |
| Random access to coefficients | Moderate | Linear search in indices array |
| Bulk FTRAN/BTRAN | Good | Sparse operations scale well with sparsity |

### 10.2 Cache Behavior

Each eta is allocated as a contiguous block (header + inline arrays), providing good spatial locality within a single eta. However, the linked list structure means successive etas may be far apart in memory, causing cache misses during list traversal.

For very sparse etas (nnz < 10), the entire eta fits in a cache line (~64 bytes), giving excellent cache behavior. For denser etas (nnz > 100), cache misses occur during the sparse vector operations.

### 10.3 Memory Bandwidth

FTRAN/BTRAN are bandwidth-sensitive when eta count is high (500+). Each eta application reads nnz indices + nnz values + writes to output vector. For eta count k and average density d, total bandwidth is O(k x d x 16 bytes). This grows linearly until refactorization resets k to 0.

## 11. Error Conditions

### 11.1 Invalid States

| Invalid State | How Detected | Recovery |
|---------------|--------------|----------|
| NULL next pointer in mid-list | List traversal hits NULL early | Should not occur; indicates corruption |
| Negative nnz | nnz < 0 | Should not occur; indicates corruption |
| Invalid index | indices[i] out of range | Should not occur; validate at creation |
| NaN or Inf in values | Floating-point check | Numerical instability; trigger refactorization |

### 11.2 Validation

| Check | When Performed | Cost |
|-------|----------------|------|
| nnz >= 0 | At creation | O(1) |
| Indices in range | At creation (debug builds) | O(nnz) |
| No duplicate indices | At creation (debug builds) | O(nnz^2) or O(nnz log nnz) if sorted |
| Finite values | At creation | O(nnz) |

## 12. Examples

### 12.1 Typical Instance

**Type 1 eta (refactorization):**
- type = 1
- pivotVar = 237 (variable being fixed)
- pivotValue = 10.0 (value at bound)
- objCoeff = 2.5 (objective coefficient)
- status = -1 (at lower bound)
- nnz = 8 (sparse constraint row)
- indices = [0, 5, 12, 23, 45, 67, 89, 102]
- values = [1.5, -2.3, 0.7, 4.2, -1.1, 3.8, 0.5, -2.9]
- Size: ~72 + 32 + 64 = 168 bytes

**Type 2 eta (pivot):**
- type = 2
- pivotVar = 512 (entering variable)
- leavingRow = 25
- pivotValue = 5.0
- objCoeff = -1.2
- status = 26 (basic in row 25, stored as 25+1)
- nnz = 12 (leaving row coefficients)
- colCount = 15 (entering column coefficients)
- Size: ~72 + 48 + 96 + 64 + 120 = 400 bytes

### 12.2 Edge Cases

| Case | Description | Representation |
|------|-------------|----------------|
| Empty eta | nnz = 0 | Rare; only for fixed variables with no non-zeros in their row |
| Dense eta | nnz = numConstrs | Large size; triggers early refactorization |
| Singleton | nnz = 1 | Very common in network problems; minimal memory |
| First eta in list | next = NULL | List tail (oldest eta) |
| Last eta in list | next = (previous head) | List head (newest eta) |

## 13. Related Structures

| Structure | Relationship |
|-----------|--------------|
| BasisState | Owner of eta list; etaListHead points to newest eta |
| SolverContext | Indirect parent; contains BasisState which owns etas |
| ConstraintMatrix | Source of coefficients for creating etas |

## 14. References

- Dantzig, G. B. (1963). *Linear Programming and Extensions*, Chapter 8: The Product Form of Inverse. Princeton University Press.
- Forrest, J. J. and Tomlin, J. A. (1972). "Updated Triangular Factors of the Basis to Maintain Sparsity in the Product Form Simplex Method." *Mathematical Programming*, 2(1):263-278.
- Bartels, R. H. and Golub, G. H. (1969). "The Simplex Method of Linear Programming Using LU Decomposition." *Communications of the ACM*, 12(5):266-268.
- Suhl, U. H. and Suhl, L. M. (1990). "Computing Sparse LU Factorizations for Large-Scale Linear Programming Bases." *ORSA Journal on Computing*, 2(4):325-335.
- Reid, J. K. (1982). "A Sparsity-Exploiting Variant of the Bartels-Golub Decomposition for Linear Programming Bases." *Mathematical Programming*, 24(1):55-69.

## 15. Validation Checklist

Before finalizing:

- [x] All fields documented with logical meaning, not byte offsets
- [x] All invariants identified (structural, consistency, temporal)
- [x] Lifecycle complete with state diagram
- [x] Thread safety analyzed (read-only safe after creation)
- [x] No implementation details leaked (no magic offsets)
- [x] Could implement structure from this spec (using standard sparse vector representation)
- [x] Standard optimization terminology used (eta matrix, PFI, FTRAN/BTRAN)
- [x] Relationships to other structures documented
- [x] Performance characteristics explained
- [x] References to linear programming literature included

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
