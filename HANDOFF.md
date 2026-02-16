# Agent Handoff

*Last updated: 2026-02-16*

---

## STATUS: P0 Variable Status Encoding COMPLETE — Next: P1 Function Renames

### Session Summary

1. **P0 variable status encoding (convexfeld-clow) — CLOSED**
   - Replaced `CxfVarStatus` enum (0-4) with spec-compliant `#define` constants
   - New encoding: `>=0` = basic (value is row index), `-1` = AT_LOWER, `-2` = AT_UPPER, `-3` = SUPERBASIC, `-4` = FIXED
   - Added `CXF_VAR_IS_BASIC(status)` helper macro
   - Fixed `warm.c` consistency check to verify `var_status[var] == row` (not just `!= 0`)
   - Fixed `helpers.c` non-basic check to use `< 0` instead of `!= CXF_BASIC`
   - Fixed `basis_state.c` to initialize var_status to `CXF_VAR_AT_LOWER` (-1) instead of 0
   - Updated all test_basis.c cases to use row-index encoding for basic vars
   - All 36/36 tests pass

2. **P0 tolerance fix (convexfeld-nso9) — CLOSED** (previous session)

### Changes Made

| File | Change |
|------|--------|
| `include/convexfeld/cxf_types.h` | Removed `CxfVarStatus` enum, added `CXF_VAR_AT_LOWER/AT_UPPER/SUPERBASIC/FIXED` defines + `CXF_VAR_IS_BASIC` macro |
| `src/basis/basis_state.c` | Default var_status init changed from 0 (calloc) to CXF_VAR_AT_LOWER (-1) |
| `src/basis/warm.c` | Consistency check: `!= CXF_BASIC` → `!= row` (proper row-index check) |
| `src/solver_state/helpers.c` | Non-basic check: `!= CXF_BASIC` → `< 0` |
| `tests/unit/test_basis.c` | All snapshot/validate tests updated to use row-index encoding |

### Note: Local #defines still exist in pricing/pivot files
The following files define their own local constants (VAR_AT_LOWER=-1, AT_LOWER=-1, etc.) which are correct but could be consolidated to use the central CXF_VAR_* constants. This is cleanup, not a correctness issue:
- `src/pricing/phase.c`, `candidates.c`, `steepest.c`
- `src/simplex/pivot_special.c`
- `tests/unit/test_pricing.c`

---

## NEXT STEP: P1 Function Renames (convexfeld-b7ow)

Run `bd show convexfeld-b7ow` for details. Apply 16 v2 function/struct renames across src/tests/include (~155 occurrences).

### Critical Path

```
P0 tolerances (convexfeld-nso9) ← DONE ✓
  → P0 var status (convexfeld-clow) ← DONE ✓
    → P1 16 function renames (convexfeld-b7ow) ← NEXT
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
