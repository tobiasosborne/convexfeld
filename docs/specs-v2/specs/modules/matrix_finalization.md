# Module: Matrix Finalization

## Purpose

The Matrix Finalization module implements matrix scaling (also called equilibration) to improve the numerical conditioning of the constraint matrix before solving a linear or quadratic programming model. Poor numerical conditioning -- where matrix coefficients span many orders of magnitude -- is a well-known source of solver instability and inaccurate solutions (Tomlin, 1975; Curtis and Reid, 1972). This module computes row and column scaling factors such that the scaled matrix has entries closer to unit magnitude, then applies those factors to the constraint matrix, right-hand side values, variable bounds, and any special constraint types (quadratic, piecewise-linear). The transformation is:

    A' = D_r * A * D_c

where D_r and D_c are diagonal matrices of row and column scaling factors, respectively. This is the standard scaling formulation used in production LP solvers (Maros, *Computational Techniques of the Simplex Method*, 2003, Chapter 9).

The module also handles constraint sense normalization for inequality constraints (converting greater-than-or-equal constraints to less-than-or-equal form by sign-flipping coefficients and bounds, using an exact IEEE 754 sign-bit operation). The computed scaling factors are stored on the MatrixData structure for later use in solution recovery (unscaling).

This module is the complement to the Matrix Core module (P3.14), which handles matrix construction, row-major conversion, and index sorting. While Matrix Core prepares the structural representation, Matrix Finalization prepares the numerical representation for the solver.

## Functions

### cxf_finalize_row_data

**Purpose:** Compute and apply matrix scaling factors to the constraint matrix, bounds, right-hand side, and special constraints, selecting the most appropriate scaling strategy based on matrix properties and user configuration.

**Signature:**
- Input: `model` : pointer-to-Model - The model whose matrix data will be scaled
- Input: `mode` : unsigned int - Scaling mode override. A sentinel value (all bits set) requests automatic mode selection; zero requests validation-only with no scaling; values one through four force specific scaling strategies
- Output: unsigned int - Zero on success, or the out-of-memory error code on allocation failure

**Preconditions:**
- The model must be structurally valid (non-null, with a valid environment pointer)
- The model must have a matrix data instance (either a working copy or a primary copy)
- The matrix must have its CSC (Compressed Sparse Column) arrays populated

**Postconditions:**
- On success (return zero):
  - If scaling was performed: row scaling factors, column scaling factors, and a global objective scale factor are computed and stored on the MatrixData structure. The constraint matrix coefficients have been multiplied by the corresponding row and column scaling factors. Row bounds, column bounds, right-hand side values, quadratic constraint coefficients, and piecewise-linear breakpoints have been transformed consistently. The matrix state is finalized.
  - If no scaling was required (zero constraints, mode is zero, or fast-path conditions apply): the matrix state is finalized without computing new scaling factors. Constraint sense normalization may have been applied.
  - If saved scaling factors were reused: the previously computed factors have been applied to the current matrix data without recomputation. The saved scaling cache has been consumed (cleared from storage).
- On failure (out-of-memory): all intermediate allocations are freed, no partial scaling is applied, the error code is returned

**Side Effects:**
- Modifies the matrix coefficient values in place (scaling applied)
- Modifies row bounds, column bounds, and right-hand side arrays
- Modifies quadratic constraint coefficient arrays (if quadratic constraints exist)
- Modifies piecewise-linear breakpoint coordinate arrays (if PWL constraints exist)
- Stores row and column scaling factor arrays on the MatrixData structure
- Stores the global objective scale factor on the MatrixData structure
- Sets the scaling mode field on the MatrixData structure
- Sets the row-ready status flag on the MatrixData structure
- May free previously saved scaling factor arrays (if dimension mismatch)
- Calls matrix preparation and finalization helper functions that may update internal state flags

**Error Conditions:**
- Memory allocation failure during scaling array allocation -> frees all temporary buffers and returns the out-of-memory error code
- Memory allocation failure during auxiliary buffer allocation in extended scaling modes -> same cleanup and error return

**Behavioral Description:**

This function executes a multi-phase pipeline that computes and applies matrix scaling. The phases are:

**Phase 1 -- Entry and Validation:**

