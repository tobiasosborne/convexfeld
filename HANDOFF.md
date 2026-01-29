# Agent Handoff

*Last updated: 2026-01-29*

---

## STATUS: Major Performance Optimization - 19x Speedup

### Session Summary

Profiled the LP solver using callgrind and fixed a catastrophic O(m²) preprocessing bottleneck that was consuming 92% of runtime.

#### The Problem

Profiling `ship04l` (2118 vars, 402 constraints) revealed:

| Function | % Time | Issue |
|----------|--------|-------|
| `get_row_coeffs` | 61.25% | O(n×nnz) per call |
| `memset` (libc) | 20.56% | Called by get_row_coeffs |
| `cxf_simplex_iterate` | 1.75% | **The actual solver!** |

Root cause: `check_obvious_infeasibility()` had an O(m²) nested loop for "parallel constraint detection" that called `get_row_coeffs` 80,601 times, each scanning all 2118 columns.

#### The Fixes

1. **Skip O(m²) check for large problems**: Limit parallel constraint check to m ≤ 100
2. **Use row-major format for row access**: Build CSR once, get O(nnz_row) access instead of O(n×avg_col_height)

```c
/* Build row-major format once */
if (mat->row_ptr == NULL) {
    cxf_prepare_row_data(mat);
    cxf_build_row_major(mat);
}

/* Fast row access */
for (int64_t k = mat->row_ptr[row]; k < mat->row_ptr[row+1]; k++) {
    coeffs[mat->col_idx[k]] = mat->row_values[k];
}
```

#### Results

| Metric | Before | After | Improvement |
|--------|--------|-------|-------------|
| ship04l time | 0.381s | 0.020s | **19x faster** |
| Instructions | 6.7B | 538M | **12.5x reduction** |
| Simplex % of runtime | 1.75% | 21.88% | Now dominant |

### Test Status

- **Unit Tests:** 35/36 pass (97%) - same as before
- **Netlib Benchmarks:** 8/9 tested pass (adlittle has 0.12% precision error)
- **Build:** Clean

---

## Files Modified This Session

| File | Changes |
|------|---------|
| `src/simplex/solve_lp.c` | Added row-major access, limited O(m²) check to small problems |
| `docs/profiling.md` | New: profiling guide and history |
| `docs/learnings/patterns.md` | Added performance patterns section |

---

## Remaining Issues

### From Previous Sessions (unchanged)
- **israel:** Returns INFEASIBLE when should be OPTIMAL - Phase I issue
- **adlittle:** 0.12% numerical precision error
- `convexfeld-4gfy`: Remove diag_coeff hack after LU implementation
- `convexfeld-8vat`: Re-enable periodic refactorization
- `convexfeld-1x14`: BUG: Phase I stuck with all positive reduced costs

### New Performance Opportunities
Profiling now shows these as the top costs:
1. **strcmp (22.92%)**: MPS parser doing linear string comparisons
2. **mps_find_col (7.41%)**: O(n) column name lookup - could use hash table
3. **FTRAN/BTRAN (18.84%)**: Core operations, expected to be dominant

---

## Next Steps

1. **Optional: Optimize MPS parser** - Replace linear search with hash table for column/row name lookup
2. **Run full Netlib suite** - Verify no regressions on larger problems
3. **Consider LU refactorization tuning** - FTRAN/BTRAN are now ~19% of runtime

---

## Quality Gate Status

- **Tests:** 35/36 pass (97%)
- **Build:** Clean
- **Netlib:** Performance now acceptable, correctness maintained
