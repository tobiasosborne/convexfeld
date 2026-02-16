# BasisState

## Purpose

BasisState represents the factored form of the current basis matrix during simplex iterations. In the revised simplex method, the basis matrix B is the square submatrix of the constraint matrix A formed by the columns corresponding to basic variables. Rather than storing B or its inverse explicitly, BasisState maintains a factored representation that supports efficient forward transformation (FTRAN: solving Bd = a for a given column a) and backward transformation (BTRAN: solving B^T pi = c_B for the simplex multipliers). The representation uses the Product Form of the Inverse (PFI), where basis updates from successive pivots are recorded as a chain of eta vectors (elementary matrices), and the factored basis is periodically recomputed via LU factorization to control numerical drift and maintain computational efficiency. BasisState exists for the duration of a single LP solve and is referenced by the SolverState.

## Fields

### Core Dimensions

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| numRows | int | Number of rows in the basis matrix (equals the number of constraints) | > 0 | Set at initialization; immutable during solve; equals the number of basic variables |
| numCols | int | Number of columns in the original problem (used for sizing) | > 0 | Set at initialization; immutable during solve |

### LU Factorization Storage

The LU factorization stores the basis matrix B = L * U in sparse triangular form after a full refactorization. Between refactorizations, the PFI update chain (eta vectors) represents subsequent basis changes.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| lowerTriangular | pointer-to-SparseTriangularMatrix | Lower triangular factor L from the LU decomposition of the basis matrix | Non-null after refactorization | L has unit diagonal; stored in sparse column format |
| upperTriangular | pointer-to-SparseTriangularMatrix | Upper triangular factor U from the LU decomposition of the basis matrix | Non-null after refactorization | No zero diagonal entries (pivot elements are non-zero) |
| pivotOrder | array-of-int [numRows] | Row permutation applied during LU factorization for numerical stability | Permutation of 0..numRows-1 | Valid permutation; no duplicates |
| columnOrder | array-of-int [numRows] | Column permutation applied during LU factorization for sparsity preservation | Permutation of 0..numRows-1 | Valid permutation; no duplicates |

### Eta Vector Chain (Product Form of Inverse)

> **Ownership note:** BasisState is the authoritative owner of the eta vector chain. SolverState (P1.04) maintains convenience aliases of `etaListHead`, `etaTotalCount`, and `etaRowCount` for fast access from simplex iteration code, but the fields defined here are the canonical source of truth.

Each simplex pivot appends one eta vector to this chain. The chain, combined with the LU factors, represents the current basis inverse.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| etaListHead | pointer-to-EtaVector | Head of the singly-linked list of eta vectors recording basis updates since the last refactorization | Null when no updates have been recorded (immediately after refactorization) | New eta vectors are prepended; list is null-terminated |
| etaTotalCount | int | Total number of eta vectors in the current chain | >= 0 | Reset to zero after refactorization; incremented by one per pivot |
| etaRowCount | int | Number of eta vectors that record row-type (pivot) operations, as distinguished from variable-fixing operations | >= 0; <= etaTotalCount | Incremented when a pivot eta is appended |

### Memory Pool

Eta vectors are allocated from a pool allocator to avoid per-allocation overhead from the system allocator. The pool uses sequential (bump) allocation within chunks, with no individual deallocation -- the entire pool is released at once during refactorization or solve cleanup.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| memoryPool | pointer-to-MemoryPool | Bump allocator for eta vector storage | Non-null during solve | All eta vectors in the chain are allocated from this pool |
| currentChunkOffset | int | Byte offset within the current chunk for the next allocation | >= 0 | Less than or equal to the capacity of the current chunk |
| currentChunkCapacity | int | Total byte capacity of the current memory chunk | > 0 | Grows exponentially up to a cap with each new chunk |
| nextChunkMinSize | int | Minimum size for the next chunk allocation, doubling up to a platform-appropriate cap | > 0 | Monotonically non-decreasing until the cap is reached |

