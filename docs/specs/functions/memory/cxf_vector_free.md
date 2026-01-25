# cxf_vector_free

**Module:** Memory Management
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Deallocate a vector container structure and all its constituent dynamically allocated arrays. Vector containers are data structures used throughout Convexfeld for storing sparse vectors, index lists, and coefficient arrays. This function performs a complete cleanup of the multi-level allocation structure.

## 2. Signature

### 2.1 Inputs

| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| vec | VectorContainer* | Pointer to vector container | NULL or valid VectorContainer pointer | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| return | void | No return value |

### 2.3 Side Effects

- Deallocates index array (if allocated)
- Deallocates value array (if allocated)
- Deallocates auxiliary data (if present)
- Deallocates container structure itself
- All pointers within structure become invalid

## 3. Contract

### 3.1 Preconditions

- [ ] vec is either NULL or a valid VectorContainer pointer
- [ ] All arrays within vec were allocated via cxf_ allocation functions
- [ ] Vector container has not been freed previously

### 3.2 Postconditions

- [ ] All memory associated with vector container is deallocated
- [ ] All constituent arrays are freed
- [ ] Caller must not access vec or any of its contents after call

### 3.3 Invariants

- [ ] Function is safe to call with NULL pointer (no-op)
- [ ] All allocated memory is freed (no leaks)

## 4. Algorithm

### 4.1 Overview

The function implements a hierarchical deallocation strategy. It first checks if the container pointer is NULL (early exit if so). Then it systematically frees contained allocations (index array, value array, auxiliary data) in order, checking each for NULL before freeing. Finally, it frees the container structure itself. This inside-out approach prevents memory leaks.

### 4.2 Detailed Steps

1. Check if vec is NULL - if so, return immediately (no-op)
2. Free index array:
   - Check if vec->indices is not NULL
   - If allocated, call cxf_free(vec->indices)
3. Free value array:
   - Check if vec->values is not NULL
   - If allocated, call cxf_free(vec->values)
4. Free auxiliary data:
   - Check if vec->auxData is not NULL
   - If present, call cxf_free(vec->auxData)
5. Free container structure:
   - Call cxf_free(vec)
6. Return (void)

### 4.3 Pseudocode (if needed)

```
PROCEDURE cxf_vector_free(vec)
  # Early exit for NULL
  IF vec = NULL THEN
    RETURN
  END IF

  # Free index array if allocated
  IF vec.indices ≠ NULL THEN
    cxf_free(vec.indices)
  END IF

  # Free value array if allocated
  IF vec.values ≠ NULL THEN
    cxf_free(vec.values)
  END IF

  # Free auxiliary data if present
  IF vec.auxData ≠ NULL THEN
    cxf_free(vec.auxData)
  END IF

  # Free container structure
  cxf_free(vec)

  RETURN
END PROCEDURE
```

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(1) - vec is NULL
- **Average case:** O(1) - fixed number of frees
- **Worst case:** O(1) - all arrays allocated

Where:
- Function calls cxf_free a fixed number of times (3-4)
- No loops or recursive operations

### 5.2 Space Complexity

- **Auxiliary space:** O(1) - no additional allocation
- **Total space:** O(-(container_size + arrays)) - releases all memory

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| vec is NULL | None (no-op) | Safe to call with NULL |
| Double-free | Undefined | Freeing same vector twice (caller error) |
| Arrays from different allocator | Undefined | Internal arrays not from cxf_ functions |

### 6.2 Error Behavior

No error return value. NULL input is handled safely. Double-free and mixed-allocator errors result in undefined behavior (heap corruption, crashes).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| NULL vector | vec=NULL | Return immediately, no-op |
| Empty vector | indices=NULL, values=NULL, auxData=NULL | Free only container |
| Partial vector | Only indices allocated | Free indices and container |
| Full vector | All fields allocated | Free indices, values, auxData, container |

## 8. Thread Safety

**Thread-safe:** Conditionally

The function itself has no internal synchronization. Thread safety depends on:
- Caller ensures exclusive access to vec during free
- No other thread is accessing vec or its arrays
- Underlying cxf_free function is thread-safe

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| cxf_free | Memory | Deallocate each array and container |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_free_solver_state | Memory | Free working vectors during solver cleanup |
| cxf_cleanup_simplex | Solver | Free temporary vectors after iteration |
| Various internal functions | Various | Cleanup of sparse operation vectors |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| cxf_alloc_vector | Inverse operation - allocates vector container |
| cxf_free | Lower-level function - frees individual allocations |
| cxf_free_solver_state | Higher-level cleanup function |

## 11. Design Notes

### 11.1 Design Rationale

Vector containers enable:
1. **Sparse representation** - Store only non-zero indices and values
2. **Type safety** - Separate integer indices from double values
3. **Flexible sizing** - Capacity can exceed current size for growth
4. **Auxiliary metadata** - Attach additional data to vectors

The hierarchical free ensures no memory leaks even if some fields are NULL.

### 11.2 Performance Considerations

- Very fast operation: ~4-5 function calls (cxf_free)
- No loops or iteration
- Typical time: <1 microsecond
- Safe to call in tight loops if needed

### 11.3 Future Considerations

- Could add reference counting for shared vectors
- Could add validation in debug mode (magic number check)
- Could zero out fields after freeing (defensive programming)

## 12. References

- "Data Structures for Sparse Matrices" - sparse vector representation
- Convexfeld internal architecture documentation

## 13. Validation Checklist

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
