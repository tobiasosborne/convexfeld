# Spec V2 Audit: Basis Operations

Auditor: Claude Opus 4.6
Date: 2026-03-13
Scope: LU factorization, FTRAN/BTRAN, eta updates, refactorization

## Files Reviewed

### Spec Files
- `docs/specs-v2/specs/modules/basis_operations.md` (P3.16)
- `docs/specs-v2/specs/modules/pivot_operations.md` (P3.19)
- `docs/specs-v2/specs/algorithms/product_form_inverse.md` (P2.1)
- `docs/specs-v2/specs/reference/numerical_stability.md`

### Implementation Files
- `src/basis/lu_factorize.c` — Sparse Markowitz LU factorization
- `src/basis/lu_factors.c` — LUFactors lifecycle
- `src/basis/lu_output.c` — COO-to-CSC assembly
- `src/basis/sparse_work.c` — Sparse working matrix for LU
- `src/basis/sparse_elim.c` — Markowitz pivot search + elimination
- `src/basis/dense_elim.c` — Dense phase elimination
- `src/basis/ftran.c` — Forward transformation
- `src/basis/btran.c` — Backward transformation
- `src/basis/btran_etas.c` — Shared BTRAN/FTRAN helpers
- `src/basis/eta_factors.c` — EtaVector lifecycle
- `src/basis/pivot_eta.c` — PFI pivot update
- `src/basis/refactor.c` — Refactorization driver
- `src/basis/basis_state.c` — BasisState lifecycle
- `src/basis/warm.c` — Validation and warm start
- `src/basis/snapshot.c` — BasisSnapshot lifecycle
- `src/basis/basis_stub.c` — Progress snapshot + diff
- `src/basis/eta_pool.c` — Arena allocator for eta vectors
- `src/basis/basis_internal.h` — Internal declarations
- `include/convexfeld/cxf_types.h` — Tolerance constants

## Compliant Functions

### LU Factorization (product_form_inverse.md Step 1)
- `cxf_lu_factorize` — Implements sparse Markowitz-ordered Gaussian elimination with threshold pivoting, dense phase transition, and growth factor monitoring. Compliant with P2.1 Step 1 and numerical_stability.md Section D.
- `cxf_lu_create` / `cxf_lu_free` / `cxf_lu_clear` — Standard lifecycle, no spec requirement beyond existence.
- `build_lu_output` — Internal COO-to-CSC conversion, no direct spec.
- `sparse_work_create/extract/free/density` — Internal helpers, compliant with Suhl & Suhl approach.
- `sparse_find_pivot` — Markowitz criterion with relative tie-breaking per Suhl & Suhl. Compliant.
- `sparse_eliminate` — Right-looking Gaussian elimination. Compliant.
- `dense_find_pivot` / `dense_eliminate` — Dense phase Markowitz. Compliant.

### Tolerances (numerical_stability.md Section D, P5.3)
- `CXF_MIN_PIVOT = 1e-13` — Matches spec "approximately 1e-13" for bound propagation.
- `CXF_PIVOT_TOL = 1e-9` — Matches spec "approximately 1e-9" Harris pivot tolerance.
- `CXF_MARKOWITZ_TOL = 0.0078125` (1/128) — Matches spec "approximately 7.8e-3, i.e., 1/128".
- `CXF_ZERO_TOL = 1e-12` — Reasonable drop tolerance, no specific spec value.
- `GROWTH_LIMIT = 1e8` — Matches spec "approximately 1e8 to 1e10".
- `DENSE_THRESHOLD = 0.4` — No specific spec value; reasonable engineering choice.

### BTRAN (product_form_inverse.md Step 4)
- `cxf_btran` / `cxf_btran_vec` — Applies etas in reverse order (newest to oldest), then LU transpose solve. Order is correct per spec Step 4.
- `btran_apply_etas` — Implements BTRAN eta formula: `result[p] = (result[p] - dot) / pivot_elem`. Compliant with spec formula `sigma = y_{p_i} * eta_{p_i} + sum_{j != p_i} y_j * eta_j; y_{p_i} = sigma`.
- `btran_apply_lu` — Applies Q permutation, U^T forward sub, L^T backward sub, P^T output. Correct transpose solve order.
- Hyper-sparse optimization (Hall 2005): skip when pivot position and dot product both zero. Compliant with spec Step 4.3.

