# Project Learnings

This file captures learnings, gotchas, and useful patterns discovered during development.

**Rule: ERRORS = SUCCESS if learnings are recorded here.**

---

## 2026-01-26: M0.4 Module Headers (Stubs) Created

### SUCCESS: All 8 structure headers created

**Headers created (~280 LOC total):**
- `cxf_env.h` - CxfEnv structure (environment)
- `cxf_model.h` - CxfModel structure (problem instance)
- `cxf_matrix.h` - SparseMatrix structure (CSC/CSR format)
- `cxf_solver.h` - SolverContext structure (runtime state)
- `cxf_basis.h` - BasisState + EtaFactors (basis representation)
- `cxf_pricing.h` - PricingContext (partial pricing)
- `cxf_callback.h` - CallbackContext + CxfCallbackFunc typedef
- `convexfeld.h` - Main API header (includes all others)

**Key design decisions:**
- All headers include only `cxf_types.h` (forward declarations there)
- Minimal fields for tracer bullet, full fields to be added later
- Function declarations in structure headers (e.g., `cxf_loadenv` in `cxf_env.h`)
- Version macros in `convexfeld.h`

**M0 Complete!** Ready to start M1 (Tracer Bullet).

---

## 2026-01-26: M0.3 Unity Test Framework Setup

### SUCCESS: Unity installed and verified with smoke test

**Setup:**
- Downloaded unity.c, unity.h, unity_internals.h from ThrowTheSwitch/Unity
- Updated tests/CMakeLists.txt with Unity library and `add_cxf_test()` helper
- Created smoke test with 3 assertions (true, int equality, double within)

**Configuration:**
- `UNITY_INCLUDE_DOUBLE` - enables double precision assertions
- `UNITY_DOUBLE_PRECISION=1e-12` - sets tolerance for double comparisons

**Note:** Unity's own code has `-Wdouble-promotion` warnings (float→double), but this is expected from third-party code and doesn't affect functionality.

---

## 2026-01-26: M0.2 Core Types Header Created

### SUCCESS: cxf_types.h created with all enums, constants, and forward declarations

**Contents (~170 LOC):**
- `CxfStatus` enum - 8 success codes + 4 error codes
- `CxfVarType` enum - 5 variable types (C, B, I, S, N)
- `CxfSense` enum - 3 constraint senses (<, >, =)
- `CxfObjSense` enum - minimize/maximize
- `CxfVarStatus` enum - 5 basis status values
- Constants: CXF_INFINITY, tolerances, CXF_MAX_NAME_LEN
- Magic numbers: CXF_ENV_MAGIC, CXF_MODEL_MAGIC, CXF_CALLBACK_MAGIC/2
- Forward declarations for all 8 structures

**Note:** Spec file path in issue was wrong (`docs/specs/arch/` vs `docs/specs/architecture/`), but implementation plan had exact code.

---

## 2026-01-26: M0.1 CMakeLists.txt Created

### SUCCESS: Project build system established

**What worked:**
1. Followed the implementation plan exactly for Step 0.1
2. Created root CMakeLists.txt with:
   - CMake 3.16+ requirement
   - C99 standard enforcement (CMAKE_C_STANDARD 99, CMAKE_C_STANDARD_REQUIRED ON)
   - Compiler flags: -Wall -Wextra -Wpedantic -O2 plus additional warnings
   - Static library target with proper include directories
   - Conditional tests and benchmarks subdirectories
3. Created tests/CMakeLists.txt and benchmarks/CMakeLists.txt stubs
4. Added placeholder.c so library builds (CMake requires at least one source file)

**Build verification:**
- `cmake ..` succeeded
- `cmake --build .` produced `libconvexfeld.a`

**Gotcha discovered:**
- CMake STATIC libraries require at least one source file
- Created src/placeholder.c as a workaround until real modules added

---

## 2026-01-26: Beads Issues Created Successfully

### SUCCESS: All 122 Implementation Issues Created

Created beads issues for every step in the implementation plan using parallel subagents.

**What worked:**
1. Read HANDOFF.md first - understood the task was ONLY issue creation
2. Spawned 9 parallel subagents (one per milestone) for efficiency
3. Each issue includes:
   - Title matching plan step (e.g., "M0.1: Create CMakeLists.txt")
   - Detailed description with file paths and LOC estimates
   - Spec file references (e.g., "Spec: docs/specs/functions/memory/cxf_malloc.md")
   - Priority based on milestone (M0=P0, M1=P1, M2-M8=P2)

