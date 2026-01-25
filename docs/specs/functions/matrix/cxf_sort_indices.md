# cxf_sort_indices

**Module:** Matrix Operations
**Spec Version:** 1.0
**Last Updated:** 2026-01-25

## 1. Purpose

Sorts arrays of integer indices, optionally maintaining synchronization with associated value arrays. This function is essential for maintaining sorted order in sparse data structures, enabling efficient binary search, merge-based operations, and cache-friendly access patterns. Used throughout Convexfeld for sparse matrix construction, quadratic term ordering, constraint coefficient management, and basis index organization.

## 2. Signature

### 2.1 Inputs

**Indices-only variant:**
| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| indices | int* | Array of indices to sort | Valid pointer | Yes |
| n | int | Number of elements | >= 0 | Yes |

**Indices-values variant:**
| Parameter | Type | Description | Valid Range | Required |
|-----------|------|-------------|-------------|----------|
| indices | int* | Array of indices to sort | Valid pointer | Yes |
| values | double* | Associated values (reordered) | Valid pointer | Yes |
| n | int | Number of elements | >= 0 | Yes |

### 2.2 Outputs

| Output | Type | Description |
|--------|------|-------------|
| indices | int* | Sorted index array (in-place modification) |
| values | double* | Reordered value array (if provided, maintains correspondence) |

### 2.3 Side Effects

Modifies input arrays in-place. Indices are sorted in ascending order. If values array is provided, it is reordered to maintain the original index-value pairing.

## 3. Contract

### 3.1 Preconditions

- [ ] indices array is allocated with at least n elements
- [ ] For values variant: values array is allocated with at least n elements
- [ ] n >= 0 (zero-length arrays are valid)
- [ ] Arrays do not overlap in memory

### 3.2 Postconditions

- [ ] indices array is sorted in ascending order
- [ ] For i < j: indices[i] <= indices[j]
- [ ] For values variant: values[i] corresponds to original value at indices[i]
- [ ] No elements are lost or duplicated
- [ ] Permutation is consistent between indices and values

### 3.3 Invariants

- [ ] Number of elements n is unchanged
- [ ] Set of index values is unchanged (only ordering changes)
- [ ] Set of values is unchanged (only ordering changes)

## 4. Algorithm

### 4.1 Overview

Implements introsort (introspective sort), a hybrid sorting algorithm that combines quicksort, heapsort, and insertion sort to achieve optimal performance across different input patterns. Quicksort provides O(n log n) average case, heapsort guarantees O(n log n) worst case by falling back when recursion depth exceeds limits, and insertion sort handles small subarrays efficiently.

The algorithm uses median-of-three pivot selection to reduce probability of worst-case quicksort behavior, and tail recursion optimization to minimize stack depth.

### 4.2 Detailed Steps

1. **Base cases:**
   - If n <= 1: return immediately (already sorted)
   - If n < 16: use insertion sort (optimal for small arrays)

2. **Compute recursion depth limit:** depth_limit = 2 × log₂(n)

3. **Quicksort with introsort optimization:**
   - **Pivot selection:** Choose median of indices[lo], indices[mid], indices[hi]
   - **Partition:** Rearrange elements around pivot
     - Elements < pivot to left
     - Elements >= pivot to right
     - Swap both indices and values (if present)
   - **Recursion depth check:**
     - If depth_limit reaches 0: switch to heapsort
   - **Tail recursion optimization:**
     - Recursively sort smaller partition
     - Iterate on larger partition (avoids stack growth)

4. **Insertion sort for small subarrays (n < 16):**
   - For each element i from 1 to n-1:
     - Save key = indices[i] (and values[i] if present)
     - Shift larger elements right
     - Insert key at correct position

5. **Heapsort fallback (if recursion too deep):**
   - Build max-heap from array
   - Repeatedly extract maximum and rebuild heap
   - Maintains O(n log n) worst case guarantee

### 4.3 Pseudocode (if needed)

```
Algorithm: Introsort(indices, values, n)
    if n ≤ 1 then
        return  // Already sorted
    end

    if n < 16 then
        InsertionSort(indices, values, n)
        return
    end

    depth_limit ← 2 × log₂(n)
    QuicksortRecursive(indices, values, 0, n-1, depth_limit)

Function: QuicksortRecursive(indices, values, lo, hi, depth)
    while lo < hi do
        size ← hi - lo + 1

        if size < 16 then
            InsertionSort(indices[lo..hi], values[lo..hi], size)
            return
        end

        if depth = 0 then
            Heapsort(indices[lo..hi], values[lo..hi], size)
            return
        end

        // Median-of-three pivot selection
        mid ← lo + (hi - lo) / 2
        pivot ← median(indices[lo], indices[mid], indices[hi])

        // Partition around pivot
        p ← Partition(indices, values, lo, hi, pivot)

        // Tail recursion: sort smaller partition, iterate on larger
        if p - lo < hi - p then
            QuicksortRecursive(indices, values, lo, p, depth - 1)
            lo ← p + 1
        else
            QuicksortRecursive(indices, values, p + 1, hi, depth - 1)
            hi ← p
        end
    end

Function: Partition(indices, values, lo, hi, pivot)
    i ← lo - 1
    j ← hi + 1

    loop
        repeat i ← i + 1 until indices[i] ≥ pivot
        repeat j ← j - 1 until indices[j] ≤ pivot

        if i ≥ j then
            return j
        end

        swap(indices[i], indices[j])
        if values ≠ NULL then
            swap(values[i], values[j])
        end
    end

Function: InsertionSort(indices, values, n)
    for i ← 1 to n-1 do
        key_idx ← indices[i]
        key_val ← values[i] (if present)
        j ← i - 1

        while j ≥ 0 and indices[j] > key_idx do
            indices[j+1] ← indices[j]
            values[j+1] ← values[j] (if present)
            j ← j - 1
        end

        indices[j+1] ← key_idx
        values[j+1] ← key_val (if present)
    end
```