### Eta Pool (product_form_inverse.md Memory Management)
- `cxf_eta_pool_create/alloc/reset/free` — Bump-pointer arena with exponential chunk growth. Compliant with spec "region-based memory pool using bump allocation" and "freed in bulk during refactorization."
- `cxf_eta_list_clear` — Resets pool or frees chain. Resets eta_count, eta_head, pivots_since_refactor. Compliant with P2.1 Step 6 (clear eta chain, reset counts).

### BasisState Lifecycle
- `cxf_basis_create/free/init` — Standard lifecycle. Compliant.

### Validation and Warm Start
- `cxf_basis_validate` / `cxf_basis_validate_ex` — Bounds and duplicate checking. No direct spec requirement but reasonable.
- `cxf_basis_warm` / `cxf_basis_warm_snapshot` — Copies basis header, clears etas. Compliant.

### Refactorization Check
- `cxf_refactor_check` — Adaptive eta count threshold `min(100, max(50, m/4))`. Matches spec numerical_stability.md Section A. Checks memory, interval, and FTRAN degradation. Compliant.

## VIOLATIONS

### [V1] cxf_pivot_with_eta — Signature deviates from spec

- **Spec says:** `cxf_pivot_with_eta(env, state, direction, leavingRow, enteringVar, leavingVar, pivotElement, etaRowLen, etaColLen) -> int`. Takes 9 parameters including Environment pointer, SolverState, direction, pivot element as scalar, and row/column length hints. Creates eta from CSR/CSC matrix data. Stores eta entries as `-coefficient / pivotElement` (the negated, scaled transformation).
- **Code does:** `cxf_pivot_with_eta(basis, pivotRow, pivotCol, enteringVar, leavingVar, leavingStatus) -> int`. Takes 6 parameters: BasisState (not SolverState), a pre-computed pivot column vector (not CSR/CSC extraction), and leavingStatus instead of direction. Stores raw pivot column values directly, not the `-coeff/pivot` transformation.
- **File:** `src/basis/pivot_eta.c:47-48`
- **Impact:** The function operates at a completely different abstraction level. The spec describes a function that extracts eta data from the constraint matrix CSR/CSC representation using the entering variable's row and the leaving row's column. The implementation takes an already-FTRAN'd pivot column and stores it directly, delegating the `-d_i/alpha` computation to FTRAN/BTRAN application time instead of eta creation time.

### [V2] cxf_pivot_with_eta — Eta value storage: raw vs. pre-computed

- **Spec says (P2.1 Step 2):** Eta vector entries are `eta_i = -d_i / alpha` for `i != p` and `eta_p = 1 / alpha`. The spec explicitly states: "The eta coefficient is computed as: -coefficient / pivotElement."
- **Code does:** Stores `pivotCol[i]` directly (raw column values) and stores `pivot_elem = pivot` (the actual pivot, not reciprocal). FTRAN applies: `factor = result[r] / pivot_elem; result[j] -= values[k] * factor`. BTRAN applies: `result[p] = (result[p] - dot) / pivot_elem`.
- **File:** `src/basis/pivot_eta.c:127` (storage), `src/basis/ftran.c:86-91` (FTRAN application), `src/basis/btran_etas.c:118-119` (BTRAN application)
- **Impact:** Mathematically equivalent. Storing raw values and dividing at apply-time produces the same result as pre-computing `-d_i/alpha` and multiplying at apply-time. This is an implementation choice deviation, not a correctness bug. However, it means the eta vector format does not match the spec's explicit P1.08 Variant 1 definition.

### [V3] cxf_pivot_with_eta — Missing CSC column data extraction

- **Spec says:** "If etaColLen is greater than zero (needed for dual simplex operations), the function additionally scans the leaving row's column in the CSC matrix representation" and stores column data for dual simplex support.
- **Code does:** No column data extraction. No CSC scanning. The EtaVector structure has no column data fields. There is no dual simplex support through eta column data.
- **File:** `src/basis/pivot_eta.c` (entire file)
- **Impact:** Dual simplex operations that rely on eta column data are unsupported. This is a missing feature, not just a deviation.

