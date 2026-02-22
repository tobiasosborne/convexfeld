# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: V2 Roadmap Complete (48/48). Netlib: 19/114 pass (1s timeout).

### What Was Done This Session (26 issues closed)

This session completed the entire v2 compliance roadmap critical path:

**Phase 4 — Pricing Rebuild (9/9 DONE):**
- P4.2: `cxf_pricing_update_var` — CSC column traversal producer (update_var.c)
- P4.3: `cxf_pricing_update_constr` — CSR row traversal producer (update_constr.c)
- P4.4: `cxf_pricing_update_queues` — V2 queue consumer with promote/demote (update.c)
- P4.5: `cxf_pricing_candidates_v2` — adaptive retrieval with 3-threshold strategy (candidates.c)
- P4.6: `cxf_pricing_end_level` — V2 cache invalidation + level activation (queue.c)
- P4.7: `cxf_pricing_mark_dirty` — now inserts into V2 4-bit flag queues (queue.c)
- P4.8: V2 wiring in step.c — removed V1 cascade fallback, Dantzig fallback for empty queues
- P4.9: `cxf_pricing_update_weights` — DSE simplified + Devex rank-1 updates (weight_update.c)

**Phase 5 — Robustness (4/4 DONE):**
- P5.1: EXPAND perturbation uses V2 pricing candidates instead of full scan
- P5.2: Bound drift prevention via saved bounds in analyze_basic
- P5.3: Crash basis integration — singleton structural columns replace artificials
- P5.4: Bland's rule triggers only after perturbation failure

**Phase 6 — Orchestration (7/7 DONE):**
- P6.1: Postsolve with fixed-variable restoration, wired into solve_lp.c
- P6.2: Complementary slackness fix in cxf_simplex_final + pricing free
- P6.3: Solution extraction for ITERATION_LIMIT and TIME_LIMIT
- P6.4: Parameter backup/restore across solve
- P6.6: Progress logging with time-throttled callback invocation
- P6.7: Dead code audit (most already removed)

**Remaining phases also completed:**
- P2.3: EtaBuffer arena allocator (eta_pool.c) with O(1) bulk reset
- P2.4: cxf_fix_variables_at_bounds clears etas via pool reset
- P3.2: SolutionData struct with iteration stats, anti-cycling history
- P3.3: Missing SolverState fields (num_slacks, solve_mode_alt, thresholds[6])
- P3.4: Missing BasisState fields (eta_row_count, numerical_flag, fill_in_estimate)
- e2t: DRY fix — shared cxf_eta_list_clear
- y1ro: Presolve CSR fast path (already implemented)
- c4bh: Constraint satisfaction tests (already exist)

### New Files Created This Session
- `src/pricing/queue_insert.c` (142 LOC) — V2 4-bit flag insertion helpers
- `src/pricing/update_var.c` (70 LOC) — P4.2 CSC producer
- `src/pricing/update_constr.c` (74 LOC) — P4.3 CSR producer
- `src/pricing/weight_update.c` (120 LOC) — P4.9 DSE/Devex weight updates
- `src/basis/eta_pool.c` (157 LOC) — P2.3 arena allocator + shared eta list clear
- `tests/unit/test_pricing_v2.c` (300 LOC) — 11 V2 pricing unit tests

### Tests: 40/40 unit+integration pass throughout

---

## Netlib Benchmark Results (1s timeout per instance)

```
PASS:    19 / 114
FAIL:    30 (infeasible=6, unbounded=18, wrong_obj=6)
TIMEOUT: 65
```

### Passed (19):
adlittle, afiro, agg, agg2, agg3, beaconfd, blend, lotfi, sc50a, sc50b, sc105, sc205, scagr7, sctap1, share1b, share2b, ship04l, ship04s, stocfor1

### Failure Modes (30 non-timeout failures):

**False INFEASIBLE (6):** bandm, bore3d, e226, scorpion, stair, tuff
- Phase I declares infeasible on problems that are feasible.
- Root cause likely: Phase I → II transition, artificial variable handling, or numerical issues in LU.

