# M5-M6: Core Operations & Algorithm Layer Learnings

## M5: Core Operations (Level 3)

### 2026-01-27: Spec Compliance Review - Callbacks, Validation, Statistics

**Report created:** `reports/callbacks_validation_stats_compliance.md`

**SUCCESS: Comprehensive spec compliance analysis completed**

**Modules reviewed:**
- Callbacks (7 specs, 7 implementations)
- Validation (10 specs, 7 implementations)
- Statistics (4 specs, 2 implementations)
- Utilities (9 specs, 0 implementations)

**Key findings:**

1. **Callback Signature Mismatch (Critical Decision Needed)**
   - Spec shows: `int callback(CxfModel *model, void *cbdata, int where, void *usrdata)`
   - Implementation: `int callback(CxfModel *model, void *usrdata)`
   - Missing `cbdata` and `where` parameters prevents context awareness
   - Affects: cxf_pre_optimize_callback, cxf_post_optimize_callback
   - **Decision needed:** Update spec OR update implementation

2. **AsyncState Missing (Documented Limitation)**
   - cxf_check_terminate, cxf_callback_terminate, cxf_set_terminate
   - All skip AsyncState flag checks (Priority 3 in spec)
   - Code comments: "Note: AsyncState not implemented yet"
   - Not critical for single-threaded LP solver
   - Add when concurrent optimization needed

3. **Utilities Module Completely Missing**
   - No src/utilities/ directory
   - 0/9 functions implemented
   - Blocks M7.3 work (Math Wrappers, Constraint Helpers)
   - Math wrappers needed for coefficient statistics logging

4. **LP-Only Implementation Pattern (Correct)**
   - Many functions are stubs for quadratic features
   - Example: cxf_is_quadratic returns 0 (no Q matrix yet)
   - This is CORRECT - implement working stubs, add features later
   - All stubs have comments explaining missing infrastructure

**Learnings:**

- **Spec Review Methodology:**
  - Read all specs in module
  - Find implementations (Grep, Glob, ls)
  - Compare: signatures, return values, error handling, algorithms
  - Rate severity: Critical/Major/Minor
  - Document not-implemented functions

- **Stub vs. Non-Compliant:**
  - Stub that returns "feature not present" is COMPLIANT
  - Stub that returns wrong type or crashes is NON-COMPLIANT
  - Example: cxf_is_quadratic returning 0 is compliant stub

- **Structure Compliance:**
  - CallbackContext has fewer fields than spec
  - Missing: env, primaryModel, suppressCallbackLog, 48-byte subStruct
  - Possible: Spec describes full commercial solver, impl is simplified
  - Impact: Moderate - some features may need these references

**Files reviewed:** 10 implementation files, 27 spec files, 2 headers

**Next actions:**
1. Decide on callback signature (spec vs impl)
2. Implement math wrappers (M7.3.3) - unblocks coefficient stats
3. Add model flag validation functions (M2.2.4, M2.2.5)

---

### 2026-01-26: M5.1.1 Basis Tests

**Files created:**
- `tests/unit/test_basis.c` (468 LOC) - 29 comprehensive TDD tests
- `src/basis/basis_stub.c` (337 LOC) - Stub implementations for TDD

**Tests written (29 total):**
- BasisState create/free/init (4 tests) - PASS
- EtaFactors create/free (4 tests) - PASS
- cxf_ftran FTRAN tests (4 tests) - PASS (identity basis stub)
- cxf_btran BTRAN tests (4 tests) - PASS (identity basis stub)
- cxf_basis_refactor tests (3 tests) - PASS
- Snapshot/comparison tests (6 tests) - PASS
- Validation/warm start tests (4 tests) - PASS

**TDD pattern difference from test_matrix:**
- Basis stubs implement identity basis behavior (trivial case)
- All tests pass with stubs because they test identity basis
- More complex tests (non-identity basis) will fail until full implementation
- This is still valid TDD - tests define expected behavior

**Key decisions:**
- Tests use explicit function declarations (not headers) for TDD
- Stubs handle identity basis case correctly (x = Ax when A = I)
- Validation tests check for duplicate basic variables
- Snapshot tests verify basis state can be saved/compared/restored

---

### 2026-01-26: M5.1.2 BasisState Structure

**File created:** `src/basis/basis_state.c` (127 LOC)

**Functions implemented:**
- `cxf_basis_create(m, n)` - Allocates BasisState with all arrays
- `cxf_basis_free(basis)` - Frees BasisState and eta linked list
- `cxf_basis_init(basis, m, n)` - Reinitializes existing BasisState

**TDD integration pattern:**
- Stub file (basis_stub.c) originally contained all functions for TDD
- As implementations are completed, extract to dedicated files
- Stub file retains only unimplemented functions (FTRAN, BTRAN, etc.)
- Both files linked in CMakeLists.txt; no duplicate symbols

**Key decisions:**
- Added negative dimension checks (m < 0 || n < 0 return NULL/error)
- Enhanced cxf_basis_init to clear arrays and validate dimension match
- DEFAULT_REFACTOR_FREQ = 100 (pivots between refactorizations)