### [V4] cxf_pivot_with_eta — Missing reduced cost storage

- **Spec says:** "The eta vector stores the leaving row, entering variable, leaving variable, pivot element, the entering variable's reduced cost, the pivot direction."
- **Code does:** Stores pivot_row, pivot_var (entering), pivot_elem, and nnz. Sets `obj_coeff = 0.0` and `status = 0`. Does not store the entering variable's reduced cost or the pivot direction. Does not store the leaving variable in the eta vector (only updates basis arrays).
- **File:** `src/basis/pivot_eta.c:90-97`
- **Impact:** The eta vector is missing information that the spec says it should contain. This could affect crossover, warm-start, or eta chain replay operations.

### [V5] cxf_pivot_with_eta — Missing work counter updates

- **Spec says:** "The work counter has been updated based on the number of matrix entries processed."
- **Code does:** No work counter update anywhere in the function.
- **File:** `src/basis/pivot_eta.c` (entire file)
- **Impact:** Performance monitoring and stall detection may be inaccurate.

### [V6] cxf_pivot_with_eta — Missing total eta count vs. row eta count distinction

- **Spec says:** "The total eta count and row eta count have each been incremented by one."
- **Code does:** Increments `basis->eta_count` only. No separate row-type eta count.
- **File:** `src/basis/pivot_eta.c:137`
- **Impact:** The spec distinguishes between total etas and row-type etas. The implementation tracks only one count.

### [V7] FTRAN eta application — Formula deviation

- **Spec says (P2.1 Step 3):** "For each nonzero entry (j, eta_j) in the eta vector where j != p_i: x_{i,j} = x_{i-1,j} + eta_j * tau" where `eta_j = -d_j / alpha`. So the application is `result[j] += eta_j * tau = (-d_j/alpha) * result[p]`.
- **Code does:** `factor = result[r] / pivot_elem; result[r] = factor; result[j] -= values[k] * factor` where values[k] = d_j (raw column). So `result[j] -= d_j * (result[r] / alpha)` = `result[j] += (-d_j/alpha) * result[r]`.
- **File:** `src/basis/ftran.c:86-91`
- **Impact:** Mathematically equivalent. The code correctly computes the same transformation as the spec despite storing raw values. NOT a correctness violation, but the intermediate storage format differs from spec.

### [V8] cxf_basis_snapshot / cxf_basis_diff — Signature and semantics deviate from spec

- **Spec says (P3.16 cxf_basis_snapshot):** `cxf_basis_snapshot(state, snapshot) -> void` where `snapshot` is a pointer-to-array-of-int output buffer of SNAPSHOT_SIZE. Copies SNAPSHOT_SIZE counters from SolverState into the buffer.
- **Code does:** `cxf_progress_snapshot(state) -> void` with no output buffer parameter. Stores counters into `state->progress_snapshot[]` array directly.
- **File:** `src/basis/basis_stub.c:54-67`
- **Impact:** Function name and signature differ. Behavior is equivalent (stores counters for later comparison), but the interface is different. The spec has the snapshot as a separate buffer; the implementation uses an in-state array.

### [V9] cxf_basis_diff — Signature and return type deviation

- **Spec says (P3.16 cxf_basis_diff):** `cxf_basis_diff(state, snapshot) -> double` takes both the current state AND a snapshot buffer.
- **Code does:** `cxf_basis_diff(state) -> double` takes only the state (uses `state->progress_snapshot[]` internally).
- **File:** `src/basis/basis_stub.c:80`
- **Impact:** Interface deviation. The snapshot is always the most recent one stored in state, not an arbitrary external snapshot.

### [V10] cxf_basis_diff — Scoring formula deviates from spec

