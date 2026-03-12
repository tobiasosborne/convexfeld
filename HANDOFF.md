# Agent Handoff

*Last updated: 2026-03-12*

---

## STATUS: 58/58 tests pass. V2 spec audit complete. 17 new deviation issues filed.

### Session Summary (2026-03-12)

**V2 Spec Compliance Audit:**
Ran 6 parallel audit agents covering all core spec modules against implementation.
Found 17 spec deviations (6 critical, 7 high, 4 medium). All filed as beads issues
with self-contained descriptions and chained dependencies.

**Fix applied:**
- C1 (convexfeld-hvbu CLOSED): Harris band = feasTol (was 10x feasTol). ratio_test.c:85.
  V2 spec harris_ratio_test.md line 188. 58/58 tests pass.

### Dependency Graph (unblocked → blocked)

```
READY (P1)                               BLOCKED
──────────                               ───────
convexfeld-4r3e  status enum ───────────→ convexfeld-4zq8  BFRT consolidation
convexfeld-k0rk  weighted basis diff ──┐
                                       ├→ convexfeld-cp29  Mechanism A/B sequencing
convexfeld-dm3g  refactor threshold ───┘
convexfeld-x9r0  level lifecycle ──────→ convexfeld-ro2u  constraint-side pricing
convexfeld-mjtu  pivot_primal V2 ──────→ convexfeld-exch  tight-bound processing
```

### Next Steps (priority order)

1. **convexfeld-4r3e** (P1): Add NORMAL_PIVOT/DEGENERATE_PIVOT/UNBOUNDED/BOUND_FLIP_ONLY
   status enum to ratio_test return. Unblocks C2 (BFRT consolidation).
2. **convexfeld-k0rk** (P1): Implement weighted basis diff scoring per perturbation.md
   Section 4.2. Unblocks C4 (Mechanism A/B sequencing).
3. **convexfeld-dm3g** (P2): Synchronize refactorization thresholds between post.c and
   step.c. Also unblocks C4.
4. **convexfeld-x9r0** (P2): Fix pricing level lifecycle ordering in queue.c. Unblocks
   C6 (constraint-side V2 pricing).
5. **convexfeld-mjtu** (P2): Rewrite pivot_primal.c to V2 spec. Unblocks H3.

### All V2 Deviation Issues

| ID | Sev | Title | Status |
|----|-----|-------|--------|
| convexfeld-hvbu | C1 | Harris band = 10x feasTol | CLOSED |
| convexfeld-4zq8 | C2 | BFRT split out of ratio_test | BLOCKED by C3 |
| convexfeld-4r3e | C3 | ratio_test missing status enum | OPEN |
| convexfeld-cp29 | C4 | Mechanism B fires without A confirmation | BLOCKED by C5+M1 |
| convexfeld-k0rk | C5 | basis diff simple count vs weighted | OPEN |
| convexfeld-ro2u | C6 | constraint-side V2 pricing missing | BLOCKED by H7 |
| convexfeld-ilr6 | H1 | ratio_test infeasibility pre-check | OPEN |
| convexfeld-mjtu | H2 | pivot_primal.c stale V1 | OPEN |
| convexfeld-exch | H3 | tight-bound processing skipped | BLOCKED by H2 |
| convexfeld-rr04 | H4 | pre-perturbation consistency check | OPEN |
| convexfeld-9kc5 | H5 | Devex delta_j ignores ref framework | OPEN |
| convexfeld-8p3j | H6 | no periodic SE weight recomputation | OPEN |
| convexfeld-x9r0 | H7 | pricing level lifecycle ordering | OPEN |
| convexfeld-dm3g | M1 | refactor threshold mismatch | OPEN |
| convexfeld-2l8i | M2 | step2.c implied bounds formula | OPEN |
| convexfeld-g8p8 | M3 | diagnostic mode bound restoration | OPEN |
| convexfeld-7jh3 | M4 | ratio_test missing theta return | OPEN |

---

## DO NOT
- Set eps_base outside [1e-8, 1e-6] — SPEC IS THE LAW
- Run Netlib after individual fixes — creates premature sadness
- Reference GLPK or other solver implementations (cleanroom)
- Lower EXPAND threshold below 100 without testing ALL Netlib instances
- Deviate from V2 spec for ANY reason including test pass rates
