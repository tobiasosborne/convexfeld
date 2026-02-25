# Agent Handoff

*Last updated: 2026-02-25*

---

## STATUS: 24/35 Netlib pass. 44/44 unit tests. Audit items C1-C4, H1-H3 complete.

### Scorecard

**PASS (24):** afiro, sc50b, sc105, share2b, israel, adlittle, blend, lotfi, beaconfd, stocfor1, ship04l, scfxm1, standata, standgub, standmps, scagr7, sctap1, brandy, scorpion, kb2, stair, e226, **recipe**, **scagr25**

**FAIL (11):** etamacro (0.016%), boeing2 (2.4%), bore3d (7.5%), finnis (7.1%), capri (10%), grow7 (12.5%), boeing1 (18%), vtp.base (20%), forplan (43%), scsd1 (TIMEOUT), bandm (TIMEOUT), tuff (TIMEOUT)

### DO NOT run Netlib suite in CI/agent sessions — it is slow. Run manually if needed.

---

## Work Completed This Session (2026-02-25)

### 7 issues closed, 2 new Netlib PASS (recipe, scagr25)

#### convexfeld-xd5l: C4 Phase I unboundedness suppression — CLOSED

Spec pivot_operations.md line 247: Phase I unboundedness doesn't imply original problem unbounded.

**Changes:**
- **step.c:531**: When ratio test returns UNBOUNDED during Phase I, suppress: refactorize + recompute + continue instead of terminating
- **pivot_special.c:180,190**: Added `if (ctx->phase == 1) return CXF_OK` before both `CXF_UNBOUNDED` returns (future-proofing, currently dead code)
- **test_pivot_special.c**: New file, 10 tests
- **tests/CMakeLists.txt**: Registered new test target

#### convexfeld-70c0: H2 Two-stage infeasibility confirmation — CLOSED

step2/step3 returned CXF_INFEASIBLE immediately. Now recompute activity from scratch before confirming.

**Changes:**
- **phase_steps.c step2**: When lb > ub, recomputes activity for affected rows, restores bounds from saved_lb/saved_ub, re-derives implied bounds, only returns INFEASIBLE if confirmed
- **phase_steps.c step3**: When activity indicates violation, recomputes via `cxf_compute_activity_bounds(state, 1, &row)`, only returns INFEASIBLE if confirmed
- Filed convexfeld-rlll for phase_steps.c refactor (331 LOC)

#### convexfeld-ifo2: H1 Harris ratio test band formula — CLOSED (+2 PASS)

Wrong formula AND band 10x too tight. Two fixes:
1. Pass 1 now uses `(slack + harrisBand) / |d|` (per-candidate band per spec)
2. Band width: `10 * feasTol` (Maros 2003 §8.3), was raw `feasTol`

**Changes:**
- **ratio_test.c**: Pass 1 adds harrisBand to slack. Pass 2 uses theta_max directly (no `+ feasTol`)
- **Result: recipe and scagr25 now PASS**

#### convexfeld-94em: H3 Convergence criterion — CLOSED

Two fixes:
1. `cxf_basis_diff` now uses per-category normalization (iter/(m+1), rows/m, cols/n, props/(n+m)) instead of single max(n,m) denominator
2. Added 5-check dead zone before convergence testing

**Changes:**
- **basis_stub.c**: Per-category normalization, added delta_props
- **solve_lp.c**: Added inner_checks counter, 5-check dead zone
- **test_basis.c**: Updated expected value

#### convexfeld-n8hn: Tolerance leak fix — CLOSED

`env->optimality_tol` was permanently set 100x tighter during Phase I near-feasibility check in phase_loop.c. Fixed by always restoring before returning.

**Changes:**
- **phase_loop.c**: `int found = has_improving_direction(...)` + restore before `if (found) return 1`

#### convexfeld-6jjb: pivot_bound/special slack range — CLOSED

`cxf_pivot_bound` and `cxf_pivot_special` rejected slack variable indices. Changed `var >= num_vars` to `var >= num_vars + num_constrs`.

**Changes:**
- **pivot_special.c**: Both functions accept slack indices
- **test_pivot_bound.c**: Expanded arrays to 5 elements, added test_slack_var_accepted
- **test_pivot_special.c**: Updated test_invalid_var_too_large to var=5

#### convexfeld-ro9z: Refine eta records + accuracy pass — CLOSED

Two fixes:
1. **solve_lp.c**: Final accuracy pass (refactorize + recompute) now runs for ITERATION_LIMIT/TIME_LIMIT too
2. **refine.c**: Creates compact Variant 2 eta records when snapping nonbasic vars

### convexfeld-n9ok: Phase II primal accuracy — INVESTIGATED, OPEN

Investigated grow7 (4.5M units past bounds). Root cause is dense LU factorization quality — parameter tuning (tighter eta limits, tighter residual thresholds) does not help. Blocked on sparse LU (audit M2).

---

## Priority Fix Order (remaining)

| Priority | What | Issues | Impact |
|----------|------|--------|--------|
| P1 | Create internal headers | convexfeld-mxjm | Unblocks 6 issues |
| P1 | Flesh out pivot_special | convexfeld-lmkg | Unblocks BFRT |
| P2 | Sparse LU (M2) | new issue needed | Fixes timeouts + grow7/boeing1 |
| P2 | V1 pricing weight update | convexfeld-l0ca | Pricing quality |
| P2 | Phase I→II constraint cleanup | convexfeld-ic80 | Transition quality |

---

## DO NOT
- Enable scaling without testing H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Re-add recovery pivots in refine.c Pass 2 (changes basis during post-solve)
- Hack refactorization parameters to fix primal accuracy — needs sparse LU
- Skip reading this file and `docs/learnings/implementation_audit.md`
