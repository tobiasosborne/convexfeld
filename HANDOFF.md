# Agent Handoff

*Last updated: 2026-02-21*

---

## STATUS: Phase 0 complete (10/10), Phase 1 complete (7/7), Phase 2 partial (3/6)

### Session Summary

**12-agent v2 spec compliance review** → `docs/v2_compliance_roadmap.md` (48 beads issues, 6 phases).

**Phase 0 (10/10 closed):** All critical bug fixes — ratio test direction, objective sign, leaving var status, aux coefficient, BTRAN error propagation, pivot element filter, refactor check, extract solution status, BFRT cascade, diag_coeff preservation.

**Phase 1 (7/7 closed):** Removed `correct_basic_variables` hack, enabled `phase_end` in Phase I, forced refactorization at transition, artificial pivot-out, pricing reset, proactive perturbation, perturbation candidate-removal-not-bound-modification.

**Phase 2 (3/6 closed):** Hyper-sparse FTRAN/BTRAN, unified tolerance constants, BTRAN error propagation (via P0.5). Remaining: P2.1 (sparse LU), P2.2 (FTRAN residual monitoring), P2.3 (eta memory pool), P2.4 (fix_variables_at_bounds).

### Test Results

- **39/39 unit tests pass** (no regressions)
- **DO NOT run Netlib benchmarks** — expected to fail until Phase 2+ complete

---

## Next Steps — Critical Path

### P2.1 (uxae): Sparse Markowitz LU — MAJOR REWRITE

This is the critical path bottleneck. Current dense O(m^4) LU in `lu_factorize.c` is 47-65% of runtime. Needs:
- Compressed sparse column storage (replace dense m×m matrix)
- Maintained column maxima (eliminate O(m) col_max scan per step)
- Linked-list row/column count structures
- Target: O(nnz * fill_in) per factorization

**This is a ~300 line rewrite.** The file is `src/basis/lu_factorize.c`.

### After P2.1

- `epf7` P2.2: FTRAN residual monitoring (add ||Bx-a|| check)
- `auj4` P2.3: Eta memory pool (bump allocator)
- `uyfk` P2.4: Implement cxf_fix_variables_at_bounds properly

### Remaining Phase 1 items that are unblocked but lower priority

None — Phase 1 is complete.

---

## Issue Scoreboard

| Phase | Total | Closed | Remaining |
|-------|-------|--------|-----------|
| P0 | 10 | 10 | 0 |
| P1 | 7 | 7 | 0 |
| P2 | 6 | 3 | 3 (P2.1, P2.2, P2.3, P2.4) |
| P3 | 5 | 0 | 5 |
| P4 | 9 | 0 | 9 |
| P5 | 4 | 0 | 4 |
| P6 | 7 | 0 | 7 |

**Critical path:** P2.1 → P3.1 → P4.1 → P4.5 → P5.1

---

## File Locations

| Item | Path |
|---|---|
| V2 compliance roadmap | `docs/v2_compliance_roadmap.md` |
| LU factorize (needs rewrite) | `src/basis/lu_factorize.c` |
| Phase I transition | `src/simplex/phase_one.c` |
| Phase loop | `src/simplex/phase_loop.c` |
| V2 specs | `docs/specs-v2/specs/modules/` |
| Beads issues | `bd ready` / `bd list --status=open -n 100` |
