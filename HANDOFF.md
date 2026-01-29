# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Eta Factor Bug Fixed - Netlib Benchmarks Now Solve

### What Was Done This Session

**Fixed:** Critical numerical instability in the simplex solver (convexfeld-aiq9)

**Root Cause:** Four bugs in the eta factor representation:
1. `pivot_eta.c` stored `1/pivot` instead of `pivot` as pivot_elem
2. `pivot_eta.c` stored negative scaled values instead of raw column values
3. `ftran.c` traversed newest-to-oldest instead of oldest-to-newest
4. `btran.c` traversed oldest-to-newest instead of newest-to-oldest

**Files Changed:**
- `src/basis/pivot_eta.c` - Store pivot directly, store raw column values
- `src/basis/ftran.c` - Collect etas into array, iterate oldest-to-newest
- `src/basis/btran.c` - Iterate newest-to-oldest, add isfinite check

**Verification:**
- All 34/35 unit tests pass (1 pre-existing test isolation issue)
- Netlib afiro: obj = -464.753143 (expected -464.75) ✓
- Netlib sc50b: obj = -70.000000 (expected -70.00) ✓
- Netlib sc105: obj = -52.202061 (expected -52.2020612) ✓

---

## CRITICAL: Next Steps for Next Agent

### Priority 1: Create Netlib Benchmark Runner (convexfeld-xkjj)

Now that the solver is numerically stable, implement infrastructure to run and verify Netlib benchmarks:
- Parse reference solutions from `benchmarks/netlib/feasible_gurobi_1e-8.csv`
- Run solver on each benchmark
- Compare results within tolerance
- Report pass/fail status

### Priority 2: Run Full Netlib Suite (convexfeld-86x5)

After benchmark runner is ready:
- Run on all 114 feasible Netlib problems
- Document which problems pass/fail
- Identify any remaining issues

### Other Open Items

- `test_unperturb_sequence` - Pre-existing test isolation issue (global state)
- Full Markowitz LU refactorization (convexfeld-w9to) - Currently refactor only clears etas

---

## Quality Gate Status

- **Tests:** 34/35 pass (97%)
- **Build:** Clean
- **Small LPs:** Working
- **Netlib:** afiro, sc50b, sc105 verified working

---

## Technical Details

### Product Form of Inverse (PFI) Representation

After a pivot, the basis change is represented as:
- `B_new = B_old * E` where E is an elementary matrix
- `B_new^(-1) = E^(-1) * B_old^(-1)`

**Correct eta storage:**
- `pivot_elem = pivot` (the actual pivot value from pivotCol[pivotRow])
- `values[k] = pivotCol[k]` (raw column values, not scaled)

**FTRAN (computing B^(-1) * a):**
- Apply E_1^(-1) first, then E_2^(-1), ..., finally E_k^(-1)
- Traverse etas from oldest to newest

**BTRAN (computing B^(-T) * a):**
- Apply (E_k^(-1))^T first, then (E_{k-1}^(-1))^T, ..., finally (E_1^(-1))^T
- Traverse etas from newest to oldest