- **Spec says:** Six terms: (1) structural change normalized by total nnz with heavy weight, (2) column reduction normalized by colDenom with unit weight, (3) iteration counters aggregating 4 counters normalized by colDenom with light weight, (4) row statistics aggregating 5 counters normalized by rowDenom with unit weight, (5) conversion term normalized by rowDenom with moderate weight, (6) work counter normalized by rowDenom with its own weight.
- **Code does:** Six terms but with different structure: (1) structural = 4.0*(d_cols+d_rows)/colDenom, (2) iteration = 0.25*(d_iter+d_piv+d_flips)/colDenom, (3) bound propagation = 1.0*d_props/rowDenom, (4) FTRAN effort = 0.5*d_ftran/rowDenom, (5) perturbation = 2.0*d_perturb/colDenom, (6) degeneracy = 0.1*d_degen/colDenom.
- **File:** `src/basis/basis_stub.c:117-136`
- **Impact:** The scoring formula is substantially different. The spec's Term 1 normalizes by total nnz (not colDenom). The spec tracks variable-fixing counter specifically; the code merges d_cols+d_rows. The spec's Term 4 aggregates five row-related counters; the code has only d_props. The spec has a conversion term and a work counter term that don't directly map. The weights are custom. The normalization denominators use different formulas: spec's rowDenom = (active constraint count + matrix status change count + bounds-processing change count); code's rowDenom = (m - snap_rows).

### [V11] cxf_basis_diff — Missing counters

- **Spec says:** Tracks: variable-fixing counter, removed columns adjusted by added variable count, basis membership changes, pricing operations, bound-type conversions, candidate evaluations, removed rows, matrix status transitions, bounds-processing work, two per-constraint counters, inequality-to-equality conversions, and per-iteration work metric.
- **Code does:** Tracks: iteration, pivots_since_refactor, ftran_count, rows_eliminated, cols_eliminated, bounds_propagated, flip_count, phase, degenerate_count, perturb_count. Many spec counters are absent or mapped differently.
- **File:** `src/basis/basis_stub.c:87-96`
- **Impact:** The progress detection mechanism tracks different signals than what the spec describes. This could affect anti-cycling trigger sensitivity.

### [V12] cxf_basis_refactor — Name collision, completely different semantics

- **Spec says (P3.16 cxf_basis_refactor):** A constraint-driven variable-fixing function that scans constraints to identify fixable variables, creates eta records, updates objective, notifies pricing. It does NOT perform LU refactorization.
- **Code does:** The function `cxf_fix_variables_at_bounds` in refactor.c is the closest match, but it only clears the eta list — it does not scan constraints, fix variables, create eta records, or update the objective. The actual LU refactorization is `cxf_solver_refactor`.
- **File:** `src/basis/refactor.c:44-53`
- **Impact:** The spec's `cxf_basis_refactor` (constraint-driven variable fixing) is NOT IMPLEMENTED. The code has a stub that just clears etas. This is a major missing feature.

### [V13] cxf_basis_warm — Completely different semantics

- **Spec says (P3.16 cxf_basis_warm):** Creates a quadratic warm-start eta vector recording Q-matrix contributions for a variable being fixed. Takes `(env, state, varIndex, varValue) -> int`.
- **Code does:** `cxf_basis_warm(basis, basic_vars, m) -> int` copies basic variable indices and clears the eta list. This is a basis warm-start restore function, not a Q-matrix eta creation function.
- **File:** `src/basis/warm.c:188-203`
- **Impact:** The spec's quadratic objective warm-start eta creation function is NOT IMPLEMENTED. The code reuses the name for a completely different purpose.

### [V14] cxf_progress_snapshot_create / cxf_progress_snapshot_diff — Snapshot functions have wrong semantics

- **Spec says (P3.16 cxf_basis_snapshot):** Captures only scalar iteration counters (SNAPSHOT_SIZE integers). "Does not capture the actual basis."
- **Code does:** `cxf_progress_snapshot_create` in snapshot.c captures the FULL basis state (basisHeader array, varStatus array, iteration number). This is a heavy snapshot, not a lightweight counter snapshot.
- **File:** `src/basis/snapshot.c:34-80`
- **Impact:** Two separate snapshot mechanisms exist: the lightweight `cxf_progress_snapshot` in basis_stub.c (roughly matches spec) and the heavy `cxf_progress_snapshot_create` in snapshot.c (does not match spec). The naming is confusing. The spec's cxf_basis_snapshot maps to `cxf_progress_snapshot`, not to `cxf_progress_snapshot_create`.

### [V15] cxf_progress_snapshot_diff — Wrong semantics vs. spec cxf_basis_diff

