# Agent Handoff

*Last updated: 2026-01-27*

---

## ⚠️ CRITICAL BUG DISCOVERED ⚠️

**Multiple constraints are NOT correctly satisfied.** The solver violates the first constraint while respecting later ones.

### Evidence
```
min -x - y s.t. x + y <= 4, x <= 2, y <= 3
Expected: x+y <= 4 (satisfied)
Got: x=2, y=3 → x+y = 5 > 4 (VIOLATED!)
```

### What Works
- Single constraint with one variable ✓
- Single constraint with multiple variables ✓

### What Doesn't Work
- Two or more constraints ✗

### Issue Tracking
- `convexfeld-y2sp`: P0 bug for multiple constraint violation

---

## Session Summary

### What Was Implemented
1. Two-phase simplex framework (Phase I with artificials, Phase II transition)
2. Array expansion for n+m variables
3. Proper reduced cost computation (dj = cj - pi^T * Aj)
4. Ratio test direction fix
5. Artificial variable column extraction

### What Needs Investigation
The bug is likely in one of:
- `cxf_simplex_step()` - basis/solution update after pivot
- FTRAN interaction with multiple rows
- How constraints beyond the first are being handled during pivoting

---

## Files to Investigate

| File | Function | Why |
|------|----------|-----|
| src/simplex/step.c | cxf_simplex_step | Updates solution after pivot |
| src/simplex/iterate.c | cxf_simplex_iterate | Main iteration loop |
| src/basis/ftran.c | cxf_ftran | Forward transformation |

---

## Test Status

**30/32 tests pass** but this is MISLEADING - the tests don't properly verify multi-constraint satisfaction.

---

## Next Steps for Next Agent

1. **Debug multi-constraint bug** (P0)
   - Add debug output to trace pivot operations
   - Verify FTRAN is computing correct pivot columns
   - Verify solution update formula is correct: x_basic[i] -= step * pivotCol[i]

2. **DO NOT proceed with benchmarks** until multi-constraint bug is fixed

3. Consider adding constraint satisfaction checks to the test suite

---

## Quick Verification Command

```c
// This should return x+y <= 4, but currently gives x+y = 5
min -x - y s.t. x + y <= 4, x <= 2, y <= 3
// Run: LD_LIBRARY_PATH=./build /tmp/debug_multi
```
