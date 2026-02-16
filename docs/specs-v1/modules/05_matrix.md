# Module: Matrix Operations

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 7

## 1. Overview

### 1.1 Purpose

The Matrix Operations module provides sparse matrix storage, manipulation, and computational primitives for the Convexfeld LP solver. This module abstracts the complexities of dual sparse matrix representations (CSC and CSR), implements efficient linear algebra kernels, and maintains data structures that enable both column-oriented (simplex) and row-oriented (API) access patterns. The module supports lazy format conversion to minimize memory overhead while ensuring high-performance access when needed.

The module serves as the foundational layer for all matrix-related operations in the optimizer, providing the data structures and algorithms that enable constraint representation, objective evaluation, basis operations, and API query functions.

### 1.2 Responsibilities

This module is responsible for:

- Maintaining constraint matrices in Compressed Sparse Column (CSC) format for column-wise solver access
- Providing on-demand Compressed Sparse Row (CSR) conversion for row-wise API queries
- Implementing sparse matrix-vector multiplication (y = Ax)
- Computing dot products between sparse and dense vectors
- Calculating vector norms (L1, L2, L-infinity)
- Sorting index arrays while maintaining correspondence with value arrays
- Managing the three-stage lazy conversion pipeline (prepare, transpose, finalize)

This module is NOT responsible for:

- Matrix factorization or basis decomposition (handled by Basis module)
- Numerical linear algebra beyond basic matrix-vector operations
- Model construction or modification (handled by API module)
- Optimization algorithms that use the matrix (handled by Simplex, Pricing modules)
- Memory allocation strategy (uses Memory module primitives)

### 1.3 Design Philosophy

The module follows a lazy evaluation philosophy for format conversion: matrices are stored in CSC format (optimal for simplex column operations) until a row-oriented query occurs, at which point CSR format is generated and cached. This minimizes memory overhead for models that never query constraint data by row (typical case) while providing efficient access when needed.

Computational kernels prioritize sparse-aware algorithms that exploit zero-skipping for performance. Functions are designed as pure computational primitives without side effects, enabling flexible composition in higher-level algorithms.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_prepare_row_data | Allocate CSR arrays for conversion | API (cxf_getconstrs) |
| cxf_build_row_major | Perform CSC-to-CSR transpose | API (after prepare) |
| cxf_finalize_row_data | Mark CSR data valid | API (after build) |
| cxf_matrix_multiply | Sparse matrix-vector product (y = Ax) | Simplex, Pricing |
| cxf_dot_product | Vector dot product (multiple variants) | Simplex, Objective |
| cxf_vector_norm | Compute vector norms (L1/L2/L∞) | Pricing, Statistics |
| cxf_sort_indices | Sort index arrays with optional values | Matrix construction, Quadratic |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| MatrixData | Sparse matrix structure with CSC and CSR representations |
| (Arrays are embedded in MatrixData, not separate types) | - |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| NORM_LINF | 0 | L-infinity norm selector |
| NORM_L1 | 1 | L1 norm selector |
| NORM_L2 | 2 | L2 (Euclidean) norm selector |

## 3. Internal Functions

### 3.1 Private Functions

The module has no purely private functions - all seven functions have well-defined interfaces used by other modules.

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| (memcpy, memset) | Standard C library functions | cxf_build_row_major, cxf_prepare_row_data |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| MatrixData.colStart | int64_t* | Model lifetime | Read-only after construction |
| MatrixData.rowIndices | int32_t* | Model lifetime | Read-only after construction |
| MatrixData.coeffValues | double* | Model lifetime | Read-only after construction |
| MatrixData.rowStart | int64_t* | On-demand, cached | Requires model lock |
| MatrixData.rowEnd | int64_t* | On-demand, cached | Requires model lock |
| MatrixData.rowColIndices | int32_t* | On-demand, cached | Requires model lock |
| MatrixData.rowCoeffValues | double* | On-demand, cached | Requires model lock |
| MatrixData.matrixType | int | Persistent flag | Protected by model lock |
| MatrixData.rowDataTrigger | void* | Transient trigger | Protected by model lock |

### 4.2 State Lifecycle

```
CREATED (CSC only)
    ↓
INITIALIZED (CSC populated by model construction)
    ↓
[Optional: Row query triggers conversion]
    ↓
PREPARE_CALLED (CSR arrays allocated)
    ↓
BUILD_CALLED (CSR arrays populated via transpose)
    ↓
FINALIZED (matrixType=1, rowDataTrigger=NULL)
    ↓
CACHED (subsequent row queries use cached CSR)
    ↓
[Model modification invalidates CSR, returns to CSC only]
    ↓
DESTROYED (model freed)
```

### 4.3 State Invariants

At all times, the following must be true:

- CSC arrays (colStart, rowIndices, coeffValues) are always valid and consistent
- If matrixType == 1, then all CSR arrays are valid and represent the transpose of CSC
- If rowStart is non-NULL, then rowEnd, rowColIndices, rowCoeffValues are also non-NULL
- CSC and CSR arrays contain the same mathematical matrix (when both present)
- Total non-zeros is consistent: colStart[numVars] == rowStart[numConstrs]
- Column indices within each row in CSR are sorted in ascending order
- Row indices within each column in CSC are typically unsorted (depends on construction)

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_malloc, cxf_free | Allocate/free CSR arrays and temporary workspace |
| Error | Error code definitions | Return standardized error codes (1001, 1003) |
| Validation | NULL pointer checks | Parameter validation in public functions |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory module (provides allocation primitives)
- **Before:** Simplex, Pricing, API query functions (consumers of matrix operations)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| memcpy | Standard C (string.h) | Copy arrays during transpose |
| memset | Standard C (string.h) | Zero-initialize arrays |
| sqrt | Standard C (math.h) | Square root for L2 norm |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Simplex | cxf_matrix_multiply, cxf_dot_product, cxf_vector_norm | Critical - must not change |
| Pricing | cxf_matrix_multiply (A^T y), cxf_dot_product | Critical |
| Basis | Matrix access for LU factorization | Critical |
| API | cxf_prepare/build/finalize_row_data for cxf_getconstrs | Public API - stable |
| Quadratic | cxf_sort_indices for Q matrix construction | Stable |
| Statistics | cxf_vector_norm for coefficient analysis | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- cxf_matrix_multiply signature and semantics (core computational kernel)
- cxf_dot_product sparse-dense variant (used throughout simplex)
- CSC and CSR array layout in MatrixData structure (offset dependencies in multiple modules)
- Three-stage conversion pipeline (prepare/build/finalize) - required for API correctness

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- CSC format is always valid and represents the current constraint matrix
- CSC-to-CSR conversion preserves all matrix entries exactly (no data loss)
- Matrix-vector multiplication produces numerically correct results (within IEEE 754 precision)
- Sorted indices are in ascending order with no duplicates (if input had no duplicates)
- All allocated memory is tracked and can be freed without leaks
- Lazy conversion is transparent - callers don't need to know which format is cached

### 7.2 Required Invariants

What this module requires from others:

- Model lock must be held during CSR conversion pipeline (no concurrent modification)
- CSC arrays must be sorted by row index within each column (for some operations)
- Matrix dimensions (numVars, numConstrs, numNonzeros) must be consistent
- Coefficient values must be finite (not NaN or Inf) for correct numerical results

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| NULL pointers | Explicit checks at function entry |
| Allocation failure | Check malloc return value |
| Invalid dimensions | Sanity checks (n > 0, nnz >= 0) |
| Index out of bounds | Assumed valid (caller responsibility in hot paths) |

### 8.2 Error Propagation

How errors flow through this module:

```
Allocation failure in cxf_prepare_row_data
    → Free partial allocations
    → Return error code 1001
    → Caller handles (typically abort optimization)

NULL pointer in cxf_matrix_multiply
    → Undefined behavior (caller error)
    → No validation in hot path for performance

Invalid matrix structure
    → Detected in debug builds via assertions
    → Undefined behavior in release builds
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Allocation failure | Clean up partial state, return error, leave CSC intact |
| NULL pointer | Caller error - undefined behavior |
| Invalid dimensions | Caller error - undefined behavior |
| Numerical overflow | IEEE 754 handling - result may be Inf/NaN |

## 9. Thread Safety

### 9.1 Concurrency Model

Computational functions (multiply, dot product, norm, sort) are thread-safe when operating on independent data. CSR conversion functions are NOT thread-safe and require external synchronization (model lock).

**Read-only operations:** Safe for concurrent execution if matrix is not being modified.
**CSR conversion:** Requires exclusive access to MatrixData structure.
**Sort operations:** Require exclusive access to arrays being sorted.

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| Model lock (external) | CSR conversion pipeline | Entire model |
| (No internal locks) | - | - |

### 9.3 Thread Safety Guarantees

- cxf_matrix_multiply: Thread-safe if matrix and vectors are not modified concurrently
- cxf_dot_product: Thread-safe (pure function)
- cxf_vector_norm: Thread-safe (pure function)
- cxf_sort_indices: NOT thread-safe (in-place modification)
- cxf_prepare/build/finalize: NOT thread-safe (require model lock)

### 9.4 Known Race Conditions

CSR conversion without model lock:
- Thread A calls cxf_build_row_major
- Thread B reads matrixType flag
- Thread B sees matrixType=1 before CSR arrays are fully populated
- Thread B reads invalid CSR data → corrupted results

Mitigation: Always acquire model lock before calling prepare/build/finalize.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_prepare_row_data | O(m) | O(nnz + m) |
| cxf_build_row_major | O(nnz + m + n) | O(m) |
| cxf_finalize_row_data | O(1) | O(1) |
| cxf_matrix_multiply | O(nnz) | O(1) |
| cxf_dot_product (dense) | O(n) | O(1) |
| cxf_dot_product (sparse-dense) | O(nnz_x) | O(1) |
| cxf_vector_norm | O(n) | O(1) |
| cxf_sort_indices | O(n log n) worst, O(n) best | O(log n) |

Where: n = vector dimension, m = number of rows, nnz = non-zeros

### 10.2 Hot Paths

Performance-critical functions called frequently:

1. **cxf_matrix_multiply** - Called every simplex iteration for constraint evaluation
   - Optimization: Zero-skipping for sparse x vectors
   - Memory access: Sequential colStart, random y writes (cache misses)
   - Typical time: 0.1-10 ms for medium problems (100K-1M non-zeros)

2. **cxf_dot_product** - Called for objective value, reduced costs
   - Optimization: Kahan summation for numerical stability
   - Memory access: Sequential reads with good cache locality
   - Typical time: <0.1 ms for dense vectors up to 10K elements

3. **cxf_vector_norm** - Called for steepest edge pricing
   - Optimization: Bitwise absolute value (faster than fabs)
   - Memory access: Sequential scan
   - Typical time: <0.1 ms for vectors up to 100K elements

Cold paths (called infrequently):
- CSR conversion: Once per optimization, acceptable 0.5-500 ms overhead
- Sort operations: During model construction, not time-critical

### 10.3 Memory Usage

Typical memory consumption patterns:

**CSC storage only (no row queries):**
- colStart: (n+1) × 8 bytes = ~80 KB for 10K variables
- rowIndices: nnz × 4 bytes = ~4 MB for 1M non-zeros
- coeffValues: nnz × 8 bytes = ~8 MB for 1M non-zeros
- **Total: ~12 MB for medium problem**

**CSC + CSR storage (after row query):**
- CSR arrays add approximately same size as CSC
- rowStart: (m+1) × 8 bytes
- rowEnd: m × 8 bytes
- rowColIndices: nnz × 4 bytes
- rowCoeffValues: nnz × 8 bytes
- **Total: ~24 MB (doubles memory), hence lazy conversion**

Temporary workspace:
- cxf_build_row_major: (m+1) × 8 bytes for workPtr array (~80 KB for 10K constraints)

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_prepare_row_data](../functions/matrix/cxf_prepare_row_data.md) - Allocate CSR arrays for on-demand conversion
2. [cxf_build_row_major](../functions/matrix/cxf_build_row_major.md) - Transpose CSC to CSR using two-pass algorithm
3. [cxf_finalize_row_data](../functions/matrix/cxf_finalize_row_data.md) - Mark CSR data as valid and ready
4. [cxf_matrix_multiply](../functions/matrix/cxf_matrix_multiply.md) - Sparse matrix-vector multiplication
5. [cxf_dot_product](../functions/matrix/cxf_dot_product.md) - Vector dot product (dense and sparse variants)
6. [cxf_vector_norm](../functions/matrix/cxf_vector_norm.md) - Compute L1, L2, or L-infinity norms
7. [cxf_sort_indices](../functions/matrix/cxf_sort_indices.md) - Sort index arrays using introsort algorithm

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Lazy CSR conversion | Minimize memory for models that never query rows | Always maintain both formats (2x memory) |
| CSC as primary format | Column-oriented simplex needs efficient column access | Row-major (worse for simplex performance) |
| Three-stage conversion | Allows caller control and error handling between stages | Single atomic conversion (less flexible) |
| Introsort for sorting | Guaranteed O(n log n) worst case | Quicksort (O(n²) worst), Merge sort (more memory) |
| Kahan summation in dot product | Improve numerical accuracy for long vectors | Naive summation (faster but less accurate) |
| Zero-skipping in multiply | Exploit sparsity of x in Ax | Always compute (wastes cycles on zeros) |

### 12.2 Known Limitations

- CSR arrays cannot be partially updated - any matrix modification invalidates entire CSR cache
- Sorting is unstable - equal indices may be reordered
- Matrix-vector multiply does not detect or handle NaN/Inf values
- No parallelization of matrix operations (single-threaded algorithms)
- CSR format assumes sufficient memory for duplication (may fail on very large models)

### 12.3 Future Improvements

- Incremental CSR updates for small matrix modifications (avoid full rebuild)
- Parallel matrix-vector multiplication for very large matrices (thread per block)
- SIMD vectorization for dense operations (AVX/FMA instructions)
- Compressed CSR format for very sparse rows (further reduce memory)
- Stable sorting variant for applications requiring it
- Blocked algorithms for better cache utilization in matrix operations

## 13. References

- Saad, Y. (2003). *Iterative Methods for Sparse Linear Systems*, 2nd ed. SIAM.
- Davis, T. A. (2006). *Direct Methods for Sparse Linear Systems*. SIAM.
- Musser, D. R. (1997). "Introspective Sorting and Selection Algorithms". *Software: Practice and Experience* 27(8): 983–993.
- Kahan, W. (1965). "Pracniques: Further Remarks on Reducing Truncation Errors". *Communications of the ACM* 8(1): 40.
- Convexfeld Optimizer Reference Manual: Matrix storage and access patterns

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear
- [x] Performance characteristics documented
- [x] State lifecycle defined
- [x] Invariants specified

---

*Date: 2026-01-25*
*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