- **Spec says (P3.16 cxf_basis_diff):** Returns a weighted, normalized progress SCORE (a double).
- **Code does:** `cxf_progress_snapshot_diff` in snapshot.c returns an integer COUNT of differing elements between two full basis snapshots. The actual weighted score is in `cxf_basis_diff` in basis_stub.c.
- **File:** `src/basis/snapshot.c:92-120`
- **Impact:** Two diff functions exist with different semantics. The spec's function is implemented as `cxf_basis_diff` (roughly), not as `cxf_progress_snapshot_diff`.

### [V16] FTRAN — Missing hyper-sparse entry-level skip

- **Spec says (P2.1 Step 3.3):** "If tau = 0 for a given eta vector (the value at the pivot position is zero), the entire eta vector application is skipped."
- **Code does:** `if (result[eta->pivot_row] == 0.0) continue;` — This IS implemented.
- **RETRACTED** — This is actually compliant.

### [V17] Eta creation via pivot_eta.c — Drop tolerance uses CXF_ZERO_TOL (1e-12) vs. spec MIN_PIVOT (1e-13)

- **Spec says (P2.1 Step 2):** Entries are filtered to "only those corresponding to active basic variables" with nonzero values.
- **Code does:** Drops entries with `fabs(pivotCol[i]) <= CXF_ZERO_TOL` (1e-12).
- **File:** `src/basis/pivot_eta.c:73,125`
- **Impact:** Minor. The drop tolerance for eta entries (1e-12) is larger than the LU factorization drop tolerance (1e-13). Entries between 1e-13 and 1e-12 are kept in LU but dropped in etas. The spec doesn't specify a numeric threshold for eta entry dropping, so this is a judgment call rather than a strict violation.

### [V18] refactor.c cxf_solver_refactor — Does not reset eta_pool

- **Spec says (P2.1 Step 6):** "Clear the eta chain. Set the eta list head to null and reset all eta counts to zero. Free eta memory. The entire eta memory pool is released at once."
- **Code does:** Calls `clear_eta_list(basis)` which does `cxf_eta_pool_reset` (keeps first chunk) and resets head/count/pivots. Also resets `ctx->eta_count`, `ctx->eta_memory`, FTRAN stats.
- **File:** `src/basis/refactor.c:74-83`
- **Impact:** Compliant in spirit. `cxf_eta_pool_reset` retains the first chunk for reuse (an optimization over freeing everything), which is a reasonable deviation from "released at once." The first chunk is reused, not leaked.

### [V19] cxf_solver_refactor — Resets total_ftran_time and ftran_count unconditionally

- **Spec says:** No specification of what performance counters to reset at refactorization.
- **Code does:** Zeros `total_ftran_time` and `ftran_count` at every refactorization.
- **File:** `src/basis/refactor.c:80-81`
- **Impact:** Not a violation per se, but `cxf_refactor_check` uses these counters for FTRAN degradation detection. Resetting them means degradation is only measured since last refactor, which seems correct.

## Missing Functions (in spec but not implemented)

### M1. cxf_basis_refactor (P3.16) — Constraint-driven variable fixing
The spec describes a complex multi-phase function that scans constraints, identifies fixable variables, creates eta records, updates objectives, and notifies pricing. The implementation has only a stub (`cxf_fix_variables_at_bounds`) that clears the eta list.

### M2. cxf_basis_warm (P3.16) — Quadratic warm-start eta creation
The spec describes Q-matrix contribution recording for variables with quadratic objectives. The implementation reuses the name for a completely different function (basis restore from saved state).

### M3. cxf_pivot_check (P3.19) — Step length bound computation
Not found in any basis source file. May be implemented elsewhere in the codebase.

### M4. cxf_pivot_bound (P3.19) — Variable fixing with eta + objective update
Not found in any basis source file. May be implemented elsewhere.

### M5. cxf_pivot_primal (P3.19) — Primal criterion variable elimination
Not found in any basis source file. May be implemented elsewhere.

### M6. cxf_pivot_special (P3.19) — Unboundedness/row elimination/bound flip
Not found in any basis source file. May be implemented elsewhere.

### M7. cxf_pivot_update (P3.19) — Incremental activity bound maintenance
Not found in any basis source file. May be implemented elsewhere.

