# EtaVector

## Purpose

An EtaVector represents a single elementary matrix in the Product Form of the Inverse (PFI) representation of a simplex basis. In the revised simplex method, the basis inverse B^{-1} is not stored explicitly. Instead, after each pivot operation, the solver records the elementary transformation that updated the basis, expressed as an "eta vector." The full basis inverse is then reconstructed on demand as B^{-1} = E_k^{-1} * E_{k-1}^{-1} * ... * E_1^{-1} * B_0^{-1}, where each E_i is an elementary matrix that differs from the identity matrix in exactly one column. EtaVector instances are linked together in a singly-linked list, with the most recent pivot at the head. The entire list is discarded and rebuilt when a basis refactorization occurs. This approach, introduced by Dantzig and Orchard-Hays (1954) and refined by subsequent authors, avoids the O(m^2) cost of explicit basis inversion after each pivot, replacing it with O(nnz) per eta vector application during FTRAN and BTRAN operations.

## Eta Vector Variants

The solver uses three distinct eta vector variants, distinguished by a type tag. All variants share a common prefix (type tag, self-referencing pointer, and linked-list pointer), but differ in the remaining fields and the variable-length data they carry.

### Variant 1: Pivot Transformation (type = PIVOT)

Records a standard simplex pivot operation where one variable enters the basis and another leaves.

#### Fixed Fields

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| type | int | Variant tag identifying this as a pivot transformation | PIVOT constant | Set once at creation; never changes |
| selfPtr | pointer-to-int | Self-referencing pointer to the inline pivot metadata area | Points to pivotRow within the same allocation | Always non-null after creation |
| next | pointer-to-EtaVector | Link to the next (older) eta vector in the chain | Null if this is the oldest eta vector | Immutable after insertion into the list |
| pivotRow | int | Row index in the basis where the leaving variable resided | 0 to numConstrs-1 | Must be a valid constraint row index |
| enteringVar | int | Column index of the variable that entered the basis | 0 to numVars-1 | Must be a valid variable index |
| leavingVar | int | Column index of the variable that left the basis | 0 to numVars-1 | Must be a valid variable index |
| pivotElement | double | The pivot coefficient A[pivotRow, enteringVar] used to scale the eta column | Nonzero finite double | Must not be zero (a zero pivot is numerically degenerate and must be avoided) |
| reducedCost | double | The reduced cost of the entering variable at the time of the pivot | Any finite double | Snapshot value; not updated after creation |
| rowNonzeroCount | int | Number of nonzero entries stored in the row-wise sparse data | >= 0 | Determines the size of the row index and row value arrays |
| rowIndices | pointer-to-array-of-int | Column indices of the nonzero entries in the eta row | Each entry in 0 to numVars-1 | Parallel with rowValues; length equals rowNonzeroCount |
| rowValues | pointer-to-array-of-double | Scaled coefficients of the eta row: -A[pivotRow, j] / pivotElement for each nonzero column j | Any finite double | Parallel with rowIndices; length equals rowNonzeroCount |
| direction | int | Encodes whether the entering variable moved from its lower or upper bound | Non-negative for lower bound, negative for upper bound | Set once at creation |

#### Optional Column Data Fields

When dual simplex mode requires column-wise access to the pivot transformation, the eta vector also stores column data.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| colIndices | pointer-to-array-of-int | Row indices of the nonzero entries in the eta column | Each entry in 0 to numConstrs-1 | Present only when column data is requested; parallel with colValues |
| colValues | pointer-to-array-of-double | Raw coefficients from the entering variable's column in the constraint matrix | Any finite double | Present only when column data is requested; parallel with colIndices |

### Variant 2: Variable Fixing (type = VARIABLE_FIX)

Records the fixing of a variable at one of its bounds. Created during bound propagation, presolve tightening, or when the simplex algorithm determines a variable is at its optimal bound. This variant exists in two sub-forms depending on whether full column data is needed for later basis reconstruction.

#### Compact Sub-Form