The function obtains the matrix data from the model, preferring the working copy if it exists and falling back to the primary copy. It extracts the matrix dimensions (number of constraints and variables), environment parameters (feasibility tolerance, scaling tolerance, scaling hint flags, solve method), and sparse matrix arrays. Two validation functions are called: one to determine whether the matrix needs scaling, and one to check matrix integrity. The matrix is prepared for further processing via a preparation helper.

If the matrix has zero constraints, the function proceeds directly to finalization without scaling.

**Fast-path: Constraint Sense Normalization:**

If certain conditions are met (no penalty terms, no previously computed row-ready state, constraint type flags are available, no quadratic constraints, and no integrality information), the function performs a lightweight normalization pass instead of full scaling. For each inequality constraint (identified by the constraint type flag), it negates the constraint coefficients, swaps and negates the row bounds, and negates the right-hand side value. This normalization converts greater-than-or-equal constraints to less-than-or-equal form, which is the internal normal form expected by the simplex solver (see MatrixData Layer 1 specification, Invariant 6). The negation uses an IEEE 754 sign-bit flip, which is exact and handles infinity and NaN correctly.

If piecewise-linear constraints exist, the constraint type flags are cleared instead, deferring normalization.

If the mode is zero, the function exits after this normalization step.

**Fast-path: Saved Scaling Factor Reuse:**

If the MatrixData has previously saved scaling factors (from a prior solve on the same model), and the saved dimension matches the current total dimension (number of constraints plus number of variables), the function reuses the saved factors rather than recomputing. It transfers the saved row and column scale arrays to the active scaling fields, applies them to the matrix coefficients, scales the row bounds and column bounds, scales the right-hand side, and handles quadratic and piecewise-linear constraints. The saved scaling cache is then consumed (the saved pointer is cleared). If the dimensions do not match, the saved factors are freed and recomputation proceeds.

**Phase 2 -- Scale Factor Array Allocation and Initialization:**

A single contiguous block of memory is allocated to hold both row scaling factors (one per constraint) and column scaling factors (one per variable). The minimum allocation is two entries. If allocation fails, the function returns the out-of-memory error code.

Row scaling factors are initialized based on constraint classification:
- Constraints in the first partition (up to an index determined by integrality information) are initialized to +1.0
- Remaining constraints are initialized to -1.0 (the negative sign serves as a processing marker that is resolved to a positive magnitude during the scaling computation)

Column scaling factors are all initialized to +1.0.

**Phase 3 -- Global Scale Factor Computation and Mode Selection:**

A global scale factor for the right-hand side and objective is computed. The computation depends on the presence of quadratic constraints:

- **Without quadratic constraints:** The global scale is derived from the user-provided scaling tolerance parameter. Specific tolerance thresholds determine whether the global scale is set to a small tolerance value, derived from the tolerance directly, or left at unity.

- **With quadratic constraints:** The function surveys the magnitudes of right-hand side values and diagonal quadratic terms to find the minimum and maximum magnitudes. A geometric mean of these extremes is computed. Based on the ratio of the geometric mean to predefined thresholds, a global scale factor is derived using logarithmic rounding (computing log, then exponentiating to produce a "round" scaling value). This approach is standard for producing scaling factors that are approximate powers of a base, reducing rounding artifacts (Tomlin, 1975).

The scaling mode is then selected. If the mode parameter requests automatic selection, the function chooses based on matrix properties:

- **Presolve-reduced models** (identified by model type): simple single-pass equilibration
- **Barrier method with appropriate row-to-column ratios:** single-pass equilibration, provided attribute quality metrics are satisfactory
- **Large problems using the barrier method** (above threshold dimensions): a bound-range metric is computed; if the range is small, a quick validation mode is selected; otherwise iterative scaling is used
- **Default:** iterative Ruiz equilibration

If the mode parameter specifies an explicit mode (one through four), that mode is used directly.

**Phase 4 -- Scaling Algorithm Execution:**

One of four scaling strategies is executed:

*Strategy 1 -- Single-Pass Equilibration:*

A standard infinity-norm equilibration pass (Curtis and Reid, 1972). For each column, the maximum absolute value of the scaled coefficient (coefficient times the absolute row scale factor) is computed. The column scale factor is set to the reciprocal of this maximum. Geometric mean adjustments may be incorporated based on bound information. Scale factors are computed via logarithmic rounding to produce approximately power-of-base values. Row and column scale factors are clamped to a bounded range to prevent extreme scaling. Finally, the scales are applied to the matrix coefficients.