### Refactorization Control

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| refactorizationThreshold | int | Maximum number of eta vectors before a full LU refactorization is triggered | > 0, typically proportional to numRows | Set at initialization based on problem size |
| fillInEstimate | int | Estimated fill-in from the most recent LU factorization, used to size the next factorization buffer | >= 0 | Updated after each refactorization |
| numericalStabilityFlag | bool | Indicates whether the most recent FTRAN/BTRAN showed signs of numerical instability, which can trigger early refactorization | true or false | Set by FTRAN/BTRAN accuracy checks; cleared after refactorization |

### Basis Tracking Arrays

These arrays track which variables are currently in the basis and their status.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| basisHeader | array-of-int [numRows] | Maps each basis position (row index) to the index of the variable currently basic in that row | 0 to numCols-1 | Contains exactly numRows distinct variable indices |
| variableStatus | array-of-int [numCols] | Status of each variable: basic (with row index), or non-basic at lower bound, upper bound, superbasic, or fixed | BASIC (non-negative row index), AT_LOWER, AT_UPPER, SUPERBASIC, FIXED | Exactly numRows entries are BASIC; for basic variable j, variableStatus[j] gives the row where j is basic |

### Progress Tracking Snapshot

The solver periodically captures a lightweight snapshot of iteration counters to detect cycling (degenerate pivots that do not improve the objective). The snapshot is a small fixed-size buffer of integer counters.

| Field | Type | Purpose | Valid Values | Invariants |
|-------|------|---------|--------------|------------|
| progressSnapshot | array-of-int [SNAPSHOT_SIZE] | Buffer holding a snapshot of iteration counters for computing a progress metric | Any integer values | Captured at periodic intervals by the snapshot function |
| snapshotSize | int | Number of counters in the progress snapshot | Fixed constant, determined at compile time | Immutable |

## Variable Status Codes

The variable status encoding follows the standard revised simplex convention as described in Maros, *Computational Techniques of the Simplex Method* (Springer, 2003, Chapter 3).

| Code | Name | Meaning |
|------|------|---------|
| >= 0 | BASIC | Variable is in the basis; the non-negative value indicates the constraint row in which this variable is basic |
| AT_LOWER | AT_LOWER | Non-basic variable held at its lower bound |
| AT_UPPER | AT_UPPER | Non-basic variable held at its upper bound |
| SUPERBASIC | SUPERBASIC | Non-basic variable between its bounds (occurs with free variables or in degenerate situations) |
| FIXED | FIXED | Non-basic variable whose lower bound equals its upper bound |

The encoding uses non-negative integers for basic variables (doubling as the basis row index) and distinct negative sentinel values for each non-basic status. This compact encoding eliminates the need for a separate "is basic" flag and a separate row-lookup structure.

## Relationships

- **Owned by** SolverState. BasisState is created as part of simplex initialization and destroyed during simplex cleanup.
- **Owns** the eta vector linked list. All eta vectors in the chain are allocated from the memory pool and are logically owned by BasisState.
- **Owns** the memory pool. The pool is created at BasisState initialization and destroyed when BasisState is freed. There is no individual deallocation of eta vectors; the entire pool is released at once.
- **Owns** the LU factors (L, U, permutation arrays). These are computed during refactorization and freed when BasisState is destroyed or when a new refactorization replaces them.
- **Owns** the basis tracking arrays (basisHeader, variableStatus). These are allocated at initialization and freed at cleanup.
- **References** the constraint matrix (via SolverState). The constraint matrix is read during FTRAN, BTRAN, and refactorization but is not owned by BasisState.
- **Referenced by** the pricing subsystem, which reads variableStatus to determine which variables are eligible for pricing, and by the pivot operations, which update both the eta chain and the basis tracking arrays.

## Lifecycle

### Creation

1. BasisState is allocated and zero-initialized during simplex initialization, after problem dimensions are known.
2. The basis tracking arrays (basisHeader, variableStatus) are allocated with sizes determined by numRows and numCols.
3. The memory pool is initialized with a modest initial chunk size (a few kilobytes).
4. The LU factor storage is initially empty; a full LU factorization of the initial basis is computed before the first simplex iteration.
5. The refactorization threshold is set based on problem characteristics (typically a fraction of numRows, or a fixed maximum such as 100-200 updates).
6. The progress snapshot buffer is allocated and zeroed.
7. If a warm-start basis is available from a previous solve, the variableStatus and basisHeader arrays are populated from it rather than from a crash procedure.