Used when the solver is in simplified eta tracking mode (no column data needed for reconstruction).

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| type | int | Variant tag identifying this as a variable fixing record | VARIABLE_FIX constant | Set once at creation; never changes |
| selfPtr | pointer-to-int | Self-referencing pointer to the inline metadata area | Points to variableIndex within the same allocation | Always non-null after creation |
| next | pointer-to-EtaVector | Link to the next (older) eta vector in the chain | Null if this is the oldest eta vector | Immutable after insertion into the list |
| variableIndex | int | Column index of the variable being fixed | 0 to numVars-1 | Must be a valid variable index |
| fixedValue | double | The value at which the variable is fixed (typically its lower or upper bound) | Any finite double | Must lie within the variable's original bounds |

#### Full Sub-Form

Used when the solver needs to record the column data for later basis reconstruction (e.g., during crossover or warm-start restoration).

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| type | int | Variant tag identifying this as a variable fixing record | VARIABLE_FIX constant | Set once at creation; never changes |
| selfPtr | pointer-to-int | Self-referencing pointer to the inline metadata area | Points to variableIndex within the same allocation | Always non-null after creation |
| next | pointer-to-EtaVector | Link to the next (older) eta vector in the chain | Null if this is the oldest eta vector | Immutable after insertion into the list |
| variableIndex | int | Column index of the variable being fixed | 0 to numVars-1 | Must be a valid variable index |
| fixedValue | double | The value at which the variable is fixed | Any finite double | Must lie within the variable's original bounds |
| previousReducedCost | double | The reduced cost of the variable before it was fixed | Any finite double | Preserved for undo operations or crossover |
| boundStatus | int | The bound at which the variable was fixed | AT_LOWER, AT_UPPER, FIXED, or SUPERBASIC | Determined by comparing fixedValue to the variable's bounds |
| nonzeroCount | int | Number of nonzero entries in the variable's column among active constraints | >= 0 | Determines the size of the column index and value arrays |
| columnRowIndices | pointer-to-array-of-int | Row indices of the nonzero entries in this variable's column, filtered to include only active constraints | Each entry in 0 to numConstrs-1 | Parallel with columnCoefficients; length equals nonzeroCount |
| columnCoefficients | pointer-to-array-of-double | Coefficient values from the constraint matrix column for this variable | Any finite double | Parallel with columnRowIndices; length equals nonzeroCount |

### Variant 3: Quadratic Warm-Start (type = WARM_START)

Records quadratic objective term contributions for a variable during warm-start scenarios. Created only when the problem has a quadratic objective and a variable with nonzero Q-matrix entries is being processed during basis initialization or variable fixing.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| type | int | Variant tag identifying this as a quadratic warm-start record | WARM_START constant | Set once at creation; never changes |
| selfPtr | pointer-to-int | Self-referencing pointer to the inline metadata area | Points to variableIndex within the same allocation | Always non-null after creation |
| next | pointer-to-EtaVector | Link to the next (older) eta vector in the chain | Null if this is the oldest eta vector | Immutable after insertion into the list |
| variableIndex | int | Column index of the variable whose Q-matrix contributions are being recorded | 0 to numVars-1 | Must be a valid variable index |
| boundStatus | int | The bound status of the variable at the time of recording | AT_LOWER, AT_UPPER, or SUPERBASIC | Determined by comparing the variable's value to its bounds |
| entryCount | int | Number of Q-matrix entries stored (diagonal plus off-diagonal) | >= 1 | At least one entry must exist (otherwise no eta vector is created) |
| qIndices | pointer-to-array-of-int | Column indices of the Q-matrix entries associated with this variable | Each entry in 0 to numVars-1 | Parallel with qValues; length equals entryCount |
| qValues | pointer-to-array-of-double | Coefficient values from the Q-matrix row/column for this variable | Any finite double, nonzero | Parallel with qIndices; length equals entryCount |

## Sparse Storage Format

All eta vector variants store their variable-length data (indices and values) using a contiguous inline layout. The data is allocated as a single block from the memory pool, with the fixed header fields at the beginning and the sparse arrays immediately following.

The sparse data uses a parallel-array representation, standard in sparse matrix computations (Saad, 2003):

1. **Index array**: An array of integers identifying which rows or columns have nonzero entries in the eta vector.
2. **Value array**: An array of doubles, stored in parallel with the index array, holding the corresponding coefficient values.

The index array is padded to the nearest 8-byte boundary to ensure the subsequent value array begins at a properly aligned address for efficient double-precision access.

## Relationships