*Strategy 2 -- Quick Validation:*

Delegates to a specialized validation function that checks whether the current scaling (or lack thereof) is acceptable. This is a lightweight check that does not modify the matrix, used when the matrix is already well-conditioned.

*Strategy 3 -- Iterative Ruiz Equilibration:*

An iterative alternating scaling algorithm based on Ruiz (2001). The algorithm alternates between:

- **Row phase:** For each row, compute the infinity norm of the row in the currently scaled matrix. Set the new row scale to the reciprocal of this norm. On early iterations (the first few), bound information is incorporated via a geometric mean of the norm-based scale and the bound magnitude.
- **Column phase:** Similarly, compute column infinity norms and update column scales.

The iteration uses a dampening factor to limit the change in scale factors per iteration, preventing oscillation. On later iterations, dampening becomes more aggressive. The iteration terminates when either no significant changes occur (convergence) or a maximum iteration count (approximately ten) is reached. This matches the typical Ruiz iteration count for LP solvers (Ruiz, 2001).

After convergence, all scale factors are clamped to a bounded range and applied to the matrix coefficients.

*Strategy 4 -- Extended Restructure:*

[UNDETERMINED] This mode appears to perform full matrix restructuring with extended support for quadratic programming models. Its detailed behavior beyond what the other modes provide is not fully characterized. It uses different clamping bounds and convergence tolerances than the iterative mode.

**Phase 5 -- Bound and Special Constraint Adjustment:**

After scaling the constraint matrix, the function propagates the scaling factors to all related data:

1. **Row bounds:** Each row's lower and upper bounds (when finite) are divided by the corresponding row scaling factor.

2. **Right-hand side:** Each RHS value is multiplied by the row scaling factor.

3. **Column bounds:** Each column's bounds (when finite) are multiplied by the column scaling factor. If auxiliary bound arrays exist, they are scaled similarly.

4. **Objective scaling (global):** If the scaling hint from the environment indicates that objective-level scaling is needed (the hint value differs from unity), an objective scale factor is computed via logarithmic rounding. This factor is applied to all column scales (dividing), column bounds (dividing finite values), and row scales (multiplying), and the global scale factor is adjusted. This ensures consistent scaling between the constraint system and the objective function.

5. **Quadratic constraints:** If quadratic constraints exist, each quadratic term coefficient is multiplied by the global scale factor and by the row scale factors corresponding to the two variable indices of that term.

6. **Piecewise-linear constraints:** If piecewise-linear constraints exist, the X-coordinates of each breakpoint are scaled by the global scale times the row scale. The Y-coordinates are scaled by the global scale. Slopes are divided by the row scale, with special handling for the last slope of each constraint (which may be infinite).

**Global RHS Scale Computation:**

A supplementary computation builds a histogram of right-hand side magnitudes using buckets spaced by powers of ten. The highest and lowest non-empty buckets define the coefficient range. If this range exceeds three orders of magnitude, a geometric mean of the range endpoints is computed and used as an additional global scaling target. This is a standard technique for identifying the characteristic magnitude of the RHS vector.

**Phase 6 -- Storage and Cleanup:**

On successful scaling:
- The row scaling factor array, column scaling factor array, and global objective scale are stored on the MatrixData structure for later use by the solver and for solution unscaling
- Temporary workspace buffers are freed
- The matrix finalization helper is called to update internal state flags

On error:
- All allocated memory (scaling arrays, temporary buffers) is freed
- The error code is returned
- No partial scaling is applied to the matrix

**Thread Safety:** Unsafe. This function modifies the matrix data in place and is not internally synchronized. The caller must ensure exclusive access to the model during scaling. In practice, scaling is performed during model preparation before concurrent solve operations begin.

**Dependencies:**
- Matrix preparation helper (prepares internal matrix state before scaling)
- Matrix finalization helper (updates state flags after scaling)
- Scaling needs assessment (determines if scaling is required)
- Matrix integrity check (validates matrix consistency)
- Scaling validation function (used by strategy 2 for quick validation)
- Memory allocation (from the Memory Primitives module, P3.01)
- MatrixData structure (Layer 1 data model)
- Environment parameters: feasibility tolerance, scaling tolerance, scaling hint flags, solve method

---

## Module-Level Behavioral Notes

### Pipeline Architecture

