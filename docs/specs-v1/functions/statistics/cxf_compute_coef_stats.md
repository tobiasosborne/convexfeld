# cxf_compute_coef_stats

**Module:** Statistics
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Computes minimum and maximum absolute values for all standard model coefficient types by scanning the constraint matrix, objective, bounds, RHS, and quadratic terms. Results are stored in a cache for reuse and returned through output parameters. This is a pure computational function used by diagnostic and logging routines to assess numerical properties of the model.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| model | CxfModel* | Model structure containing problem data | Valid pointer | Yes |
| objMax | double* | Output: maximum objective coefficient | Valid pointer | Yes |
| objMin | double* | Output: minimum objective coefficient (nonzero) | Valid pointer | Yes |
| qobjMax | double* | Output: maximum quadratic objective coefficient | Valid pointer | Yes |
| qobjMin | double* | Output: minimum quadratic objective coefficient (nonzero) | Valid pointer | Yes |
| rhsMax | double* | Output: maximum RHS value | Valid pointer | Yes |
| rhsMin | double* | Output: minimum RHS value (nonzero) | Valid pointer | Yes |
| boundsMax | double* | Output: maximum bounds (excluding infinity) | Valid pointer | Yes |
| boundsMin | double* | Output: minimum bounds (nonzero, excluding infinity) | Valid pointer | Yes |
| matrixMax | double* | Output: maximum matrix coefficient | Valid pointer | Yes |
| matrixMin | double* | Output: minimum matrix coefficient (nonzero) | Valid pointer | Yes |
| qmatrixMax | double* | Output: maximum quadratic constraint matrix coefficient | Valid pointer | Yes |
| qmatrixMin | double* | Output: minimum quadratic constraint matrix coefficient (nonzero) | Valid pointer | Yes |
| qlmatrixMax | double* | Output: maximum quadratic linear part coefficient | Valid pointer | Yes |
| qlmatrixMin | double* | Output: minimum quadratic linear part coefficient (nonzero) | Valid pointer | Yes |
| qrhsMax | double* | Output: maximum quadratic RHS value | Valid pointer | Yes |
| qrhsMin | double* | Output: minimum quadratic RHS value (nonzero) | Valid pointer | Yes |
| reserved | void* | Reserved parameter (unused) | NULL | No |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | int | 0 on success, error code on allocation failure |
| objMax | double | Maximum absolute objective coefficient |
| objMin | double | Minimum nonzero absolute objective coefficient |
| qobjMax | double | Maximum absolute quadratic objective coefficient |
| qobjMin | double | Minimum nonzero absolute quadratic objective coefficient |
| rhsMax | double | Maximum absolute RHS value |
| rhsMin | double | Minimum nonzero absolute RHS value |
| boundsMax | double | Maximum absolute bound (excluding infinity) |
| boundsMin | double | Minimum nonzero absolute bound (excluding infinity) |
| matrixMax | double | Maximum absolute matrix coefficient |
| matrixMin | double | Minimum nonzero absolute matrix coefficient |
| qmatrixMax | double | Maximum absolute quadratic constraint matrix coefficient |
| qmatrixMin | double | Minimum nonzero absolute quadratic constraint matrix coefficient |
| qlmatrixMax | double | Maximum absolute quadratic linear part coefficient |
| qlmatrixMin | double | Minimum nonzero absolute quadratic linear part coefficient |
| qrhsMax | double | Maximum absolute quadratic RHS value |
| qrhsMin | double | Minimum nonzero absolute quadratic RHS value |

### 2.3 Side Effects

Allocates 128-byte cache at model if not already allocated. Cache persists for model lifetime to avoid recomputation.

## 3. Contract

### 3.1 Preconditions

- [ ] model pointer must be valid
- [ ] model->env must be valid for memory allocation
- [ ] model->matrix must be initialized with coefficient data
- [ ] All output pointers must be valid

### 3.2 Postconditions

- [ ] All output parameters contain valid min/max ranges
- [ ] Zero coefficients are excluded from min ranges
- [ ] Infinite bounds are excluded from bounds ranges
- [ ] Cache is allocated and populated at model
- [ ] For all ranges: min <= max

