# Agent Handoff

*Last updated: 2026-02-17*

---

## STATUS: P1 Constraint satisfaction tests COMPLETE + >= solver bug FIXED

### Session Summary

1. **P1 constraint satisfaction tests (convexfeld-c4bh) — CLOSED**
   - Created `tests/integration/test_constraint_satisfaction.c` (4 tests)
   - Tests verify constraint satisfaction AND variable bounds in solutions
   - Covers all 3 constraint senses: <= , >=, = (mixed and pure)
   - Multi-constraint scenarios (2-4 constraints, 2-3 variables)
   - Registered in `tests/CMakeLists.txt`
   - 37/37 tests pass

2. **BUG FIX: >= constraints violated in Phase II (solver bug)**
   - **Root cause:** Phase I sets `diag_coeff=+1` for violated >= constraints (artificial).
     At Phase I→II transition, this was never flipped to `-1` (surplus direction).
     BTRAN uses `diag_coeff` for B₀^(-T) scaling, so the stale +1 produced wrong
     dual prices (pi), wrong reduced costs, and Phase II would violate >= constraints
     when the objective pushed against the constraint direction.
   - **Fix:** In `cxf_transition_to_phase_two()` (`src/simplex/phase_one.c`):
     flip `diag_coeff[i]` from +1 to -1 for all >= constraints, then force
     `cxf_solver_refactor()` to rebuild LU factors with the corrected basis.
   - **Impact:** >= constraints now work correctly in all cases (not just when
     the objective aligned with the constraint direction).

3. **Environment setup (new Linux install)**
   - Ubuntu 24.04 on WSL2
   - Installed: gcc 13.3, cmake 3.28, make, Go 1.22, bd 0.52.0 (beads)
   - bd requires `$HOME/go/bin` in PATH and `libicu-dev` for building

### Previous sessions (all CLOSED):
   - P2 decompose solve_lp (convexfeld-23p6)
   - P1 struct renames (convexfeld-dv0k)
   - P1 function renames (convexfeld-b7ow)
   - P0 variable status encoding (convexfeld-clow)
   - P0 tolerance fix (convexfeld-nso9)

---

## NEXT STEP: Check `bd ready` for remaining work

10 issues were ready at session start. 1 closed, 1 filed (refactor). Run `bd ready`.

---

## File Locations

| Item | Path |
|------|------|
| Constraint satisfaction tests | `tests/integration/test_constraint_satisfaction.c` |
| >= bug fix | `src/simplex/phase_one.c` (transition function) |
| Audit reports (20) | `docs/audit/01_*.md` through `20_*.md` |
| Remediation plan | `docs/audit/REMEDIATION_PLAN.md` |
| V2 specs (ground truth) | `docs/specs-v2/specs/` |
| FUNCTION_MAP | `docs/specs-v2/FUNCTION_MAP.md` |
| Learnings | `docs/learnings/` |
