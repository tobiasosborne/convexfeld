# ConvexFeld LP Solver - Implementation Plan

**Language:** C99 (ISO/IEC 9899:1999)
**Total Functions:** 142 (112 internal + 30 API)
**Total Modules:** 17 across 6 layers
**Total Structures:** 8 core data structures
**Target:** 100-200 LOC per file, maximum parallelization, TDD approach

---

## Directory Structure

| File | Description |
|------|-------------|
| [m0_setup.md](m0_setup.md) | M0: Project Setup |
| [m1_tracer.md](m1_tracer.md) | M1: Tracer Bullet |
| [m2_foundation.md](m2_foundation.md) | M2: Foundation Layer (Memory, Parameters, Validation) |
| [m3_infrastructure.md](m3_infrastructure.md) | M3: Infrastructure Layer (Error, Logging, Threading) |
| [m4_data.md](m4_data.md) | M4: Data Layer (Matrix, Timing, Analysis) |
| [m5_core.md](m5_core.md) | M5: Core Operations (Basis, Callbacks, SolverState) |
| [m6_algorithm.md](m6_algorithm.md) | M6: Algorithm Layer (Pricing) |
| [m7_simplex.md](m7_simplex.md) | M7: Simplex Engine (Simplex, Crossover, Utilities) |
| [m8_api.md](m8_api.md) | M8: Public API |
| [structures.md](structures.md) | Structure definitions and checklist |
| [functions.md](functions.md) | Function checklist by module |
| [parallelization.md](parallelization.md) | Parallelization guide |

---

## Architecture Layers

```
Layer 6: Model API (30 functions)
    - docs/specs/modules/17_model_api.md

Layer 5: Simplex Engine
    - Simplex Core (21 functions) - docs/specs/modules/06_simplex_core.md
    - Crossover (2 functions) - docs/specs/modules/15_crossover.md

Layer 4: Algorithm Components
    - Pricing (6 functions) - docs/specs/modules/08_pricing.md

Layer 3: Core Operations
    - Basis Operations (8 functions) - docs/specs/modules/07_basis.md
    - Callbacks (6 functions) - docs/specs/modules/13_callbacks.md
    - Solver State (4 functions) - docs/specs/modules/10_solver_state.md

Layer 2: Data Layer
    - Matrix Operations (7 functions) - docs/specs/modules/09_matrix.md
    - Timing (5 functions) - docs/specs/modules/11_timing.md
    - Model Analysis (6 functions) - docs/specs/modules/14_model_analysis.md

Layer 1: Infrastructure
    - Error Handling (10 functions) - docs/specs/modules/02_error_handling.md
    - Logging (5 functions) - docs/specs/modules/03_logging.md
    - Threading (7 functions) - docs/specs/modules/12_threading.md

Layer 0: Foundation
    - Memory Management (9 functions) - docs/specs/modules/01_memory.md
    - Parameters (4 functions) - docs/specs/modules/04_parameters.md
    - Validation (2 functions) - docs/specs/modules/05_validation.md
```

---

## Key Constraints

- **Language:** C99 (ISO/IEC 9899:1999)
- **TDD Required:** Tests written BEFORE implementation
- **Test:Code Ratio:** 1:3 to 1:4
- **File Size:** 100-200 LOC max per file
- **Test Framework:** Unity (lightweight C test framework)
- **Build System:** CMake 3.16+
- **Parallelization:** Steps within same milestone can run concurrently

---

## Project Directory Structure

```
convexfeld/
├── CMakeLists.txt
├── include/
│   └── convexfeld/
│       ├── cxf_types.h         # Core types and constants
│       ├── cxf_env.h           # CxfEnv structure
│       ├── cxf_model.h         # CxfModel structure
│       ├── cxf_matrix.h        # SparseMatrix structure
│       ├── cxf_solver.h        # SolverContext structure
│       ├── cxf_basis.h         # BasisState, EtaFactors
│       ├── cxf_pricing.h       # PricingContext structure
│       ├── cxf_callback.h      # CallbackContext structure
│       └── convexfeld.h        # Public API header
├── src/
│   ├── memory/                 # Memory management (9 functions)
│   ├── parameters/             # Parameter access (4 functions)
│   ├── validation/             # Input validation (2 functions)
│   ├── error/                  # Error handling (10 functions)
│   ├── logging/                # Logging (5 functions)
│   ├── threading/              # Threading (7 functions)
│   ├── matrix/                 # Matrix operations (7 functions)
│   ├── timing/                 # Timing (5 functions)
│   ├── analysis/               # Model analysis (6 functions)
│   ├── basis/                  # Basis operations (8 functions)
│   ├── callbacks/              # Callback handling (6 functions)
│   ├── solver_state/           # Solver state (4 functions)
│   ├── pricing/                # Pricing (6 functions)
│   ├── simplex/                # Simplex core (21 functions)
│   ├── crossover/              # Crossover (2 functions)
│   ├── utilities/              # Utilities (10 functions)
│   └── api/                    # Public API (30 functions)
├── tests/
│   ├── unity/                  # Unity test framework
│   ├── unit/                   # Unit tests per module
│   └── integration/            # Integration tests
└── benchmarks/
    └── benchmark_main.c        # Benchmark suite
```

---

## Summary Statistics

| Metric | Value |
|--------|-------|
| **Total Steps** | ~130 |
| **Total Functions** | 142 |
| **Total Structures** | 8 |
| **Estimated Implementation LOC** | 15,000-20,000 |
| **Estimated Test LOC** | 5,000-7,000 |
| **Total Estimated LOC** | 20,000-27,000 |
| **Target File Size** | 100-200 LOC |
| **Language** | C99 |
| **Build System** | CMake 3.16+ |
| **Test Framework** | Unity |