### 3.3 Invariants

- [ ] Model coefficient data is not modified
- [ ] Cache allocation is idempotent (allocate once, reuse thereafter)

## 4. Algorithm

### 4.1 Overview

The function allocates or retrieves a 128-byte cache to store 16 double-precision min/max pairs. It then scans the model data structures in sequence: linear objective coefficients, quadratic objective, constraint matrix (in CSC format), constraint RHS, variable bounds (lower and upper), quadratic constraint matrices (Q and QL parts), and quadratic constraint RHS. For each coefficient type, it tracks the minimum and maximum absolute values, excluding zeros from minimum calculations and excluding infinite values from bounds calculations. After scanning, results are stored in the cache and copied to output parameters.

### 4.2 Detailed Steps

1. Retrieve or allocate 128-byte cache at model
2. If cache is NULL, allocate 128 bytes via memory allocator
3. If allocation fails, return out-of-memory error code
4. Extract model dimensions: numVars, numConstrs, numQConstrs
5. Determine if row-major matrix representation is needed, prepare if necessary
6. Initialize all min ranges to infinity (1e100), all max ranges to zero
7. Scan linear objective coefficients:
   - For each variable, extract objective coefficient
   - Update objMin/objMax with absolute value (skip zeros)
8. If quadratic constraints exist:
   - For each quadratic constraint:
     - Retrieve Q matrix data (quadratic part)
     - Scan Q coefficients, update qmatrixMin/qmatrixMax
     - Retrieve QL matrix data (linear part)
     - Scan QL coefficients, update qlmatrixMin/qlmatrixMax
9. Scan quadratic objective coefficients:
   - For each quadratic objective term, update qobjMin/qobjMax
10. Scan variable bounds:
    - For each variable:
      - Extract lower bound, update boundsMin/boundsMax (exclude if >= infinity threshold)
      - Extract upper bound, update boundsMin/boundsMax (exclude if >= infinity threshold)
    - Include quadratic constraint bounds if present
11. Scan constraint matrix (CSC format):
    - For each variable (column):
      - Iterate through nonzero entries in column
      - Update matrixMin/matrixMax with absolute coefficient values
12. Scan constraint RHS:
    - For each constraint, update rhsMin/rhsMax
13. Scan quadratic constraint RHS:
    - For each quadratic constraint, update qrhsMin/qrhsMax
14. Ensure min <= max for all ranges (handle empty ranges)
15. Store all 16 values in cache array
16. Copy cache values to output parameters
17. Return 0 (success)

### 4.3 Pseudocode (if needed)

```
FUNCTION update_range(value, min, max):
  absValue ← |value|
  IF absValue == 0:
    RETURN  // Skip zeros
  min ← min(min, absValue)
  max ← max(max, absValue)

FUNCTION update_bounds_range(value, min, max):
  absValue ← |value|
  IF absValue == 0 OR absValue >= INFINITY_THRESHOLD:
    RETURN  // Skip zeros and infinities
  min ← min(min, absValue)
  max ← max(max, absValue)

// Main algorithm
cache ← model.cache_ptr
IF cache == NULL:
  cache ← ALLOCATE(128 bytes)
  IF cache == NULL:
    RETURN ERROR_OUT_OF_MEMORY
  model.cache_ptr ← cache

objMin ← ∞, objMax ← 0
qobjMin ← ∞, qobjMax ← 0
matMin ← ∞, matMax ← 0
// ... initialize all ranges

FOR j in 0..numVars-1:
  update_range(objCoeffs[j], objMin, objMax)

IF numQConstrs >= 2:
  FOR i in 0..numQConstrs-1:
    qData ← GET_QCONSTR_DATA(Q_matrix, i)
    FOR each coefficient in qData:
      update_range(coefficient, qmatrixMin, qmatrixMax)
    qlData ← GET_QCONSTR_DATA(QL_matrix, i)
    FOR each coefficient in qlData:
      update_range(coefficient, qlmatrixMin, qlmatrixMax)

FOR i in 0..numQObjTerms-1:
  update_range(qobjCoeffs[i], qobjMin, qobjMax)

FOR j in 0..numVars-1:
  update_bounds_range(lb[j], boundsMin, boundsMax)
  update_bounds_range(ub[j], boundsMin, boundsMax)

FOR j in 0..numVars-1:
  start ← colStart[j]
  len ← colLen[j]
  FOR k in start..start+len-1:
    update_range(coeffValues[k], matMin, matMax)

FOR i in 0..numConstrs-1:
  update_range(rhs[i], rhsMin, rhsMax)

FOR i in 0..numQConstrs-1:
  qrhsData ← GET_QCONSTR_DATA(QRHS, i)
  FOR each value in qrhsData:
    update_range(value, qrhsMin, qrhsMax)

// Store in cache and output parameters
STORE_ALL_RANGES(cache, output_params)
RETURN 0
```

