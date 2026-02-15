# Agent Handoff

*Last updated: 2026-02-15*

---

## STATUS: V2 Spec Gap Analysis Complete — Major Rework Needed

### Session Summary

Compared ConvexFeld's entire codebase against the corrected v2 cleanroom specification from GRB-decomp. The v1 spec ConvexFeld was built against was ~36% hallucinated. This analysis identifies every divergence.

**Overall alignment: ~40-50%.** Foundation (matrix, eta vectors, basic simplex loop, memory) is sound. Two subsystems are fundamentally wrong, and several critical features are missing entirely.

---

## RED — Complete Rewrite Required

### 1. Pricing System (all files in `src/pricing/`)

V1 hallucinated a simple sectional partial pricing scheme. V2 reveals a **13-function producer-consumer architecture**:
- Dual queue systems (constraint + variable queues)
- Committed/pending split within each queue
- Per-element flag arrays for O(1) duplicate prevention
- Neighbor-based expansion through constraint matrix (not fixed partitions)
- Adaptive strategy selection with 3 threshold checks
- DSE weight updates with +1 term from ||alpha_q - e_r||^2
- Devex weights with reference framework tracking

ConvexFeld has **none of this**. Its pricing is Dantzig sectional scanning, and SE/Devex weight updates are **stubs that set weights to 1.0**. PricingContext struct is 20% aligned — needs full replacement.

### 2. Bound Propagation (entirely missing — ~1,500 LOC new subsystem)

V2's `step2`/`step3` are **bidirectional bound propagation** (variable-side / constraint-side), an FBBT system integrated with simplex. ConvexFeld's `phase_steps.c` implements them as primal/dual pivot extensions — **completely wrong purpose**.

### 3. Perturbation (`perturbation.c`)

Direction is **reversed**: shrinks bounds (`lb += eps, ub -= eps`) when v2 says expand (`lb -= eps, ub += eps`). Also only perturbs original vars, not auxiliaries. Scale wrong. Known bug — stubs in context.c shadow the broken code.

---

## YELLOW — Significant Rework Needed

### 4. Data Structures

| Struct | Alignment | Key Gap |
|--------|-----------|---------|
| Model | 60% | Missing dual matrix (primary/working), attribute table |
| SparseMatrix | 85% | **Missing scaling system** (critical for numerics) |
| SolverState | 65% | Missing duplicated matrix, steepest edge arrays |
| BasisState | 90% | Missing memory pool, cycling detection snapshot |
| PricingContext | 20% | Fundamentally wrong (see above) |
| EtaVector | 60% | Missing VARIABLE_FIX and WARM_START variants |

### 5. Ratio Test — Missing BFRT

Has Harris two-pass (correct) but **no Bound-Flipping Ratio Test**. BFRT gives 20-50% iteration reduction on bounded-variable problems. V2 spec P2.4 has full algorithm.

### 6. Phase I→II Transition

Missing 3 of 6 state transformations: reduced cost recomputation via BTRAN, constraint cleanup, tolerance adjustment.

### 7. Matrix Scaling (entirely missing — ~500 LOC new subsystem)

V2's P3.15 specifies row/column equilibration (Curtis & Reid 1972). Critical for ill-conditioned problems. ConvexFeld has **zero scaling infrastructure**.

### 8. Method Selection

`solve_lp.c` has simplified heuristic vs v2's scoring system with quantitative thresholds.

---

## GREEN — Mostly Correct

### 9. Basis Operations (~70% correct)

- `pivot_eta.c`: Exact match to v2
- `eta_factors.c`, `lu_factors.c`, `basis_state.c`: Correct
- `snapshot.c`: Better than v2 (captures full basis, not just counters)
- `ftran.c` / `btran.c`: Correct algorithms, wrong abstraction (v2 uses PFI inline, not standalone APIs). Keep as internal helpers.
- `lu_factorize.c`: Over-engineered (v2 says use external LU) but functional
- `refactor.c`: Works, naming is confusing (`cxf_basis_refactor` is really just clearing etas)
- `basis_stub.c`: Can be deleted (all functions implemented elsewhere)