**False UNBOUNDED (18):** boeing1, boeing2, capri, etamacro, finnis, fit1d, fit1p, grow7, nesm, perold, pilot4, recipe, scfxm1, scsd1, scsd6, scsd8, seba, vtp.base
- LARGEST failure category. Ratio test returns UNBOUNDED when it shouldn't.
- Root cause hypotheses:
  1. Free variables / variables with both finite bounds not handled correctly in ratio test
  2. BFRT not flipping when it should (bound-flip path skipped)
  3. Negative entering direction (AT_UPPER) with ratio test sign issues
  4. Refactorization not triggered often enough → numerical drift → wrong pivotCol → empty ratio test

**Wrong Objective (6):**
- brandy: 0.04% error (close — likely numerical drift)
- forplan: 43% error (wrong answer)
- israel: 9.4% error (wrong answer)
- standata, standgub, standmps: sign wrong (negative vs positive) — likely MPS parsing maximization issue

**Timeout (65):**
- All medium-large problems (>500 vars). Expected with 1s limit and dense LU working matrix.

---

## Next Steps — Failure Investigation

**MANDATORY PROTOCOL: Detective work, NOT fixes.**
1. Only run small failing instances for fast feedback
2. Take holistic view — failures may be from component interaction
3. NO local bandaid fixes — catalog errors
4. Understand HOW instances fail before ANY remediation
5. Only deliverable: a remediation plan

### Recommended Investigation Order:

**Tier 1 — Small false UNBOUNDED (fastest feedback):**
- `recipe` (180 vars, 91 constrs) — very small, should solve instantly
- `scsd1` (760 vars, 77 constrs) — small constraints
- `grow7` (140 vars, 140 constrs) — square, tiny
- `vtp.base` (203 vars, 198 constrs) — tiny
- `boeing2` (143 vars, 166 constrs) — tiny

**Tier 2 — Small false INFEASIBLE:**
- `scorpion` (358 vars, 388 constrs)
- `tuff` (587 vars, 333 constrs)
- `bandm` (472 vars, 305 constrs)

**Tier 3 — Wrong objective (sign):**
- `standata` (1075 vars, 359 constrs) — sign inversion suggests max/min confusion

### Investigation Techniques:
- Compile with `-DDEBUG_PHASE1` for Phase I tracing
- Add iteration-by-iteration obj/status tracing in step.c
- Check MPS parser for OBJSENSE/RANGES handling
- Check entering direction `s` for AT_UPPER variables in ratio_test.c
- Check BFRT flip eligibility conditions
- Trace specific failing pivot sequences

---

## File Locations

| Item | Path |
|---|---|
| Main solver loop | `src/simplex/solve_lp.c` |
| Step iteration engine | `src/simplex/step.c` |
| Ratio test | `src/simplex/ratio_test.c` |
| Phase I setup + transition | `src/simplex/phase_one.c` |
| Phase I termination | `src/simplex/phase_loop.c` |
| FTRAN | `src/basis/ftran.c` |
| BTRAN | `src/basis/btran.c` |
| LU factorization | `src/basis/lu_factorize.c` |
| Refactorization | `src/basis/refactor.c` |
| Reduced costs | `src/simplex/reduced_costs.c` |
| Perturbation | `src/simplex/perturbation.c` |
| Crash basis | `src/simplex/crash.c` |
| Postsolve | `src/simplex/cleanup.c` |
| V2 pricing | `src/pricing/queue_insert.c`, `update_var.c`, `update_constr.c` |
| V2 candidates | `src/pricing/candidates.c` |
| V2 queue consumer | `src/pricing/update.c` |
| Weight updates | `src/pricing/weight_update.c` |
| MPS parser | `src/api/mps_parse.c`, `mps_build.c`, `mps_state.c` |
| Netlib benchmark | `benchmarks/bench_netlib.c` |
| Reference solutions | `benchmarks/netlib/feasible_reference_1e-8.csv` |
| V2 compliance roadmap | `docs/v2_compliance_roadmap.md` |
| Unit tests | `tests/unit/` (40 tests) |
| Integration tests | `tests/integration/` (3 tests) |

## DO NOT
- Run Netlib with >1s timeout during investigation
- Apply quick fixes before holistic understanding
- Modify solver code until remediation plan is written
- Skip the investigation protocol
