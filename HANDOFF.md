# Agent Handoff

*Last updated: 2026-02-22*

---

## STATUS: Phase I rewritten to spec-compliant bound-violation approach. 40/40 tests pass.

### What Was Done This Session

1. **Full architectural audit** against v2 specs (17 spec files, 14 source files)
   - Cross-referenced implementation contracts across all simplex modules
   - Found spec internally contradicts itself on BFRT row negation (3 docs disagree)
   - Confirmed Phase I was entirely unspecified (until two_phase_method.md was added)

2. **New spec `two_phase_method.md` compliance audit**
   - Found FUNDAMENTAL mismatch: spec says implicit bound-violation Phase I, code had explicit artificials
   - Identified 9 specific violations
   - The explicit artificials approach caused test_geq_constraints bug

3. **Implemented Option A: Rewrote Phase I to match spec** (convexfeld-avjr)
   - Removed `art_coeff` field from SolverState
   - Shrunk all arrays from `n+2m` to `n+m` (15 files changed)
   - Rewrote `cxf_setup_phase_one` — slacks always basic, dynamic w coefficients
   - Added Phase I bound-crossing to ratio test (both passes) and compute_step
   - Added post-pivot w-coefficient recomputation in step.c
   - Simplified Phase I→II transition (no artificial pivot-out needed)
   - Unified fallback auxiliary coefficients to unconditional convention
   - **Result: 40/40 tests pass** (test_geq_constraints now passes for first time)

4. **Post-rewrite architectural audit** — found and fixed:
   - Missing `cxf_compute_reduced_costs` after Phase I→II transition
   - Objective recomputation at transition only covered [0,n), should cover [0,n+m)
   - RHS-conditional fallback functions inconsistent with unconditional diag_coeff

5. **Diagnostic validation (boeing1, capri, finnis, stair)**
   - boeing1: Phase I succeeds (obj=936→0 in 120 iters), Phase II has numerical drift
   - capri: Free variable entering bug found and fixed (x = lb + step = -1e100 for free vars)
   - finnis: Phase I gets to obj=0.009 but can't finish — degeneracy, needs perturbation
   - stair: 87% degenerate pivots, 3 real improvements in 200 iters — needs perturbation

6. **Fixed free variable entering bug** (step.c cxf_apply_pivot)
   - Bug: `work_x[entering] = work_lb[entering] + step` → -1e100 for free vars
   - Fix: `work_x[entering] = work_x[entering] + step` (use current value, not lb)

### Netlib Impact

| Instance | Before | After |
|----------|--------|-------|
| scorpion | INFEASIBLE (RC2 bug) | **PASS** |
| israel | wrong obj (RC2 bug) | **PASS** |
| share2b | PASS | PASS |
| boeing1 | INFEASIBLE | wrong obj (Phase II needs work) |
| etamacro | PASS | 0.08% obj error (minor regression) |
| recipe | UNBOUNDED | 0.76% obj error (improvement) |

---

## Next Steps

1. **Phase I degeneracy** — finnis, stair, capri, tuff all get close but can't
   finish Phase I. Root cause: deep degeneracy with insufficient perturbation.
   The perturbation system needs improvement (currently too weak/inactive).

2. **RC3 (OBJSENSE)** and **RC4 (RANGES)** — straightforward parser additions.

3. **Phase II numerical drift** — boeing1, e226 reach OPTIMAL but with wrong obj.
   Pre-existing issue now visible because Phase I succeeds.

## Key Files Modified

| File | Change |
|------|--------|
| `include/convexfeld/cxf_solver.h` | Removed `art_coeff` field |
| `src/simplex/phase_one.c` | **Rewritten** — implicit bound-violation Phase I |
| `src/simplex/step.c` | Phase I w-update, removed artificial branch, fixed compute_step |
| `src/simplex/ratio_test.c` | Phase I bound-crossing guards in both passes |
| `src/simplex/reduced_costs.c` | Removed artificial branch, unconditional fallback |
| `src/simplex/phase_loop.c` | Phase I obj = sum of violations |
| `src/simplex/context.c` | Arrays n+m, removed art_coeff alloc |
| `src/basis/lu_factorize.c` | Removed artificial column extraction |
| `src/basis/refactor.c` | Simplified diagonal check |
| `src/simplex/cleanup.c` | n+m bound |
| `src/simplex/perturbation.c` | n+m bound |
| `src/simplex/post.c` | n+m bound |
| `src/simplex/refine.c` | n+m bound |
| `src/pricing/weight_update.c` | n+m bound |
| `docs/learnings/gotchas.md` | Phase I rewrite learnings |

## DO NOT
- Run full Netlib suite (use targeted `--filter` or diagnostic tool)
- Skip reading `docs/remediation_plan.md`
- Re-introduce explicit artificial variables (the spec says don't)