### 4.4 Mathematical Foundation (if applicable)

The minimum and maximum absolute values provide a measure of the numerical range (dynamic range) of coefficients. The coefficient range ratio max/min indicates potential numerical conditioning issues in linear algebra operations. Excluding zeros from minimum calculations prevents degeneracy (min=0 for sparse data). Excluding infinite bounds prevents contamination of statistics with sentinel values.

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n + m + nnz) - No quadratic terms
- **Average case:** O(n + m + nnz + q)
- **Worst case:** O(n + m + nnz + q)

Where:
- n = number of variables
- m = number of constraints
- nnz = number of nonzero matrix entries
- q = total number of quadratic terms (Q + QL + qobj)

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - Fixed-size cache (128 bytes)
- **Total space:** O(1) - Cache persists in model structure

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| Cache allocation failure | 1001 | cxf__ERROR_OUT_OF_MEMORY |

### 6.2 Error Behavior

Returns immediately on allocation failure. No partial results are stored. Model state is unchanged.

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| All zeros | All coefficients = 0 | min = max = 0 for all ranges |
| Infinite bounds | lb = -∞, ub = +∞ | Bounds excluded from boundsMin/boundsMax |
| No quadratic terms | numQObjTerms = 0, numQConstrs = 0 | Q ranges set to 0 |
| Single variable | numVars = 1 | All ranges computed correctly |
| Empty matrix | nnz = 0 | matrixMin = matrixMax = 0 |
| Cache already allocated | model.cache != NULL | Reuse existing cache, skip allocation |

## 8. Thread Safety

**Thread-safe:** No

**Synchronization required:** Caller must ensure model is not modified during execution. Cache allocation is not thread-safe.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_alloc | Memory | Allocate cache memory from environment |
| cxf_prepare_rowmajor | Matrix | Prepare row-major matrix representation if needed |
| cxf_get_qconstr_data | Quadratic | Retrieve quadratic constraint data arrays |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_coefficient_stats | Statistics | Compute ranges for logging and warnings |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_coefficient_stats | Primary caller - uses results for logging |
| cxf_gencon_stats | Computes ranges for general constraints (complementary) |

## 11. Design Notes

### 11.1 Design Rationale

Caching results avoids redundant computation if called multiple times during optimization setup. Separating quadratic matrix (Q) from quadratic linear (QL) allows fine-grained diagnostics. Excluding infinities from bounds prevents misleading statistics. Scanning in CSC format is efficient for column-wise access patterns.

### 11.2 Performance Considerations

Cache allocation on first call adds small overhead but eliminates recomputation. Loop unrolling in production code (processing 2 elements per iteration) improves cache locality and reduces loop overhead. Scanning all coefficients is unavoidable for accurate statistics.

### 11.3 Future Considerations

Could detect and cache row-major vs. column-major matrix access patterns to optimize scanning order. Could provide per-constraint or per-variable granularity for diagnostics.

## 12. References

- Compressed Sparse Column (CSC) matrix format
- IEEE 754 floating-point representation
- Convexfeld Optimizer internal matrix structures

## 13. Validation Checklist

Before finalizing this spec, verify:

- [x] No code copied from implementation
- [x] Algorithm description is implementation-agnostic
- [x] All parameters documented
- [x] All error conditions listed
- [x] Complexity analysis complete
- [x] Edge cases identified
- [x] A competent developer could implement from this spec alone

---

*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
