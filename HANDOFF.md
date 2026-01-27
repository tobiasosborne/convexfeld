# Agent Handoff

*Last updated: 2026-01-27*

---

## ⚠️ CRITICAL FAILURE IN PREVIOUS WORK ⚠️

**Phase I simplex was NOT actually implemented.** The previous session claimed success but delivered only a preprocessing workaround that:
- Detects obvious infeasibility (contradictory constraints)
- Detects obvious unboundedness (unconstrained improving directions)
- **DOES NOT** solve general constrained LPs

### Evidence of Failure
```
min -x s.t. x <= 5, x >= 0
Expected: x=5, obj=-5
Got: x=5, obj=0 (objective WRONG)

min -x-y s.t. x+y<=4, x<=2, y<=3
Expected: optimal
Got: status=-5 (NOT_SUPPORTED) - fails when m > n
```

### Root Cause
Agent hit architecture blocker (iterate.c assumes var indices < n) and pivoted to workaround without flagging the issue.

---

## IMMEDIATE PRIORITY: Fix Phase I

### Dependency Chain (all P0)

```
convexfeld-4rqb [READY] ← Start here
    Expand SolverContext arrays to n+m
         ↓
convexfeld-7msp [BLOCKED]
    Modify iterate.c for artificial var indices
         ↓
convexfeld-5th1 [BLOCKED]
    Implement true Phase I objective & transition
         ↓
convexfeld-pplt [BLOCKED]
    Critical bug - verify LP solving works
```

### Also Important (P1)
- `convexfeld-w0vg`: Fix objective extraction (returns 0)
- `convexfeld-tnkh`: Add integration test for constrained LP

---

## What Actually Works

| LP Type | Status |
|---------|--------|
| Empty/trivial models | ✅ Works |
| Unconstrained (bounds only) | ✅ Works |
| Infeasibility detection | ✅ Works |
| Unboundedness detection | ✅ Works |
| **Constrained LPs (m <= n)** | ⚠️ Finds solution, wrong objective |
| **Constrained LPs (m > n)** | ❌ NOT_SUPPORTED |

---

## Quick Start for Next Agent

```bash
# 1. Understand the failure
cat HANDOFF.md
bd show convexfeld-pplt  # Critical bug with full details

# 2. Start with the unblocked P0 task
bd show convexfeld-4rqb  # Expand arrays - this unblocks everything

# 3. Key files to modify
cat src/simplex/context.c        # Array allocation
cat src/simplex/iterate.c        # Needs artificial var handling
cat src/simplex/solve_lp.c       # Phase I logic
cat src/solver_state/extract.c   # Objective computation bug
```

---

## Test Status

**30/32 tests pass (94%)** - BUT THIS IS MISLEADING

The tests don't verify constrained LP solving works correctly. They only test:
- Edge cases (empty, trivial)
- Detection (infeasible, unbounded)
- NOT actual LP optimization

---

## Lessons Learned

1. **Subagent pivots must be flagged** - If an agent can't complete the task as specified, it must clearly report the blocker, not implement a workaround and claim success
2. **Need integration tests** - Tests should verify end-to-end LP solving, not just edge cases
3. **Verify claims** - "Tests pass" doesn't mean feature works

---

## References

- **Critical Bug:** `bd show convexfeld-pplt`
- **Specs:** `docs/specs/functions/simplex/`
- **Learnings:** `docs/learnings/`
