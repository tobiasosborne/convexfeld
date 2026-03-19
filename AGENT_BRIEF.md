# Agent Brief — ConvexFeld LP Solver

*Read this instead of spending 10 minutes on discovery. Then read HANDOFF.md for details.*

---

## What This Is

A cleanroom C99 LP solver implementing the revised simplex method. The solver was built from V2 specs, but an 8-agent source comparison against a decompiled binary (`docs/convexfeld_source_comparison.md`) revealed the specs misidentified three core algorithms and had ~30 bugs. All 30 have been fixed. The solver passes 147/147 unit+integration tests.

## Where Things Stand (2026-03-19)

**Done:** All 30 source comparison fixes (3 algorithm rewrites + 27 bug fixes). Three rigorous review agents verified every fix against the comparison report: 42 CORRECT, 6 PARTIALLY CORRECT, 0 BUGS.

**The 6 gaps have beads issues filed.** The three most impactful:

| ID | Priority | Gap | Impact |
|----|----------|-----|--------|
| convexfeld-8fnv | P2 | T2.2: Perturbation should set `varStatus=-1` (active pricing removal), not just stop dirtying | Anti-cycling (kb2, recipe) |
| convexfeld-5q42 | P2 | T2.8: cleanup propagation needs 2-pass conservative+aggressive + iterative FBBT | Wrong objectives |
| convexfeld-gfvr | P2 | T3.7: Eta-mode expansion in pricing (linked-list traversal between refactorizations) | Timeouts |

**Netlib status is UNKNOWN** since the source comparison work. Last known: 46/114 pass (pre-comparison). The fixes should significantly improve this but no benchmark has been run.

## Workflow Rules (MANDATORY)

1. **Proposer/Reviewer protocol:** For every code change, spawn 2 proposer subagents → review & implement → spawn reviewer subagent to verify against `docs/convexfeld_source_comparison.md`
2. **Never run Netlib** (`bench_netlib`) unless the user explicitly asks
3. **Run commands once cleanly.** Don't retry 5 ways. If background, wait for notification.
4. **Spec V2 is the law.** `docs/specs-v2/` is the single source of truth. Deviations are bugs.
5. **200 LOC limit** per file. File a refactor issue if exceeded.
6. **Session close:** Record learnings → update HANDOFF.md → `bd sync` → `git commit` → `git push`. Work is NOT done until pushed.

## Key Files

| File | What |
|------|------|
| `HANDOFF.md` | Full session history, all fix details, next steps |
| `docs/convexfeld_source_comparison.md` | The 30-item comparison report (ground truth) |
| `src/simplex/step.c` | Iteration engine (764 LOC — pricing, FTRAN, ratio test, pivot, RC update) |
| `src/simplex/solve_lp.c` | Main solve loop (outer rounds, perturbation, convergence) |
| `src/simplex/ratio_test.c` | Single-pass SE-weighted ratio test (185 LOC) |
| `src/simplex/step2.c` | BFRT deferred bound-flip processing |
| `src/simplex/step3.c` | Constraint elimination |
| `src/simplex/final.c` | Post-solve variable fixing (5-phase) |
| `src/simplex/cleanup_propagate.c` | Implied bound propagation (Savelsbergh 1994) |
| `src/simplex/pivot_special.c` | Unboundedness detection + bound flip + row elimination |
| `src/basis/lu_factorize.c` | Sparse LU (Markowitz + dense phase) |

## Architecture in 30 Seconds

```
cxf_solve_lp (solve_lp.c)
  └─ outer rounds (mode-dependent: 5/10/100)
      └─ inner iterations:
          1. cxf_simplex_perturbation (proactive round 0, reactive after)
          2. cxf_simplex_step (step.c):
             pricing → FTRAN → ratio_test → BFRT → BTRAN → pivot → RC+weights → refactor
          3. cxf_simplex_step2 (BFRT post-processing)
          4. cxf_simplex_step3 (constraint elimination)
          5. cxf_simplex_phase_end (post-pivot only)
          6. cxf_basis_diff (convergence detection)
          7. cxf_simplex_post_iterate (stall, limits)
  └─ post-solve: unperturb → refine → final → extract → cleanup
```

## Quick Start

```bash
bd ready                    # See available work
bd show <id>                # Read issue details
bd update <id> --status in_progress  # Claim it
cmake --build build         # Build
ctest --test-dir build --timeout 5   # Test (should be <1s for 147 tests)
```

## What NOT To Do

- Don't run Netlib without being asked
- Don't lower EXPAND threshold below 100
- Don't reference GLPK or other solvers (cleanroom)
- Don't implement workarounds that pass tests but don't solve problems
- Don't delete/weaken rules in HANDOFF.md's DO NOT section
- Don't amend commits — always create new ones
