# Agent Handoff

*Last updated: 2026-02-23*

---

## STATUS: 22/35 Netlib pass. 40/40 unit tests. 4 bug fixes this session.

### Scorecard

**PASS (22):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226

**FAIL (13):** etamacro (0.016%), recipe (0.02%), boeing2 (0.09%), scagr25 (2.2%), bore3d (7.5%), finnis (7.3%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

---

## Work Completed This Session (2026-02-23)

### Issues Fixed (4)

**convexfeld-3lpg: P0 ODR violations — 3 functions defined in both stub and real files** — CLOSED
- Deleted 5 stub files: validation_stub.c, solve_lp_stub.c, error_stub.c, callback_stub.c, threading_stub.c
- Removed duplicate `cxf_addqconstr` from constr_stub.c (real in quadratic_api.c)
- Removed duplicate `cxf_log10_wrapper` from format.c (real in math_wrappers.c)
- Updated CMakeLists.txt (228 lines deleted)

**convexfeld-v0s3: P0 crash.c mutates original model matrix (CSR col_idx)** — CLOSED
- Removed `mat->col_idx[k] = -1` write in crash.c that corrupted original model
- Calling cxf_optimize() twice now produces consistent results
- Updated test_crash to verify model preservation (not buggy -1 sentinels)

**convexfeld-mo98: P1 eta_count in SolverState never incremented** — CLOSED
- Added `state->eta_count = basis->eta_count` after both cxf_pivot_with_eta callsites in step.c
- Eta-count-based refactorization trigger now functional (was permanently 0)

**convexfeld-9wdg: P1 Complementary slackness fix runs AFTER solution extraction** — CLOSED
- Moved CS correction from cxf_simplex_final (context.c) to solve_lp.c before cxf_extract_solution
- Added `#include "convexfeld/cxf_basis.h"` to solve_lp.c for BasisState definition

### Previous Session (also 2026-02-23)

**convexfeld-pdv0: Integer overflow + realloc double-free in lu_factorize.c** — CLOSED
**convexfeld-u7f3: Memory leak — state_cleanup.c only frees 6 of 24+ arrays** — CLOSED
**convexfeld-0drc: Silent FTRAN/BTRAN corruption on malloc failure** — CLOSED

### Priority Fix Order (remaining)

| Priority | What | Issues | Impact |
|----------|------|--------|--------|
| P0 | Fix error/status code values | convexfeld-7rvr | 1 hr |
| P0 | Implement full cxf_pivot_bound | convexfeld-h8xm | medium |
| P0 | Redesign cxf_pivot_update for Kahan-stable addition | convexfeld-heyz | 1 day |
| P1 | Create internal headers | convexfeld-mxjm | 2 hrs |
| P1 | Fix magic number clash (ENV == MODEL) | convexfeld-aal4 | 15 min |
| P1 | Fix helpers.c dead code (tautological comparison) | convexfeld-vk8l | 15 min |
| P1 | Fix model_stub.c partial realloc | convexfeld-yhmx | 30 min |
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
