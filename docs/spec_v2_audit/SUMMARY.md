# Spec V2 Compliance Audit — Consolidated Summary

**Date:** 2026-03-13
**Auditors:** 10 parallel Claude Opus 4.6 agents (read-only)
**Scope:** Every implemented function checked against every spec V2 module

---

## Aggregate Stats

| Audit Area | Violations | Missing Fns | Compliant Fns |
|---|---|---|---|
| Simplex Iteration | 29 | 4 | 6 |
| Two-Phase & Lifecycle | 31 | 2 | 5 |
| Basis Operations | 15 | 7 | ~12 |
| Pricing | 21 | 1 | 6 |
| Data Model & Structures | 153 findings | 4 HIGH | 3 |
| Numerical Stability | 14 | 3 | ~15 |
| Matrix & Memory | 16 | 6 | 1 |
| API & Model | 44 | 25+ | 4 |
| Error/Callbacks/Logging | 18 | 13 | 4 |
| Threading/Validation/Misc | 27 | 22 | 3 |

**Total: ~200 violations, ~87 missing functions, ~59 compliant items**

---

## CRITICAL Violations (Bugs That Affect Solver Correctness)

### 1. step3 implied bound formula missing RHS term
- **File:** `src/simplex/step3.c:161-162`
- **Spec:** `x_k <= lb_k + (rhs - min_act) / a_ik`
- **Code:** `impl = lb - min_act / a` (no RHS subtraction)
- **Impact:** Wrong implied bounds → missed tightenings or incorrect infeasibility
- **Ref:** simplex_iteration.md V28

### 2. BFRT flips don't update constraint activities or negate row coefficients
- **File:** `src/simplex/step.c:613-640`
- **Spec:** harris_ratio_test.md Stage 3 step 6 requires activity updates + row negation
- **Code:** Only sets x values to bounds for flipped rows
- **Impact:** Constraint matrix inconsistent after BFRT flips
- **Ref:** simplex_iteration.md V12

### 3. pivot_primal modifies original model RHS instead of working copy
- **File:** `src/simplex/pivot_primal.c:185-211`
- **Spec:** "Working copies... may modify without affecting the original model data"
- **Code:** Updates `matrix->rhs` (original)
- **Impact:** Corrupts original problem data
- **Ref:** simplex_iteration.md V21

### 4. compute_phase1_objective missing feasibility tolerance
- **File:** `src/simplex/phase_loop.c:32-39`
- **Spec:** Violations measured as `x < lb - epsilon_feas`
- **Code:** Uses `x < lb` (no tolerance)
- **Impact:** Near-feasible vars contribute to Phase I obj → delayed Phase II transition
- **Ref:** two_phase_and_lifecycle.md V25

### 5. NaN step length not detected before pivot application
- **File:** `src/simplex/step.c:603`
- **Spec:** "After step length computation, before applying the pivot"
- **Code:** `stepSize > STEP_CLAMP` fails silently on NaN (NaN > anything = false)
- **Impact:** NaN propagates into x_B, corrupting all primal values
- **Ref:** numerical_stability.md V6

### 6. Pricing tolerance levels significantly wrong
- **File:** `src/simplex/step.c:270-272`
- **Spec:** Fast=1e-6, Standard=1e-10, Aggressive=1e-9
- **Code:** Fast=1e-5, Standard=1e-6, Aggressive=1e-7
- **Impact:** Standard fallback (for difficulty) provides no tightening at all
- **Ref:** numerical_stability.md V3

### 7. EtaVector.next linked list direction may be wrong
- **File:** `include/convexfeld/cxf_basis.h:63`
- **Spec:** next points to OLDER eta (prepend at head)
- **Code:** Comment says "Link to next eta (newer)"
- **Impact:** If runtime behavior matches comment, FTRAN/BTRAN traversal order is wrong
- **Ref:** data_model.md V104

---

## HIGH-Priority Violations (Spec Deviations Affecting Behavior)

### Simplex Iteration
- **V5** Stall detection logic may be inverted (> vs < threshold)
- **V29** Cycling uses Bland's rule instead of spec's basis-snapshot + perturbation

### Two-Phase & Lifecycle
- **V13** cxf_simplex_final is a deallocator, not the spec's variable-fixing function
- **V30** Proactive perturbation on first 2 iterations (unconditional, not stalling-triggered)
- **V7** cxf_simplex_cleanup (post-solve bound propagation) never called

### Basis Operations
- **V1-V4** cxf_pivot_with_eta has completely different abstraction level from spec
- **V12** cxf_basis_refactor (constraint-driven variable fixing) not implemented
- **V13** cxf_basis_warm reused for different purpose (basis restore, not Q-matrix eta)

### Pricing
- **V18** cxf_pricing_invalidate is a cache manager, not a single-var dirty marker
- **V3/V4** Constraint candidate full scan has no status filtering at all
- **V15-V17** Eta vector traversal mode entirely absent from all producer functions

### Data Model
- **V29** SolverState.varStatus/basisHeader on BasisState only (spec: both)
- **V53** Model has one matrix pointer (spec requires primary + working)
- **V93** Only pivot eta variant exists (VARIABLE_FIX and WARM_START missing)

