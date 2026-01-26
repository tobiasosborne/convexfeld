# Project Learnings

This directory captures learnings, gotchas, and useful patterns discovered during development.

**Rule: ERRORS = SUCCESS if learnings are recorded here.**

## Directory Structure

| File | Description |
|------|-------------|
| [m0-m1_setup.md](m0-m1_setup.md) | M0 Project Setup & M1 Tracer Bullet |
| [m2-m4_foundation.md](m2-m4_foundation.md) | M2-M4 Foundation, Infrastructure, Data layers |
| [m5-m6_core.md](m5-m6_core.md) | M5-M6 Core Operations & Algorithm layers |
| [patterns.md](patterns.md) | Useful patterns and code examples |
| [gotchas.md](gotchas.md) | Gotchas, failures, and things to avoid |

## Quick Reference

### Key Patterns
- **Stub Extraction**: TDD stubs in `*_stub.c`, extract to dedicated files when implementing
- **Arena Allocator**: EtaBuffer for fast allocation with reset
- **Product Form of Inverse**: FTRAN/BTRAN with eta vectors

### Common Gotchas
- Use `CXF_ERROR_OUT_OF_MEMORY`, not `CXF_ERROR_MEMORY`
- `clock_gettime(CLOCK_MONOTONIC)` requires `#define _POSIX_C_SOURCE 199309L`
- Use `isfinite()` for NaN/Inf detection, not manual comparisons

## Adding New Learnings

1. Find the appropriate file based on milestone
2. Add entry with date header: `## YYYY-MM-DD: Brief Title`
3. Include: SUCCESS/FAILURE, files created/modified, key learnings
4. Update patterns.md or gotchas.md if generally applicable
