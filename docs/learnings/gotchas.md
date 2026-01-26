# Gotchas, Failures, and Things to Avoid

This file captures mistakes made and lessons learned the hard way.

---

## Critical Failures

### Wrong Language (C99 vs Rust)
**FAILURE:** Implementation plan written for Rust instead of C99.

- The PRD explicitly states C99
- Agent failed to read the PRD carefully
- Entire implementation plan had wrong file structure, syntax, tooling
- Required full rewrite (3 agent resets)

**Lesson:** ALWAYS verify the target language from PRD before writing ANY implementation details.

---

### Jumped to Implementation Without Creating Issues
**FAILURE:** Agent started writing code instead of creating beads issues.

- The task was ONLY to create beads issues for each step in the plan
- Agent misread the workflow and started implementing M0
- Created ~10 files that had to be deleted
- User had to intervene forcefully

**Lesson:** After a plan is complete, the NEXT STEP is to create beads issues for tracking. DO NOT START IMPLEMENTING until issues exist for every step.

**Correct Workflow:**
1. Plan is written ✓
2. Create beads issues for EVERY step in the plan ← NEXT STEP
3. THEN start implementing (claiming issues one at a time)

---

### Not Reading HANDOFF.md First
**FAILURE:** Agents repeated same mistakes from previous sessions.

**Lesson:** ALWAYS read HANDOFF.md first. It contains:
- What work was completed
- Current state of the project
- What the next agent should do
- Known issues and blockers

---

## C99 Gotchas

### Error Code Names
**Wrong:** `CXF_ERROR_MEMORY`
**Right:** `CXF_ERROR_OUT_OF_MEMORY`

Always check `cxf_types.h` for correct error code names.

---

### POSIX Time Functions
**Problem:** `clock_gettime(CLOCK_MONOTONIC)` requires a feature macro.

```c
/* MUST be at the top of the file, before any includes */
#define _POSIX_C_SOURCE 199309L

#include <time.h>

double cxf_get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
```

---

### NaN/Inf Detection
**Wrong approach:**
```c
/* This fails for DBL_MAX (~1.8e308) */
if (val > 1e308) return 1;  /* BAD */
```

**Right approach:**
```c
#include <math.h>

/* Use isfinite() for NaN/Inf detection */
if (!isfinite(val)) return 1;

/* Or for NaN specifically: val != val is portable */
if (val != val) return 1;  /* NaN only */
```

---

### CMake Static Library Requires Source
**Problem:** CMake STATIC libraries require at least one source file.

**Solution:** Create `src/placeholder.c` as a workaround until real modules added.

---

### Header Guards
**Always include header guards in .h files:**
```c
#ifndef CXF_TYPES_H
#define CXF_TYPES_H

/* ... header content ... */

#endif /* CXF_TYPES_H */
```

---

### Forward Declarations
Required for circular struct references. Put in `cxf_types.h`:
```c
typedef struct CxfEnv CxfEnv;
typedef struct CxfModel CxfModel;
/* etc. */
```

---

### Magic Number Types
Use `uint32_t` for 32-bit magic, `uint64_t` for 64-bit:
```c
#define CXF_ENV_MAGIC 0xC0FEFE1D              /* 32-bit */
#define CXF_CALLBACK_MAGIC2 0xF1E1D5AFE7E57A7EULL  /* 64-bit */
```

---

## Build System Gotchas

### CMake Version
**Requires CMake 3.16+** for modern C99 support and Unity integration.

---

### Unity Test Framework Warnings
Unity's own code has `-Wdouble-promotion` warnings (float→double). This is expected from third-party code and doesn't affect functionality.

---

### Include Order
Standard headers first, then project headers:
```c
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
```

---

## Design Gotchas

### Spec vs Implementation Staging
**Lesson:** Specs describe the full-featured version, but implementation can be staged.

Example: Memory functions spec describes full environment tracking, but that requires threading infrastructure. Implement simple working version first, add features as dependencies are built.

---

### qsort with Global State
C's `qsort` doesn't support user data, so comparison functions need global state:
```c
/* File-static global - NOT thread-safe */
static const double *g_reduced_costs = NULL;

static int compare_by_rc(const void *a, const void *b) {
    int ia = *(const int *)a;
    int ib = *(const int *)b;
    return (fabs(g_reduced_costs[ib]) > fabs(g_reduced_costs[ia])) ? 1 : -1;
}
```

**Note:** This is a C limitation. For thread safety, would need qsort_r (BSD/glibc extension).

---

### Singly-Linked List Reverse Traversal
When you only have `next` pointers but need reverse order, collect pointers into an array first:
```c
/* Stack allocation for small cases, heap for large */
#define MAX_STACK_ETAS 64
```

---

## Things That Didn't Work

1. **Writing implementation plan in Rust when spec says C99** - Major failure
2. **Not reading HANDOFF.md first** - Repeated same mistakes
3. **Not consulting PRD for language requirement** - Root cause of Rust error
4. **Simple `val > 1e308` check for infinity** - Fails for DBL_MAX
5. **Starting implementation without creating tracking issues** - Lost work context