cxf_finalize_row_data is a single function but embodies a six-phase pipeline that performs several logically distinct operations:

1. **Validation and early-exit decisions** (can terminate without any work)
2. **Array setup** (memory allocation for scaling factors)
3. **Analysis** (surveying matrix properties to choose a strategy)
4. **Core computation** (the selected scaling algorithm)
5. **Propagation** (applying scales to all dependent data)
6. **Persistence** (storing results and cleaning up)

The pipeline includes two fast-paths that can short-circuit the full computation:
- **Sense normalization only:** When the matrix needs only inequality sign-flipping, no full scaling is performed.
- **Saved scaling reuse:** When previously computed scaling factors match the current dimensions, they are reapplied without recomputation. This supports warm-starting scenarios where the same model is re-solved after parameter changes.

### Relationship to Matrix Core (P3.14)

The Matrix Core module (P3.14) and this module together handle all matrix data preparation:

- **Matrix Core** is responsible for structural operations: constructing the CSC and CSR representations, sorting indices, and building row-major data.
- **Matrix Finalization** is responsible for numerical preparation: scaling the coefficient matrix and propagating scale factors to bounds and special constraints.

cxf_finalize_row_data calls matrix preparation and finalization helpers that are likely defined in or closely associated with the Matrix Core module.

### Scaling Algorithm Literature

The scaling algorithms implemented in this module correspond to well-established techniques:

| Strategy | Literature Reference | Characteristics |
|----------|---------------------|-----------------|
| Single-pass equilibration | Curtis and Reid (1972); Tomlin (1975) | Fast, one sweep over the matrix; suitable for well-conditioned problems |
| Iterative Ruiz equilibration | Ruiz (2001), RAL-TR-2001-034 | Converges to doubly-stochastic scaling in the infinity norm; typically 5-25 iterations |
| Geometric mean scaling | Tomlin (1975) | Used as a component within the global RHS scale computation |
| Logarithmic rounding | Standard practice | Produces scaling factors that are approximate powers of a base, minimizing rounding artifacts |

### Constraint Sense Normalization

The fast-path normalization converts all greater-than-or-equal constraints to less-than-or-equal form by negating coefficients and swapping/negating bounds. This is consistent with the MatrixData internal storage convention (Layer 1 specification, Invariant 6), which states that all constraints are stored internally in less-than-or-equal form. The sign-bit flip technique (XOR with the IEEE 754 sign bit) is used rather than arithmetic negation, which is computationally cheaper and exactly preserves special values (infinity, NaN, negative zero).

### Scaling Factor Clamping

All computed scaling factors (both row and column) are clamped to a bounded range before application. The bounds are chosen to prevent extreme scale factors that would introduce more numerical instability than they resolve. The typical clamping range spans approximately twelve orders of magnitude (six below unity to six above). This is consistent with the MatrixData invariant that all scaling factors must be strictly positive.

### Quadratic and Piecewise-Linear Constraint Handling

When the model contains quadratic or piecewise-linear constraints, the scaling factors must be propagated to those constraints as well:

- **Quadratic terms** Q[i,j] are scaled by the product of the row (or variable) scale factors for indices i and j, plus the global scale factor. This ensures that the quadratic form x^T Q x transforms consistently with the linear constraints.
- **Piecewise-linear breakpoints** have their coordinates scaled to match the scaled variable space. Slopes are adjusted inversely, with care taken for infinite slopes (which represent vertical segments in the piecewise-linear function).

### Memory Layout

The row and column scaling factors are allocated as a single contiguous block: the first portion holds row scale factors (one per constraint) and the second portion holds column scale factors (one per variable). This single-allocation pattern reduces memory management overhead and improves cache locality during the scaling application passes.

### Thread Safety Summary

| Function | Thread Safety | Notes |
|----------|---------------|-------|
| cxf_finalize_row_data | Unsafe | Modifies matrix data in place; caller must ensure exclusive access |

---

## Verification Checklist

```
[x] No hex addresses or offsets
[x] No Ghidra artifacts (param_, local_, iVar, DAT_, FUN_)
[x] No binary-specific constants or magic numbers
[x] No copied code fragments
[x] All descriptions are behavioral, not implementational
[x] All data structures described semantically using Layer 1 types
[x] Passes the Clean Room Test
[x] Published algorithm references cited (Curtis/Reid, Tomlin, Ruiz, Maros)
```
