# Module: Matrix Core

## Purpose

The Matrix Core module provides the foundational sparse matrix operations required by the LP solver: preparing matrix data for row-wise access, constructing the row-major (CSR) representation from the column-major (CSC) storage, partitioning active constraints for efficient iteration, and sorting sparse index arrays. These operations collectively implement the lazy CSC-to-CSR conversion pipeline that enables efficient constraint-wise access to the model's sparse data.

The constraint matrix A is stored natively in Compressed Sparse Column (CSC) format, which is optimal for the column-oriented operations that dominate simplex method computations (Maros, *Computational Techniques of the Simplex Method*, 2003, Chapter 8). However, certain operations -- retrieving constraint data, dual simplex pricing, and exporting model data -- require row-wise access. Rather than maintaining both representations eagerly, the solver builds the Compressed Sparse Row (CSR) representation on demand when first needed and caches it for subsequent use. This lazy conversion is a standard technique in production LP solvers, deferring the O(nnz) conversion cost until it is actually needed (Saad, *Iterative Methods for Sparse Linear Systems*, 2003, Section 3.4).

The conversion pipeline has four stages, three of which are provided by this module:

| Stage | Function | Role |
|-------|----------|------|
| 1 | cxf_prepare_row_data | Reverse scaling, prepare for row-major construction |
| 1.5 | cxf_matrix_setup | Partition active/removed constraints, swap array pointers |
| 2 | cxf_build_row_major | Perform two-pass CSC-to-CSR conversion |
| 3 | cxf_finalize_row_data (external) | Re-apply scaling, restore original arrays |

The fourth function, cxf_sort_indices, is a general-purpose hybrid sorting utility used throughout the matrix subsystem for ordering sparse index arrays.

## Functions

### cxf_prepare_row_data

**Purpose:** Reverse the scaling transformation on matrix data, converting from the scaled internal representation back to original coefficient values, and prepare the matrix for row-major construction.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose matrix data will be unscaled
- Input: `mode` : int - Operation mode controlling the degree of state cleanup (see Behavioral Description)
- Output: void

**Preconditions:**
- The model must have a valid matrix data instance (either working copy or primary)
- The model must have a valid environment reference

**Postconditions:**
- On mode 0 (full unscaling): All matrix coefficients, objective coefficients, bounds, quadratic constraint coefficients, and piecewise-linear constraint data have been converted from scaled to original values. The scaling state has been fully cleared (global scale factor reset to 1.0, scaling mode cleared, row scaling factors freed). Penalty data and model lock have been released. If range constraint negation was active, it has been reversed.
- On mode 1 (partial unscaling): All coefficients have been unscaled as in mode 0, but the scaling state has been saved for later restoration rather than cleared. The scaling factor arrays are detached from the matrix but preserved.
- On mode 2+ (finalization only): No unscaling is performed. Only the swap data column length restoration (if active) is executed.
- In all modes: If swap data was active (from a prior cxf_matrix_setup call), the temporarily swapped column length arrays are restored to their original positions.

**Side Effects:**
- Modifies all coefficient arrays on the matrix data structure (objective, bounds, constraint coefficients, quadratic coefficients, piecewise-linear breakpoints and slopes)
- May release penalty data and model lock (mode 0 only)
- May free scaling factor arrays (mode 0 only)
- May save scaling state for later restoration (mode 1 only)
- Issues a matrix change notification after clearing or saving scaling state
- Temporarily swaps and restores column length arrays when swap data is active

**Error Conditions:**
- Null working matrix and null primary matrix -> undefined behavior (no explicit guard)
- Missing scaling factor arrays when mode 0 or 1 -> scaling is skipped; state cleanup proceeds

**Behavioral Description:**
The function operates in multiple phases controlled by the mode parameter:

1. **Matrix selection:** The function selects the working copy of the matrix data if available, falling back to the primary matrix. It extracts dimension counts and scaling factor arrays.

2. **Swap data handling (entry):** If a swap data structure is present with active entries (from a prior partitioning operation), the column length arrays are temporarily swapped so that subsequent operations see the full (unpartitioned) column lengths.