**Issue breakdown:**
- M0: 4 issues (P0 - critical, must complete first)
- M1: 9 issues (P1 - tracer bullet proves architecture)
- M2-M8: 109 issues (P2 - main implementation)

**Lesson: Parallel subagents are efficient for bulk issue creation. Each agent handles one milestone independently.**

---

## 2026-01-25: Agent Started Implementing Instead of Creating Issues

### CRITICAL FAILURE: Jumped to Implementation Without Creating Beads Issues

**FAILURE: Agent started writing code (CMakeLists.txt, headers, etc.) instead of creating beads issues for each step in the implementation plan.**

- The task was ONLY to create beads issues for each step in the plan
- Agent misread the workflow and started implementing M0
- Created ~10 files that had to be deleted
- User had to intervene forcefully

**Lesson: After a plan is complete, the NEXT STEP is to create beads issues for tracking. DO NOT START IMPLEMENTING until issues exist for every step.**

**Correct Workflow:**
1. Plan is written ✓
2. Create beads issues for EVERY step in the plan ← NEXT STEP
3. THEN start implementing (claiming issues one at a time)

---

## 2026-01-25: C99 Implementation Plan Complete

### SUCCESS: Implementation Plan Rewritten for C99

After 3 failed attempts by previous agents (all wrote for Rust), this session successfully rewrote the plan for C99.

**What worked:**
1. Read HANDOFF.md first to understand the critical error
2. Spawned 5 parallel research agents to gather all spec details
3. Read all 8 structure specs directly to get C99 field types
4. Systematically mapped all 142 functions to their spec files
5. Included concrete C99 code examples throughout

---

## 2025-01-25: Initial Setup (Previous Session)

### CRITICAL ERROR: Wrong Language

**FAILURE: Implementation plan written for Rust instead of C99.**

- The PRD explicitly states C99
- Agent failed to read the PRD carefully
- Entire implementation plan had wrong file structure, syntax, tooling
- Required full rewrite

**Lesson: ALWAYS verify the target language from PRD before writing ANY implementation details.**

---

### Planning Phase Learnings

1. **Previous planning attempts failed** because they didn't:
   - Map every function to its spec file
   - Include explicit checklists for all 142 functions
   - Define a tracer bullet milestone to prove architecture
   - Specify parallelization strategy
   - **Use the correct language (C99)**

2. **Tracer bullet pattern is critical**
   - Prove end-to-end works before investing in full implementation
   - A 1-variable LP test exercises: API -> Simplex -> Solution extraction

3. **File size discipline**
   - 200 LOC limit prevents complexity accumulation
   - Forces proper decomposition from the start

4. **Spec structure**
   - 142 functions across 17 modules in 6 layers
   - 8 core data structures
   - Implementation language: C99

5. **Parallel research is efficient**
   - Spawning multiple research agents simultaneously speeds discovery
   - Agent results can be combined for comprehensive understanding

---

## Gotchas

### C99 Specific

1. **Magic number validation** - Use `uint32_t` for 32-bit magic, `uint64_t` for 64-bit
2. **Forward declarations** - Required for circular struct references
3. **Header guards** - Essential for all `.h` files: `#ifndef FOO_H / #define FOO_H / #endif`
4. **Include order** - Standard headers first, then project headers

### Build System

1. **CMake 3.16+** - Required for modern C99 support and Unity integration
2. **Unity test framework** - Lightweight, C-native, no external dependencies

---

## Useful Patterns

### C99 Structure Definition Pattern
```c
struct CxfEnv {
    uint32_t magic;           /* 0xC0FEFE1D for validation */
    int active;               /* 1 if environment is active */
    char error_buffer[512];   /* Last error message */
    /* ... fields ... */
};
```

### Unity Test Pattern
```c
#include "unity.h"
void setUp(void) {}
void tearDown(void) {}
void test_function_name(void) {
    /* Test code */
    TEST_ASSERT_EQUAL_INT(expected, actual);
}
int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_function_name);
    return UNITY_END();
}
```

---

## Things That Didn't Work

1. **Writing implementation plan in Rust when spec says C99** - Major failure, caused 3 agent resets
2. **Not reading HANDOFF.md first** - Agents repeated same mistakes
3. **Not consulting PRD for language requirement** - Root cause of Rust error
