# Agent Handoff

*Last updated: 2026-02-20*

---

## STATUS: Major v2 compliance push — 5 commits, all core iteration functions rewritten

### Session Summary

**V2 spec compliance overhaul.** Upgraded beads to Dolt backend. Conducted full v2 audit. Rewrote 6 core functions across 8 files. Net -250 LOC (cleaner, less code, more functionality).

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

### V2 Compliance Status

| Function | Before | After |
|---|---|---|
| `cxf_simplex_step` | 40% | **75%** |
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
- **8 Netlib failures**: all INFEASIBLE except israel (wrong obj). False UNBOUNDED eliminated by BFRT fix.

---

## Next Steps (Priority Order)

### 1. Investigate kb2/bore3d false UNBOUNDED
kb2 was passing before this session (HANDOFF showed it passing). The BFRT or cascade changes may have introduced a regression. Debug by bisecting commits.

### 2. Remaining v2 gaps
- **step**: Tolerance tiers, tight bound handling via `cxf_pivot_bound`, free variable handling
- **step2/step3**: Eta records for bound changes, two-stage infeasibility detection
- **refine**: Missing Pass 2 (basic variable recovery near upper bounds)
- **log_iteration_progress**: Needs logging infrastructure (deferred)

### 3. False INFEASIBLE root cause
The 4 false INFEASIBLE problems (bandm, brandy, share1b, scorpion) are Phase I degeneracy — the solver can't find improving directions. These need either:
- Better crash basis (more structural variables in initial basis)
- Actual bound perturbation (random ε to break ties, not just variable removal)
- Or dual simplex (which avoids Phase I entirely)

---

## File Locations

| Item | Path |
|---|---|
| step.c (BFRT engine) | `src/simplex/step.c` |
| phase_steps.c (step2/step3) | `src/simplex/phase_steps.c` |
| post.c (phase_end + post_iterate) | `src/simplex/post.c` |
| setup.c (activity bounds) | `src/simplex/setup.c` |
| perturbation.c (EXPAND) | `src/simplex/perturbation.c` |
| queue.c (cascade) | `src/pricing/queue.c` |
| solve_lp.c (orchestrator) | `src/simplex/solve_lp.c` |
| V2 specs | `docs/specs-v2/specs/modules/` |