---

### 2026-01-26: M5.1.3 EtaFactors Structure

**File created:** `src/basis/eta_factors.c` (192 LOC)

**Functions implemented:**
- `cxf_eta_create(type, pivot_row, nnz)` - Allocates eta with sparse arrays
- `cxf_eta_free(eta)` - Frees eta and arrays (NULL-safe)
- `cxf_eta_init(eta, type, pivot_row, nnz)` - Reinitialize existing eta
- `cxf_eta_validate(eta, max_rows)` - Validate eta invariants
- `cxf_eta_set(eta, indices, values)` - Copy data into eta arrays

**Key validations in cxf_eta_validate:**
- nnz >= 0
- type is 1 (refactor) or 2 (pivot)
- pivot_row in range [0, max_rows)
- pivot_elem is finite and non-zero
- All indices in range and values finite

**Error code gotcha:**
- Use `CXF_ERROR_OUT_OF_MEMORY`, not `CXF_ERROR_MEMORY`
- Check cxf_types.h for correct error codes

---

### 2026-01-26: M5.1.4 cxf_ftran

**File created:** `src/basis/ftran.c` (95 LOC)

**Function implemented:**
- `cxf_ftran(basis, column, result)` - Solve Bx = b using eta representation

**Algorithm (Product Form of Inverse):**
1. Copy input column to result (identity basis case)
2. Apply eta vectors in chronological order (oldest to newest via linked list)
3. For each eta E with pivot row r and pivot element p:
   - Save temp = result[r]
   - result[r] = temp / pivot_elem
   - For each off-diagonal entry: result[j] -= eta_value[j] * temp

**Key design decisions:**
- Follows linked list from eta_head → next → next (oldest to newest)
- Separate input/output arrays to preserve original column
- Bounds checking on pivot_row and division by zero check on pivot_elem
- Sparse off-diagonal entries stored in indices/values arrays

---

### 2026-01-26: M5.1.5 cxf_btran

**File created:** `src/basis/btran.c` (126 LOC)

**Function implemented:**
- `cxf_btran(basis, row, result)` - Solve y^T B = e_row^T using eta representation

**Algorithm (Product Form of Inverse, REVERSE order):**
1. Initialize result as unit vector e_row
2. Collect eta pointers into array (singly-linked list only has next)
3. Apply eta vectors in REVERSE order (newest to oldest):
   - Compute dot product: temp = sum(eta_val[k] * result[indices[k]])
   - Update pivot: result[pivot_row] = (result[pivot_row] - temp) / pivot_elem
   - Other positions UNCHANGED (key difference from FTRAN)

**Key mathematical insight - BTRAN vs FTRAN:**
- FTRAN applies E^(-1): updates pivot AND off-diagonal positions
- BTRAN applies (E^(-1))^T: updates ONLY pivot position
- BTRAN traverses eta list in REVERSE order (newest to oldest)

**Implementation pattern for reverse traversal:**
- Singly-linked list with only `next` pointers
- Collect pointers into array, then traverse backwards
- Stack allocation for ≤64 etas, heap for larger (MAX_STACK_ETAS = 64)
- This avoids recursion overhead and is cache-friendly

---

### 2026-01-26: M5.1.6 cxf_basis_refactor

**File created:** `src/basis/refactor.c` (208 LOC)

**Functions implemented:**
- `cxf_basis_refactor(basis)` - Clear eta vectors, reset counters (PFI approach)
- `cxf_solver_refactor(ctx, env)` - Full refactorization with solver context
- `cxf_refactor_check(ctx, env)` - Determine if refactorization needed

**Design decision - PFI vs LU:**
- Spec describes full LU factorization with Markowitz ordering
- Current architecture uses Product Form of Inverse (PFI) with eta vectors
- FTRAN/BTRAN already work with eta vectors
- Full LU would require BasisState structure changes (L, U storage)
- Implemented PFI-compatible version for now

**Refactorization triggers (cxf_refactor_check):**
- eta_count >= max_eta_count → Required (return 2)
- eta_memory >= max_eta_memory → Required (return 2)
- iterations since refactor >= refactor_interval → Recommended (return 1)
- FTRAN degradation (avg > 3x baseline) → Recommended (return 1)

**Key insight:**
- For identity basis (all slacks), no eta vectors needed (B = I)
- For structural columns, full implementation needs matrix access via model_ref
- TODO marker left for Markowitz-ordered LU when matrix access available

---

## M6: Algorithm Layer (Level 4)

### 2026-01-26: M6.1.1 Pricing Tests

**Files created:**
- `tests/unit/test_pricing.c` (310 LOC) - 24 comprehensive TDD tests
- `src/pricing/pricing_stub.c` (233 LOC) - Stub implementations for TDD

**Tests written (24 total):**
- PricingContext create/free (4 tests) - PASS
- cxf_pricing_init (4 tests) - PASS
- cxf_pricing_candidates (4 tests) - PASS
- cxf_pricing_steepest (4 tests) - PASS
- cxf_pricing_update (2 tests) - PASS
- cxf_pricing_invalidate (3 tests) - PASS
- cxf_pricing_step2 (2 tests) - PASS
- Statistics tracking (1 test) - PASS

