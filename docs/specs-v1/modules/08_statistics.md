# Module: Statistics

**Spec Version:** 1.0
**Last Updated:** 2026-01-25
**Functions:** 4

## 1. Overview

### 1.1 Purpose

The Statistics module provides diagnostic and reporting functions that analyze model structure and numerical properties before optimization begins. This module computes coefficient ranges, detects potential numerical issues, logs model characteristics, and validates variable properties for specialized algorithms. The module serves as an early warning system for problematic models and provides users with visibility into their optimization problem structure.

The module enables informed decision-making about parameter settings, model reformulation, and expected solver behavior by surfacing relevant statistics before expensive optimization work begins.

### 1.2 Responsibilities

This module is responsible for:

- Computing min/max ranges for all coefficient types (matrix, objective, bounds, RHS, quadratic)
- Detecting numerical issues (large ranges, extreme values) and issuing warnings
- Logging model structure statistics (quadratic terms, SOS, PWL, general constraints)
- Validating variable eligibility for special pivot algorithms
- Suggesting parameter adjustments (NumericFocus) when numerical issues detected
- Caching computed statistics to avoid redundant scans

This module is NOT responsible for:

- Modifying model structure or coefficients (read-only analysis)
- Enforcing numerical thresholds (only warns, doesn't reject)
- Reformulating models to fix issues (user's responsibility)
- Logging implementation (uses Logging module primitives)
- Performance profiling (handled by Timing module)

### 1.3 Design Philosophy

The module follows a defensive diagnostics philosophy: scan all model data to detect issues before they cause solver failures, provide actionable warnings with specific thresholds, and suggest concrete remediation steps. Statistics are computed lazily and cached to avoid redundant work.

The module prioritizes user-friendliness by providing human-readable messages with clear explanations of detected issues and recommended actions.

## 2. Public Interface

### 2.1 Exported Functions

| Function | Purpose | Typical Caller |
|----------|---------|----------------|
| cxf_presolve_stats | Log model structure statistics (Q, SOS, PWL, gencons) | cxf_optimize (pre-solve) |
| cxf_coefficient_stats | Compute ranges and warn about numerical issues | cxf_optimize (pre-solve) |
| cxf_compute_coef_stats | Compute min/max for all coefficient types | cxf_coefficient_stats |
| cxf_special_check | Validate variable for special pivot handling | Simplex (pivot selection) |

### 2.2 Exported Types

| Type | Purpose |
|------|---------|
| (No exported types - uses standard types) | - |

### 2.3 Exported Constants

| Constant | Value | Purpose |
|----------|-------|---------|
| COEF_RANGE_THRESHOLD | 13 | Max log10(max/min) before warning |
| LARGE_COEF_THRESHOLD | 1e13 | Max absolute coefficient before warning |
| PWL_VALUE_THRESHOLD | 1e10 | Max PWL breakpoint value before warning |

## 3. Internal Functions

### 3.1 Private Functions

All four functions are public-facing to other modules; no purely internal helper functions.

### 3.2 Helper Functions

| Function | Purpose | Used By |
|----------|---------|---------|
| cxf_log_printf | Format and write log messages | cxf_presolve_stats, cxf_coefficient_stats |
| log10 | Compute logarithm for range calculations | cxf_coefficient_stats |
| cxf_get_qconstr_data | Retrieve quadratic constraint data | cxf_compute_coef_stats |
| cxf_gencon_stats | Compute general constraint ranges | cxf_coefficient_stats |

## 4. State Management

### 4.1 Module State

| State Element | Type | Lifetime | Thread Safety |
|--------------|------|----------|---------------|
| model.cache_ptr (coef stats) | double[16] | Model lifetime | Not thread-safe |
| (No module-global state) | - | - | - |

### 4.2 State Lifecycle

```
MODEL_CREATED
    ↓
[Matrix coefficients populated]
    ↓
READY_FOR_STATISTICS
    ↓
cxf_presolve_stats called
    ↓
Logs model structure (Q, SOS, PWL, gencons)
    ↓
cxf_coefficient_stats called
    ↓
cxf_compute_coef_stats allocates cache
    ↓
Computes min/max ranges
    ↓
    ↓
STATISTICS_CACHED
    ↓
Subsequent calls reuse cache
    ↓
[If verbose=1, logs ranges and warnings]
    ↓
OPTIMIZATION_BEGINS
    ↓
[Model modification invalidates cache]
    ↓
CLEANUP (cache freed with model)
```

### 4.3 State Invariants

At all times, the following must be true:

- If cache exists, all 16 values (8 min/max pairs) are valid
- For all ranges: min <= max
- For all min values: >= 0 (absolute values)
- Cache persists across multiple calls (idempotent)
- Model structure is never modified by statistics functions

## 5. Dependencies

### 5.1 Required Modules

| Module | What We Use | Why |
|--------|-------------|-----|
| Memory | cxf_alloc for cache allocation | Store computed statistics |
| Logging | cxf_log_printf for output | User-facing diagnostic messages |
| Matrix | Matrix data access | Read coefficients for analysis |
| Parameters | cxf_getintparam(NumericFocus) | Check if warning already heeded |

### 5.2 Initialization Order

This module must be initialized:
- **After:** Memory, Logging, Matrix, Parameters
- **Before:** Optimization algorithms (consumers of diagnostics)

### 5.3 External Dependencies

| Dependency | Type | Purpose |
|------------|------|---------|
| log10 | Standard C (math.h) | Compute logarithm for range analysis |
| snprintf | Standard C (stdio.h) | Format warning messages |

## 6. Dependents

### 6.1 Modules That Depend on Us

| Module | What They Use | Stability |
|--------|---------------|-----------|
| Solver Core | cxf_presolve_stats, cxf_coefficient_stats | Stable (called before solve) |
| Simplex | cxf_special_check for pivot decisions | Critical |
| API | Indirect (via cxf_optimize) | Stable |

### 6.2 API Stability

The following interfaces are stable and must not change:

- cxf_special_check signature and return semantics (0/1 boolean)
- Warning threshold values (users rely on consistent behavior)

## 7. Invariants

### 7.1 Module Invariants

What this module guarantees:

- Statistics functions are read-only (never modify model)
- Computed ranges are mathematically correct (actual min/max)
- Zero coefficients excluded from min ranges (avoids degenerate min=0)
- Infinite bounds excluded from bounds ranges (sentinel values)
- Cache allocation is idempotent (allocate once, reuse)
- Warnings issued based on well-defined, documented thresholds

### 7.2 Required Invariants

What this module requires from others:

- Model structure is consistent and initialized
- Matrix arrays (CSC format) are valid
- Coefficient values are finite (not NaN, though Inf bounds are handled)
- Quadratic structures are valid if present

## 8. Error Handling

### 8.1 Error Detection

How errors are detected in this module:

| Error Type | Detection Method |
|------------|------------------|
| Cache allocation failure | Check malloc return value |
| Model already solved | Check model.status != 0 |
| NULL pointers | Assumed valid (caller responsibility) |
| Invalid quadratic data | No explicit check |

### 8.2 Error Propagation

How errors flow through this module:

```
Cache allocation failure in cxf_compute_coef_stats
    → Return error code 1001
    → Caller handles (typically continue without caching)

Model already solved in cxf_coefficient_stats
    → Return 0 immediately (early exit)
    → No error, just skip redundant work

Invalid coefficient (NaN/Inf)
    → May produce invalid min/max ranges
    → No explicit handling
```

### 8.3 Recovery Strategies

| Error Type | Recovery |
|------------|----------|
| Allocation failure | Return error code, continue without cache |
| Model solved | Early return (no-op) |
| Invalid data | Undefined behavior (garbage in, garbage out) |

## 9. Thread Safety

### 9.1 Concurrency Model

Statistics functions are NOT thread-safe. They read model data extensively and may allocate cache. Multiple threads should not call these functions concurrently on the same model.

**Single-threaded access:** Safe
**Concurrent reads:** Unsafe (cache allocation, model traversal)
**Concurrent with solve:** Unsafe (model may be modified)

### 9.2 Synchronization Primitives

| Primitive | Protects | Granularity |
|-----------|----------|-------------|
| (External model lock) | Cache allocation, model reads | Model-wide |

### 9.3 Thread Safety Guarantees

- cxf_presolve_stats: NOT thread-safe
- cxf_coefficient_stats: NOT thread-safe
- cxf_compute_coef_stats: NOT thread-safe
- cxf_special_check: Thread-safe for reads (if model not modified)

### 9.4 Known Race Conditions

**Concurrent cache allocation:**
- Thread A: checks cache_ptr == NULL
- Thread B: checks cache_ptr == NULL
- Both allocate cache
- One allocation leaks memory

Mitigation: Hold model lock during statistics computation.

**Statistics during model modification:**
- Thread A: modifying matrix coefficients
- Thread B: computing coefficient ranges
- Thread B may see inconsistent data (partial update)

Mitigation: Only call statistics functions before optimization begins.

## 10. Performance Characteristics

### 10.1 Complexity Summary

| Operation | Time | Space |
|-----------|------|-------|
| cxf_presolve_stats | O(g) where g = general constraints | O(1) |
| cxf_coefficient_stats | O(n + m + nnz + q) | O(1) cached |
| cxf_compute_coef_stats | O(n + m + nnz + q) | O(1) |
| cxf_special_check | O(q_var) where q_var = Q terms for var | O(1) |

Where: n = variables, m = constraints, nnz = non-zeros, q = quadratic terms

### 10.2 Hot Paths

Most statistics functions are called infrequently (once before optimization):

**Cold paths:**
- cxf_presolve_stats: Once per optimization, ~0.1-1 ms typical
- cxf_coefficient_stats: Once per optimization, 0.5-50 ms (depends on model size)
- cxf_compute_coef_stats: Once per optimization (cached thereafter)

**Hot path:**
- cxf_special_check: Called during pricing/pivot for each candidate variable
  - Time: 0.1-1 microseconds typical (few Q entries per variable)
  - Overhead: Negligible compared to pivot cost

### 10.3 Memory Usage

Minimal memory footprint:

**Cache storage:**
- Allocated once, persists for model lifetime

**Temporary allocations:**
- cxf_get_physical_cores: Up to 12 KB for processor info (freed immediately)
- No other dynamic allocations

**Total persistent:** 128 bytes per model (negligible)

## 11. Function Index

Complete list of functions in this module with links to individual specs:

### Public Functions

1. [cxf_presolve_stats](../functions/statistics/cxf_presolve_stats.md) - Log model structure statistics
2. [cxf_coefficient_stats](../functions/statistics/cxf_coefficient_stats.md) - Compute ranges and warn about issues
3. [cxf_compute_coef_stats](../functions/statistics/cxf_compute_coef_stats.md) - Compute min/max for coefficient types
4. [cxf_special_check](../functions/statistics/cxf_special_check.md) - Validate variable for special pivot

## 12. Design Decisions

### 12.1 Key Design Choices

| Decision | Rationale | Alternatives Considered |
|----------|-----------|------------------------|
| Cached coefficient ranges | Avoid redundant scans of large matrices | Recompute every time (wasteful) |
| Separate min/max pairs | Detect both range and absolute magnitude issues | Single metric (less informative) |
| Threshold-based warnings | Concrete, actionable guidance for users | Generic "model may be ill-conditioned" |
| PWL middle points only | Focus on approximation quality | All breakpoints (endpoints are bounds) |
| Read-only analysis | Safety (can't corrupt model) | Auto-reformulation (complex, risky) |
| Suggest NumericFocus | Concrete remediation step | No suggestion (less helpful) |

### 12.2 Known Limitations

- Numerical thresholds are heuristic (may miss some issues or false-positive)
- Cache invalidation requires model modification tracking (not always perfect)
- No detailed per-constraint or per-variable diagnostics (only aggregates)
- Special check doesn't explain why variable failed (only returns 0/1)
- PWL statistics only cover objective PWL (not constraint PWL)
- No cross-platform CPU detection (Windows only)

### 12.3 Future Improvements

- Per-constraint coefficient range analysis (identify specific problem rows)
- Automatic scaling suggestions based on detected ranges
- Detect correlated coefficients (near-linear dependence)
- Histogram of coefficient distribution (not just min/max)
- Cross-platform CPU detection for physical cores
- Detailed rejection reasons from cxf_special_check
- Caching of special check results (if called repeatedly)

## 13. References

- Gill, Murray, Saunders, Wright (2005). *Numerical Linear Algebra and Optimization*
- IEEE 754 Standard for Floating-Point Arithmetic
- Convexfeld Optimizer Reference Manual: NumericFocus parameter
- Higham, N. J. (2002). *Accuracy and Stability of Numerical Algorithms*, 2nd ed. SIAM.

## 14. Validation Checklist

Before finalizing this spec:

- [x] All public functions documented
- [x] All dependencies identified
- [x] Thread safety analyzed
- [x] Error handling complete
- [x] No implementation details leaked
- [x] Module boundaries clear
- [x] Numerical thresholds justified
- [x] Cache mechanism explained
- [x] Read-only guarantee stated
- [x] Warning conditions documented

---

*Date: 2026-01-25*
*Reviewed by: Pending*

---
*Convexfeld LP Solver Specification*
*Based on published optimization literature*
