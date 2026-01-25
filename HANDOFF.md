# Agent Handoff

*Last updated: 2026-01-25*

---

## CRITICAL: READ THIS FIRST

### NEXT STEP IS **NOT** IMPLEMENTATION

**DO NOT START WRITING CODE.**

The implementation plan is complete. The **ONLY** next step is:

## CREATE BEADS ISSUES FOR EVERY STEP IN THE PLAN

Read `docs/IMPLEMENTATION_PLAN.md` and create a beads issue for **EACH STEP**.

Example:
```bash
bd create --title "M0.1: Create CMakeLists.txt" --type task --priority 1 \
  --description "Create root CMakeLists.txt per docs/IMPLEMENTATION_PLAN.md Step 0.1"

bd create --title "M0.2: Create cxf_types.h" --type task --priority 1 \
  --description "Create include/convexfeld/cxf_types.h per Step 0.2"
```

**DO THIS FOR ALL ~50+ STEPS IN THE PLAN.**

Only AFTER all issues are created should ANY implementation begin.

---

## Previous Agent Error (This Session)

An agent started implementing M0 (created CMakeLists.txt, headers, etc.) instead of creating beads issues. This was wrong. Those files were deleted.

**Correct workflow:**
1. Plan written ✓
2. **Create beads issues for every step** ← YOU ARE HERE
3. Then implementation begins (one issue at a time)

---

## Current State

### Implementation Plan
- **Location:** `docs/IMPLEMENTATION_PLAN.md`
- **Language:** C99 (ISO/IEC 9899:1999)
- **Functions:** 142 (112 internal + 30 API)
- **Structures:** 8 core data structures
- **Milestones:** 9 (M0-M8)

### Milestones to Create Issues For
```
M0: Project Setup (~4 steps)
M1: Tracer Bullet (~8 steps)
M2: Foundation Layer (~15 steps)
M3: Infrastructure Layer (~22 steps)
M4: Data Layer (~18 steps)
M5: Core Operations (~18 steps)
M6: Algorithm Layer (~6 steps)
M7: Simplex Engine (~23 steps)
M8: Public API (~30 steps)
```

---

## References

- Implementation Plan: `docs/IMPLEMENTATION_PLAN.md`
- Learnings: `docs/learnings.md`
- All Specs: `docs/specs/`