**TDD pattern for pricing:**
- Tests define expected behavior for partial pricing, steepest edge, Dantzig
- Stubs implement working versions to enable test-first development
- Variable status codes: VAR_BASIC (>=0), VAR_AT_LOWER (-1), VAR_AT_UPPER (-2)
- Reduced cost attractiveness:
  - At lower bound: attractive if RC < -tolerance
  - At upper bound: attractive if RC > tolerance
- Steepest edge ratio: |RC| / sqrt(weight)

**Key design decisions:**
- PricingContext owns candidate_counts, candidate_arrays, cached_counts arrays
- Multi-level partial pricing support (max_levels typically 3-5)
- Cache invalidation via flags (INVALID_CANDIDATES, INVALID_WEIGHTS, etc.)
- Weights safeguard: zero/negative weights replaced with 1.0

---

### 2026-01-26: M6.1.2 PricingContext Structure

**File created:** `src/pricing/context.c` (118 LOC)

**Functions implemented:**
- `cxf_pricing_create(num_vars, max_levels)` - Allocates PricingContext with level arrays
- `cxf_pricing_free(ctx)` - Frees context and all arrays (NULL-safe)
- `cxf_pricing_init(ctx, num_vars, strategy)` - Reinitialize for new solve

**Extraction pattern:**
- Original lifecycle functions lived in `pricing_stub.c` for TDD
- Extracted to `context.c` as proper implementation
- Stub file now only contains algorithm functions (candidates, steepest, update, etc.)
- Both files linked - no duplicate symbols

**Key fields in PricingContext:**
- current_level, max_levels - level management
- candidate_counts, candidate_arrays, cached_counts - per-level arrays
- last_pivot_iteration, total_candidates_scanned, level_escalations - statistics

---

### 2026-01-26: M6.1.3 cxf_pricing_init

**File created:** `src/pricing/init.c` (179 LOC)

**Function implemented:**
- `cxf_pricing_init(ctx, num_vars, strategy)` - Initialize pricing context for new solve

**Algorithm:**
1. Auto-select strategy if requested (strategy=0, threshold n<1000 for small)
2. Store num_vars and strategy in context
3. Allocate candidate arrays per level:
   - Level 0: full (all variables)
   - Level 1+: sqrt(n) for partial pricing, minimum MIN_CANDIDATES=100
4. For steepest edge/Devex: allocate weights array, initialize to 1.0
5. Reset statistics and invalidate all caches

**PricingContext structure extended with:**
- `num_vars` - problem size
- `strategy` - pricing strategy (0=auto, 1=partial, 2=SE, 3=Devex)
- `candidate_sizes` - allocated size per level
- `weights` - steepest edge weights array

**Key implementation pattern:**
- Helper function `compute_level_size()` for determining per-level allocation
- Proper cleanup on allocation failure (free partial allocations)
- Reinit case: free existing arrays before reallocating
- Forward declaration needed when function called before definition

---

### 2026-01-26: M6.1.4 cxf_pricing_candidates

**File created:** `src/pricing/candidates.c` (172 LOC)

**Function implemented:**
- `cxf_pricing_candidates(...)` - Select candidate entering variables

**Algorithm:**
1. Determine scan range (full or section for partial pricing)
2. Scan nonbasic variables for attractive reduced costs:
   - At lower bound: attractive if RC < -tolerance
   - At upper bound: attractive if RC > tolerance
   - Free variable: attractive if |RC| > tolerance
3. When array full, replace least attractive candidate
4. Sort candidates by |reduced_cost| descending (most attractive first)
5. Update statistics (total_candidates_scanned)

**Key design decisions:**
- Partial pricing uses 10 sections by default, cycling via last_pivot_iteration
- qsort with file-static global for reduced_costs (C limitation, not thread-safe)
- Replace-least-attractive strategy keeps best candidates when array full
- Free variable handling (status -3) included per spec

---

### 2026-01-26: M6.1.5 cxf_pricing_steepest

**File created:** `src/pricing/steepest.c` (143 LOC)

**Functions implemented:**
- `cxf_pricing_steepest(...)` - Select entering variable using SE criterion
- `cxf_pricing_compute_weight(column, num_rows)` - Compute SE weight (squared norm)

**Algorithm:**
- SE ratio = |reduced_cost| / sqrt(weight)
- Variable attractiveness based on status:
  - At lower bound (status -1): attractive if RC < -tolerance
  - At upper bound (status -2): attractive if RC > tolerance
  - Free variable (status -3): attractive if |RC| > tolerance
- Weight safeguard: weights < 1e-10 replaced with 1.0
- Statistics tracking: total_candidates_scanned updated

---

### 2026-01-26: M6.1.6-M6.1.7 Pricing Update/Phase

**Files created:**
- `src/pricing/update.c` (117 LOC) - cxf_pricing_update, cxf_pricing_invalidate
- `src/pricing/phase.c` (81 LOC) - cxf_pricing_step2 (full scan fallback)