- **Owned by** SolverState: The SolverState holds the head pointer of the eta vector linked list. All eta vectors in the list belong to the current solve.
- **Allocated from** the memory pool (arena allocator) owned by the SolverState. Eta vectors are never individually freed; the entire memory pool is released at once during cleanup or refactorization.
- **References** problem data from SolverState: The row and column indices stored within an eta vector refer to the constraint matrix dimensions of the enclosing problem. These indices are meaningful only within the context of the current solve.
- **Consumed by** FTRAN and BTRAN operations: When solving a system involving the basis inverse (B^{-1} * b for FTRAN, or c_B^T * B^{-1} for BTRAN), the solver traverses the eta vector chain from head to tail (for FTRAN) or tail to head (for BTRAN), applying each elementary transformation in sequence.
- **Created by** pivot operations, variable-fixing operations, and quadratic warm-start operations. Each operation appends exactly one eta vector to the head of the list.

## Lifecycle

### Creation

1. The creating function determines how much memory is needed: a fixed header size plus variable-length storage for the sparse index and value arrays.
2. Memory is obtained from the SolverState's arena allocator in O(1) amortized time (bump allocation within pre-allocated chunks).
3. The type tag is set to identify the variant.
4. The self-referencing pointer and internal array pointers are initialized to point to the correct inline data offsets within the allocation.
5. The new eta vector's next pointer is set to the current list head.
6. The SolverState's list head pointer is updated to point to the new eta vector (prepend operation).
7. The SolverState's eta count is incremented.
8. For pivot eta vectors: the pivot column data is extracted from the constraint matrix. Each nonzero coefficient in the pivot row is negated and divided by the pivot element, yielding the eta column entries. If column data is also needed, the raw column coefficients are copied without scaling.
9. For variable-fixing eta vectors: the affected column's nonzero entries among active constraints are copied from the CSC matrix representation.
10. For warm-start eta vectors: the diagonal and off-diagonal Q-matrix entries for the variable are copied.

### Mutation

Eta vectors are **write-once** structures. After creation and insertion into the linked list, their contents are never modified. This immutability simplifies concurrency reasoning and ensures that FTRAN/BTRAN operations see a consistent snapshot.

### Destruction

Eta vectors are never individually deallocated. Destruction occurs in two circumstances:

1. **Basis refactorization**: When the accumulated eta vector count exceeds a threshold or numerical accuracy has degraded, the solver performs a full LU factorization of the current basis. This resets the eta vector list head to null and the eta count to zero. The memory pool chunks containing the old eta vectors are reclaimed in bulk.

2. **Simplex cleanup**: At the end of the LP solve, the SolverState's memory pool is freed entirely, releasing all eta vector allocations in O(n) time where n is the number of pool chunks (not the number of eta vectors).

## Invariants

1. **Type tag validity**: The type field must be one of the three defined variant constants (PIVOT, VARIABLE_FIX, or WARM_START). No other values are permitted.

2. **List integrity**: Starting from the SolverState's eta list head and following next pointers, the chain contains exactly as many nodes as the SolverState's eta total count, and terminates with a null pointer.

3. **Nonzero pivot element**: For pivot-type eta vectors, the pivotElement field is never zero. A zero pivot would make the elementary matrix singular, which is algebraically invalid.

4. **Index range validity**: All row indices stored in an eta vector's sparse arrays are in the range [0, numConstrs). All column indices are in the range [0, numVars). These ranges are determined by the problem dimensions at the time the eta vector was created.

5. **Parallel array consistency**: For every eta vector, the index and value arrays have the same logical length (rowNonzeroCount, nonzeroCount, or entryCount depending on variant).

6. **Chronological ordering**: The linked list is ordered from most recent (head) to oldest (tail). This ordering is significant for FTRAN, which applies transformations from oldest to newest, and BTRAN, which applies them from newest to oldest.

7. **Immutability after insertion**: Once an eta vector has been linked into the list, none of its fields are modified. The list is append-only at the head.

8. **Eta coefficient scaling**: For pivot-type eta vectors, each entry in rowValues equals -A[pivotRow, j] / pivotElement. This ensures that applying the eta transformation is equivalent to multiplying by the elementary matrix E_i^{-1}.

## Thread Safety

EtaVector instances are **not thread-safe**. They exist within a single SolverState, which is owned by a single thread during a solve.

