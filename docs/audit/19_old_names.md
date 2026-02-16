# Audit Report: Old V1 Names Still in Code
**Auditor:** Agent F1
**Date:** 2026-02-16

## Summary
- Old names searched: 16 (15 functions + 1 structure)
- Old names found in code: 16 (ALL)
- Total occurrences in src/tests/include: 96
- Additional occurrences in docs/specs-v1: Hundreds (expected)
- Additional occurrences in reports/build artifacts: Many (expected)

## Critical Finding

**ALL 16 old v1 names are still present in the production codebase** (src/, tests/, include/). The v2 spec renames exist only in documentation (docs/specs-v2/). No code migration has occurred.

---

## Findings by Name

### 1. cxf_simplex_iterate (should be cxf_log_iteration_progress)

**Found in PRODUCTION CODE:**
- `include/convexfeld/cxf_solver.h:143` - function declaration
- `src/simplex/iterate.c:3` - file header comment
- `src/simplex/iterate.c:8` - spec reference
- `src/simplex/iterate.c:122` - function definition
- `src/simplex/solve_lp.c:31` - extern declaration
- `src/simplex/solve_lp.c:737` - function call
- `src/simplex/solve_lp.c:1233` - function call
- `src/simplex/context.c:217` - comment reference

**Found in TESTS:**
- `tests/unit/test_simplex_iteration.c:16` - extern declaration
- `tests/unit/test_simplex_iteration.c:43` - test call (NULL check)
- `tests/unit/test_simplex_iteration.c:48` - test call (NULL check)
- `tests/unit/test_simplex_iteration.c:58` - test call (iteration test)
- `tests/unit/test_simplex_iteration.c:73` - test call (increment test)

**Total in src/tests/include:** 13 occurrences

---

### 2. cxf_simplex_cleanup (should be cxf_simplex_postsolve)

**Found in PRODUCTION CODE:**
- `include/convexfeld/cxf_solver.h:210` - function declaration
- `src/simplex/cleanup.c:5` - file header comment
- `src/simplex/cleanup.c:37` - function definition

**Found in TESTS:**
- None found in test files

**Total in src/tests/include:** 3 occurrences

---

### 3. cxf_basis_refactor (should be cxf_fix_variables_at_bounds)

**Found in PRODUCTION CODE:**
- `include/convexfeld/cxf_basis.h:86` - comment reference (LU factorization)
- `src/basis/refactor.c:10` - spec reference comment
- `src/basis/refactor.c:61` - function definition
- `src/basis/lu_factorize.c:8` - spec reference comment
- `src/basis/basis_stub.c:33` - comment reference
- `src/simplex/post.c:15` - extern declaration
- `src/simplex/post.c:39` - function call

**Found in TESTS:**
- `tests/unit/test_basis.c:34` - extern declaration
- `tests/unit/test_basis.c:344` - comment header
- `tests/unit/test_basis.c:354` - test call (basic test)
- `tests/unit/test_basis.c:370` - test call (non-identity test)
- `tests/unit/test_basis.c:377` - test call (NULL test)

**Total in src/tests/include:** 12 occurrences

---

### 4. cxf_basis_snapshot (should be cxf_progress_snapshot)

**Found in PRODUCTION CODE:**
- `include/convexfeld/cxf_basis.h:181` - function declaration (cxf_basis_snapshot_create)
- `include/convexfeld/cxf_basis.h:191` - function declaration (cxf_basis_snapshot_diff)
- `include/convexfeld/cxf_basis.h:200` - function declaration (cxf_basis_snapshot_equal)
- `include/convexfeld/cxf_basis.h:207` - function declaration (cxf_basis_snapshot_free)
- `src/basis/snapshot.c:8` - spec reference comment
- `src/basis/snapshot.c:34` - function definition (cxf_basis_snapshot_create)
- `src/basis/snapshot.c:92` - function definition (cxf_basis_snapshot_diff)
- `src/basis/snapshot.c:129` - function definition (cxf_basis_snapshot_equal)
- `src/basis/snapshot.c:130` - function call (cxf_basis_snapshot_diff)
- `src/basis/snapshot.c:142` - function definition (cxf_basis_snapshot_free)
- `src/basis/basis_stub.c:44` - function definition (stub)

