# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Netlib Readiness Assessment Complete

### What Was Done This Session

**Assessment:** Evaluated how close the solver is to running on Netlib benchmarks.

**Key Finding:** The solver works for small LPs (2-3 vars) but fails on larger problems like Netlib's afiro (32 vars, 27 constraints) due to numerical instability.

**Root Cause Identified:**
- After ~12-20 simplex iterations, eta factors accumulate numerical errors
- Objective value becomes NaN, causing `CXF_ERROR_INVALID_ARGUMENT`
- Small problems solve before instability hits; Netlib problems need 50-200+ iterations

**Evidence from afiro test:**
```
Phase I: 10 iterations -> OPTIMAL (feasible basis found)
Phase II: iterations 1-11 -> obj improving
Phase II: iteration 12 -> obj becomes -nan
Phase II: iteration 13 -> ERROR -3 (CXF_ERROR_INVALID_ARGUMENT)
```

### Infrastructure Ready

- MPS parser: Complete, tested on afiro/sc50b/sc105
- Netlib benchmarks: 114 feasible + 29 infeasible in `./benchmarks/netlib/`
- Reference solutions: `benchmarks/netlib/feasible_gurobi_1e-8.csv`
- Integration tests: Small constrained LPs pass

---

## CRITICAL: Next Steps for Next Agent

### Priority 1: Fix Numerical Instability (convexfeld-aiq9)

**This is the blocking issue for Netlib testing.**

**Action Required:**
1. Research the relevant specs for numerical stability:
   - `docs/specs/functions/simplex/cxf_simplex_iterate.md`
   - `docs/specs/functions/basis/cxf_basis_refactor.md`
   - `docs/specs/modules/07_basis.md`

2. Look for spec guidance on:
   - Basis refactorization frequency
   - Eta vector overflow handling
   - Numerical tolerance thresholds
   - When to trigger LU refactorization

3. Potential fixes to investigate:
   - More aggressive refactorization (every 20-30 pivots vs current 100)
   - NaN/Inf detection before each iteration
   - Implement proper Markowitz LU factorization (convexfeld-w9to)

### After Numerical Fix

- convexfeld-xkjj (P2): Create Netlib benchmark runner infrastructure
- convexfeld-86x5 (P2): Run solver on small Netlib benchmarks

---

## Quality Gate Status

- **Tests:** 29/32 pass (91%)
- **Build:** Clean
- **Small LPs:** Working
- **Netlib:** Blocked by numerical instability

---

## Pre-existing Issues

- `test_unperturb_sequence` fails due to global state (test isolation, not functionality)
- 3 edge case tests failing in test_simplex_edge.c