3. **Unscaling (modes 0 and 1 only):** If both row and column scaling factors exist, the function reverses the scaling transformation D_r * A * D_c * g (where D_r and D_c are diagonal scaling matrices and g is the global objective scale factor) applied during the solve. This involves:
   - Dividing each objective coefficient by the product of the global scale factor and the corresponding column scale factor
   - Multiplying finite row bounds by the corresponding column scale factor (undoing the division applied during scaling)
   - Dividing each matrix coefficient by the product of its row and column scaling factors
   - Dividing finite column bounds (constraint right-hand sides in internal storage) by their row scaling factors
   - For quadratic constraints: dividing each quadratic coefficient by the product of both variable scale factors and the global scale
   - For piecewise-linear constraints: unscaling breakpoint coordinates and slopes by the appropriate combinations of scale factors
   - Infinite values (positive or negative) are left unchanged during all unscaling operations

4. **State management:** In mode 0, the function releases penalty data and the model lock, resets the global scale factor to 1.0, clears the scaling mode, frees the row scaling factor array, and issues a change notification. In mode 1, the function saves the current global scale factor and row scaling pointer for later restoration, records the expected dimension count (for validation on restore), clears the active scaling pointers, and issues a change notification.

5. **Range constraint reversal (mode 0 only):** If range constraints were previously negated for internal normal form storage, the function reverses that negation: for each variable flagged as a range constraint type, it negates the objective coefficient, swaps and negates the row bounds, and negates all matrix coefficients in that column. The range negation flag is then cleared. This uses an exact IEEE 754 sign-bit flip that handles infinity and NaN correctly without floating-point rounding (a standard optimization described in IEEE 754-2008).

6. **Swap data handling (exit):** If swap data was active on entry, the column length arrays are swapped back to their partitioned state.

**Thread Safety:** Unsafe. This function modifies matrix data arrays in place and releases locks. The caller must ensure exclusive access.

**Dependencies:**
- Memory deallocation (Memory Primitives module)
- Penalty data release
- Model lock release
- Matrix change notification

---

### cxf_matrix_setup

**Purpose:** Partition each column's nonzero entries so that active constraints appear before removed constraints, enabling the row-major construction to operate on only the active subset.

**Signature:**
- Input: `matrix` : pointer-to-MatrixData - The matrix data structure to partition
- Output: void

**Preconditions:**
- The matrix's swap data structure must be allocated and present
- The swap data must be in row-major conversion mode (not yet initialized for partitioning)
- The swap data must contain a valid constraint status array classifying each constraint as active or removed

**Postconditions:**
- The swap data reference count has been incremented
- If the swap data was already initialized (partitioning previously performed): only the array pointer swap (below) is executed
- If partitioning was needed:
  - For each column, the CSC row index and coefficient arrays have been rearranged in place so that entries corresponding to active constraints (constraint status non-negative) appear at the beginning of the column's span, followed by entries for removed constraints
  - The active nonzero count for each column has been recorded in the swap data
  - A copy of the column bounds (constraint right-hand sides) has been made to the swap data buffer, with bounds for active constraints zeroed out
  - The swap data initialization flag has been set
- The matrix's column length array pointer has been exchanged with the swap data's active count array, so that subsequent operations see only active nonzero counts
- The matrix's column bounds array pointer has been exchanged with the swap data's bounds buffer, so that subsequent operations see the prepared bounds
- The original column length and column bounds pointers have been saved in the swap data for later restoration

