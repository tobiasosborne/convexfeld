# Agent Handoff

*Last updated: 2026-02-16*

---

## STATUS: P0 Tolerance Fix In Progress — All Data Gathered, Ready to Execute

### Session Summary

1. **Full V2 spec audit** — 20 parallel Sonnet agents, 20 reports (512KB) at `docs/audit/01-20_*.md`
2. **Remediation plan** — 8 phases, 45-67 days, ~10K LOC at `docs/audit/REMEDIATION_PLAN.md`
3. **Beads cleanup** — Closed 33 stale/duplicate/superseded issues, created 29 new issues with dependencies
4. **Started P0 tolerance fix** — `convexfeld-nso9` is `in_progress`, all data gathered below

---

## NEXT STEP: Execute Tolerance Fix (convexfeld-nso9)

**All research is done. Just apply the changes below.**

### Tolerance Values to Fix

Read the spec first: `docs/specs-v2/specs/reference/tolerances_constants.md`

#### File 1: `include/convexfeld/cxf_types.h`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 122 | `#define CXF_PIVOT_TOL 1e-10` | Harris pivot tolerance = 1e-9 | Change to `1e-9` |
| 125 | `#define CXF_ZERO_TOL 1e-12` | Significant bound change = 1e-12 | **KEEP** (correct) |
| 113 | `#define CXF_INFINITY 1e100` | 1e100 | **KEEP** (correct) |
| 116 | `#define CXF_FEASIBILITY_TOL 1e-6` | 1e-6 | **KEEP** (correct) |
| 119 | `#define CXF_OPTIMALITY_TOL 1e-6` | 1e-6 | **KEEP** (correct) |

#### File 2: `src/basis/lu_factorize.c`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 21 | `#define MARKOWITZ_THRESHOLD 0.1` | ~7.8e-3 (1/128) | Change to `0.0078125` |
| 22 | `#define MIN_PIVOT 1e-12` | Minimum pivot threshold = 1e-13 | Change to `1e-13` |

#### File 3: `src/simplex/perturbation.c`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 17 | `#define PERTURB_BASE_SCALE 1e-6` | Perturbation floor = 1e-10 | Change to `1e-10` |
| 20 | `#define PERTURB_MAX_SCALE 1e-3` | Perturbation ceiling = 1e-6 | Change to `1e-6` |
| 23 | `#define MIN_OBJ_COEFF 1e-8` | Not in spec | Investigate or remove |

#### File 4: `src/basis/refactor.c`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 27 | `#define MIN_PIVOT_TOL 1e-10` | Harris pivot = 1e-9 | Change to `1e-9` |

#### File 5: `src/simplex/pivot_primal.c`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 31 | `#define TINY_THRESHOLD 1e-8` | Not directly in spec | Investigate — may be Harris pivot (1e-9) or numerical zero (1e-10) |

#### File 6: `src/simplex/pivot_special.c`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 25 | `#define THRESHOLD 1e-10` | Numerical zero (tight) = 1e-10 | **KEEP** (correct) |

#### File 7: `src/error/pivot_check.c`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 23 | `#define NEG_INFINITY_THRESHOLD (-1e99)` | Spec uses -1e100 | Change to `(-1e100)` or use `-CXF_INFINITY` |
| 24 | `#define POS_INFINITY_THRESHOLD (1e99)` | Spec uses 1e100 | Change to `(1e100)` or use `CXF_INFINITY` |

#### File 8: `src/simplex/refine.c`
| Line | Current | Spec Value | Change |
|------|---------|-----------|--------|
| 13 | `#define NEAR_ZERO_TOL 1e-12` | Significant bound change = 1e-12 | **KEEP** (correct) |

### Missing Constants to ADD (to `cxf_types.h` or appropriate header)
```c
#define CXF_HARRIS_PIVOT_TOL    1e-9    /* Harris ratio test pivot threshold */
#define CXF_MIN_PIVOT           1e-13   /* Absolute floor on pivot magnitude */
#define CXF_MARKOWITZ_TOL       0.0078125  /* 1/128, Markowitz pivot tolerance */
#define CXF_BOUND_EQUALITY_TOL  1e-10   /* Bound gap below which var is fixed */
#define CXF_PERTURB_FLOOR       1e-10   /* Minimum perturbation magnitude */
#define CXF_PERTURB_CEILING     1e-6    /* Maximum perturbation magnitude */
#define CXF_LARGE_BOUND_MARKER  1e20    /* Effectively infinite bound threshold */
```

### After Tolerance Fix
- Run `make test` to verify nothing breaks
- Close `convexfeld-nso9`
- Move to `convexfeld-clow` (variable status encoding fix — P0)

---

## After P0: Critical Path

```
P0 tolerances (convexfeld-nso9) ← YOU ARE HERE
  → P0 var status (convexfeld-clow)
    → P1 16 function renames (convexfeld-b7ow) — unblocks 2
      → P1 4 struct renames (convexfeld-dv0k) — unblocks 5
        → P2 decompose solve_lp (convexfeld-23p6) — unblocks 5
          → P2 algorithm work (kyns, mpo9, ypf9, f1k1, 1azn)
```

Run `bd ready` to see all unblocked work.

---

## Beads State
- **55 open issues** (was 85+, cleaned 33 stale/duplicate)
- **14 blocked** (dependency chain above)
- **41 ready** (unblocked)
- **0 in_progress** (except nso9)

---

## File Locations

| Item | Path |
|------|------|
| Audit reports (20) | `docs/audit/01_*.md` through `20_*.md` |
| Remediation plan | `docs/audit/REMEDIATION_PLAN.md` |
| V2 specs (ground truth) | `docs/specs-v2/specs/` |
| Tolerance spec | `docs/specs-v2/specs/reference/tolerances_constants.md` |
| FUNCTION_MAP | `docs/specs-v2/FUNCTION_MAP.md` |
| Learnings | `docs/learnings/` |