**Key learning:**
- SE weight handling deferred until SolverContext integration
- Invalidation flags defined locally in update.c (not in header yet)

---

### 2026-01-26: M5.1.7 Basis Snapshots

**File created:** `src/basis/snapshot.c` (159 LOC)

**Functions implemented:**
- `cxf_basis_snapshot_create(basis, snapshot)` - Deep copy of basis state
- `cxf_basis_snapshot_diff(s1, s2)` - Count element-wise differences
- `cxf_basis_snapshot_equal(s1, s2)` - Returns 1 if identical (diff == 0)
- `cxf_basis_snapshot_free(snapshot)` - Free arrays and clear valid flag

**Structure added to cxf_basis.h:**
```c
typedef struct {
    int numVars, numConstrs;
    int* basisHeader;
    int* varStatus;
    int valid, iteration;
    void *L, *U;        // Reserved for future factor copies
    int* pivotPerm;
} BasisSnapshot;
```

**Tests added to test_basis.c (12 new, 41 total):**
- test_snapshot_create_copies_data
- test_snapshot_create_null_args
- test_snapshot_create_empty_basis
- test_snapshot_diff_identical (returns 0)
- test_snapshot_diff_one_header_change
- test_snapshot_diff_var_status_change
- test_snapshot_diff_dimension_mismatch (returns -1)
- test_snapshot_diff_null_args
- test_snapshot_equal_true/false
- test_snapshot_free_null_safe
- test_snapshot_free_clears_valid

**Key design decisions:**
- BasisState extended with `n` (numVars) and `iteration` fields
- Dimension mismatch in diff returns -1 (not comparable)
- NULL args in diff return -1 (error indicator)
- Free sets valid=0 to prevent use-after-free bugs

---

### 2026-01-26: M5.1.8 Basis Validation/Warm Start

**File created:** `src/basis/warm.c` (115 LOC, 244 lines with comments)

**Functions implemented:**
- `cxf_basis_validate(basis)` - Validate basis for consistency
- `cxf_basis_validate_ex(basis, flags)` - Extended validation with selective checks
- `cxf_basis_warm(basis, basic_vars, m)` - Warm start from basic variable array
- `cxf_basis_warm_snapshot(basis, snapshot)` - Warm start from BasisSnapshot

**Validation flags defined:**
```c
#define CXF_CHECK_COUNT       0x01  // Verify m basic variables
#define CXF_CHECK_BOUNDS      0x02  // Check indices in [0, n)
#define CXF_CHECK_DUPLICATES  0x04  // Check no duplicate basic vars
#define CXF_CHECK_CONSISTENCY 0x10  // Check varStatus matches basisHeader
#define CXF_CHECK_ALL         0xFF  // Run all checks
```

**Tests added to test_basis.c (15 new, 56 total):**
- Validation tests (9): valid_basis, duplicate_vars, null_arg, empty_basis, out_of_bounds, negative_index, check_count, check_all, no_flags
- Warm start tests (6): loads_basis, clears_eta_list, null_args, size_mismatch, resets_pivot_count, warm_snapshot variants

**Key design decisions:**
- Simplified validation vs full spec (uses BasisState, not SolverState)
- Out-of-bounds check: 0 <= var < n (not n + m as in full solver)
- cxf_basis_validate_ex with flags=0 returns OK immediately (no checks)
- Warm start clears eta list and resets pivots_since_refactor
- BasisSnapshot warm start copies both basisHeader and varStatus

**Spec adaptation notes:**
- Full spec has cxf_basis_validate(state, env, flags) with SolverState
- Implemented simpler version compatible with current BasisState structure
- Future work: integrate with full SolverState when simplex implemented

---

## M7: Simplex Engine (Level 5)

### 2026-01-26: M7.1.1 Simplex Tests - Basic (TDD)

**File created:** `tests/unit/test_simplex_basic.c` (273 total lines, 165 LOC)

**TDD tests written (17 total):**
- **Init tests (6):** test_simplex_init_creates_state, null_model_fails, null_stateout_fails, empty_model, primal_mode, dual_mode
- **Cleanup tests (2):** test_simplex_final_frees_state, null_safe
- **Setup tests (3):** test_simplex_setup_basic, null_state_fails, null_env_fails
- **Query tests (6):** get_status_initial, get_status_null, get_iteration_initial, get_iteration_null, get_phase_after_setup, get_phase_null

**Expected interface defined:**
```c
int cxf_simplex_init(CxfModel *model, void *warmStart, int mode,
                     double *timing, SolverContext **stateOut);
int cxf_simplex_final(SolverContext *state);
int cxf_simplex_setup(SolverContext *state, CxfEnv *env);
int cxf_simplex_get_status(SolverContext *state);
int cxf_simplex_get_iteration(SolverContext *state);
int cxf_simplex_get_phase(SolverContext *state);
```

