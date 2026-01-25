# Project Learnings

This file captures learnings, gotchas, and useful patterns discovered during development.

**Rule: ERRORS = SUCCESS if learnings are recorded here.**

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
