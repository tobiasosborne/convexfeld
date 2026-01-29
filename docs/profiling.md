# Performance Profiling Guide

## Quick Start

```bash
# Build with debug symbols
mkdir -p build-profile && cd build-profile
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_FLAGS="-O2 -g -fno-omit-frame-pointer"
cmake --build . -j$(nproc)

# Profile a benchmark
valgrind --tool=callgrind --callgrind-out-file=callgrind.out \
    ./benchmarks/bench_netlib --filter ship04l

# Analyze results
callgrind_annotate --auto=yes callgrind.out | head -60
```

## Tools Available

| Tool | Use Case | Overhead |
|------|----------|----------|
| callgrind | Detailed function-level profiling | ~50x |
| gprof | Quick profiling with call graph | ~2x (requires `-pg`) |
| perf | Low-overhead sampling (if kernel supports) | <5% |

## Key Functions to Monitor

### Simplex Core (should be dominant)
- `cxf_simplex_iterate` - Main iteration loop
- `cxf_ftran` / `cxf_btran_vec` - Basis transformations
- `cxf_ratio_test` - Leaving variable selection
- `cxf_pivot_with_eta` - Pivot updates

### Known Hotspots
- `mps_find_col` / `mps_find_row` - O(n) string matching in MPS parser
- `strcmp` - MPS parsing overhead

## Profiling History

### 2026-01-29: O(m²) Preprocessing Fix

**Problem:** `check_obvious_infeasibility()` had O(m²) loop calling `get_row_coeffs` 80,000+ times.

**Before:**
```
get_row_coeffs:     61.25%  <- BAD: preprocessing dominates
memset:             20.56%
cxf_simplex_iterate: 1.75%  <- actual solver
```

**After (fixed):**
```
cxf_simplex_iterate: 21.88%  <- now dominant
ftran/btran:         18.84%
MPS parsing:         30%     <- I/O overhead
```

**Fixes applied:**
1. Skip parallel constraint check for m > 100 constraints
2. Build CSR (row-major) format once, use for O(nnz_row) row access
3. Result: **19x speedup** on ship04l (0.381s -> 0.020s)

## Future Optimization Opportunities

1. **MPS Parser** (~30% time): Replace linear search in `mps_find_col`/`mps_find_row` with hash table
2. **FTRAN/BTRAN** (~19%): Consider LU refactorization frequency tuning
3. **Memory allocation**: Profile malloc/free patterns, consider arena allocator
