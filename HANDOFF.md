# Agent Handoff

*Last updated: 2026-02-16*

---

## STATUS: P1 Function Renames COMPLETE — Next: P1 Struct Renames

### Session Summary

1. **P1 function renames (convexfeld-b7ow) — CLOSED**
   - Applied all 13 implemented v2 function renames (3 of 16 not yet implemented)
   - Renames applied across src/, include/, tests/unit/ via whole-word sed
   - All 36/36 tests pass, clean build

2. **P0 variable status encoding (convexfeld-clow) — CLOSED**
   - Replaced `CxfVarStatus` enum (0-4) with spec-compliant row-index encoding

3. **P0 tolerance fix (convexfeld-nso9) — CLOSED** (previous session)

### Renames Applied

| Old Name (v1) | New Name (v2) |
|---|---|
| `cxf_simplex_iterate` | `cxf_log_iteration_progress` |
| `cxf_simplex_cleanup` | `cxf_simplex_postsolve` |
| `cxf_basis_refactor` | `cxf_fix_variables_at_bounds` |
| `cxf_basis_snapshot_*` | `cxf_progress_snapshot_*` |
| `cxf_sort_indices` | `cxf_sort_by_values` |
| `cxf_sort_indices_values` | `cxf_sort_by_values_paired` |
| `cxf_check_nan_or_inf` | `cxf_is_finite` |
| `cxf_cleanup_helper` | `cxf_propagate_bounds` |
| `cxf_free_solver_state` | `cxf_free_attribute_table` |
| `cxf_errorlog` | `cxf_set_error_string` |
| `cxf_pre_optimize_callback` | `cxf_pre_optimize_hook` |
| `cxf_post_optimize_callback` | `cxf_post_optimize_hook` |
| `cxf_acquire_solve_lock` | `cxf_save_locale_state` |
| `cxf_set_thread_count` | `cxf_validate_thread_count` |

### Note: Local #defines still exist in pricing/pivot files
VAR_AT_LOWER=-1, AT_LOWER=-1 etc. in `src/pricing/phase.c`, `candidates.c`, `steepest.c`, `src/simplex/pivot_special.c`, `tests/unit/test_pricing.c` — correct values, consolidation is cleanup only.

---

## NEXT STEP: P1 Struct Renames (convexfeld-dv0k)

Run `bd show convexfeld-dv0k` for details. Rename 4 core structures:
- SolverContext → SolverState
- PricingContext → PricingState
- EtaFactors → EtaVector
- SparseMatrix → MatrixData

### Critical Path

```
P0 tolerances (convexfeld-nso9) ← DONE ✓
  → P0 var status (convexfeld-clow) ← DONE ✓
    → P1 function renames (convexfeld-b7ow) ← DONE ✓
      → P1 4 struct renames (convexfeld-dv0k) ← NEXT
        → P2 decompose solve_lp (convexfeld-23p6) — unblocks 5
```

---

## File Locations

| Item | Path |
|------|------|
| Audit reports (20) | `docs/audit/01_*.md` through `20_*.md` |
| Remediation plan | `docs/audit/REMEDIATION_PLAN.md` |
| V2 specs (ground truth) | `docs/specs-v2/specs/` |
| FUNCTION_MAP | `docs/specs-v2/FUNCTION_MAP.md` |
| Learnings | `docs/learnings/` |