**Found in TESTS:**
- `tests/unit/test_basis.c:37` - extern declaration (stub)
- `tests/unit/test_basis.c:42` - extern declaration (cxf_basis_snapshot_create)
- `tests/unit/test_basis.c:44` - extern declaration (cxf_basis_snapshot_diff)
- `tests/unit/test_basis.c:45` - extern declaration (cxf_basis_snapshot_equal)
- `tests/unit/test_basis.c:46` - extern declaration (cxf_basis_snapshot_free)
- `tests/unit/test_basis.c:392` - function call (stub)
- `tests/unit/test_basis.c:473` - function call (create)
- `tests/unit/test_basis.c:492` - function call (free)
- `tests/unit/test_basis.c:498` - function call (create NULL test)
- `tests/unit/test_basis.c:502` - function call (create NULL test)
- `tests/unit/test_basis.c:512` - function call (create)
- `tests/unit/test_basis.c:518` - function call (free)
- `tests/unit/test_basis.c:531` - function calls (create x2)
- `tests/unit/test_basis.c:534` - function call (diff)
- `tests/unit/test_basis.c:537-538` - function calls (free x2)
- `tests/unit/test_basis.c:549` - function call (create)
- `tests/unit/test_basis.c:553` - function call (create)
- `tests/unit/test_basis.c:555` - function call (diff)
- `tests/unit/test_basis.c:558-559` - function calls (free x2)
- `tests/unit/test_basis.c:572` - function call (create)
- `tests/unit/test_basis.c:576` - function call (create)
- `tests/unit/test_basis.c:578` - function call (diff)
- `tests/unit/test_basis.c:581-582` - function calls (free x2)
- `tests/unit/test_basis.c:591-592` - function calls (create x2)
- `tests/unit/test_basis.c:594` - function call (diff)
- `tests/unit/test_basis.c:597-598` - function calls (free x2)
- `tests/unit/test_basis.c:609` - function call (diff NULL test)
- `tests/unit/test_basis.c:612` - function call (diff NULL test)
- `tests/unit/test_basis.c:623-624` - function calls (create x2)
- `tests/unit/test_basis.c:626` - function call (equal)
- `tests/unit/test_basis.c:629-630` - function calls (free x2)
- `tests/unit/test_basis.c:641` - function call (create)
- `tests/unit/test_basis.c:645` - function call (create)
- `tests/unit/test_basis.c:647` - function call (equal)
- `tests/unit/test_basis.c:650-651` - function calls (free x2)
- `tests/unit/test_basis.c:656` - function call (free NULL test)
- `tests/unit/test_basis.c:663` - function call (create)
- `tests/unit/test_basis.c:666` - function call (free)
- `tests/unit/test_basis.c:857` - function call (create)
- `tests/unit/test_basis.c:874` - function call (free)
- `tests/unit/test_basis.c:911` - function call (create)
- `tests/unit/test_basis.c:918` - function call (free)
- `tests/unit/test_basis.c:929` - function call (create)
- `tests/unit/test_basis.c:940` - function call (free)

**Total in src/tests/include:** 57+ occurrences (extensive test coverage)

---

### 5. cxf_sort_indices (should be cxf_sort_by_values)

**Found in PRODUCTION CODE:**
- `src/matrix/sort.c:8` - spec reference comment
- `src/matrix/sort.c:46` - comment header
- `src/matrix/sort.c:55` - function definition
- `src/matrix/sort.c:64` - comment header (cxf_sort_indices_values)
- `src/matrix/sort.c:77` - function definition (cxf_sort_indices_values)

**Found in TESTS:**
- `tests/unit/test_matrix.c:42` - extern declaration
- `tests/unit/test_matrix.c:43` - extern declaration (cxf_sort_indices_values)
- `tests/unit/test_matrix.c:344` - function call (sorted test)
- `tests/unit/test_matrix.c:355` - function call (unsorted test)
- `tests/unit/test_matrix.c:363` - function call (reverse test)
- `tests/unit/test_matrix.c:372` - function call (single element test)
- `tests/unit/test_matrix.c:376` - function call (NULL test)
- `tests/unit/test_matrix.c:384` - function call (values variant)

**Total in src/tests/include:** 13 occurrences

---

### 6. cxf_check_nan_or_inf (should be cxf_is_finite)

