# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Netlib Benchmark Runner Complete - Solver Bugs Blocking Full Suite

### What Was Done This Session

**Created:** Netlib benchmark runner infrastructure (convexfeld-xkjj)

**Files Created:**
- `benchmarks/bench_netlib.c` - Benchmark runner that:
  - Parses reference solutions from `feasible_gurobi_1e-8.csv`
  - Runs solver on MPS files
  - Compares results within 0.01% tolerance
  - Reports pass/fail with timing

**Benchmark Results (19 small problems tested):**
- **PASS (4):** afiro, sc50a, sc50b, sc105
- **FAIL (15):** Most due to solver bugs

**Failure Categories:**
1. False INFEASIBLE (10): adlittle, boeing2, israel, bandm, e226, lotfi, beaconfd, scorpion, brandy, capri
2. Wrong objective (3): blend (269% error), share2b (614% error), stocfor1 (32% error)
3. UNBOUNDED (1): kb2
4. ITER_LIMIT (1): recipe

---

## CRITICAL: Next Steps for Next Agent

### Priority 1: Fix False INFEASIBLE Detection (convexfeld-zim5)

Most benchmarks fail because the solver reports INFEASIBLE when problems are actually feasible.

**Debug approach:**
1. Disable presolve `check_obvious_infeasibility()` temporarily
2. Add debug logging to Phase I to trace artificial variable values
3. Verify constraint sense handling for >= constraints

### Priority 2: Fix Wrong Objective Values (convexfeld-8rt5)

Some benchmarks converge but with incorrect objectives.

**Affected:** blend, share2b, stocfor1

### Other Open Items

- `convexfeld-7ddt` - Run medium Netlib benchmarks (blocked by solver bugs)
- `convexfeld-w9to` - Full Markowitz LU refactorization
- `test_unperturb_sequence` - Test isolation issue

---

## Using the Benchmark Runner

```bash
# Run all benchmarks
./build/benchmarks/bench_netlib

# Run specific benchmark
./build/benchmarks/bench_netlib --filter afiro

# Run with custom paths
./build/benchmarks/bench_netlib --dir path/to/mps --csv path/to/ref.csv
```

---

## Quality Gate Status

- **Tests:** 34/35 pass (97%)
- **Build:** Clean
- **Netlib Pass Rate:** 4/19 (21%) - blocked by solver bugs

---

## Technical Details

### Benchmark Runner Design

The runner (`benchmarks/bench_netlib.c`):
1. Loads 114 reference solutions from CSV
2. Iterates over MPS files in directory
3. For each file: parse MPS, solve, compare objective
4. Reports pass/fail with relative error and timing

### Failure Analysis

Most failures show INFEASIBLE status on feasible problems. Hypothesis:
- `check_obvious_infeasibility()` presolve check may be too aggressive
- Phase I may not be correctly handling >= constraints
- Constraint coefficient signs may be incorrect

The few problems that converge with wrong objectives suggest:
- Objective extraction issue
- MPS parsing issue with objective row
