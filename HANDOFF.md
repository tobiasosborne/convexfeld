# Agent Handoff

*Last updated: 2026-02-16*

---

## STATUS: P0 Tolerance Fix COMPLETE — Next: P0 Variable Status Encoding

### Session Summary

1. **P0 tolerance fix (convexfeld-nso9) — CLOSED**
   - Aligned all tolerance #defines to v2 spec values
   - Added 6 named constants to `cxf_types.h` (CXF_MIN_PIVOT, CXF_MARKOWITZ_TOL, CXF_BOUND_EQUALITY_TOL, CXF_PERTURB_FLOOR, CXF_PERTURB_CEILING, CXF_LARGE_BOUND_MARKER)
   - Fixed perturbation.c to use spec values as absolute magnitudes (not multiplied by feas_tol)
   - Fixed pivot_check.c infinity thresholds to use CXF_INFINITY
   - Removed duplicate perturbation/unperturb stubs from context.c that were shadowing real implementations
   - Fixed pre-existing test_unperturb_sequence failure (static state leak + stub override)
   - Fixed test_pivot_check threshold test for new infinity value
   - All 36/36 tests pass

### Changes Made

| File | Change |
|------|--------|
| `include/convexfeld/cxf_types.h` | CXF_PIVOT_TOL 1e-10→1e-9, added 6 new named constants |
| `src/basis/lu_factorize.c` | MARKOWITZ_THRESHOLD 0.1→CXF_MARKOWITZ_TOL, MIN_PIVOT 1e-12→CXF_MIN_PIVOT |
| `src/simplex/perturbation.c` | PERTURB_BASE_SCALE→CXF_PERTURB_FLOOR, PERTURB_MAX_SCALE→CXF_PERTURB_CEILING, use as absolute values |
| `src/basis/refactor.c` | MIN_PIVOT_TOL 1e-10→CXF_PIVOT_TOL (1e-9) |
| `src/error/pivot_check.c` | Infinity thresholds -1e99/1e99→-CXF_INFINITY/CXF_INFINITY |
| `src/simplex/context.c` | Removed perturbation/unperturb stubs (real impl is in perturbation.c) |
| `tests/unit/test_pivot_check.c` | Updated unbounded lower test to use -2e100 |
| `tests/unit/test_simplex_edge.c` | Added unperturb cleanup in test_perturbation_basic |

### Values NOT Changed (already correct)
- CXF_ZERO_TOL 1e-12, CXF_INFINITY 1e100, CXF_FEASIBILITY_TOL 1e-6, CXF_OPTIMALITY_TOL 1e-6
- pivot_special.c THRESHOLD 1e-10, refine.c NEAR_ZERO_TOL 1e-12

### Values Left for Investigation
- `pivot_primal.c` TINY_THRESHOLD 1e-8 — no direct spec mapping, used as relative scaling factor
- `perturbation.c` MIN_OBJ_COEFF 1e-8 — heuristic, not in spec

---

## NEXT STEP: P0 Variable Status Encoding (convexfeld-clow)

Run `bd show convexfeld-clow` for details. This is the second P0 issue.

### Critical Path

```
P0 tolerances (convexfeld-nso9) ← DONE ✓
  → P0 var status (convexfeld-clow) ← NEXT
    → P1 16 function renames (convexfeld-b7ow) — unblocks 2
      → P1 4 struct renames (convexfeld-dv0k) — unblocks 5
        → P2 decompose solve_lp (convexfeld-23p6) — unblocks 5
```

---

## File Locations

| Item | Path |
|------|------|
| Audit reports (20) | `docs/audit/01_*.md` through `20_*.md` |
| Remediation plan | `docs/audit/REMEDIATION_PLAN.md` |
| V2 specs (ground truth) | `docs/specs-v2/specs/` |
| Tolerance spec | `docs/specs-v2/specs/reference/tolerances_constants.md` |
| FUNCTION_MAP | `docs/specs-v2/FUNCTION_MAP.md` |
| Learnings | `docs/learnings/` |