### Mutation

- **Each pivot operation**: One new eta vector is allocated from the memory pool and prepended to the eta chain. The etaTotalCount and etaRowCount are incremented. The basisHeader and variableStatus arrays are updated to reflect the entering and leaving variables.
- **Variable fixing**: When a variable is fixed at a bound (during bound tightening or crossover), a variable-fixing eta vector may be recorded, and the variableStatus is updated to reflect the non-basic status.
- **Warm-start recording**: When a variable with quadratic objective contributions is fixed, a warm-start eta vector records the quadratic terms for correct reduced-cost maintenance.
- **Refactorization**: The entire eta chain is discarded (the memory pool is reset), the LU factors are recomputed from the current basis matrix, and etaTotalCount and etaRowCount are reset to zero. This occurs when etaTotalCount exceeds the refactorization threshold or when numerical instability is detected.
- **Progress snapshot capture**: The snapshot function copies a fixed set of iteration counters into the progressSnapshot buffer. This is an O(1) operation with no memory allocation.
- **Progress diff computation**: The diff function computes a weighted, normalized score comparing the current counters against a saved snapshot. A low score triggers anti-cycling measures (perturbation).

### Destruction

1. All eta vectors are implicitly freed by releasing the memory pool (no per-vector deallocation needed).
2. The LU factor storage (L, U, permutation arrays) is freed.
3. The basis tracking arrays (basisHeader, variableStatus) are freed.
4. The progress snapshot buffer is freed.
5. The BasisState structure itself is freed.
6. The owning SolverState's reference to BasisState is set to null.

Destruction must release the memory pool before freeing the BasisState structure, since the eta vectors reside within pool-allocated memory.

## Invariants

1. **Basis size**: The basisHeader array has exactly numRows entries, and exactly numRows entries in variableStatus have the BASIC status (non-negative value). These counts are always equal.

2. **Basis consistency**: For every row i in 0..numRows-1, variableStatus[basisHeader[i]] == i. That is, the variable recorded as basic in row i reports row i as its basis position.

3. **Eta chain length**: The eta chain starting from etaListHead contains exactly etaTotalCount entries and is null-terminated. etaRowCount <= etaTotalCount.

4. **Eta chain bound**: etaTotalCount <= refactorizationThreshold at the start of each iteration. If etaTotalCount reaches the threshold, refactorization occurs before the next iteration.

5. **Pool containment**: Every eta vector reachable from etaListHead resides within a chunk of the memory pool. No eta vector exists outside the pool.

6. **LU validity**: Immediately after refactorization, L * U (with row and column permutations applied) equals the basis matrix B to within numerical tolerance. Between refactorizations, the effective basis inverse is the product of the LU-factor inverse and all eta vector inverses in the chain.

7. **Permutation validity**: pivotOrder and columnOrder are each valid permutations of 0..numRows-1 with no duplicates.

8. **Status exclusivity**: Each variable has exactly one status. A variable cannot simultaneously be basic and non-basic.

9. **Dimension immutability**: numRows and numCols are set at initialization and never modified. All array sizes depend on these dimensions.

## Thread Safety

BasisState is **not thread-safe**. It is designed as a single-threaded working structure for one simplex solve invocation.

- All fields are read and written without synchronization.
- Each concurrent solve must have its own independent BasisState.
- The memory pool is not shared and must not be accessed from multiple threads.
- The parent SolverState, which owns BasisState, is also single-threaded. Thread safety for concurrent solves is achieved at the model level by creating independent solver instances.

## Design Rationale