**Found in PRODUCTION CODE:**
- `src/error/error_stub.c:8` - comment reference
- `src/error/nan_check.c:18` - comment reference
- `src/error/nan_check.c:46` - function definition

**Found in TESTS:**
- `tests/unit/test_error.c:24` - extern declaration
- `tests/unit/test_error.c:137` - comment header
- `tests/unit/test_error.c:142` - function call (all finite test)
- `tests/unit/test_error.c:148` - function call (has NaN test)
- `tests/unit/test_error.c:154` - function call (has +inf test)
- `tests/unit/test_error.c:160` - function call (has -inf test)
- `tests/unit/test_error.c:165` - function call (NULL test)
- `tests/unit/test_error.c:384` - comment reference

**Total in src/tests/include:** 11 occurrences

---

### 7. cxf_cleanup_helper (should be cxf_propagate_bounds)

**Found in PRODUCTION CODE:**
- `src/solver_state/helpers.c:24` - function definition

**Found in TESTS:**
- None found in test files

**Total in src/tests/include:** 1 occurrence

---

### 8. cxf_setup_basis (should be cxf_free_warmstart_basis)

**Found in PRODUCTION CODE:**
- None found (name only in specs-v2)

**Found in TESTS:**
- None found

**Total in src/tests/include:** 0 occurrences (not implemented yet)

---

### 9. cxf_setup_work_arrays (should be cxf_free_work_arrays)

**Found in PRODUCTION CODE:**
- None found (name only in specs-v2)

**Found in TESTS:**
- None found

**Total in src/tests/include:** 0 occurrences (not implemented yet)

---

### 10. cxf_free_solver_state (should be cxf_free_attribute_table)

**Found in PRODUCTION CODE:**
- `src/memory/state_cleanup.c:24` - comment header
- `src/memory/state_cleanup.c:40` - function definition

**Found in TESTS:**
- `tests/unit/test_memory.c:26` - extern declaration
- `tests/unit/test_memory.c:146` - function call (NULL test)

**Total in src/tests/include:** 4 occurrences

---

### 11. cxf_errorlog (should be cxf_set_error_string)

**Found in PRODUCTION CODE:**
- `src/error/error_stub.c:7` - comment reference
- `src/error/core.c:5` - file header comment
- `src/error/core.c:66` - function definition

**Found in TESTS:**
- `tests/unit/test_error.c:20` - extern declaration
- `tests/unit/test_error.c:85` - comment header
- `tests/unit/test_error.c:89` - function call (NULL env test)
- `tests/unit/test_error.c:94` - function call (NULL message test)
- `tests/unit/test_error.c:99` - function call (basic test)
- `tests/unit/test_error.c:372` - comment reference

**Total in src/tests/include:** 9 occurrences

---

### 12. cxf_pre_optimize_callback (should be cxf_pre_optimize_hook)

**Found in PRODUCTION CODE:**
- `src/api/optimize_api.c:20` - extern declaration
- `src/api/optimize_api.c:79` - function call
- `src/callbacks/invoke.c:11` - spec reference comment
- `src/callbacks/invoke.c:42` - function definition

**Found in TESTS:**
- `tests/unit/test_callbacks.c:12` - comment reference
- `tests/unit/test_callbacks.c:33` - extern declaration
- `tests/unit/test_callbacks.c:354` - comment header
- `tests/unit/test_callbacks.c:361` - function call (NULL env test)
- `tests/unit/test_callbacks.c:366` - function call (no callback test)
- `tests/unit/test_callbacks.c:433` - comment reference

**Total in src/tests/include:** 10 occurrences

---

### 13. cxf_post_optimize_callback (should be cxf_post_optimize_hook)

**Found in PRODUCTION CODE:**
- `src/api/optimize_api.c:21` - extern declaration
- `src/api/optimize_api.c:93` - function call
- `src/callbacks/invoke.c:12` - spec reference comment
- `src/callbacks/invoke.c:124` - function definition

**Found in TESTS:**
- `tests/unit/test_callbacks.c:13` - comment reference
- `tests/unit/test_callbacks.c:34` - extern declaration
- `tests/unit/test_callbacks.c:371` - comment header
- `tests/unit/test_callbacks.c:378` - function call (NULL env test)
- `tests/unit/test_callbacks.c:383` - function call (no callback test)
- `tests/unit/test_callbacks.c:437` - comment reference