**Side Effects:**
- Modifies the CSC row index and coefficient arrays in place (rearranging entries within each column's span, not changing the values)
- Swaps array pointers on the matrix data structure
- Modifies swap data state fields (reference count, initialization flag, saved pointers)
- Copies and zeroes portions of the column bounds array

**Error Conditions:**
- Null swap data pointer -> silent return, no action
- Swap data not in row-major conversion mode -> reference count incremented, then silent return

**Behavioral Description:**
The function implements an active constraint partitioning scheme that enables the subsequent CSR construction to operate only on the active (non-removed) portion of the matrix. This is essential for correct behavior after presolve, which may remove constraints from the problem.

1. **Reference counting:** The swap data reference count is incremented on entry to support nested setup/cleanup call pairs.

2. **Mode and initialization check:** The function verifies that the swap data is in row-major conversion mode. If already initialized (from a prior call), it skips directly to the array pointer swap.

3. **Column partitioning:** For each column in the CSC representation, the function performs a two-pointer partition (analogous to Hoare's partition scheme from quicksort; see Hoare, 1962). A left pointer advances from the column start while pointing to active entries, and a right pointer retreats from the column end while pointing to removed entries. When the left pointer finds a removed entry and the right pointer finds an active entry, their row indices and coefficients are swapped. After partitioning, the count of active entries for the column is the distance from the column start to the final left pointer position.

4. **Bounds preparation:** The column bounds (which in internal storage correspond to constraint right-hand sides) are copied to the swap buffer. Bounds for active constraints are then zeroed out, as they will be recomputed during row-major processing.

5. **Array pointer swap:** The matrix's column length array pointer is exchanged with the swap data's active count array, and the column bounds pointer is exchanged with the swap data's prepared bounds buffer. The original pointers are saved in the swap data so they can be restored during finalization. This pointer-swapping technique avoids copying entire arrays, providing O(1) overhead for the swap itself.

**Thread Safety:** Unsafe. The function modifies CSC arrays in place and performs pointer swaps without synchronization. The caller must ensure exclusive access to the matrix data.

**Dependencies:**
- Memory copy utility (for column bounds preparation)

---

### cxf_build_row_major

**Purpose:** Construct Compressed Sparse Row (CSR) indices from the native CSC format for all constraint types, enabling efficient row-wise (constraint-wise) access to the model's sparse data.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose matrix data will have CSR arrays built
- Output: int - Zero on success, or an error code on failure

**Preconditions:**
- The model must have a valid primary matrix data instance
- The model must have a valid environment reference
- CSC arrays on the matrix must be consistent (column starts, column lengths, row indices, and coefficient values all properly populated)
- If cxf_matrix_setup has been called, the column length array reflects only active nonzero counts

**Postconditions:**
- On success (return value zero):
  - All previously cached CSR arrays have been freed and rebuilt from the current CSC data
  - For linear constraints: the CSR arrays (row starts, row ends, row start working copy, row column indices, and row coefficient values) are populated, providing O(1) access to any row's nonzeros
  - For general constraints (if present): a variable-to-constraint reverse mapping has been built, enabling efficient lookup of all general constraints referencing a given variable
  - For quadratic constraints (if present): a per-variable CSR index has been built for all quadratic constraint terms, including both linear and quadratic parts. Off-diagonal quadratic terms are indexed from both variables. A separate single-variable quadratic constraint index is built for constraints involving only one variable, with entries sorted by constraint index.
  - For SOS constraints (if present): the SOS CSR arrays have been built by delegation to a helper function
  - The row-major ready flag on the matrix is implicitly set (via the presence of non-null CSR arrays)
- On failure (nonzero return):
  - An out-of-memory error code is returned if any allocation fails during construction
  - A size limit error code is returned if the total number of quadratic constraint nonzeros exceeds two billion (the maximum representable in 32-bit indices)
  - Partially allocated arrays may remain on the matrix (the caller should handle cleanup)

**Side Effects:**
- Frees all previously allocated CSR arrays (linear, quadratic, SOS, and general constraint row-major arrays)
- Allocates new CSR arrays for all applicable constraint types
- Stores the newly allocated array pointers on the matrix data structure
- May report an error to the environment (for the quadratic constraint overflow case)

**Error Conditions:**
- Memory allocation failure during any phase -> returns the out-of-memory error code
- Quadratic constraint nonzero count exceeds two billion -> reports an error and returns the size limit error code

**Behavioral Description:**
The function performs the standard two-pass CSC-to-CSR conversion algorithm (Saad, 2003, Section 3.4; also described in Duff, Erisman, and Reid, *Direct Methods for Sparse Linear Systems*, 1986) and extends it to handle multiple constraint types:

1. **Cache invalidation:** All existing CSR arrays are freed (linear CSR, quadratic constraint CSR, SOS CSR, and variable-to-constraint mappings). This ensures a clean rebuild after any model modification that invalidated the cached CSR data.

2. **Linear constraint CSR construction:** If the matrix has both variables and constraints, the function performs the two-pass conversion:
   - **Pass 1 (counting):** Traverse all CSC columns, counting the number of nonzeros per row. Compute a prefix sum over these counts to obtain row start pointers.
   - **Pass 2 (filling):** Traverse all CSC columns again. For each nonzero entry, place its column index and coefficient value at the next available position in the CSR arrays (tracked by a working copy of the row start pointers that is incremented as entries are placed).
   - The result is a set of CSR arrays where each row's entries can be accessed in O(1). A working copy of the row start pointers is preserved for use during subsequent operations.
   - For edge cases (no variables, or no constraints but variables exist), minimal empty arrays are allocated.

3. **Variable-to-constraint mapping:** If general constraints are present with sparse row activity flags, the function builds a reverse index from variables to the general constraints that reference them. This uses the same two-pass counting and filling approach.

4. **Quadratic constraint CSR construction:** If quadratic or general constraints exist, the function builds a per-variable index to all quadratic constraint terms. For each variable, this index provides all linear and quadratic terms from quadratic constraints that involve that variable. Off-diagonal quadratic terms (where the two variable indices differ) are counted and indexed from both variables to support symmetric access to the Q matrix. A separate optimized index is built for single-variable quadratic constraints (those with exactly one quadratic term), with entries sorted by constraint index for efficient lookup.

5. **SOS constraint delegation:** If SOS (Special Ordered Set) constraints exist, their row-major representation is built by a dedicated helper function.

The overall time complexity is O(nnz) for the linear CSR construction plus O(nnz_qc) for the quadratic constraint index, where nnz and nnz_qc are the respective nonzero counts. The space complexity is O(nnz + numConstrs + numVars) for the linear CSR and proportional amounts for the other constraint types.

**Thread Safety:** Unsafe. The function allocates and stores arrays on the matrix data structure without synchronization. The caller must hold the model-level critical section.

**Dependencies:**
- Memory allocation and deallocation (Memory Primitives module)
- Error reporting (Error Handling module)
- SOS row-major construction helper
- Quadratic constraint index sorting helper (cxf_sort_indices or a related sorting utility)

---

### cxf_sort_indices

**Purpose:** Sort a pair of parallel sparse arrays (values and associated integer indices) in ascending order of the values, using a hybrid sorting algorithm optimized for the array sizes typical in LP solver operations.

**Signature:**
- Input: `count` : int64 - The number of elements to sort
- Input: `values` : pointer-to-array-of-double - The values array (primary sort key, sorted in ascending order)
- Input: `indices` : pointer-to-array-of-int - The indices array (permuted to maintain correspondence with the values)
- Output: void

**Preconditions:**
- `count` must be non-negative
- `values` and `indices` must point to arrays of at least `count` elements
- Both arrays must be writable

**Postconditions:**
- The `values` array is sorted in ascending order
- The `indices` array has been permuted identically to the values, so that the original pairing between values[k] and indices[k] is preserved after sorting
- The sort is not guaranteed to be stable (equal values may have their relative order changed)

**Side Effects:**
- Modifies both the `values` and `indices` arrays in place

**Error Conditions:**
- Count of zero or one -> no-op, arrays unchanged

**Behavioral Description:**
The function implements a hybrid sorting algorithm combining quicksort and shellsort, similar in spirit to the introsort algorithm (Musser, 1997, "Introspective Sorting and Selection Algorithms"):

1. **Quicksort phase:** For arrays of 100 or more elements, the function uses a quicksort variant with three-way partitioning (the Dutch National Flag algorithm; Dijkstra, 1976). The pivot is selected using the median-of-three strategy (choosing the median of the first, middle, and last elements) to reduce the probability of worst-case partitioning. Three-way partitioning handles duplicate values efficiently by collecting elements equal to the pivot in the middle partition, avoiding unnecessary recursive calls on equal-valued segments.

2. **Recursion depth limiting:** To prevent excessive stack depth in pathological cases, the quicksort phase is limited to a fixed maximum recursion depth. If the depth limit is reached, the function falls through to the shellsort phase rather than continuing to recurse. This provides O(n log n) worst-case behavior similar to introsort.

3. **Shellsort phase:** For arrays smaller than the quicksort threshold, or when the recursion depth limit is reached, the function uses shellsort with a diminishing gap sequence derived by dividing the current gap by a shrink factor (approximately 2.2). Shellsort is well-suited for small to medium arrays and provides good performance on nearly-sorted data. The final pass uses gap 1, which is equivalent to insertion sort, ensuring correctness.

4. **Parallel array management:** Throughout all sorting operations, whenever two elements are compared and swapped in the values array, the corresponding elements in the indices array are swapped identically, maintaining the value-index correspondence.

Note: Despite the function name suggesting sorting of indices, the primary sort key is the values array. The indices are permuted as satellites. This function is used in contexts such as ordering sparse vector entries by coefficient magnitude, sorting pricing candidates by reduced cost, and arranging quadratic constraint terms by constraint index.

**Thread Safety:** Safe (operates only on the provided arrays with no shared state).

**Dependencies:** None (self-contained sorting implementation).

---

## Module-Level Behavioral Notes

### The Lazy CSR Conversion Pipeline

The four functions in this module (together with the external cxf_finalize_row_data) form a pipeline that is triggered lazily on the first row-wise access to the matrix after any model modification. The pipeline proceeds as follows:

1. **cxf_prepare_row_data** reverses any scaling transformation that was applied during the solve, converting matrix coefficients back to their original (user-provided) values. It also handles range constraint negation reversal and manages the swap data column length state.

2. **cxf_matrix_setup** partitions each column's entries in the CSC arrays so that active constraints (those not removed by presolve) appear before removed constraints. It then swaps array pointers so that subsequent operations see only active column lengths and prepared bounds.

3. **cxf_build_row_major** performs the actual CSC-to-CSR conversion using the standard two-pass algorithm. It also constructs row-major indices for quadratic constraints, SOS constraints, and general constraint variable mappings.

4. **cxf_finalize_row_data** (external to this module) re-applies scaling, restores the original array pointers from swap data, and decrements the swap data reference count.

The CSR arrays are cached on the matrix data structure for reuse. Any model modification invalidates the cache by setting all CSR array pointers to null, causing the next row-wise access to re-trigger the full pipeline.

### Active Constraint Partitioning

The partitioning performed by cxf_matrix_setup is a critical optimization for presolved models. After presolve removes constraints, the matrix still contains entries for those constraints in the CSC arrays. Rather than allocating a new compressed matrix with only active entries, the solver partitions each column in place using Hoare's two-pointer scheme (Hoare, 1962) and temporarily swaps the column length array with an active-count array. This achieves O(nnz) partitioning without additional memory allocation for the matrix copy, and the original layout can be restored in O(1) by swapping the pointers back.

### Scaling and Row-Major Access Interaction

The matrix may be stored in scaled form during optimization (where all coefficients have been multiplied by row and column scaling factors to improve numerical conditioning; see Tomlin, 1975; Curtis and Reid, 1972). Row-major access requests from the user API expect unscaled (original) coefficients. The pipeline handles this by unscaling before CSR construction and re-scaling afterward, ensuring that the cached CSR always reflects the appropriate coefficient form for its context.

### Naming Note

The function cxf_sort_indices sorts by values, not by indices, despite its name. The "indices" in the name refers to the fact that integer index arrays are sorted alongside their associated values (as satellites), which is the typical use case in sparse matrix operations.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_prepare_row_data | Unsafe | Modifies matrix data in place, releases locks |
| cxf_matrix_setup | Unsafe | Partitions CSC arrays in place, swaps pointers |
| cxf_build_row_major | Unsafe | Allocates and stores arrays on the matrix |
| cxf_sort_indices | Safe | Operates only on provided arrays, no shared state |

All unsafe functions require the caller to hold the model-level critical section or otherwise ensure exclusive access to the matrix data.

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] All algorithms cite published sources
[x] Passes the Clean Room Test
```
