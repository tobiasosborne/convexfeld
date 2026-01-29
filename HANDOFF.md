# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Netlib Bugs Decomposed into Proper Sub-Issues

### Previous Session Summary

A band-aid fix was attempted using `diag_coeff` to handle ±1 initial basis diagonal. This is a **non-spec workaround** that:
- Partially fixed some benchmarks (afiro, sc50a, sc50b)
- Required disabling refactorization (REFACTOR_INTERVAL=10000)
- Does not address the root cause

### Root Cause Analysis

**The core problem:** FTRAN and BTRAN don't implement the spec correctly.

Per the specs:
- `cxf_ftran.md`: Should use L, U factors from LU factorization, then apply eta vectors
- `cxf_btran.md`: Should apply eta vectors in reverse, then use L^T, U^T solve
- `cxf_basis_refactor.md`: Should compute Markowitz-ordered LU factorization

**Current implementation:**
- FTRAN/BTRAN use ONLY eta vectors (no L, U factors)
- `cxf_basis_refactor` just clears eta vectors (no actual factorization)
- Without refactorization: numerical error accumulates
- With refactorization: FTRAN/BTRAN break completely

---

## CRITICAL: Issue Dependency Chain

**Work must proceed in this order:**

### Phase 1: Infrastructure (P0)
```
convexfeld-fg2x: Add L, U factor storage to BasisState struct
       ↓
convexfeld-gfwv: Implement proper LU factorization in cxf_basis_refactor
       ↓
    ┌──┴──┐
    ↓     ↓
convexfeld-wje9: Implement spec-compliant FTRAN with LU solve
convexfeld-pix8: Implement spec-compliant BTRAN with LU solve
```

### Phase 2: Cleanup (P1)
```
convexfeld-4gfy: Remove diag_coeff hack after LU implementation
convexfeld-8vat: Re-enable periodic refactorization (REFACTOR_INTERVAL)
```

### Phase 3: Bug Resolution (P1)
These should auto-resolve after Phase 1 & 2:
```
convexfeld-o2th: BUG: Phase II returns UNBOUNDED incorrectly
convexfeld-1x14: BUG: Phase I stuck with all positive reduced costs
convexfeld-p7wr: BUG: Phase II returns objective = 0 (may have independent cause)
```

---

## Immediate Next Step

**Start with:** `convexfeld-fg2x` - Add L, U factor storage to BasisState struct

This is the only unblocked P0 issue. Once complete, the LU factorization implementation can begin.

---

## Key Specs to Reference

- `docs/specs/functions/basis/cxf_basis_refactor.md` - Markowitz LU algorithm
- `docs/specs/functions/basis/cxf_ftran.md` - Forward solve with LU + etas
- `docs/specs/functions/basis/cxf_btran.md` - Backward solve with etas + LU

---

## Files with Non-Spec Workarounds (to be fixed)

| File | Workaround | Fix After |
|------|------------|-----------|
| `cxf_basis.h` | `diag_coeff` field | convexfeld-4gfy |
| `basis_state.c` | `diag_coeff` alloc | convexfeld-4gfy |
| `ftran.c` | Apply `diag_coeff` | convexfeld-wje9 |
| `btran.c` | Apply `diag_coeff` | convexfeld-pix8 |
| `refactor.c` | Reset `diag_coeff` | convexfeld-gfwv |
| `solve_lp.c` | Compute `diag_coeff` | convexfeld-4gfy |
| `iterate.c` | `REFACTOR_INTERVAL=10000` | convexfeld-8vat |

---

## Quality Gate Status

- **Tests:** 34/35 pass (97%)
- **Build:** Clean
- **Netlib:** ~60% passing (with workarounds)

---

## Closed Issues (Superseded)

- `convexfeld-zim5` - Decomposed into sub-issues above
- `convexfeld-w9to` - Superseded by convexfeld-gfwv
- `convexfeld-8rt5` - Superseded by convexfeld-p7wr