**Total in src/tests/include:** 10 occurrences

---

### 14. cxf_acquire_solve_lock (should be cxf_save_locale_state)

**Found in PRODUCTION CODE:**
- `src/threading/threading_stub.c:8` - comment reference
- `src/threading/locks.c:62` - function definition

**Found in TESTS:**
- None found (tested via higher-level functions)

**Total in src/tests/include:** 2 occurrences

---

### 15. cxf_set_thread_count (should be cxf_validate_thread_count)

**Found in PRODUCTION CODE:**
- `src/threading/threading_stub.c:9` - comment reference
- `src/threading/config.c:58` - function definition

**Found in TESTS:**
- `tests/unit/test_threading.c:13` - extern declaration
- `tests/unit/test_threading.c:66` - comment header
- `tests/unit/test_threading.c:70` - function call (valid test)
- `tests/unit/test_threading.c:75` - function call (NULL env test)
- `tests/unit/test_threading.c:80` - function call (zero test)
- `tests/unit/test_threading.c:81` - function call (negative test)
- `tests/unit/test_threading.c:86` - function call (exceeds logical test)
- `tests/unit/test_threading.c:160` - comment reference

**Total in src/tests/include:** 10 occurrences

---

### 16. WorkArrays (should be SolutionData)

**Found in PRODUCTION CODE:**
- None found (structure never implemented under either name)

**Found in TESTS:**
- None found

**Total in src/tests/include:** 0 occurrences (not implemented)

---

## Old Names NOT Found (already renamed or never implemented)

- `cxf_setup_basis` - not found (never implemented)
- `cxf_setup_work_arrays` - not found (never implemented)
- `WorkArrays` - not found (structure never implemented under this name)

---

## Impact Analysis

### Production Code Files Requiring Updates

**Headers (include/):**
1. `include/convexfeld/cxf_solver.h` - 2 function declarations
2. `include/convexfeld/cxf_basis.h` - 5 function declarations (snapshot family)

**Source Files (src/):**
1. `src/simplex/iterate.c` - 1 definition + 2 comments
2. `src/simplex/solve_lp.c` - 1 extern + 2 calls
3. `src/simplex/cleanup.c` - 1 definition + 1 comment
4. `src/simplex/context.c` - 1 comment
5. `src/simplex/post.c` - 1 extern + 1 call
6. `src/basis/refactor.c` - 1 definition + 1 comment
7. `src/basis/lu_factorize.c` - 1 comment
8. `src/basis/basis_stub.c` - 2 comments + 1 stub definition
9. `src/basis/snapshot.c` - 1 comment + 9 definitions/calls
10. `src/matrix/sort.c` - 1 comment + 4 definitions/comments
11. `src/error/error_stub.c` - 2 comments
12. `src/error/nan_check.c` - 1 definition + 1 comment
13. `src/error/core.c` - 1 definition + 1 comment
14. `src/solver_state/helpers.c` - 1 definition
15. `src/memory/state_cleanup.c` - 1 definition + 1 comment
16. `src/api/optimize_api.c` - 2 externs + 2 calls
17. `src/callbacks/invoke.c` - 2 comments + 2 definitions
18. `src/threading/threading_stub.c` - 2 comments
19. `src/threading/locks.c` - 1 definition
20. `src/threading/config.c` - 1 definition

**Test Files (tests/):**
1. `tests/unit/test_simplex_iteration.c` - 1 extern + 4 calls
2. `tests/unit/test_basis.c` - 51+ declarations/calls (snapshot family)
3. `tests/unit/test_matrix.c` - 2 externs + 6 calls
4. `tests/unit/test_error.c` - 2 externs + 9 calls
5. `tests/unit/test_memory.c` - 1 extern + 1 call
6. `tests/unit/test_callbacks.c` - 2 externs + 8 calls
7. `tests/unit/test_threading.c` - 1 extern + 6 calls

### Migration Complexity

**High Risk (extensive test coverage, multiple call sites):**
- `cxf_basis_snapshot` family (57+ test occurrences, 11 production occurrences)
- `cxf_simplex_iterate` (13 total occurrences)
- `cxf_sort_indices` (13 total occurrences)

