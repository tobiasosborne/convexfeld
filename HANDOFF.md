# Agent Handoff

*Last updated: 2026-02-26*

---

## STATUS: 45/45 unit tests pass. Architecture significantly improved this session.

### Session Summary (2026-02-26) — 8 issues closed

| Issue | Priority | What |
|-------|----------|------|
| convexfeld-n426 | P1 | Decomposed `cxf_simplex_step` 447→104 lines |
| convexfeld-mxjm | P1 | Created 5 internal headers, replaced 88 extern declarations |
| convexfeld-ojwu | P2 | VAR status constants centralized (by mxjm) |
| convexfeld-lmkg | P1 | Equality constraint scan in `cxf_pivot_special` |
| convexfeld-s9am | P2 | Re-enabled BFRT via standard clamping (Koberstein 2005) |
| convexfeld-36qh | P2 | Basis snapshot/diff rewritten per spec |
| convexfeld-rcrg | P2 | MPS parser: `atof()`→`strtod()`, name buffer 16→64 |
| convexfeld-l0ca | P2 | Removed dead V1 pricing weight stub |

### Audit Status

Previous sessions fixed the major implementation audit items. Current status:

| Audit Item | Status | Notes |
|------------|--------|-------|
| C1: Kahan-stable addition | DONE | `safe_add()` in `pivot_update.c`, `negUnbdCount`/`posUnbdCount` wired |
| C2: `cxf_pivot_bound` stub | DONE | 7-phase implementation with eta, pricing, activity propagation |
| C3: `cxf_pivot_update` API | DONE | New signature `(oldLB, newLB, oldUB, newUB, infinity)` |
| C4: `cxf_pivot_special` | DONE | Equality constraint scan + Phase I suppression |
| H1: Harris tolerance 10x tight | DONE | `harrisBand = 10.0 * feasTol` |
| H2: Two-stage infeasibility | DONE | From-scratch recompute + re-derive in step2/step3 |
| H3: Inner loop convergence | DONE | Weighted categories + `max(0,k-5)*tau` threshold |
| H4: Phase I w-coefficients | DONE | Recomputed from scratch after each pivot |
| M1: BFRT disabled | DONE | Standard clamping, no row negation |
| M2: Dense LU | TODO | Causes timeouts on bandm/tuff (m>300) |
| M3: Basis snapshot | DONE | 10-slot counter snapshot, colDenom/rowDenom normalization |
| M4: V1 pricing weight | DONE | Dead code removed; V2 `weight_update.c` is correct |

### What's Currently Deployed

- **4nrf REVERTED** (794fdad) — activity `-rhs` init caused regressions. Zero-init is active.
- **ic80 active** — Phase I→II constraint cleanup with `rhs - max_a` slack formula.
- All unit + integration tests pass (45/45).
- Netlib: share2b, afiro, adlittle, beaconfd, kb2 confirmed OPTIMAL this session.

### Architecture Improvements This Session

1. **5 internal headers** eliminate 88 scattered `extern` declarations. Signature changes now produce compiler errors at all call sites.
2. **step.c decomposed** from 724→656 lines. `cxf_simplex_step` from 447→104 lines via `pricing_and_ftran()` and `post_pivot_updates()` extraction.
3. **BFRT re-enabled** via standard clamping (no row negation). Does not trigger on tested Netlib instances (slacks dominate departures).
4. **Convergence detection** rewritten with weighted multi-category formula and spec-compliant threshold.

### Next Steps (for next agent)

1. **Sparse LU** (M2) — Dense `calloc(m*m)` working matrix causes timeouts on bandm (m=305), tuff (m=333). High effort but eliminates remaining timeout failures.
2. **Run full Netlib suite** — Many audit fixes deployed since last full run. Some previously-failing instances may now pass.
3. **Phase I convergence** (convexfeld-pr0h) — Near feasibility boundary pricing issues.
4. **Phase II primal accuracy** (convexfeld-n9ok) — Basic vars past bounds on boeing1/e226/bore3d/grow7.

---

## DO NOT
- Enable scaling without testing H1 (Harris tolerance) first
- Change diag_coeff based on RHS sign (causes regressions on israel/stair/e226)
- Re-add RC-based status reassignment in refine.c (corrupts obj)
- Hack refactorization parameters to fix primal accuracy — needs sparse LU
- Use `cols_eliminated` counter for constraint cleanup — it feeds stall detection
- Use row negation for BFRT (corrupts LU/eta factorization)
