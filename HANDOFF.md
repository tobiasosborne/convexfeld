# Agent Handoff

*Last updated: 2026-01-26*

---

## CRITICAL: READ THIS FIRST

### M1.0 COMPLETE - Continue with M1.1-M1.7 (Stub Implementations)

**The tracer bullet test is written. Now implement the stubs to make it pass.**

Next steps: **M1.1 through M1.7** (can be done in any order, or in parallel)

---

## Work Completed This Session

### M1.0: Tracer Bullet Test - COMPLETE

| Issue | Description | Status |
|-------|-------------|--------|
| `convexfeld-cz6` | M1.0: Tracer Bullet Test | ✓ |

**Files created:**
- `tests/integration/test_tracer_bullet.c` (87 LOC)
- Updated `tests/CMakeLists.txt`

**Current state:**
- Test compiles but fails to link (expected - TDD)
- 8 undefined references prove test is calling the right API functions

---

## Current State

### Project Structure
```
convexfeld/
├── CMakeLists.txt              ✓
├── include/convexfeld/
│   ├── cxf_types.h             ✓ (enums, constants, forward decls)
│   ├── cxf_env.h               ✓ (CxfEnv structure + API decls)
│   ├── cxf_model.h             ✓ (CxfModel structure + API decls)
│   ├── cxf_matrix.h            ✓ (SparseMatrix structure)
│   ├── cxf_solver.h            ✓ (SolverContext structure)
│   ├── cxf_basis.h             ✓ (BasisState, EtaFactors)
│   ├── cxf_pricing.h           ✓ (PricingContext)
│   ├── cxf_callback.h          ✓ (CallbackContext)
│   └── convexfeld.h            ✓ (main API header)
├── src/
│   └── placeholder.c           (to be replaced with real sources)
├── tests/
│   ├── CMakeLists.txt          ✓
│   ├── unity/                  ✓ (Unity test framework)
│   ├── unit/test_smoke.c       ✓ (passes)
│   └── integration/
│       └── test_tracer_bullet.c ✓ (compiles, needs implementations)
└── benchmarks/
    └── CMakeLists.txt          ✓
```

### Build Status
- `cmake --build .` partially succeeds (libconvexfeld.a, unity, test_smoke)
- `test_tracer_bullet` fails to link (expected - no implementations yet)

---

## Next Steps: M1.1 through M1.7

These issues implement stub functions to make the tracer bullet test pass.

### Recommended Order (but can parallelize)
```bash
# 1. Memory stubs first (other stubs depend on these)
bd show convexfeld-6uc   # M1.6: Stub Memory Functions
bd update convexfeld-6uc --status in_progress

# 2. Error stubs
bd show convexfeld-9t5   # M1.7: Stub Error Functions

# 3. Env/Model stubs
bd show convexfeld-z6p   # M1.1: Stub CxfEnv Structure
bd show convexfeld-ae7   # M1.2: Stub CxfModel Structure

# 4. Matrix stub (minimal)
bd show convexfeld-bko   # M1.3: Stub SparseMatrix Structure

# 5. API stubs
bd show convexfeld-z1h   # M1.4: Stub API Functions

# 6. Simplex stub
bd show convexfeld-7he   # M1.5: Stub Simplex Entry

# 7. Benchmark (optional, after test passes)
bd show convexfeld-9b2   # M1.8: Tracer Bullet Benchmark
```

### Key Files to Create
1. `src/memory/alloc_stub.c` - cxf_malloc, cxf_calloc, cxf_free
2. `src/error/error_stub.c` - cxf_error, cxf_geterrormsg
3. `src/api/env_stub.c` - cxf_loadenv, cxf_freeenv
4. `src/api/model_stub.c` - cxf_newmodel, cxf_freemodel, cxf_addvar
5. `src/api/api_stub.c` - cxf_optimize, cxf_getintattr, cxf_getdblattr
6. `src/simplex/solve_lp_stub.c` - Trivial 1-var LP solver

### Update CMakeLists.txt
Add new source files to `target_sources(convexfeld ...)` in root CMakeLists.txt.

---

## References

- **Implementation Plan:** `docs/IMPLEMENTATION_PLAN.md`
- **Learnings:** `docs/learnings.md`
- **Specs:** `docs/specs/`

---

## Issue Status

### Completed (M0 + M1.0)
- `convexfeld-2by` - M0.1: Create CMakeLists.txt ✓
- `convexfeld-x85` - M0.2: Create Core Types Header ✓
- `convexfeld-dw2` - M0.3: Setup Unity Test Framework ✓
- `convexfeld-n99` - M0.4: Create Module Headers (Stubs) ✓
- `convexfeld-cz6` - M1.0: Tracer Bullet Test ✓

### Next Up (M1.1-M1.8 - P1)
- `convexfeld-z6p` - M1.1: Stub CxfEnv Structure
- `convexfeld-ae7` - M1.2: Stub CxfModel Structure
- `convexfeld-bko` - M1.3: Stub SparseMatrix Structure
- `convexfeld-z1h` - M1.4: Stub API Functions
- `convexfeld-7he` - M1.5: Stub Simplex Entry
- `convexfeld-6uc` - M1.6: Stub Memory Functions
- `convexfeld-9t5` - M1.7: Stub Error Functions
- `convexfeld-9b2` - M1.8: Tracer Bullet Benchmark

Run `bd ready` to see all available work.