**Medium Risk (moderate usage):**
- `cxf_basis_refactor` (12 total occurrences)
- `cxf_check_nan_or_inf` (11 total occurrences)
- `cxf_pre_optimize_callback` (10 total occurrences)
- `cxf_post_optimize_callback` (10 total occurrences)
- `cxf_set_thread_count` (10 total occurrences)

**Low Risk (minimal usage):**
- `cxf_errorlog` (9 total occurrences)
- `cxf_free_solver_state` (4 total occurrences)
- `cxf_simplex_cleanup` (3 total occurrences)
- `cxf_acquire_solve_lock` (2 total occurrences)
- `cxf_cleanup_helper` (1 total occurrence)

---

## Recommendations

### Option 1: Comprehensive Migration (Recommended)

Rename all 15 implemented functions in a single coordinated effort:

1. **Pre-migration:**
   - Ensure all tests pass with current names
   - Create feature branch for rename
   - Document current test coverage

2. **Migration order (by risk):**
   - Start with low-risk functions (1 occurrence)
   - Progress to medium-risk (4-12 occurrences)
   - End with high-risk (13+ occurrences)

3. **For each rename:**
   - Update header declarations
   - Update source definitions
   - Update all call sites
   - Update test declarations/calls
   - Update comments/documentation
   - Run full test suite

4. **Post-migration:**
   - Verify 100% test pass rate
   - Update build system if needed
   - Update any generated documentation

### Option 2: Incremental Migration

Rename functions module-by-module:

1. **Phase 1: Utilities** (low coupling)
   - cxf_cleanup_helper → cxf_propagate_bounds
   - cxf_sort_indices → cxf_sort_by_values

2. **Phase 2: Error/Threading** (system-level)
   - cxf_errorlog → cxf_set_error_string
   - cxf_check_nan_or_inf → cxf_is_finite
   - cxf_set_thread_count → cxf_validate_thread_count
   - cxf_acquire_solve_lock → cxf_save_locale_state

3. **Phase 3: Callbacks** (isolated subsystem)
   - cxf_pre_optimize_callback → cxf_pre_optimize_hook
   - cxf_post_optimize_callback → cxf_post_optimize_hook

4. **Phase 4: Basis** (high coupling, high risk)
   - cxf_basis_refactor → cxf_fix_variables_at_bounds
   - cxf_basis_snapshot → cxf_progress_snapshot (entire family)

5. **Phase 5: Simplex** (core solver)
   - cxf_simplex_iterate → cxf_log_iteration_progress
   - cxf_simplex_cleanup → cxf_simplex_postsolve

6. **Phase 6: Memory** (foundational)
   - cxf_free_solver_state → cxf_free_attribute_table

### Option 3: Status Quo (Not Recommended)

Keep v1 names in code, v2 names only in specs-v2 documentation. This creates permanent divergence between code and spec.

**Consequences:**
- Every new developer must learn two naming systems
- Spec-to-code tracing becomes error-prone
- Future maintainers will be confused
- Documentation becomes less trustworthy

---

## Testing Requirements

### Test Updates Per Rename

For each renamed function:
1. Update extern declarations
2. Update test function names (if test name includes function name)
3. Update all function calls in test assertions
4. Update comments referencing the function
5. Verify test still passes with identical coverage

### Regression Testing

After each rename (or batch of renames):
1. Run full unit test suite
2. Run integration tests (if any)
3. Run valgrind/memory checks
4. Verify no test coverage loss

### Build System

Check if any of these names appear in:
- Makefiles
- CMakeLists.txt
- Documentation generators
- Code analysis scripts
- Performance profiling scripts

---

## Conclusion

All 16 v1 names from the rename_misnomers.py script are still present in the codebase. Of these, 15 are implemented functions actively used in production code and tests. A comprehensive migration is needed to align the codebase with the v2 specification.

**Recommended Action:** Execute Option 1 (Comprehensive Migration) in a dedicated feature branch with full test coverage verification at each step.

**Estimated Effort:**
- Low-risk renames: 0.5 hour each (5 functions = 2.5 hours)
- Medium-risk renames: 1 hour each (7 functions = 7 hours)
- High-risk renames: 2 hours each (3 functions = 6 hours)
- **Total: ~15-20 hours** for complete migration with testing

**Risk Mitigation:**
- Automated search/replace with whole-word matching
- Test-driven: verify tests pass after each rename
- Git branching: easy rollback if issues arise
- Documentation: update HANDOFF.md to reflect completion
