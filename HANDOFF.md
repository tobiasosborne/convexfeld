# Agent Handoff

*Last updated: 2026-02-23*

---

## STATUS: 22/35 Netlib pass. 40/40 unit tests. 3 P0 memory safety bugs fixed.

### Scorecard

**PASS (22):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226

**FAIL (13):** etamacro (0.016%), recipe (0.02%), boeing2 (0.09%), scagr25 (2.2%), bore3d (7.5%), finnis (7.3%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

---

## Work Completed This Session (2026-02-23)

### P0 Issues Fixed (3)

**convexfeld-pdv0: Integer overflow + realloc double-free in lu_factorize.c** — CLOSED
- lu_factorize.c:293: explicit `(size_t)m * (size_t)m` cast
- lu_factorize.c: 3 realloc sites fixed with assign-immediately pattern (eliminate_step L arrays, build_lu_output U arrays, build_lu_output L arrays)
- mps_state.c: same realloc pattern fix (removed incorrect `cxf_free(new_idx)` that caused double-free)

**convexfeld-u7f3: Memory leak — state_cleanup.c only frees 6 of 24+ arrays** — CLOSED
- `cxf_free_attribute_table()` now frees all dynamically allocated SolverState fields (was 6, now 24+): work_counter/column/cB, saved_lb/ub, min/max_activity, row_status/col_nz_count, csc_*/csr_* (8 arrays), work_rhs/sense, row/col_scale, timing
- Added NULL guard on `cxf_pricing_free()` call

**convexfeld-0drc: Silent FTRAN/BTRAN corruption on malloc failure** — CLOSED
- ftran.c: `apply_lu_solve` changed from `void` to `int`, returns `CXF_ERROR_OUT_OF_MEMORY`; `cxf_ftran` propagates error
- btran.c: `apply_lu_btran` changed from `void` to `int`; both `cxf_btran` and `cxf_btran_vec` propagate error
- recompute.c: `cxf_ftran_residual` returns `INFINITY` on error (was `0.0` which looked like "perfect accuracy")

### Priority Fix Order (remaining)

| Priority | What | Issues | Impact |
|----------|------|--------|--------|
| P0 | Delete ODR-violating stub files | convexfeld-3lpg | 15 min |
| P0 | Fix crash.c model mutation | convexfeld-v0s3 | 30 min |
| P0 | Fix error/status code values | convexfeld-7rvr | 1 hr |
| P1 next | Create internal headers | convexfeld-mxjm | 2 hrs |
| P1 | Add Kahan summation | convexfeld-heyz | 1 day |
| P1 | Fix eta_count sync | convexfeld-mo98 | 15 min |
| P1 | Fix CS ordering | convexfeld-9wdg | 15 min |
| P1 | Eliminate hot-path malloc | convexfeld-7nyb | 1 hr |
| P1 | Add core algorithm tests | convexfeld-sxgk | 1 day |

---

## DO NOT
- Enable scaling without fixing C1+C3 (pivot_update) and H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Skip reading this file and `docs/learnings/implementation_audit.md`
- Run Netlib benchmarks (currently broken, that's OK — focus on fixing the code first)