### Numerical Stability
- **V2** step2/step3 use 1e-9 instead of spec's 1e-13 for bound propagation
- **V9** No consecutive small pivot refactorization trigger
- **V12** No Kahan compensated summation anywhere

### Matrix & Memory
- **V4** cxf_prepare_row_data is a CSR allocator, not the spec's unscaling pipeline
- **V8** cxf_vector_free frees a VectorContainer, not the spec's Model destructor
- **V6/V7** cxf_calloc/cxf_realloc lack environment parameter and memory tracking

### API & Model
- **V29** Parameter system: 7 of ~150 parameters, no table-driven architecture
- **V7** cxf_updatemodel is a stub returning NOT_SUPPORTED
- **V10/V11** cxf_optimize lifecycle is a trivial wrapper
- **V39** modification_blocked flag never set during optimization

### Error/Callbacks/Logging
- **V8** cxf_init_callback_struct does memset instead of mutex allocation
- **V15** cxf_log replaced by cxf_log_printf (missing 5 of 5 output destinations)
- **V10/V11** cxf_pre/post_optimize_callback missing entirely

### Threading/Validation/Misc
- **V12/V13** cxf_check_model_flags1/2 check wrong things entirely
- **V10/V11** cxf_check_nan/cxf_is_finite have array signatures instead of scalar
- **V24** cxf_propagate_bounds uses variable worklist instead of constraint worklist

---

## Systemic Patterns

### 1. Name Collisions / Semantic Mismatches
Functions reuse spec names for completely different behavior:
- `cxf_simplex_final` (deallocator vs variable fixer)
- `cxf_basis_warm` (basis restore vs Q-matrix eta)
- `cxf_basis_refactor` (eta clear vs constraint-driven fixing)
- `cxf_vector_free` (VectorContainer free vs Model destructor)
- `cxf_pricing_invalidate` (cache manager vs dirty marker)
- `cxf_check_model_flags1/2` (MIP/QP check vs active optimization/dual data)

### 2. Missing Infrastructure Layers
- **Parameter table system** — only 7 hardcoded params vs spec's ~150 table-driven
- **Attribute table system** — strcmp if/else vs spec's hash map with pointer wiring
- **Memory tracking** — plain malloc/calloc vs spec's environment-scoped tracking
- **Logging system** — printf stub vs spec's 5-destination, reentrant, line-buffered system
- **Statistics/Diagnostics** — entire module (7 functions) unimplemented
- **State cleanup** — most cleanup functions missing or wrong

### 3. Data Location Mismatches
Spec distributes fields differently across structures:
- obj/bounds/vtype: spec → MatrixData, code → Model
- varStatus/basisHeader: spec → SolverState + BasisState, code → BasisState only
- progressSnapshot: spec → BasisState, code → SolverState
- solution data: spec → WorkArrays, code → Model
- callback function: spec → Environment, code → CallbackContext

### 4. Scope Gaps (Expected for LP-Only)
- No dual simplex
- No barrier / concurrent / PDHG
- No MIP / QP / SOCP / PWL / SOS
- No licensing / compute server / WLS
- No presolve (full) / crossover
- No multi-scenario / solution pool

---

## What IS Compliant

### Core Solver (strongest compliance)
- LU factorization (Markowitz, threshold pivoting, growth monitoring)
- BTRAN/FTRAN (eta application, hyper-sparse skip)
- Eta arena allocator (bump allocation, exponential growth)
- Harris two-pass ratio test (relaxed/strict passes, Bland's tie-breaking)
- Phase I w-coefficients (dynamic, bound-violation approach)
- Reduced cost computation (BTRAN + CSC dot product)
- Activity bound maintenance (cancellation-safe updates)
- Refactorization check (adaptive eta threshold)
- All 10 tolerance #defines match spec values exactly

### Partial Compliance
- Crash basis (2 medium deviations)
- BFRT extend step (cap at 10, otherwise correct)
- step2/step3 bound propagation (formula correct, thresholds wrong)
- EXPAND perturbation (mechanism A+B present, activation heuristic differs)
- PricingState structure (all V2 fields present)
- Queue insertion protocol (flag encoding correct)

---

## Detailed Reports

Each audit file is in `docs/spec_v2_audit/`:

1. `simplex_iteration.md` — 29 items, 3 HIGH
2. `two_phase_and_lifecycle.md` — 31 items, 5 HIGH
3. `basis_operations.md` — 15 items + 7 missing, 2 CRITICAL
4. `pricing.md` — 21 items + 1 missing, 1 CRITICAL
5. `data_model.md` — 153 items, 4 HIGH
6. `numerical_stability.md` — 14 items + 3 missing
7. `matrix_and_memory.md` — 16 items + 6 missing, 2 CRITICAL
8. `api_and_model.md` — 44 items + 25 missing, 6 CRITICAL
9. `error_callbacks_logging.md` — 18 items + 13 missing, 3 CRITICAL
10. `threading_validation_misc.md` — 27 items + 22 missing