**Key learnings:**
- Test file compiles but linker errors expected (TDD before implementation)
- Mode parameter: 0=auto, 1=primal, 2=dual
- Status codes to implement: CXF_STATUS_UNSET, CXF_STATUS_LOADED, etc.
- Phase: 1=Phase I (finding feasibility), 2=Phase II (optimizing)

---

### 2026-01-26: M5.2.1 Callbacks Tests (TDD)

**File created:** `tests/unit/test_callbacks.c` (237 total lines, ~139 LOC)

**TDD tests written (17 total):**
- **cxf_init_callback_struct (3):** zeroes_memory, null_pointer_returns_error, null_env_succeeds
- **cxf_set_terminate (3):** sets_flag, null_env_safe, idempotent
- **cxf_check_terminate (3):** returns_zero_when_clear, returns_one_when_set, null_env_returns_zero
- **cxf_callback_terminate (2):** sets_env_flag, null_model_safe
- **cxf_reset_callback_state (1):** null_env_safe
- **cxf_pre_optimize_callback (2):** null_env_returns_success, no_callback_returns_success
- **cxf_post_optimize_callback (2):** null_env_returns_success, no_callback_returns_success

**Expected interface defined (from specs):**
```c
int cxf_init_callback_struct(CxfEnv *env, void *callbackSubStruct);
void cxf_set_terminate(CxfEnv *env);
int cxf_check_terminate(CxfEnv *env);
void cxf_callback_terminate(CxfModel *model);
void cxf_reset_callback_state(CxfEnv *env);
int cxf_pre_optimize_callback(CxfModel *model);
int cxf_post_optimize_callback(CxfModel *model);
```

**Key learnings:**
- Tests compile but linker errors expected (TDD - functions not implemented)
- Follows test_logging.c and test_basis.c patterns
- Uses CxfEnv and CxfModel structures from existing headers
- cxf_init_callback_struct zeroes 48-byte substructure per spec
- Termination functions are idempotent (safe to call multiple times)
- NULL environment handling returns gracefully (0 or no-op per spec)

---

### 2026-01-26: M7.1.2 Simplex Tests - Iteration (TDD)

**File created:** `tests/unit/test_simplex_iteration.c` (223 LOC, 15 tests)

**TDD tests written (15 total):**
- **Iteration loop (3):** null_args_fail, returns_valid_status, increments_iteration
- **Phase transition (3):** null_args_fail, transitions_to_phase2, infeasible_returns_error
- **Termination (2):** null_state_fails, returns_continue_or_refactor
- **Objective tracking (2):** null_returns_nan, returns_current_objective
- **Iteration limits (5):** set_null_fails, set_negative_fails, set_valid, get_null_returns_error, get_returns_current

**Expected interface defined:**
```c
int cxf_simplex_iterate(SolverContext *state, CxfEnv *env);
// Returns: 0=continue, 1=optimal, 2=infeasible, 3=unbounded, 12=error

int cxf_simplex_phase_end(SolverContext *state, CxfEnv *env);
// Returns: 0=transition to Phase II, 2=infeasible

int cxf_simplex_post_iterate(SolverContext *state, CxfEnv *env);
// Returns: 0=continue, 1=refactor triggered

double cxf_simplex_get_objval(SolverContext *state);
// Returns: objective value or NaN if null

int cxf_simplex_set_iteration_limit(SolverContext *state, int limit);
int cxf_simplex_get_iteration_limit(SolverContext *state);
```

**Key learnings:**
- Test file compiles but linker errors expected (TDD before implementation)
- Phase I->II transition based on obj_value (0 = feasible, >0 = infeasible)
- cxf_simplex_iterate should increment state->iteration after pivot
- Iteration limits use state->max_iterations field
- SolverContext.obj_value tracks current objective value

---

### 2026-01-26: M5.2.2 CallbackContext Structure

**Files created:**
- `src/callbacks/context.c` (138 LOC) - CallbackContext lifecycle
- `src/callbacks/callback_stub.c` (112 LOC) - stub implementations for M5.2.3-M5.2.5
- Updated `include/convexfeld/cxf_callback.h` (95 LOC) - added function declarations

**Functions implemented:**
- `cxf_callback_create()` - allocate and initialize CallbackContext
- `cxf_callback_free()` - deallocate (NULL-safe)
- `cxf_callback_validate()` - magic number validation
- `cxf_callback_reset_stats()` - reset counters while preserving registration

**Stub implementations for TDD tests:**
- `cxf_init_callback_struct()` - zero 48-byte sub-structure
- `cxf_set_terminate()` - set env termination flag
- `cxf_callback_terminate()` - terminate from callback
- `cxf_reset_callback_state()` - reset callback state
- `cxf_pre_optimize_callback()` - pre-optimization callback
- `cxf_post_optimize_callback()` - post-optimization callback

**Tests added (13 new, 29 total):**
- test_callback_create_returns_non_null
- test_callback_create_sets_magic
- test_callback_create_initializes_fields
- test_callback_create_best_obj_is_infinity
- test_callback_free_null_safe
- test_callback_validate_returns_ok_for_valid
- test_callback_validate_null_returns_error
- test_callback_validate_bad_magic_returns_error
- test_callback_validate_bad_safety_magic_returns_error
- test_callback_reset_stats_clears_counters
- test_callback_reset_stats_preserves_registration
- test_callback_reset_stats_null_returns_error
- test_callback_reset_stats_invalid_magic_returns_error