**Product Form of the Inverse (PFI)**: The basis inverse is maintained as a sequence of elementary matrices (eta vectors) rather than an explicit dense inverse matrix. Each simplex pivot changes one column of B, and the corresponding change to B^-1 can be represented by a single eta vector -- the pivot column scaled by the reciprocal of the pivot element. FTRAN and BTRAN then apply the sequence of eta transformations in order. This is the classical approach introduced by Dantzig and Orchard-Hays (1954) and remains the foundation of production simplex implementations. The key advantage is that each pivot costs O(nnz) where nnz is the number of nonzeros in the pivot column, compared to O(m^2) for explicit inverse maintenance.

**Periodic LU refactorization**: As eta vectors accumulate, FTRAN and BTRAN become progressively more expensive (each requires applying all accumulated eta transformations) and numerical errors compound due to floating-point arithmetic. Periodically recomputing a fresh LU factorization of the current basis matrix restores both computational efficiency and numerical accuracy. The refactorization threshold balances the cost of factorization (typically O(m * nnz(L+U))) against the increasing per-iteration cost of a long eta chain. Bartels and Golub (1969) established the theoretical framework for LU updates in the simplex method, and Forrest and Tomlin (1972) developed practical sparse update techniques that remain in wide use.

**Bump allocator for eta storage**: Eta vectors have a create-once, read-many, free-all-at-once lifecycle: they are created during pivots, read during FTRAN/BTRAN, and all discarded together at refactorization. A bump (arena/region) allocator is ideal for this pattern because allocation is O(1) (just a pointer increment), there is zero per-allocation bookkeeping overhead, and bulk deallocation is O(1) (just reset the offset). The exponential chunk growth strategy (doubling up to a cap) keeps the number of system allocations logarithmic in the total eta storage consumed. This pattern is described in the region-based memory management literature (Tofte and Talpin, 1997).

**Variable status encoding**: Using the basis row index as the status code for basic variables is a standard space-efficient technique (Maros, 2003, Section 3.2). Non-negative values double as the row index, while distinct negative values encode the non-basic bound status. This eliminates the need for separate "is basic" and "basis row" arrays.

**Progress snapshot for cycling detection**: Degenerate linear programs can cause the simplex algorithm to cycle through the same sequence of bases without improving the objective. The snapshot mechanism captures a lightweight summary of solver progress (iteration counts, structural changes) and the diff function computes a normalized score. When the score is small relative to the number of iterations elapsed, the solver applies perturbation to break the degeneracy. This is a practical implementation of the monitoring approach described by Maros (2003, Section 9.7), which detects stalling as a proxy for cycling.

**Separation from SolverState**: Although the implementation may co-locate some basis-related fields within the solver's main state structure for cache efficiency, BasisState is conceptually separate because it has a distinct lifecycle (it can be reset by refactorization independently of the rest of the solver state) and encapsulates a coherent algorithmic concern (basis representation and maintenance). This separation supports clean module boundaries in a reimplementation.

## References

- Dantzig, G.B. (1963). *Linear Programming and Extensions*. Princeton University Press.
- Dantzig, G.B. and Orchard-Hays, W. (1954). "The Product Form for the Inverse in the Simplex Method." *Mathematical Tables and Other Aids to Computation*, 8(46):64-67.
- Bartels, R.H. and Golub, G.H. (1969). "The Simplex Method of Linear Programming Using LU Decomposition." *Communications of the ACM*, 12(5):266-268.
- Forrest, J.J.H. and Tomlin, J.A. (1972). "Updated Triangular Factors of the Basis to Maintain Sparsity in the Product Form Simplex Method." *Mathematical Programming*, 2(1):263-278.
- Suhl, U.H. and Suhl, L.M. (1990). "Computing Sparse LU Factorizations for Large-Scale Linear Programming Bases." *ORSA Journal on Computing*, 2(4):325-335.
- Reid, J.K. (1982). "A Sparsity-Exploiting Variant of the Bartels-Golub Decomposition for Linear Programming Bases." *Mathematical Programming*, 24(1):55-69.
- Maros, I. (2003). *Computational Techniques of the Simplex Method*. Springer.
- Tofte, M. and Talpin, J.-P. (1997). "Region-Based Memory Management." *Information and Computation*, 132(2):109-176.

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
