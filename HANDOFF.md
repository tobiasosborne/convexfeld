# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Performance Optimizations Complete - 45% Total Improvement

### Session Summary

Continued hardcore profiling and implemented two major optimizations.

#### Optimization Results

| Optimization | Before | After | Improvement |
|--------------|--------|-------|-------------|
| Hash table (previous) | 531M | 357M | -33% |
| **Batch CSC construction** | 357M | 301M | **-15.8%** |
| **Preallocated arrays** | 301M | 294M | **-2.4%** |
| **Total from baseline** | 531M | 294M | **-45%** |

#### New Profile Breakdown (post-optimization)

| Function | % Time | Notes |
|----------|--------|-------|
| cxf_simplex_iterate | 39.64% | Simplex core |
| cxf_ftran | 18.14% | Simplex core |
| cxf_btran_vec | 16.37% | Simplex core |
| cxf_solve_lp | 10.19% | Simplex core |
| memset | 3.27% | Array clearing |
| cxf_pivot_with_eta | 2.26% | Basis updates |

**Simplex core is now 84% of runtime** - optimal for an LP solver.

### Files Modified

| File | Changes |
|------|---------|
| `src/api/mps_build.c` | Replaced O(nnz²) cxf_addconstr calls with O(nnz) direct CSC construction |
| `include/convexfeld/cxf_solver.h` | Added work_column and work_cB arrays to SolverContext |
| `src/simplex/context.c` | Allocate/free preallocated work arrays |
| `src/simplex/iterate.c` | Use preallocated arrays instead of calloc/free per iteration |

### Test Status

- **Unit Tests:** 35/36 pass (97%) - same as before
- **Netlib Benchmarks:** All tested pass (kb2 has pre-existing 0.016% numerical precision issue)
- **Build:** Clean

---

## Next Steps

### Priority 1: FTRAN/BTRAN Inner Loop Optimization

**Issue:** `convexfeld-7ahj` - Remove bounds checks from inner loops

The inner loop bounds check `if (j >= 0 && j < m && j != pivot_row)` takes ~10% of runtime:
- FTRAN: 5.07% (18M instructions)
- BTRAN: 5.09% (18M instructions)

**Expected:** ~5-8% additional improvement

### Other Ready Work

Run `bd ready` - notable items:
- `convexfeld-gcrd`: Incremental reduced cost updates (P3)
- `convexfeld-pmgj`: Incremental reduced cost updates (P3)
- `convexfeld-7ddt`: Run medium Netlib benchmarks (P2)
- `convexfeld-lmp8`: Benchmark full suite and profile (P2)

---

## Remaining Correctness Issues (Unchanged)

- **israel:** Returns INFEASIBLE when should be OPTIMAL - Phase I issue
- **adlittle:** 0.12% numerical precision error
- **kb2:** 0.016% numerical precision error
- `convexfeld-4gfy`: Remove diag_coeff hack after LU implementation
- `convexfeld-8vat`: Re-enable periodic refactorization
- `convexfeld-1x14`: BUG: Phase I stuck with all positive reduced costs

---

## Quality Gate Status

- **Tests:** 35/36 pass (97%)
- **Build:** Clean
- **Performance:** 45% cumulative improvement achieved