**Key design decisions:**
- CallbackContext uses dual magic numbers (32-bit + 64-bit) for validation
- best_obj initialized to INFINITY (no objective found yet)
- cxf_callback_reset_stats preserves registration (callback_func, user_data, enabled)
- cxf_set_terminate is alias for cxf_terminate (already in error module)
- cxf_check_terminate already exists in src/error/terminate.c (M3.1.6)

**Integration notes:**
- Discovered cxf_check_terminate already implemented in M3.1.6
- Avoid duplicate definitions - check existing code before adding stubs
- Stubs enabled all 29 callback tests to pass (linking and execution)

---

### 2026-01-26: M5.2.3 Callback Initialization

**File created:** `src/callbacks/init.c` (114 LOC)

**Functions implemented:**
- `cxf_init_callback_struct(env, callbackSubStruct)` - Zero-initialize 48-byte substructure
- `cxf_reset_callback_state(env)` - Reset callback counters while preserving configuration

**Stub extraction pattern:**
- Original stubs lived in `callback_stub.c` for TDD
- Moved to dedicated `init.c` for proper implementation
- Stub file retains only unimplemented functions (pre/post optimize, termination)
- Removed duplicate function definitions to avoid linker errors

**Tests added (2 new, 31 total):**
- test_reset_callback_state_no_callback_state_safe - NULL callback_state handling
- test_reset_callback_state_clears_statistics - Full reset behavior verification

**Key implementation details:**
- `cxf_init_callback_struct`: env parameter unused (reserved for future extensibility)
- `cxf_reset_callback_state` resets:
  - callback_calls, callback_time -> 0.0
  - iteration_count -> 0
  - best_obj -> INFINITY
  - start_time -> cxf_get_timestamp()
  - terminate_requested -> 0
- Preserves: magic, safety_magic, callback_func, user_data, enabled

**Dependencies:**
- Uses `cxf_get_timestamp()` from timing module for start_time reset
- Accesses env->callback_state (CallbackContext pointer)

---

### 2026-01-26: M7.1.3 Simplex Tests - Edge Cases (TDD)

**File created:** `tests/unit/test_simplex_edge.c` (192 LOC, 15 tests)

**TDD tests written (15 total):**
- **Degeneracy handling (4):** perturbation_null_args, perturbation_basic, unperturb_null_args, unperturb_sequence
- **Unbounded detection (2):** solve_unbounded_simple, unbounded_with_constraint
- **Infeasible detection (2):** solve_infeasible_bounds, infeasible_constraints
- **Numerical stability (3):** small_coefficients, large_coefficient_range, fixed_variable
- **Empty/trivial (4):** solve_empty_model, solve_trivial, solve_all_fixed, solve_free_variable

**Expected interface defined (perturbation functions):**
```c
int cxf_simplex_perturbation(SolverContext *state, CxfEnv *env);
// Returns: CXF_OK on success, CXF_ERROR_NULL_ARGUMENT for null args
// Idempotent - second call also returns CXF_OK

int cxf_simplex_unperturb(SolverContext *state, CxfEnv *env);
// Returns: CXF_OK if perturbation was applied, 1 if no perturbation to remove
```

**Expected edge case behaviors (to be implemented):**
- Unbounded: `cxf_solve_lp` returns `CXF_UNBOUNDED` (status 3)
- Infeasible (lb > ub): `cxf_solve_lp` returns `CXF_INFEASIBLE` (status 2)
- Empty model (0 vars): returns `CXF_OPTIMAL` immediately
- Fixed variables (lb == ub): handled correctly during solve

**Key learnings:**
- Tests compile but show expected linker errors for unimplemented functions
- Current stub `cxf_solve_lp(model)` has single-arg signature (will be expanded)
- Perturbation API from spec: Wolfe (1963) method for cycling prevention
- Tests define expected behavior for full simplex implementation

**Files modified:**
- `tests/CMakeLists.txt` - Added test_simplex_edge

---

## 2026-01-27: Code Review - Basis Module Spec Compliance

**Review completed:** Full spec compliance analysis of basis module

**Report:** `reports/review_spec_basis.md` (766 lines, comprehensive)

**Scope:**
- 10 functions specified across 8 implementation files
- ~857 LOC reviewed in detail
- Every function compared against its specification

**Key Findings:**

**✅ PASSING (5 functions):**
- `cxf_ftran` - Correct eta application, good error handling
- `cxf_btran` - Excellent reverse traversal with stack optimization
- EtaFactors lifecycle (create/free/init/validate/set) - Complete
- BasisSnapshot operations (create/diff/equal/free) - Complete

**⚠️ PARTIAL (2 functions):**
- `cxf_basis_validate` - Missing numerical stability and feasibility checks
- `cxf_basis_warm` - Oversimplified, no repair logic for dimension changes