### 10. Support Modules

- **Memory** (`src/memory/`): 90% correct
- **Callbacks** (`src/callbacks/`): 75% correct (missing thread-safe mutex)
- **Error handling** (`src/error/`): 60% correct (missing buffer locking)
- **Matrix core** (`src/matrix/`): 85% correct for LP
- **Crossover, barrier, MIP**: Intentionally absent (LP-only scope — fine)

### 11. Simplex Core

- `iterate.c`: Right general structure, needs BFRT and stall detection
- `crash.c`: Minor gaps only
- `cleanup.c`: Minor gaps only
- `step.c`: Needs BFRT integration

---

## Root Cause of Current Bugs (explained by v2 spec)

| Bug Category | Root Cause per V2 |
|-------------|-------------------|
| **Cycling** (capri, grow7, seba) | Perturbation direction reversed + no-op stubs. V2: expand bounds. |
| **Phase I false INFEASIBLE** (8+ problems) | Incomplete Phase I→II transition + missing bound propagation (FBBT pre-tightens bounds) |
| **Numerical errors** (5 problems) | No matrix scaling. V2 specifies row/column equilibration. |

---

## Estimated Rework

| Area | LOC | Type |
|------|-----|------|
| Pricing system | ~2,000 | Full rewrite |
| Bound propagation | ~1,500 | New subsystem |
| Matrix scaling | ~500 | New subsystem |
| BFRT ratio test | ~400 | Add to existing |
| Perturbation fix | ~300 | Rewrite |
| DSE/Devex weights | ~300 | Implement stubs |
| Phase transitions | ~200 | Modify existing |
| PricingState struct | ~200 | Replace existing |
| **Total** | **~5,400** | |

~10,000 LOC of existing code is correct and needs no changes.

---

## Recommended Priority Order

### P0: Fix perturbation (unblocks 3 cycling problems)
1. Remove no-op stubs from `context.c`
2. Fix `perturbation.c`: expand bounds (`lb -= eps`, `ub += eps`), extend to auxiliaries, scale ~1e-8
3. Recompute basic variable values after perturbing

### P1: Add matrix scaling (unblocks 5 numerical-error problems)
- Row/column equilibration per Curtis & Reid 1972
- Apply D_r * A * D_c, scale bounds and RHS
- Unscale solution after solve

### P2: Add BFRT to ratio test (20-50% iteration reduction)
- Extend Harris two-pass with bound-flipping per v2 P2.4

### P3: Fix Phase I→II transition (unblocks 8+ false-INFEASIBLE problems)
- Add reduced cost recomputation via BTRAN
- Constraint cleanup, tolerance adjustment

### P4: Rewrite pricing system (performance, not correctness)
- Implement dual queue producer-consumer architecture
- DSE weight updates with correct +1 term
- Devex with reference framework

### P5: Add bound propagation (new subsystem)
- Variable-side and constraint-side propagation
- Integrate with simplex iteration

---

## Quality Gate Status

- **Tests:** 35/36 pass (pre-existing `test_simplex_edge` failure)
- **Build:** Clean (no warnings)
- **Netlib:** 11 pass, 13 fail, 3 timeout (unchanged)

---

## Reference

Full v2 spec: `/home/tobiasosborne/Projects/GRB-decomp/cleanroom/v2/output/SPECIFICATION.md` (24,070 lines)
Key sections for each rework area:
- Pricing: P2.3, P3.17, P3.18
- Bound propagation: P2.8, `step2`/`step3` in P3.21
- Perturbation: P2.6
- Ratio test: P2.4
- Scaling: P3.15
- Phase transitions: P3.21 (6 state transformations)
- DSE/Devex formulas: P2.1 Step 6