### 4.4 Mathematical Foundation (if applicable)

Sorting is a fundamental computational problem with well-established theory:

**Comparison-based sorting lower bound:** Ω(n log n) comparisons required in worst case.

**Introsort achieves optimal complexity:**
- Best case: O(n) for nearly-sorted inputs (insertion sort)
- Average case: O(n log n) (quicksort)
- Worst case: O(n log n) (heapsort fallback)

**Median-of-three pivot selection:** Reduces probability of O(n²) quicksort behavior from 1/n to approximately 1/n².

## 5. Complexity

### 5.1 Time Complexity

- **Best case:** O(n) for already sorted or nearly sorted arrays (insertion sort path)
- **Average case:** O(n log n) via quicksort
- **Worst case:** O(n log n) via heapsort fallback

Where:
- n = number of elements

Introsort guarantees worst-case O(n log n) by switching to heapsort when recursion depth exceeds 2 log₂(n).

### 5.2 Space Complexity

- **Auxiliary space:** O(log n) for recursion stack
- **Total space:** O(log n)

Tail recursion optimization ensures stack depth is logarithmic in n.

## 6. Error Handling

### 6.1 Error Conditions

| Condition | Error Code | Description |
|-----------|------------|-------------|
| indices is NULL | N/A | Undefined behavior |
| n < 0 | N/A | Undefined behavior |
| Overlapping arrays | N/A | Undefined behavior (data corruption) |

### 6.2 Error Behavior

No explicit error checking for performance. Invalid inputs cause undefined behavior. n = 0 is handled gracefully (no-op).

## 7. Edge Cases

| Case | Input | Expected Behavior |
|------|-------|-------------------|
| Empty array | n = 0 | Return immediately, no operations |
| Single element | n = 1 | Return immediately, already sorted |
| Already sorted | Ascending order | Fast path via insertion sort (O(n)) |
| Reverse sorted | Descending order | O(n log n), triggers heapsort or quicksort |
| All equal | Same value repeated | O(n) via insertion sort or early partition exit |
| Two unique values | Binary pattern | Efficient partitioning |
| Random order | Uniformly random | O(n log n) via quicksort |

## 8. Thread Safety

**Thread-safe:** No

In-place sorting modifies shared arrays. Concurrent sorting of the same array causes data races.

**Synchronization required:** Caller must ensure exclusive access to arrays during sorting. Safe to sort different arrays concurrently.

## 9. Dependencies

### 9.1 Functions Called

| Function | Module | Purpose |
|----------|--------|---------|
| None | - | Self-contained implementation |

### 9.2 Functions That Call This

| Function | Module | Context |
|----------|--------|---------|
| cxf_build_row_major | Matrix | Sort column indices within CSR rows |
| cxf_addqpterms | Quadratic | Sort Q matrix terms by (row, col) |
| cxf_addconstr | API | Sort variable indices in constraint |
| cxf_basis_refactor | Basis | Sort basis variable indices |

## 10. Related Functions

| Function | Relationship |
|----------|--------------|
| qsort | Standard C library sort (similar functionality) |
| std::sort | C++ STL sort (uses introsort) |
| cxf_build_row_major | Caller for CSR column index sorting |

## 11. Design Notes

### 11.1 Design Rationale

**Why introsort:**
1. Quicksort: Fast average case O(n log n)
2. Heapsort fallback: Guarantees O(n log n) worst case
3. Insertion sort: Optimal for small/nearly-sorted arrays
4. Combined: Best of all three approaches

**Median-of-three pivot:** Simple improvement over single-element pivot, significantly reduces pathological quicksort cases.

**Threshold n=16 for insertion sort:** Empirically optimal balance between insertion sort overhead and recursion overhead.

**Tail recursion:** Reduces stack depth from O(n) to O(log n), prevents stack overflow on large inputs.

### 11.2 Performance Considerations

**Cache behavior:**
- Insertion sort: Excellent (sequential access)
- Quicksort: Good (partition is cache-friendly)
- Heapsort: Poor (random access pattern)

**Typical performance:**
- Small arrays (n < 100): Insertion sort dominates, very fast
- Medium arrays (100 < n < 10000): Quicksort, ~2-3 comparisons per element
- Large arrays (n > 10000): Memory bandwidth limited

**Comparison count:**
- Average: ~1.38 n log₂(n) comparisons
- Worst case: ~2 n log₂(n) comparisons

### 11.3 Future Considerations

**Optimizations:**
- Radix sort for integers (O(n) but high constant factor)
- Parallel quicksort (partition across threads)
- SIMD for insertion sort on small arrays
- Adaptive threshold based on data characteristics

**Stability:** Current implementation is not stable (equal elements may be reordered). For stable sort, use merge sort variant.

## 12. References

- Introsort: Musser, D. R. (1997). "Introspective Sorting and Selection Algorithms". Software: Practice and Experience 27 (8): 983–993.
- Quicksort: Hoare, C. A. R. (1962). "Quicksort". The Computer Journal 5 (1): 10–16.
- Sorting algorithms: Knuth, D. E. (1998). The Art of Computer Programming, Volume 3: Sorting and Searching, 2nd ed.

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