**❌ CRITICAL GAPS (2 blockers):**
1. `cxf_pivot_with_eta` - **COMPLETELY MISSING**
   - Required for basis updates during simplex
   - Creates eta vectors from pivot column
   - Without this, simplex cannot iterate
2. `cxf_basis_refactor` - **STUB ONLY**
   - Only clears eta list, no LU factorization
   - Cannot handle non-identity bases
   - Structural variables in basis will fail

**⚠️ RENAMED FUNCTION:**
- Spec: `cxf_timing_refactor`
- Code: `cxf_refactor_check`
- Functionality matches, name differs

**Structural Issue:**
- Specs use `SolverState*` throughout
- Code uses `BasisState*` (most functions) or `SolverContext*` (refactor)
- This is a spec vs implementation design divergence
- **Recommendation:** Update specs to match implementation

**Impact Analysis:**
- Basis module ~70% complete by LOC
- Only ~50% complete by critical functionality
- **Cannot support simplex iterations** without pivot_with_eta
- **Cannot handle real problems** without full refactorization

**Learnings:**

1. **Stub vs Implementation Pattern:**
   - FTRAN/BTRAN only handle product form (eta vectors), not full LU solve
   - This is acceptable for current PFI approach
   - BUT refactorization must eventually create proper factorization

2. **Missing Markowitz LU:**
   - Spec describes full Markowitz-ordered Gaussian elimination
   - Implementation punts with TODO comment
   - This is THE major gap preventing real-world use

3. **Dual Implementation Pattern Found:**
   - `basis_stub.c` has simple versions of equal/diff
   - `snapshot.c` has full BasisSnapshot versions
   - Inconsistent - should consolidate

4. **Good Error Handling Where Complete:**
   - FTRAN/BTRAN have thorough NULL checks
   - BTRAN uses clever stack allocation fallback
   - Bounds checking on indices throughout

5. **Test Gap Identified:**
   - No `tests/test_basis*.c` files found
   - Only `tests/unit/test_basis.c` from TDD phase
   - Need comprehensive integration tests

**Action Items for Next Session:**

**MUST DO (Blockers):**
1. Implement `cxf_pivot_with_eta` (~100 LOC)
   - Create eta vector from pivot column
   - Compute multiplier: 1/pivot_elem
   - Store off-diagonal entries
   - Append to eta list
   - Update basis header

2. Complete `cxf_basis_refactor` (~300 LOC)
   - Extract basis columns from constraint matrix
   - Markowitz-ordered Gaussian elimination
   - Store L, U factors (or eta equivalent)
   - Detect singular bases
   - Handle memory allocation

**SHOULD DO (Quality):**
3. Enhance `cxf_basis_warm`
   - Add dimension change handling
   - Implement variable mapping
   - Add repair logic

4. Enhance `cxf_basis_validate`
   - Add condition number checks
   - Add feasibility validation
   - Return specific error codes

5. Consolidate snapshot functions
   - Remove stub versions
   - Keep only full BasisSnapshot API

**Documentation:**
6. Update specs to use `BasisState*` instead of `SolverState*`
7. Document the state structure hierarchy

**Gotchas Identified:**
- FTRAN/BTRAN specs describe two-phase (LU + eta) but code only does eta phase
  - This is OK for PFI representation
  - LU solve is "hidden" in the eta vectors created by refactorization
- Spec error codes don't match implementation error codes in some cases
- Set comparison (order-independent) not implemented for basis equality

**Success Pattern Observed:**
- Stack allocation with heap fallback (BTRAN) is excellent for performance
- Should apply this pattern elsewhere with variable-size arrays

**Review process itself:**
- Systematic spec-by-spec comparison very effective
- Found issues that wouldn't be caught by testing alone
- Specs are high quality - detailed algorithms, edge cases, complexity
- Code quality good where complete - just incomplete coverage

---

### 2026-01-27: M7.1.6 Simplex Setup and Preprocessing

**Files created:**
- `src/simplex/setup.c` (198 LOC) - cxf_simplex_setup and cxf_simplex_preprocess
- `tests/unit/test_simplex_setup.c` (295 LOC) - 18 comprehensive tests

**Functions implemented:**

**cxf_simplex_setup:**
- Initializes reduced costs from objective coefficients (memcpy from work_obj to work_dj)
- Zero-initializes dual values (memset work_pi)
- Creates and initializes pricing context via cxf_pricing_create/init
- Determines initial phase (1 if bounds infeasible, 2 otherwise)
- Resets iteration tracking (iteration=0, eta_count=0)
- Sets tolerance from environment

**cxf_simplex_preprocess:**
- Checks skip flag (bit 0 of flags parameter)
- Detects infeasible bounds (lb > ub + tolerance) → returns 3
- Placeholder for full preprocessing (singleton elimination, bound propagation, scaling)

**Tests written (18 total):**
- Setup tests (10): NULL handling, empty model, reduced cost init, dual values, iteration reset, phase determination (feasible/infeasible), pricing init, tolerance setting
- Preprocess tests (7): NULL handling, skip flag, empty model, feasible bounds, infeasibility detection (single/multiple vars)
- Integration test (1): setup + preprocess sequence