*Note: M3-M7 are from the pivot_operations module (P3.19), which may be implemented in simplex-layer files rather than in src/basis/. These are listed for completeness but may not be missing from the project.*

## Extra Functions (in code but not in spec)

### E1. cxf_btran_vec (btran.c:92)
BTRAN variant accepting an arbitrary input vector instead of a unit vector. Not in the spec, but a natural extension needed for computing dual prices (pi = B^(-T) * c_B).

### E2. cxf_eta_create / cxf_eta_free / cxf_eta_init / cxf_eta_validate / cxf_eta_set (eta_factors.c)
EtaVector lifecycle functions using individual calloc allocation. These appear to be an older API predating the arena allocator. The spec describes eta allocation through the memory pool only. These functions are still in use as a fallback when `eta_pool == NULL`.

### E3. cxf_basis_validate / cxf_basis_validate_ex (warm.c)
Basis validation with configurable check flags. Not specified in the V2 spec but reasonable utility functions.

### E4. cxf_basis_warm_snapshot (warm.c:219)
Warm start from a full BasisSnapshot. Not specified under this name in V2 spec.

### E5. cxf_progress_snapshot_create / cxf_progress_snapshot_diff / cxf_progress_snapshot_equal / cxf_progress_snapshot_free (snapshot.c)
Full basis snapshot functions (heavy copy of basisHeader + varStatus). These are separate from and additional to the lightweight progress counter snapshot mechanism. Not in the V2 spec.

### E6. cxf_lu_create / cxf_lu_clear (lu_factors.c)
LU factor lifecycle functions. The spec mentions LU factorization but does not specify lifecycle API signatures.

## Notes

### N1. Architectural difference: pivot column storage vs. matrix extraction
The most significant architectural difference is in cxf_pivot_with_eta. The spec describes a function that extracts eta data from the CSR/CSC constraint matrix at pivot time. The implementation receives a pre-computed FTRAN'd pivot column. This means the implementation is tightly coupled with the FTRAN output rather than with the constraint matrix representation. The mathematical result is equivalent, but the data flow is fundamentally different.

### N2. Two snapshot mechanisms coexist
The codebase has two separate snapshot systems:
1. Lightweight: `cxf_progress_snapshot` + `cxf_basis_diff` (basis_stub.c) — roughly matches spec
2. Heavy: `cxf_progress_snapshot_create` + `cxf_progress_snapshot_diff` (snapshot.c) — full basis copy

Only the lightweight mechanism matches the spec. The heavy mechanism appears to be used for debugging/warm-start purposes and is an additional feature.

### N3. EtaVector type codes
The spec mentions three variants: Variant 1 (PIVOT), Variant 2 (VARIABLE_FIX), Variant 3 (WARM_START). The code uses `type = 1` for "refactorization" and `type = 2` for "pivot". The mapping does not directly correspond. There is no type for VARIABLE_FIX or WARM_START eta records, because cxf_basis_refactor (variable fixing) and cxf_basis_warm (Q-matrix recording) are not implemented.

### N4. LU storage format
The LU factors store L in CSC (column-compressed) with columns indexed by elimination step, and U in CSC (row-of-step-compressed) with entries converted to step space via inverse permutations. This is a valid but implementation-specific choice. The spec does not prescribe a specific storage format for LU factors.

### N5. Refactorization trigger — missing residual monitoring
The spec (numerical_stability.md Section A) lists residual monitoring as the "most robust trigger" for refactorization. The code in `cxf_refactor_check` checks eta count, memory, interval, and FTRAN timing — but not residual magnitude. However, `ctx->thresholds[5]` appears to store a residual-triggered adaptive reduction, suggesting residual monitoring exists elsewhere (likely in step.c per MEMORY.md).

### N6. Missing compensated summation
The spec (numerical_stability.md Section B) recommends Kahan compensated summation for long accumulations. No compensated summation is visible in any basis file. This is a quality-of-implementation concern rather than a strict algorithmic violation.

### N7. Pivot column NaN/Inf detection
The spec (numerical_stability.md Section C) requires NaN/Inf detection after FTRAN results. `cxf_pivot_with_eta` checks `isfinite(pivot)` for the pivot element, but does not check the entire pivot column. FTRAN/BTRAN results are not checked for NaN/Inf in ftran.c or btran.c.
