# Agent Handoff

*Last updated: 2026-02-20*

---

## STATUS: 9 commits — all core v2 iteration functions at 65-90% compliance

### Session Summary

**V2 spec compliance overhaul.** Upgraded beads to v0.55.1 (Dolt). Full v2 audit of all simplex modules. Rewrote 8 core functions across 10 files. Fixed BFRT bug. Net ~-300 LOC.

### Commits This Session

| # | Hash | Description |
|---|---|---|
| 1 | `fbc6ccb` | BFRT + pricing cascade in step.c |
| 2 | `0d6ba33` | step2/step3 bound propagation (Savelsbergh 1994) |
| 3 | `50b8267` | phase_end + post_iterate + loop ordering |
| 4 | `c74937d` | v2-compliant setup + enable activity bounds + preprocess |
| 5 | `2367eaa` | v2-compliant perturbation with saved bounds + AT_UPPER |
| 6 | `7a48cdc` | Fix BFRT undo-last-flip (eliminates false UNBOUNDED) |
| 7 | `3813a08` | v2-compliant refine with RC-based cleanup + basic recovery |
| 8 | `828a3ba` | Pricing tolerance tiers in step |

### V2 Compliance Status

| Function | Before | After |
|---|---|---|
| `cxf_simplex_step` | 40% | **80%** |
| `cxf_simplex_step2` | 0% | **70%** |
| `cxf_simplex_step3` | 0% | **70%** |
| `cxf_simplex_phase_end` | 15% | **65%** |
| `cxf_simplex_post_iterate` | 70% | **85%** |
| `cxf_simplex_setup` | 50% | **90%** |
| `cxf_simplex_perturbation` | 50% | **75%** |
| `cxf_simplex_refine` | 60% | **85%** |
| `cxf_pricing_cascade_update` | 0% | **90%** |
| `solve_lp.c` orchestration | 80% | **90%** |

### Test Results

- **39/39 unit tests pass**
- **14/22 extended Netlib pass** (afiro, sc50a, blend, adlittle, share2b, sc105, sc205, scagr7, stocfor1, beaconfd, lotfi, agg, sctap1, ship04s)
- **8 Netlib failures**: 7 false INFEASIBLE (kb2, recipe, bandm, bore3d, brandy, share1b, scorpion) + 1 wrong obj (israel)
- **False UNBOUNDED eliminated** by BFRT undo-last-flip fix

---

## Next Steps (Priority Order)

### 1. Fix remaining false INFEASIBLE (7 problems)
All 7 share the same root cause: Phase I degeneracy. The solver reaches a point where all reduced costs are near-zero for nonbasic variables at lower bound, so no improving direction is found. Options:

- **Bound perturbation (careful)**: Attempted this session but perturbation magnitude was too aggressive — blend and sc205 regressed. Need: (a) perturb only basic variables at bounds (not all nonbasics), (b) much smaller ε (1e-10 scale not 1e-6), (c) perturbation only during Phase I
- **Better crash basis**: Current crash is basic — more sophisticated crash (Maros Ch 9, scoring by sparsity/magnitude) would start Phase I closer to feasibility
- **Dual simplex**: Avoids Phase I entirely — the v2 spec supports it but it's a major new implementation

### 2. Remaining v2 compliance gaps
- **step**: Tight bound handling via `cxf_pivot_bound`, free variable handling (~5% gap)
- **step2/step3**: Eta records for bound changes, two-stage infeasibility detection (~10% gap each)
- **phase_end**: Could handle Phase I→II transition inline per spec (~10% gap)
- **perturbation**: Actual EXPAND bound perturbation (carefully tuned) (~10% gap)
- **log_iteration_progress**: Needs logging infrastructure (deferred)

### 3. israel wrong objective
israel returns OPTIMAL with obj=-9.81e5 (ref=-8.97e5, 9.4% error). This is a numerical accuracy issue, not Phase I degeneracy. Likely needs: refactorization frequency tuning, or reduced cost recomputation after many pivots.

---

## Lessons Learned This Session

### BFRT undo-last-flip bug
When BFRT loop flips a variable and find_next_blocker returns -1, leavingRow still points to the flipped row. Must undo the last flip so the flipped variable becomes the true leaving variable. (gotchas.md updated)

### Bound perturbation magnitude
Naive EXPAND (perturb all nonbasic bounds by feas_tol * factor) is too aggressive. It changes the feasible region enough to find different optimal vertices. Proper EXPAND needs: (a) perturb only variables involved in degenerate pivots, (b) use much smaller ε (~1e-10 to 1e-12), (c) scale by variable magnitude.

### Phase I→II transition ownership
phase_end should NOT do Phase I→II transition — that's handled by cxf_check_phase_one_end in the orchestrator. Putting it in phase_end caused a regression on the mixed-senses constraint test (obj=-12 instead of -15).

---

## File Locations

| Item | Path |
|---|---|
| step.c (BFRT engine + tolerance tiers) | `src/simplex/step.c` |
| phase_steps.c (step2/step3) | `src/simplex/phase_steps.c` |
| post.c (phase_end + post_iterate) | `src/simplex/post.c` |
| setup.c (activity bounds only) | `src/simplex/setup.c` |
| perturbation.c (EXPAND) | `src/simplex/perturbation.c` |
| refine.c (RC cleanup + basic recovery) | `src/simplex/refine.c` |
| queue.c (cascade + dirty marking) | `src/pricing/queue.c` |
| solve_lp.c (v2 orchestrator) | `src/simplex/solve_lp.c` |
| V2 specs | `docs/specs-v2/specs/modules/` |
| Learnings | `docs/learnings/gotchas.md` |