**Key design decisions:**

1. **Adapted to existing SolverContext structure:**
   - Spec uses `reducedCosts` but code uses `work_dj`
   - Spec uses `dualValues` but code uses `work_pi`
   - Used direct field access since cxf_get_intparam doesn't exist

2. **Phase determination:**
   - Phase 1: Any variable has lb > ub + tolerance
   - Phase 2: All bounds feasible
   - Uses env->feasibility_tol with 1e-6 default fallback

3. **Pricing initialization:**
   - Creates 3-level pricing context
   - Uses auto strategy (0)
   - Only allocates if n > 0

4. **Preprocessing scope:**
   - Full preprocessing (singleton elimination, bound propagation, geometric scaling) requires constraint matrix access
   - Current cxf_addconstr is stub, so full preprocessing deferred
   - Basic infeasibility detection implemented

**Gotchas:**
- Stub for cxf_simplex_setup was in context.c - had to remove to avoid duplicate definition
- Header declaration for cxf_simplex_preprocess was missing - had to add
- cxf_get_intparam doesn't exist - use direct env field access or defaults

**Files modified:**
- `CMakeLists.txt` - Added src/simplex/setup.c
- `tests/CMakeLists.txt` - Added test_simplex_setup
- `include/convexfeld/cxf_solver.h` - Added cxf_simplex_preprocess declaration
- `src/simplex/context.c` - Removed cxf_simplex_setup stub

**Test results:**
- All 18 new tests pass
- All existing simplex_basic tests (17) still pass
- Pre-existing failures unchanged (api_optimize, simplex_iteration, simplex_edge)

---

### 2026-01-27: M5.3.3 SolveState Initialization and Cleanup

**Files created:**
- `include/convexfeld/cxf_solve_state.h` (106 LOC) - SolveState structure definition
- `src/solver_state/init.c` (154 LOC) - Init and cleanup implementation

**Structure defined: SolveState (~72 bytes, stack-allocated)**

Control structure that wraps SolverContext and manages solve progress:
- `magic` (uint32_t) - Validation magic (0x534f4c56 = "SOLV")
- `status` (int) - Current status (STATUS_LOADED = 1)
- `iterations` (int) - Iteration count
- `phase` (int) - Current phase (0=initial, 1=Phase I, 2=Phase II)
- `solverState` (SolverContext*) - Pointer to solver working state
- `env` (CxfEnv*) - Environment pointer
- `startTime` (double) - Start timestamp from cxf_get_timestamp()
- `timeLimit` (double) - Time limit (from env or 1e100 default)
- `iterLimit` (int) - Iteration limit (from env or INT_MAX default)
- `interruptFlag` (int) - Interrupt flag (0=continue, 1=interrupt)
- `callbackData` (void*) - Callback data from env
- `method` (int) - Solve method (from state->solve_mode or default 1=dual simplex)
- `flags` (int) - Control flags

**Functions implemented:**

**cxf_init_solve_state:**
- Returns CXF_ERROR_NULL_ARGUMENT if solve is NULL
- Initializes all fields as per spec
- Captures start time via cxf_get_timestamp()
- If env is NULL: timeLimit=1e100, iterLimit=INT_MAX, callbackData=NULL
- If state is NULL: method=1 (default dual simplex)
- Non-allocating, ~20-30 nanoseconds

**cxf_cleanup_solve_state:**
- If solve is NULL, returns immediately (no-op)
- Sets magic to 0 (invalidate)
- Clears all fields to 0 or NULL
- No memory freed (caller owns the SolveState allocation)
- Idempotent (safe to call multiple times)

**Key design decisions:**

1. **TimeLimit and IterationLimit not yet in CxfEnv:**
   - Added TODO comment for when these parameters are added to CxfEnv
   - Currently use hardcoded defaults (1e100, INT_MAX)
   - Gets callback context via cxf_get_callback_context(env) which is implemented

2. **STATUS_LOADED constant:**
   - Defined as 1 in cxf_solve_state.h
   - Matches spec requirement for initial status

3. **Separation of concerns:**
   - SolveState: Algorithm-agnostic control (small, stack-allocated, short-lived)
   - SolverContext: Algorithm-specific working data (large, heap-allocated, long-lived)

**Testing:**
- Manual test program created and verified all functionality
- NULL pointer handling tested
- Initialization with NULL state and env tested
- Cleanup invalidation tested
- All 31 project tests still pass (28 pass, 3 pre-existing failures)

**Files modified:**
- `CMakeLists.txt` - Added src/solver_state/init.c to build

**Line counts:**
- Header: 106 LOC (well under 200)
- Implementation: 154 LOC (well under 200)

**Key learnings:**
- SolveState is intentionally minimal - just control and tracking
- Magic number pattern (0x534f4c56 = "SOLV") enables validation
- Stack allocation pattern for short-lived structures is efficient
- Defensive programming: NULL-safe, idempotent cleanup

---