- The linked list is manipulated only by the thread performing the simplex iterations.
- No synchronization primitives are required for eta vector access.
- If concurrent solves occur, each thread must have its own SolverState with its own independent eta vector chain.
- The memory pool from which eta vectors are allocated must also be thread-local or otherwise protected.

## Design Rationale

### Product Form of the Inverse

The PFI approach to maintaining the basis inverse was introduced by Dantzig and Orchard-Hays (1954) as a practical alternative to explicit basis inversion. Rather than computing and storing B^{-1} as a dense m x m matrix (requiring O(m^2) storage and O(m^2) work per update), PFI records each pivot as a single elementary matrix. The basis inverse at any point is:

    B_k^{-1} = E_k^{-1} * E_{k-1}^{-1} * ... * E_1^{-1} * B_0^{-1}

where B_0^{-1} is obtained from an initial LU factorization. Each E_i has the form I + (eta_i - e_p) * e_p^T, where eta_i is the eta vector and e_p is the p-th unit vector. Applying E_i^{-1} to a vector requires only O(nnz_i) operations, where nnz_i is the number of nonzeros in the i-th eta vector.

### Variant Types

Three distinct eta vector types are used because the solver performs three fundamentally different basis modifications:

- **Pivot transformations** record the standard simplex pivot, which exchanges one column of the basis matrix. These contain the scaled pivot column data needed for FTRAN/BTRAN.
- **Variable fixing** records a variable being removed from consideration (fixed at a bound). These may store the variable's column data for later basis reconstruction during crossover or re-optimization.
- **Quadratic warm-start** records Q-matrix contributions when fixing a variable with a quadratic objective. These are needed to properly update reduced costs in QP problems.

### Singly-Linked List

A singly-linked list is chosen over alternatives (doubly-linked list, array) because:
- Insertion is O(1) at the head.
- FTRAN and BTRAN traverse the list in opposite directions, but since BTRAN naturally processes from head (newest) to tail (oldest), and FTRAN requires the reverse order, practical implementations typically use the LU factorization for the initial solve and then apply only the eta vectors accumulated since the last refactorization.
- No random access is needed; the list is always traversed sequentially.
- No individual deletion is needed; the entire list is discarded during refactorization.

### Arena (Pool) Allocation

Eta vectors are allocated from a bump-allocating memory pool rather than the general-purpose heap. This provides:
- O(1) allocation cost (pointer increment within a chunk).
- Zero per-object overhead (no free-list metadata).
- No memory fragmentation within chunks.
- Bulk deallocation: when the eta list is cleared during refactorization, the entire pool is reset or freed, avoiding the cost of individual deallocation calls.

The pool uses an exponentially-growing chunk strategy, starting with a small initial chunk and doubling the chunk size for each new allocation, up to a maximum chunk size. This amortizes the cost of system-level memory allocation over many eta vector creations.

### Periodic Refactorization

As eta vectors accumulate, the cost of FTRAN and BTRAN grows linearly with the number of eta vectors, and numerical errors accumulate due to repeated floating-point arithmetic. When the eta count exceeds a heuristic threshold (typically proportional to the basis dimension), or when numerical accuracy degrades below tolerance, the solver performs a full LU factorization of the current basis matrix. This resets the eta vector list and restores both performance and numerical accuracy. The refactorization frequency represents a trade-off: refactorizing too often wastes time on the LU factorization, while refactorizing too rarely degrades per-iteration FTRAN/BTRAN performance and numerical stability. Typical thresholds are in the range of 50 to 200 pivots between refactorizations, depending on problem size and sparsity (Maros, 2003, Chapter 9).

## References

- Dantzig, G.B. and Orchard-Hays, W. (1954). "The Product Form for the Inverse in the Simplex Method." *Mathematical Tables and Other Aids to Computation*, 8(46):64-67.
- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Bartels, R.H. and Golub, G.H. (1969). "The Simplex Method of Linear Programming Using LU Decomposition." *Communications of the ACM*, 12(5):266-268.
- Forrest, J.J.H. and Tomlin, J.A. (1972). "Updated Triangular Factors of the Basis to Maintain Sparsity in the Product Form Simplex Method." *Mathematical Programming*, 2(1):263-278.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer. Chapters 5 and 9.
- Saad, Y. (2003). *Iterative Methods for Sparse Linear Systems*. 2nd ed. SIAM. Section 3.4.

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
