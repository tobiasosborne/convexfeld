# Agent Handoff

*Last updated: 2026-01-28*

---

## CRITICAL: SPEC VIOLATION FOUND

**The simplex implementation violates specs.** Reduced cost computation uses a simplified approximation instead of BTRAN as specified.

### Root Cause Chain

1. **Spec says**: Use `cxf_btran` to compute dual prices π = cB * B^-1
2. **Implementation does**: `pi[i] = work_obj[basic_vars[i]]` (only correct when B = I)
3. **Result**: After first pivot, B ≠ I, reduced costs become wrong
4. **Symptom**: Phase I declares "optimal" prematurely (no improving variable found)
5. **Outcome**: Multi-constraint LPs return INFEASIBLE incorrectly

### Debug Evidence

```
After Phase I setup (correct):
  obj_value = 9.000000 (sum of artificials 4+2+3)
  dj[0] = -2, dj[1] = -2 (x,y have improving reduced costs)

After iterate 1: obj = 5.000000 (good, decreasing)
After iterate 2: obj = 1.000000, status = OPTIMAL (BAD!)
  → Declares optimal but artificials still sum to 1 > 0
  → Returns INFEASIBLE incorrectly
```

### Files with Issues

| File | Issue |
|------|-------|
| `src/simplex/solve_lp.c:166-216` | `compute_reduced_costs()` uses pi=cB instead of BTRAN |
| `src/simplex/iterate.c:243-285` | Same wrong approximation copied here |

---

## Changes Made This Session

### Fixes Applied (Partial)

1. **step.c:77-82** - Fixed artificial variable update skip
   - Changed `basicVar >= state->num_vars` to `basicVar >= total_vars`
   - Artificials now get solution updates during pivots

2. **iterate.c** - Added reduced cost recomputation after pivot
   - But uses same wrong approximation, so doesn't help

### Issues Created

| ID | Priority | Title |
|----|----------|-------|
| convexfeld-c251 | P0 | FIX: step.c skips artificial variable updates |
| convexfeld-kkmu | P0 | RESEARCH: Spec vs Implementation gap - BTRAN not used |
| convexfeld-fyi2 | P0 | RESEARCH: Review all simplex specs for implementation gaps |
| convexfeld-nd1r | P1 | ARCH: No slack variable introduction |
| convexfeld-c4bh | P1 | TEST: Add constraint satisfaction verification |
| convexfeld-cnvw | P1 | RESEARCH: Review docs/learnings for missed patterns |
| convexfeld-vz03 | P2 | FIX: Reduced cost uses simplified approximation |

---

## Next Steps for Next Agent

### MANDATORY FIRST STEPS

1. **Read specs thoroughly**:
   - `docs/specs/functions/simplex/cxf_simplex_iterate.md`
   - `docs/specs/functions/basis/cxf_btran.md`
   - `docs/specs/modules/13_pricing.md`

2. **Read learnings**:
   - `docs/learnings/gotchas.md`
   - `docs/learnings/patterns.md`

### Fix Required

**Implement proper BTRAN-based reduced cost computation:**

```c
// WRONG (current):
for (int i = 0; i < m; i++) {
    pi[i] = work_obj[basic_vars[i]];  // Only correct when B = I
}

// CORRECT (needed):
// 1. Set up cB vector (objective coeffs of basic vars)
// 2. Solve π^T B = cB^T using BTRAN
// 3. Use π to compute dj = cj - π^T * Aj
```

The `cxf_btran` function exists but only accepts unit vector input. Either:
- Call it m times (once per row) and combine, OR
- Modify to accept arbitrary input vector

---

## Test Command

```bash
# Compile and run multi-constraint test
cd /home/tobiasosborne/Projects/convexfeld
gcc -o /tmp/test_mc /tmp/test_multi_constraint.c -I./include -L./build -lconvexfeld -lm -Wl,-rpath,./build
/tmp/test_mc

# Expected: x=2, y=2, obj=-4
# Current: Returns INFEASIBLE (wrong)
```

---

## Quality Gate Status

- Tests: 30/32 pass (same as before)
- Build: Clean
- The 2 failing tests are pre-existing unrelated issues
