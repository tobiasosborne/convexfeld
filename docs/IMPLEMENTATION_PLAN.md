# ConvexFeld LP Solver - C99 Implementation Plan

**Language:** C99 (ISO/IEC 9899:1999)
**Total Functions:** 142 (112 internal + 30 API)
**Total Modules:** 17 across 6 layers
**Total Structures:** 8 core data structures
**Target:** 100-200 LOC per file, maximum parallelization, TDD approach

---

## Table of Contents

1. [Overview](#overview)
2. [Milestone 0: Project Setup](#milestone-0-project-setup)
3. [Milestone 1: Tracer Bullet](#milestone-1-tracer-bullet)
4. [Milestone 2: Foundation Layer (Level 0)](#milestone-2-foundation-layer-level-0)
5. [Milestone 3: Infrastructure Layer (Level 1)](#milestone-3-infrastructure-layer-level-1)
6. [Milestone 4: Data Layer (Level 2)](#milestone-4-data-layer-level-2)
7. [Milestone 5: Core Operations (Level 3)](#milestone-5-core-operations-level-3)
8. [Milestone 6: Algorithm Layer (Level 4)](#milestone-6-algorithm-layer-level-4)
9. [Milestone 7: Simplex Engine (Level 5)](#milestone-7-simplex-engine-level-5)
10. [Milestone 8: Public API (Level 6)](#milestone-8-public-api-level-6)
11. [Benchmarks](#benchmarks)
12. [Structure Definitions](#structure-definitions)
13. [Function Checklist](#function-checklist)
14. [Parallelization Guide](#parallelization-guide)

---

## Overview

### Architecture Layers

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

### Key Constraints

- **Language:** C99 (ISO/IEC 9899:1999)
- **TDD Required:** Tests written BEFORE implementation
- **Test:Code Ratio:** 1:3 to 1:4
- **File Size:** 100-200 LOC max per file
- **Test Framework:** Unity (lightweight C test framework)
- **Build System:** CMake 3.16+
- **Parallelization:** Steps within same milestone can run concurrently

### Directory Structure

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

## Milestone 0: Project Setup

**Goal:** Initialize C99 project structure
**Parallelizable:** No (must complete first)
**LOC Total:** ~300

### Step 0.1: Create CMakeLists.txt

**LOC:** ~80
**File:** `CMakeLists.txt`

```cmake
cmake_minimum_required(VERSION 3.16)
project(convexfeld VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler flags
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra -Wpedantic -O2)
endif()

# Library
add_library(convexfeld STATIC)
target_include_directories(convexfeld PUBLIC include)

# Tests
enable_testing()
add_subdirectory(tests)

# Benchmarks
add_subdirectory(benchmarks)
```

### Step 0.2: Create Core Types Header

**LOC:** ~150
**File:** `include/convexfeld/cxf_types.h`
**Spec:** `docs/specs/arch/02_types_and_constants.md`

```c
#ifndef CXF_TYPES_H
#define CXF_TYPES_H

#include <stdint.h>
#include <stddef.h>

/* Status codes */
typedef enum {
    CXF_OK = 0,
    CXF_OPTIMAL = 1,
    CXF_INFEASIBLE = 2,
    CXF_UNBOUNDED = 3,
    CXF_INF_OR_UNBD = 4,
    CXF_ITERATION_LIMIT = 5,
    CXF_TIME_LIMIT = 6,
    CXF_NUMERIC = 7,
    CXF_ERROR_OUT_OF_MEMORY = -1,
    CXF_ERROR_NULL_ARGUMENT = -2,
    CXF_ERROR_INVALID_ARGUMENT = -3,
    CXF_ERROR_DATA_NOT_AVAILABLE = -4
} CxfStatus;

/* Variable types */
typedef enum {
    CXF_CONTINUOUS = 'C',
    CXF_BINARY = 'B',
    CXF_INTEGER = 'I',
    CXF_SEMICONT = 'S',
    CXF_SEMIINT = 'N'
} CxfVarType;

/* Constraint senses */
typedef enum {
    CXF_LESS_EQUAL = '<',
    CXF_GREATER_EQUAL = '>',
    CXF_EQUAL = '='
} CxfSense;

/* Constants */
#define CXF_INFINITY 1e100
#define CXF_FEASIBILITY_TOL 1e-6
#define CXF_OPTIMALITY_TOL 1e-6
#define CXF_PIVOT_TOL 1e-10
#define CXF_MAX_NAME_LEN 255

/* Magic numbers for validation */
#define CXF_ENV_MAGIC 0xC0FEFE1D
#define CXF_MODEL_MAGIC 0xC0FEFE1D
#define CXF_CALLBACK_MAGIC 0xCA11BAC7
#define CXF_CALLBACK_MAGIC2 0xF1E1D5AFE7E57A7EULL

/* Forward declarations */
typedef struct CxfEnv CxfEnv;
typedef struct CxfModel CxfModel;
typedef struct SparseMatrix SparseMatrix;
typedef struct SolverContext SolverContext;
typedef struct BasisState BasisState;
typedef struct EtaFactors EtaFactors;
typedef struct PricingContext PricingContext;
typedef struct CallbackContext CallbackContext;

#endif /* CXF_TYPES_H */
```

### Step 0.3: Setup Unity Test Framework

**LOC:** ~50
**File:** `tests/CMakeLists.txt`

```cmake
# Include Unity
add_library(unity STATIC unity/unity.c)
target_include_directories(unity PUBLIC unity)

# Test executable template
function(add_cxf_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name} PRIVATE unity convexfeld m)
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()
```

### Step 0.4: Create Module Headers (Stubs)

**LOC:** ~20 each, ~160 total
**Files:** All header files in `include/convexfeld/`

Create stub headers for all 8 structures with forward declarations.

---

## Milestone 1: Tracer Bullet

**Goal:** Minimal end-to-end test from API to simplex core
**Parallelizable:** Steps 1.1-1.8 after Step 1.0
**Spec Reference:** End-to-end integration demonstrating all layers

This milestone proves every layer works together by solving a trivial 1-variable LP.

### Step 1.0: Tracer Bullet Test (FIRST)

**LOC:** ~80
**File:** `tests/integration/test_tracer_bullet.c`
**Spec:** End-to-end integration test

```c
#include "unity.h"
#include "convexfeld/convexfeld.h"

void setUp(void) {}
void tearDown(void) {}

void test_tracer_bullet_1var_lp(void) {
    /* Solve: min x subject to x >= 0 */
    /* Expected: x* = 0, obj* = 0 */

    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;
    double objval;

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(env);

    status = cxf_newmodel(env, &model, "tracer");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_NOT_NULL(model);

    /* Add variable: lb=0, ub=inf, obj=1.0, type='C', name="x" */
    status = cxf_addvar(model, 0.0, CXF_INFINITY, 1.0, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    int opt_status;
    status = cxf_getintattr(model, "Status", &opt_status);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt_status);

    status = cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, objval);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tracer_bullet_1var_lp);
    return UNITY_END();
}
```

### Step 1.1: Stub CxfEnv Structure

**LOC:** ~100
**File:** `include/convexfeld/cxf_env.h`, `src/api/env_stub.c`
**Spec:** `docs/specs/structures/cxf_env.md`

```c
/* include/convexfeld/cxf_env.h */
#ifndef CXF_ENV_H
#define CXF_ENV_H

#include "cxf_types.h"

struct CxfEnv {
    uint32_t magic;           /* 0xC0FEFE1D for validation */
    int active;               /* 1 if environment is active */
    char error_buffer[512];   /* Last error message */
    double feasibility_tol;   /* Primal feasibility tolerance */
    double optimality_tol;    /* Dual optimality tolerance */
    double infinity;          /* Infinity threshold */
};

/* Stub functions for tracer bullet */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
void cxf_freeenv(CxfEnv *env);

#endif /* CXF_ENV_H */
```

### Step 1.2: Stub CxfModel Structure

**LOC:** ~120
**File:** `include/convexfeld/cxf_model.h`, `src/api/model_stub.c`
**Spec:** `docs/specs/structures/cxf_model.md`

```c
/* include/convexfeld/cxf_model.h */
#ifndef CXF_MODEL_H
#define CXF_MODEL_H

#include "cxf_types.h"
#include "cxf_env.h"

struct CxfModel {
    uint32_t magic;           /* 0xC0FEFE1D for validation */
    CxfEnv *env;              /* Parent environment */
    char name[CXF_MAX_NAME_LEN + 1];
    int num_vars;             /* Number of variables */
    int num_constrs;          /* Number of constraints */
    double *obj_coeffs;       /* Objective coefficients [num_vars] */
    double *lb;               /* Lower bounds [num_vars] */
    double *ub;               /* Upper bounds [num_vars] */
    double *solution;         /* Solution values [num_vars] */
    int status;               /* Optimization status */
    double obj_val;           /* Objective value */
};

/* Stub functions for tracer bullet */
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name);
void cxf_freemodel(CxfModel *model);
int cxf_addvar(CxfModel *model, double lb, double ub, double obj,
               char vtype, const char *name);

#endif /* CXF_MODEL_H */
```

### Step 1.3: Stub SparseMatrix Structure

**LOC:** ~80
**File:** `include/convexfeld/cxf_matrix.h`, `src/matrix/sparse_stub.c`
**Spec:** `docs/specs/structures/sparse_matrix.md`

```c
/* include/convexfeld/cxf_matrix.h */
#ifndef CXF_MATRIX_H
#define CXF_MATRIX_H

#include "cxf_types.h"

/* CSC (Compressed Sparse Column) format */
struct SparseMatrix {
    int num_rows;             /* Number of rows (m) */
    int num_cols;             /* Number of columns (n) */
    int64_t nnz;              /* Number of non-zeros */
    int64_t *col_ptr;         /* Column pointers [num_cols + 1] */
    int *row_idx;             /* Row indices [nnz] */
    double *values;           /* Non-zero values [nnz] */
    /* Optional CSR (lazy conversion) */
    int64_t *row_ptr;         /* Row pointers (NULL if not built) */
    int *col_idx;             /* Column indices (NULL if not built) */
    double *row_values;       /* Row-major values (NULL if not built) */
};

#endif /* CXF_MATRIX_H */
```

### Step 1.4: Stub API Functions

**LOC:** ~150
**File:** `src/api/api_stub.c`
**Spec:** `docs/specs/modules/17_model_api.md`

Implement minimal stubs for:
- `cxf_loadenv()` - Allocate and initialize CxfEnv
- `cxf_freeenv()` - Free CxfEnv
- `cxf_newmodel()` - Allocate and initialize CxfModel
- `cxf_freemodel()` - Free CxfModel
- `cxf_addvar()` - Add variable to model arrays
- `cxf_optimize()` - Call simplex stub
- `cxf_getintattr()` - Return status
- `cxf_getdblattr()` - Return objective value

### Step 1.5: Stub Simplex Entry

**LOC:** ~100
**File:** `src/simplex/solve_lp_stub.c`
**Spec:** `docs/specs/functions/simplex/cxf_solve_lp.md`

```c
#include "convexfeld/cxf_model.h"

/* Stub for tracer bullet: trivial 1-var LP */
int cxf_solve_lp(CxfModel *model) {
    if (model == NULL) return CXF_ERROR_NULL_ARGUMENT;

    /* For unconstrained 1-var LP: optimal at lower bound */
    if (model->num_vars == 1 && model->num_constrs == 0) {
        if (model->solution == NULL) {
            model->solution = (double *)malloc(sizeof(double));
            if (model->solution == NULL) return CXF_ERROR_OUT_OF_MEMORY;
        }

        double x_opt = model->lb[0];  /* Optimal at lower bound for min */
        if (model->obj_coeffs[0] < 0) {
            x_opt = model->ub[0];     /* Optimal at upper bound if c < 0 */
        }

        model->solution[0] = x_opt;
        model->obj_val = model->obj_coeffs[0] * x_opt;
        model->status = CXF_OPTIMAL;
        return CXF_OK;
    }

    /* Full implementation comes later */
    return CXF_ERROR_DATA_NOT_AVAILABLE;
}
```

### Step 1.6: Stub Memory Functions

**LOC:** ~60
**File:** `src/memory/alloc_stub.c`
**Spec:** `docs/specs/functions/memory/cxf_malloc.md`, `docs/specs/functions/memory/cxf_free.md`

```c
#include <stdlib.h>
#include "convexfeld/cxf_types.h"

void *cxf_malloc(size_t size) {
    if (size == 0) return NULL;
    return malloc(size);
}

void *cxf_calloc(size_t count, size_t size) {
    if (count == 0 || size == 0) return NULL;
    return calloc(count, size);
}

void cxf_free(void *ptr) {
    free(ptr);
}
```

### Step 1.7: Stub Error Functions

**LOC:** ~60
**File:** `src/error/error_stub.c`
**Spec:** `docs/specs/functions/error_logging/cxf_error.md`

```c
#include <string.h>
#include <stdio.h>
#include "convexfeld/cxf_env.h"

int cxf_error(CxfEnv *env, int errcode, const char *msg) {
    if (env != NULL && msg != NULL) {
        snprintf(env->error_buffer, sizeof(env->error_buffer), "%s", msg);
    }
    return errcode;
}

const char *cxf_geterrormsg(CxfEnv *env) {
    if (env == NULL) return "Invalid environment";
    return env->error_buffer;
}
```

### Step 1.8: Tracer Bullet Benchmark

**LOC:** ~50
**File:** `benchmarks/bench_tracer.c`

```c
#include <stdio.h>
#include <time.h>
#include "convexfeld/convexfeld.h"

#define ITERATIONS 10000

int main(void) {
    clock_t start, end;
    double cpu_time;

    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        CxfEnv *env;
        CxfModel *model;
        cxf_loadenv(&env, NULL);
        cxf_newmodel(env, &model, "bench");
        cxf_addvar(model, 0.0, CXF_INFINITY, 1.0, 'C', "x");
        cxf_optimize(model);
        cxf_freemodel(model);
        cxf_freeenv(env);
    }
    end = clock();

    cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Tracer bullet: %d iterations in %.3f sec (%.3f us/iter)\n",
           ITERATIONS, cpu_time, (cpu_time * 1e6) / ITERATIONS);
    return 0;
}
```

**Milestone 1 Complete When:** `ctest -R tracer_bullet` passes

---

## Milestone 2: Foundation Layer (Level 0)

**Goal:** Complete Memory, Parameters, Validation modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/01_memory.md`
- `docs/specs/modules/04_parameters.md`
- `docs/specs/modules/05_validation.md`

### Module: Memory Management (9 functions)

**Spec Directory:** `docs/specs/functions/memory/`

#### Step 2.1.1: Memory Tests

**LOC:** ~150
**File:** `tests/unit/test_memory.c`

```c
#include "unity.h"
#include "convexfeld/cxf_types.h"

/* External declarations */
void *cxf_malloc(size_t size);
void *cxf_calloc(size_t count, size_t size);
void *cxf_realloc(void *ptr, size_t size);
void cxf_free(void *ptr);

void setUp(void) {}
void tearDown(void) {}

void test_cxf_malloc_basic(void) {
    void *ptr = cxf_malloc(100);
    TEST_ASSERT_NOT_NULL(ptr);
    cxf_free(ptr);
}

void test_cxf_malloc_zero_size(void) {
    void *ptr = cxf_malloc(0);
    TEST_ASSERT_NULL(ptr);
}

void test_cxf_calloc_zeroed(void) {
    int *arr = (int *)cxf_calloc(10, sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    for (int i = 0; i < 10; i++) {
        TEST_ASSERT_EQUAL_INT(0, arr[i]);
    }
    cxf_free(arr);
}

void test_cxf_realloc_grow(void) {
    int *arr = (int *)cxf_malloc(10 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    arr[0] = 42;
    arr = (int *)cxf_realloc(arr, 20 * sizeof(int));
    TEST_ASSERT_NOT_NULL(arr);
    TEST_ASSERT_EQUAL_INT(42, arr[0]);
    cxf_free(arr);
}

void test_cxf_free_null_safe(void) {
    cxf_free(NULL); /* Should not crash */
    TEST_PASS();
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cxf_malloc_basic);
    RUN_TEST(test_cxf_malloc_zero_size);
    RUN_TEST(test_cxf_calloc_zeroed);
    RUN_TEST(test_cxf_realloc_grow);
    RUN_TEST(test_cxf_free_null_safe);
    return UNITY_END();
}
```

#### Step 2.1.2: cxf_malloc, cxf_calloc, cxf_realloc, cxf_free

**LOC:** ~100
**File:** `src/memory/alloc.c`
**Specs:**
- `docs/specs/functions/memory/cxf_malloc.md`
- `docs/specs/functions/memory/cxf_calloc.md`
- `docs/specs/functions/memory/cxf_realloc.md`
- `docs/specs/functions/memory/cxf_free.md`

#### Step 2.1.3: cxf_vector_free, cxf_alloc_eta

**LOC:** ~100
**File:** `src/memory/vectors.c`
**Specs:**
- `docs/specs/functions/memory/cxf_vector_free.md`
- `docs/specs/functions/memory/cxf_alloc_eta.md`

#### Step 2.1.4: State Deallocators

**LOC:** ~120
**File:** `src/memory/state_cleanup.c`
**Specs:**
- `docs/specs/functions/memory/cxf_free_solver_state.md`
- `docs/specs/functions/memory/cxf_free_basis_state.md`
- `docs/specs/functions/memory/cxf_free_callback_state.md`

---

### Module: Parameters (4 functions)

**Spec Directory:** `docs/specs/functions/parameters/`

#### Step 2.2.1: Parameters Tests

**LOC:** ~80
**File:** `tests/unit/test_parameters.c`

```c
#include "unity.h"
#include "convexfeld/cxf_env.h"

void setUp(void) {}
void tearDown(void) {}

void test_cxf_get_feasibility_tol(void) {
    CxfEnv env = {0};
    env.magic = CXF_ENV_MAGIC;
    env.feasibility_tol = 1e-6;

    double tol = cxf_get_feasibility_tol(&env);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1e-6, tol);
}

void test_cxf_get_optimality_tol(void) {
    CxfEnv env = {0};
    env.magic = CXF_ENV_MAGIC;
    env.optimality_tol = 1e-6;

    double tol = cxf_get_optimality_tol(&env);
    TEST_ASSERT_DOUBLE_WITHIN(1e-12, 1e-6, tol);
}

void test_cxf_get_infinity(void) {
    CxfEnv env = {0};
    env.magic = CXF_ENV_MAGIC;
    env.infinity = CXF_INFINITY;

    double inf = cxf_get_infinity(&env);
    TEST_ASSERT_DOUBLE_WITHIN(1e90, CXF_INFINITY, inf);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cxf_get_feasibility_tol);
    RUN_TEST(test_cxf_get_optimality_tol);
    RUN_TEST(test_cxf_get_infinity);
    return UNITY_END();
}
```

#### Step 2.2.2: Parameter Getters

**LOC:** ~100
**File:** `src/parameters/getters.c`
**Specs:**
- `docs/specs/functions/parameters/cxf_getdblparam.md`
- `docs/specs/functions/parameters/cxf_get_feasibility_tol.md`
- `docs/specs/functions/parameters/cxf_get_optimality_tol.md`
- `docs/specs/functions/parameters/cxf_get_infinity.md`

---

### Module: Validation (2 functions)

**Spec Directory:** `docs/specs/functions/validation/`

#### Step 2.3.1: Validation Tests

**LOC:** ~100
**File:** `tests/unit/test_validation.c`

```c
#include "unity.h"
#include <math.h>

void setUp(void) {}
void tearDown(void) {}

void test_cxf_validate_array_valid(void) {
    double arr[] = {1.0, 2.0, 3.0};
    int result = cxf_validate_array(arr, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_array_nan(void) {
    double arr[] = {1.0, NAN, 3.0};
    int result = cxf_validate_array(arr, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_array_inf(void) {
    double arr[] = {1.0, INFINITY, 3.0};
    int result = cxf_validate_array(arr, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

void test_cxf_validate_vartypes_valid(void) {
    char types[] = {'C', 'B', 'I'};
    int result = cxf_validate_vartypes(types, 3);
    TEST_ASSERT_EQUAL_INT(CXF_OK, result);
}

void test_cxf_validate_vartypes_invalid(void) {
    char types[] = {'C', 'X', 'I'};
    int result = cxf_validate_vartypes(types, 3);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, result);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_cxf_validate_array_valid);
    RUN_TEST(test_cxf_validate_array_nan);
    RUN_TEST(test_cxf_validate_array_inf);
    RUN_TEST(test_cxf_validate_vartypes_valid);
    RUN_TEST(test_cxf_validate_vartypes_invalid);
    return UNITY_END();
}
```

#### Step 2.3.2: Array Validation

**LOC:** ~80
**File:** `src/validation/arrays.c`
**Specs:**
- `docs/specs/functions/validation/cxf_validate_array.md`
- `docs/specs/functions/validation/cxf_validate_vartypes.md`

---

## Milestone 3: Infrastructure Layer (Level 1)

**Goal:** Complete Error Handling, Logging, Threading modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/02_error_handling.md`
- `docs/specs/modules/03_logging.md`
- `docs/specs/modules/12_threading.md`

### Module: Error Handling (10 functions)

**Spec Directory:** `docs/specs/functions/error_logging/`, `docs/specs/functions/validation/`

#### Step 3.1.1: Error Tests

**LOC:** ~150
**File:** `tests/unit/test_error.c`

Tests for all 10 error handling functions.

#### Step 3.1.2: Core Error Functions

**LOC:** ~100
**File:** `src/error/core.c`
**Specs:**
- `docs/specs/functions/error_logging/cxf_error.md`
- `docs/specs/functions/error_logging/cxf_errorlog.md`

#### Step 3.1.3: NaN/Inf Detection

**LOC:** ~80
**File:** `src/error/nan_check.c`
**Specs:**
- `docs/specs/functions/validation/cxf_check_nan.md`
- `docs/specs/functions/validation/cxf_check_nan_or_inf.md`

#### Step 3.1.4: Environment Validation

**LOC:** ~60
**File:** `src/error/env_check.c`
**Spec:** `docs/specs/functions/validation/cxf_checkenv.md`

#### Step 3.1.5: Model Flag Checks

**LOC:** ~100
**File:** `src/error/model_flags.c`
**Specs:**
- `docs/specs/functions/validation/cxf_check_model_flags1.md`
- `docs/specs/functions/validation/cxf_check_model_flags2.md`

#### Step 3.1.6: Termination Check

**LOC:** ~60
**File:** `src/error/terminate.c`
**Spec:** `docs/specs/functions/callbacks/cxf_check_terminate.md`

#### Step 3.1.7: Pivot Validation

**LOC:** ~80
**File:** `src/error/pivot_check.c`
**Specs:**
- `docs/specs/functions/ratio_test/cxf_pivot_check.md`
- `docs/specs/functions/statistics/cxf_special_check.md`

---

### Module: Logging (5 functions)

**Spec Directory:** `docs/specs/functions/error_logging/`, `docs/specs/functions/utilities/`

#### Step 3.2.1: Logging Tests

**LOC:** ~100
**File:** `tests/unit/test_logging.c`

#### Step 3.2.2: Log Output

**LOC:** ~100
**File:** `src/logging/output.c`
**Specs:**
- `docs/specs/functions/error_logging/cxf_log_printf.md`
- `docs/specs/functions/error_logging/cxf_register_log_callback.md`

#### Step 3.2.3: Format Helpers

**LOC:** ~80
**File:** `src/logging/format.c`
**Specs:**
- `docs/specs/functions/utilities/cxf_log10_wrapper.md`
- `docs/specs/functions/utilities/cxf_snprintf_wrapper.md`

#### Step 3.2.4: System Info

**LOC:** ~60
**File:** `src/logging/system.c`
**Spec:** `docs/specs/functions/threading/cxf_get_logical_processors.md`

---

### Module: Threading (7 functions)

**Spec Directory:** `docs/specs/functions/threading/`

#### Step 3.3.1: Threading Tests

**LOC:** ~150
**File:** `tests/unit/test_threading.c`

#### Step 3.3.2: Lock Management

**LOC:** ~120
**File:** `src/threading/locks.c`
**Specs:**
- `docs/specs/functions/threading/cxf_acquire_solve_lock.md`
- `docs/specs/functions/threading/cxf_release_solve_lock.md`
- `docs/specs/functions/threading/cxf_env_acquire_lock.md`
- `docs/specs/functions/threading/cxf_leave_critical_section.md`

#### Step 3.3.3: Thread Configuration

**LOC:** ~100
**File:** `src/threading/config.c`
**Specs:**
- `docs/specs/functions/threading/cxf_get_threads.md`
- `docs/specs/functions/threading/cxf_set_thread_count.md`

#### Step 3.3.4: CPU Detection

**LOC:** ~80
**File:** `src/threading/cpu.c`
**Specs:**
- `docs/specs/functions/threading/cxf_get_logical_processors.md`
- `docs/specs/functions/threading/cxf_get_physical_cores.md`

#### Step 3.3.5: Seed Generation

**LOC:** ~60
**File:** `src/threading/seed.c`
**Spec:** `docs/specs/functions/threading/cxf_generate_seed.md`

---

## Milestone 4: Data Layer (Level 2)

**Goal:** Complete Matrix, Timing, Model Analysis modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/09_matrix.md`
- `docs/specs/modules/11_timing.md`
- `docs/specs/modules/14_model_analysis.md`

### Module: Matrix Operations (7 functions)

**Spec Directory:** `docs/specs/functions/matrix/`

#### Step 4.1.1: Matrix Tests

**LOC:** ~200
**File:** `tests/unit/test_matrix.c`

Comprehensive tests including SpMV, dot product, norms.

#### Step 4.1.2: SparseMatrix Structure (Full)

**LOC:** ~150
**File:** `src/matrix/sparse_matrix.c`
**Spec:** `docs/specs/structures/sparse_matrix.md`

Full CSC/CSR implementation with:
- Creation/destruction
- Validation
- Format conversion

#### Step 4.1.3: cxf_matrix_multiply

**LOC:** ~100
**File:** `src/matrix/multiply.c`
**Spec:** `docs/specs/functions/matrix/cxf_matrix_multiply.md`

```c
/* Sparse matrix-vector multiply: y = A * x */
int cxf_matrix_multiply(const SparseMatrix *A, const double *x,
                        double *y, int transpose);
```

#### Step 4.1.4: cxf_dot_product, cxf_vector_norm

**LOC:** ~100
**File:** `src/matrix/vectors.c`
**Specs:**
- `docs/specs/functions/matrix/cxf_dot_product.md`
- `docs/specs/functions/matrix/cxf_vector_norm.md`

#### Step 4.1.5: Row-Major Conversion

**LOC:** ~150
**File:** `src/matrix/row_major.c`
**Specs:**
- `docs/specs/functions/matrix/cxf_build_row_major.md`
- `docs/specs/functions/matrix/cxf_prepare_row_data.md`
- `docs/specs/functions/matrix/cxf_finalize_row_data.md`

#### Step 4.1.6: cxf_sort_indices

**LOC:** ~80
**File:** `src/matrix/sort.c`
**Spec:** `docs/specs/functions/matrix/cxf_sort_indices.md`

---

### Module: Timing (5 functions)

**Spec Directory:** `docs/specs/functions/timing/`

#### Step 4.2.1: Timing Tests

**LOC:** ~100
**File:** `tests/unit/test_timing.c`

#### Step 4.2.2: Timestamp

**LOC:** ~60
**File:** `src/timing/timestamp.c`
**Spec:** `docs/specs/functions/timing/cxf_get_timestamp.md`

```c
#include <time.h>

double cxf_get_timestamp(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
```

#### Step 4.2.3: Timing Sections

**LOC:** ~100
**File:** `src/timing/sections.c`
**Specs:**
- `docs/specs/functions/timing/cxf_timing_start.md`
- `docs/specs/functions/timing/cxf_timing_end.md`
- `docs/specs/functions/timing/cxf_timing_update.md`

#### Step 4.2.4: Operation Timing

**LOC:** ~80
**File:** `src/timing/operations.c`
**Specs:**
- `docs/specs/functions/timing/cxf_timing_pivot.md`
- `docs/specs/functions/basis/cxf_timing_refactor.md`

---

### Module: Model Analysis (6 functions)

**Spec Directory:** `docs/specs/functions/validation/`, `docs/specs/functions/statistics/`

#### Step 4.3.1: Analysis Tests

**LOC:** ~100
**File:** `tests/unit/test_analysis.c`

#### Step 4.3.2: Model Type Checks

**LOC:** ~80
**File:** `src/analysis/model_type.c`
**Specs:**
- `docs/specs/functions/validation/cxf_is_mip_model.md`
- `docs/specs/functions/validation/cxf_is_quadratic.md`
- `docs/specs/functions/validation/cxf_is_socp.md`

#### Step 4.3.3: Coefficient Statistics

**LOC:** ~120
**File:** `src/analysis/coef_stats.c`
**Specs:**
- `docs/specs/functions/statistics/cxf_coefficient_stats.md`
- `docs/specs/functions/statistics/cxf_compute_coef_stats.md`

#### Step 4.3.4: Presolve Statistics

**LOC:** ~80
**File:** `src/analysis/presolve_stats.c`
**Spec:** `docs/specs/functions/statistics/cxf_presolve_stats.md`

---

## Milestone 5: Core Operations (Level 3)

**Goal:** Complete Basis, Callbacks, Solver State modules
**Parallelizable:** All three modules can run in parallel
**Spec References:**
- `docs/specs/modules/07_basis.md`
- `docs/specs/modules/13_callbacks.md`
- `docs/specs/modules/10_solver_state.md`

### Module: Basis Operations (8 functions)

**Spec Directory:** `docs/specs/functions/basis/`
**Structure Spec:** `docs/specs/structures/basis_state.md`, `docs/specs/structures/eta_factors.md`

#### Step 5.1.1: Basis Tests

**LOC:** ~250
**File:** `tests/unit/test_basis.c`

Comprehensive tests for FTRAN, BTRAN, refactorization.

#### Step 5.1.2: BasisState Structure

**LOC:** ~150
**File:** `include/convexfeld/cxf_basis.h`, `src/basis/basis_state.c`
**Spec:** `docs/specs/structures/basis_state.md`

```c
struct BasisState {
    int m;                    /* Number of basic variables */
    int *basic_vars;          /* Indices of basic variables [m] */
    int *var_status;          /* Status of each variable [n] */
    int eta_count;            /* Number of eta vectors */
    int eta_capacity;         /* Capacity for eta vectors */
    EtaFactors **eta_list;    /* Array of eta factor pointers */
    double *work;             /* Working array [m] */
    int refactor_freq;        /* Refactorization frequency */
    int pivots_since_refactor;/* Pivots since last refactor */
};
```

#### Step 5.1.3: EtaFactors Structure

**LOC:** ~100
**File:** `src/basis/eta_factors.c`
**Spec:** `docs/specs/structures/eta_factors.md`

```c
struct EtaFactors {
    int type;                 /* 1=column, 2=row based */
    int pivot_row;            /* Row index for pivot */
    int nnz;                  /* Non-zeros in eta vector */
    int *indices;             /* Row indices [nnz] */
    double *values;           /* Values [nnz] */
    double pivot_elem;        /* Pivot element */
};
```

#### Step 5.1.4: cxf_ftran

**LOC:** ~150
**File:** `src/basis/ftran.c`
**Spec:** `docs/specs/functions/basis/cxf_ftran.md`

Forward transformation: solve Bx = b using eta representation.

#### Step 5.1.5: cxf_btran

**LOC:** ~150
**File:** `src/basis/btran.c`
**Spec:** `docs/specs/functions/basis/cxf_btran.md`

Backward transformation: solve y^T B = c^T.

#### Step 5.1.6: cxf_basis_refactor

**LOC:** ~200
**File:** `src/basis/refactor.c`
**Spec:** `docs/specs/functions/basis/cxf_basis_refactor.md`

LU factorization of basis matrix.

#### Step 5.1.7: Basis Snapshots

**LOC:** ~120
**File:** `src/basis/snapshot.c`
**Specs:**
- `docs/specs/functions/basis/cxf_basis_snapshot.md`
- `docs/specs/functions/basis/cxf_basis_diff.md`
- `docs/specs/functions/basis/cxf_basis_equal.md`

#### Step 5.1.8: Basis Validation/Warm Start

**LOC:** ~100
**File:** `src/basis/warm.c`
**Specs:**
- `docs/specs/functions/basis/cxf_basis_validate.md`
- `docs/specs/functions/basis/cxf_basis_warm.md`

---

### Module: Callbacks (6 functions)

**Spec Directory:** `docs/specs/functions/callbacks/`
**Structure Spec:** `docs/specs/structures/callback_context.md`

#### Step 5.2.1: Callbacks Tests

**LOC:** ~120
**File:** `tests/unit/test_callbacks.c`

#### Step 5.2.2: CallbackContext Structure

**LOC:** ~100
**File:** `include/convexfeld/cxf_callback.h`, `src/callbacks/context.c`
**Spec:** `docs/specs/structures/callback_context.md`

```c
struct CallbackContext {
    uint32_t magic;                    /* 0xCA11BAC7 */
    uint64_t safety_magic;             /* 0xF1E1D5AFE7E57A7E */
    int (*callback_func)(CxfModel *, void *);
    void *user_data;
    int terminate_requested;
    double start_time;
    int iteration_count;
    double best_obj;
};
```

#### Step 5.2.3: Callback Initialization

**LOC:** ~80
**File:** `src/callbacks/init.c`
**Specs:**
- `docs/specs/functions/callbacks/cxf_init_callback_struct.md`
- `docs/specs/functions/callbacks/cxf_reset_callback_state.md`

#### Step 5.2.4: Callback Invocation

**LOC:** ~100
**File:** `src/callbacks/invoke.c`
**Specs:**
- `docs/specs/functions/callbacks/cxf_pre_optimize_callback.md`
- `docs/specs/functions/callbacks/cxf_post_optimize_callback.md`

#### Step 5.2.5: Termination Handling

**LOC:** ~80
**File:** `src/callbacks/terminate.c`
**Specs:**
- `docs/specs/functions/callbacks/cxf_callback_terminate.md`
- `docs/specs/functions/callbacks/cxf_set_terminate.md`

---

### Module: Solver State (4 functions)

**Spec Directory:** `docs/specs/functions/memory/`, `docs/specs/functions/utilities/`
**Structure Spec:** `docs/specs/structures/solver_context.md`

#### Step 5.3.1: Solver State Tests

**LOC:** ~100
**File:** `tests/unit/test_solver_state.c`

#### Step 5.3.2: SolverContext Structure

**LOC:** ~150
**File:** `include/convexfeld/cxf_solver.h`, `src/solver_state/context.c`
**Spec:** `docs/specs/structures/solver_context.md`

```c
struct SolverContext {
    CxfModel *model_ref;      /* Back-pointer to model */
    int phase;                /* 0=setup, 1=phase I, 2=phase II */
    int num_vars;
    int num_constrs;
    int64_t num_nonzeros;
    int solve_mode;           /* 0=primal, 1=dual, 2=barrier */
    int max_iterations;
    double tolerance;
    double obj_value;
    /* Working arrays */
    double *work_lb;          /* Working lower bounds [num_vars] */
    double *work_ub;          /* Working upper bounds [num_vars] */
    double *work_obj;         /* Working objective [num_vars] */
    double *work_x;           /* Current solution [num_vars] */
    double *work_pi;          /* Dual values [num_constrs] */
    double *work_dj;          /* Reduced costs [num_vars] */
    /* Subcomponents */
    BasisState *basis;
    PricingContext *pricing;
};
```

#### Step 5.3.3: State Initialization

**LOC:** ~100
**File:** `src/solver_state/init.c`
**Specs:**
- `docs/specs/functions/memory/cxf_init_solve_state.md`
- `docs/specs/functions/memory/cxf_cleanup_solve_state.md`

#### Step 5.3.4: Helper Functions

**LOC:** ~80
**File:** `src/solver_state/helpers.c`
**Spec:** `docs/specs/functions/utilities/cxf_cleanup_helper.md`

#### Step 5.3.5: Solution Extraction

**LOC:** ~100
**File:** `src/solver_state/extract.c`
**Spec:** `docs/specs/functions/utilities/cxf_extract_solution.md`

---

## Milestone 6: Algorithm Layer (Level 4)

**Goal:** Complete Pricing module
**Parallelizable:** All steps can run in parallel
**Spec Reference:** `docs/specs/modules/08_pricing.md`
**Structure Spec:** `docs/specs/structures/pricing_context.md`

### Module: Pricing (6 functions)

**Spec Directory:** `docs/specs/functions/pricing/`

#### Step 6.1.1: Pricing Tests

**LOC:** ~180
**File:** `tests/unit/test_pricing.c`

Tests for all pricing strategies including steepest edge.

#### Step 6.1.2: PricingContext Structure

**LOC:** ~120
**File:** `include/convexfeld/cxf_pricing.h`, `src/pricing/context.c`
**Spec:** `docs/specs/structures/pricing_context.md`

```c
struct PricingContext {
    int current_level;        /* Active pricing level (0=full) */
    int max_levels;           /* Number of levels (typically 3-5) */
    int *candidate_counts;    /* Candidates at each level */
    int **candidate_arrays;   /* Variable indices per level */
    int *cached_counts;       /* Cached result count (-1=invalid) */
    int last_pivot_iteration;
    int64_t total_candidates_scanned;
    int level_escalations;
};
```

#### Step 6.1.3: cxf_pricing_init

**LOC:** ~100
**File:** `src/pricing/init.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_init.md`

#### Step 6.1.4: cxf_pricing_candidates

**LOC:** ~120
**File:** `src/pricing/candidates.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_candidates.md`

#### Step 6.1.5: cxf_pricing_steepest

**LOC:** ~150
**File:** `src/pricing/steepest.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_steepest.md`

Steepest edge pricing implementation.

#### Step 6.1.6: cxf_pricing_update, cxf_pricing_invalidate

**LOC:** ~100
**File:** `src/pricing/update.c`
**Specs:**
- `docs/specs/functions/pricing/cxf_pricing_update.md`
- `docs/specs/functions/pricing/cxf_pricing_invalidate.md`

#### Step 6.1.7: cxf_pricing_step2

**LOC:** ~80
**File:** `src/pricing/phase.c`
**Spec:** `docs/specs/functions/pricing/cxf_pricing_step2.md`

---

## Milestone 7: Simplex Engine (Level 5)

**Goal:** Complete Simplex Core, Crossover modules
**Parallelizable:** Simplex phases sequential; Crossover parallel with late Simplex
**Spec References:**
- `docs/specs/modules/06_simplex_core.md`
- `docs/specs/modules/15_crossover.md`

### Module: Simplex Core (21 functions)

**Spec Directory:** `docs/specs/functions/simplex/`, `docs/specs/functions/ratio_test/`, `docs/specs/functions/pivot/`

#### Step 7.1.1: Simplex Tests - Basic

**LOC:** ~200
**File:** `tests/unit/test_simplex_basic.c`

Tests for initialization, setup, cleanup.

#### Step 7.1.2: Simplex Tests - Iteration

**LOC:** ~200
**File:** `tests/unit/test_simplex_iteration.c`

Tests for iteration loop, phases.

#### Step 7.1.3: Simplex Tests - Edge Cases

**LOC:** ~200
**File:** `tests/unit/test_simplex_edge.c`

Tests for degeneracy, unbounded, infeasible.

#### Step 7.1.4: cxf_solve_lp

**LOC:** ~150
**File:** `src/simplex/solve_lp.c`
**Spec:** `docs/specs/functions/simplex/cxf_solve_lp.md`

Main entry point replacing stub.

#### Step 7.1.5: cxf_simplex_init, cxf_simplex_final

**LOC:** ~120
**File:** `src/simplex/lifecycle.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_init.md`
- `docs/specs/functions/simplex/cxf_simplex_final.md`

#### Step 7.1.6: cxf_simplex_setup, cxf_simplex_preprocess

**LOC:** ~150
**File:** `src/simplex/setup.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_setup.md`
- `docs/specs/functions/simplex/cxf_simplex_preprocess.md`

#### Step 7.1.7: cxf_simplex_crash

**LOC:** ~120
**File:** `src/simplex/crash.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_crash.md`

Initial basis heuristic.

#### Step 7.1.8: cxf_simplex_iterate

**LOC:** ~150
**File:** `src/simplex/iterate.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_iterate.md`

Main iteration loop.

#### Step 7.1.9: cxf_simplex_step

**LOC:** ~150
**File:** `src/simplex/step.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_step.md`

Single iteration.

#### Step 7.1.10: cxf_simplex_step2, cxf_simplex_step3

**LOC:** ~120
**File:** `src/simplex/phase_steps.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_step2.md`
- `docs/specs/functions/simplex/cxf_simplex_step3.md`

#### Step 7.1.11: cxf_simplex_post_iterate, cxf_simplex_phase_end

**LOC:** ~100
**File:** `src/simplex/post.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_post_iterate.md`
- `docs/specs/functions/simplex/cxf_simplex_phase_end.md`

#### Step 7.1.12: cxf_simplex_refine

**LOC:** ~120
**File:** `src/simplex/refine.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_refine.md`

Iterative refinement.

#### Step 7.1.13: cxf_simplex_perturbation, cxf_simplex_unperturb

**LOC:** ~120
**File:** `src/simplex/perturbation.c`
**Specs:**
- `docs/specs/functions/simplex/cxf_simplex_perturbation.md`
- `docs/specs/functions/simplex/cxf_simplex_unperturb.md`

Anti-cycling.

#### Step 7.1.14: cxf_simplex_cleanup

**LOC:** ~80
**File:** `src/simplex/cleanup.c`
**Spec:** `docs/specs/functions/simplex/cxf_simplex_cleanup.md`

#### Step 7.1.15: cxf_pivot_primal

**LOC:** ~150
**File:** `src/simplex/pivot_primal.c`
**Spec:** `docs/specs/functions/pivot/cxf_pivot_primal.md`

#### Step 7.1.16: cxf_pivot_bound, cxf_pivot_special

**LOC:** ~120
**File:** `src/simplex/pivot_special.c`
**Specs:**
- `docs/specs/functions/ratio_test/cxf_pivot_bound.md`
- `docs/specs/functions/ratio_test/cxf_pivot_special.md`

#### Step 7.1.17: cxf_pivot_with_eta

**LOC:** ~100
**File:** `src/simplex/eta_pivot.c`
**Spec:** `docs/specs/functions/basis/cxf_pivot_with_eta.md`

#### Step 7.1.18: cxf_ratio_test

**LOC:** ~150
**File:** `src/simplex/ratio_test.c`
**Spec:** `docs/specs/functions/ratio_test/cxf_ratio_test.md`

Harris two-pass ratio test.

#### Step 7.1.19: cxf_quadratic_adjust

**LOC:** ~80
**File:** `src/simplex/quadratic.c`
**Spec:** `docs/specs/functions/simplex/cxf_quadratic_adjust.md`

---

### Module: Crossover (2 functions)

**Spec Directory:** `docs/specs/functions/crossover/`

#### Step 7.2.1: Crossover Tests

**LOC:** ~100
**File:** `tests/unit/test_crossover.c`

#### Step 7.2.2: cxf_crossover

**LOC:** ~150
**File:** `src/crossover/crossover.c`
**Spec:** `docs/specs/functions/crossover/cxf_crossover.md`

#### Step 7.2.3: cxf_crossover_bounds

**LOC:** ~80
**File:** `src/crossover/bounds.c`
**Spec:** `docs/specs/functions/crossover/cxf_crossover_bounds.md`

---

### Module: Utilities (10 functions)

**Spec Directory:** `docs/specs/functions/utilities/`, `docs/specs/functions/pivot/`

#### Step 7.3.1: Utilities Tests

**LOC:** ~150
**File:** `tests/unit/test_utilities.c`

#### Step 7.3.2: cxf_fix_variable

**LOC:** ~80
**File:** `src/utilities/fix_var.c`
**Spec:** `docs/specs/functions/pivot/cxf_fix_variable.md`

#### Step 7.3.3: Math Wrappers

**LOC:** ~80
**File:** `src/utilities/math.c`
**Specs:**
- `docs/specs/functions/utilities/cxf_floor_ceil_wrapper.md`
- `docs/specs/functions/utilities/cxf_log10_wrapper.md`

#### Step 7.3.4: Constraint Helpers

**LOC:** ~100
**File:** `src/utilities/constraints.c`
**Specs:**
- `docs/specs/functions/utilities/cxf_count_genconstr_types.md`
- `docs/specs/functions/utilities/cxf_get_genconstr_name.md`
- `docs/specs/functions/utilities/cxf_get_qconstr_data.md`

#### Step 7.3.5: Multi-Objective Check

**LOC:** ~60
**File:** `src/utilities/multi_obj.c`
**Spec:** `docs/specs/functions/utilities/cxf_is_multi_obj.md`

#### Step 7.3.6: Misc Utility

**LOC:** ~60
**File:** `src/utilities/misc.c`
**Spec:** `docs/specs/functions/utilities/cxf_misc_utility.md`

---

## Milestone 8: Public API (Level 6)

**Goal:** Complete Model API module (30 functions)
**Parallelizable:** All API functions can be implemented in parallel
**Spec Reference:** `docs/specs/modules/17_model_api.md`

### Module: Model API (30 functions)

#### Step 8.1.1: API Tests - Environment

**LOC:** ~100
**File:** `tests/unit/test_api_env.c`

Tests for `cxf_loadenv`, `cxf_emptyenv`, `cxf_freeenv`.

#### Step 8.1.2: API Tests - Model

**LOC:** ~150
**File:** `tests/unit/test_api_model.c`

Tests for `cxf_newmodel`, `cxf_freemodel`, `cxf_copymodel`, `cxf_updatemodel`.

#### Step 8.1.3: API Tests - Variables

**LOC:** ~150
**File:** `tests/unit/test_api_vars.c`

Tests for `cxf_addvar`, `cxf_addvars`, `cxf_delvars`.

#### Step 8.1.4: API Tests - Constraints

**LOC:** ~150
**File:** `tests/unit/test_api_constrs.c`

Tests for `cxf_addconstr`, `cxf_addconstrs`, `cxf_addqconstr`, `cxf_addgenconstrIndicator`.

#### Step 8.1.5: API Tests - Optimize

**LOC:** ~200
**File:** `tests/unit/test_api_optimize.c`

Tests for `cxf_optimize`, `cxf_optimize_internal`, `cxf_terminate`.

#### Step 8.1.6: API Tests - Queries

**LOC:** ~150
**File:** `tests/unit/test_api_query.c`

Tests for `cxf_getintattr`, `cxf_getdblattr`, `cxf_getconstrs`, `cxf_getcoeff`.

#### Step 8.1.7: CxfEnv Structure (Full)

**LOC:** ~200
**File:** `src/api/env.c` (expand from stub)
**Spec:** `docs/specs/structures/cxf_env.md`

#### Step 8.1.8: CxfModel Structure (Full)

**LOC:** ~200
**File:** `src/api/model.c` (expand from stub)
**Spec:** `docs/specs/structures/cxf_model.md`

#### Step 8.1.9: Environment API

**LOC:** ~120
**File:** `src/api/env_api.c`
**Functions:** `cxf_loadenv`, `cxf_emptyenv`, `cxf_freeenv`

#### Step 8.1.10: Model Creation API

**LOC:** ~150
**File:** `src/api/model_api.c`
**Functions:** `cxf_newmodel`, `cxf_freemodel`, `cxf_copymodel`, `cxf_updatemodel`

#### Step 8.1.11: Variable API

**LOC:** ~150
**File:** `src/api/vars_api.c`
**Functions:** `cxf_addvar`, `cxf_addvars`, `cxf_delvars`

#### Step 8.1.12: Constraint API

**LOC:** ~200
**File:** `src/api/constrs_api.c`
**Functions:** `cxf_addconstr`, `cxf_addconstrs`, `cxf_chgcoeffs`, `cxf_getconstrs`, `cxf_getcoeff`

#### Step 8.1.13: Quadratic API

**LOC:** ~120
**File:** `src/api/quadratic_api.c`
**Functions:** `cxf_addqpterms`, `cxf_addqconstr`, `cxf_addgenconstrIndicator`

#### Step 8.1.14: Optimize API

**LOC:** ~150
**File:** `src/api/optimize_api.c`
**Functions:** `cxf_optimize`, `cxf_optimize_internal`, `cxf_terminate`

#### Step 8.1.15: Attribute API

**LOC:** ~120
**File:** `src/api/attrs_api.c`
**Functions:** `cxf_getintattr`, `cxf_getdblattr`

#### Step 8.1.16: Parameter API

**LOC:** ~100
**File:** `src/api/params_api.c`
**Functions:** `cxf_setintparam`, `cxf_getintparam`

#### Step 8.1.17: I/O API

**LOC:** ~150
**File:** `src/api/io_api.c`
**Functions:** `cxf_read`, `cxf_write`

#### Step 8.1.18: Info API

**LOC:** ~80
**File:** `src/api/info_api.c`
**Functions:** `cxf_version`, `cxf_geterrormsg`, `cxf_setcallbackfunc`

---

## Benchmarks

### Benchmark Suite

**File:** `benchmarks/benchmark_main.c`

```c
#include <stdio.h>
#include <time.h>
#include "convexfeld/convexfeld.h"

typedef struct {
    const char *name;
    int (*func)(void);
    double target_ms;
} Benchmark;

/* Benchmark implementations */
int bench_tracer_bullet(void);
int bench_small_lp(void);
int bench_medium_lp(void);
int bench_netlib_afiro(void);
int bench_ftran_btran(void);
int bench_pricing(void);

int main(void) {
    Benchmark benchmarks[] = {
        {"Tracer Bullet (1 var)", bench_tracer_bullet, 1.0},
        {"Small LP (10x10)", bench_small_lp, 10.0},
        {"Medium LP (100x100)", bench_medium_lp, 100.0},
        {"Netlib afiro", bench_netlib_afiro, 1000.0},
        {"FTRAN/BTRAN ops", bench_ftran_btran, 10.0},
        {"Pricing operations", bench_pricing, 10.0},
    };

    int num_benchmarks = sizeof(benchmarks) / sizeof(Benchmark);

    printf("ConvexFeld Benchmark Suite\n");
    printf("==========================\n\n");

    for (int i = 0; i < num_benchmarks; i++) {
        clock_t start = clock();
        int result = benchmarks[i].func();
        clock_t end = clock();

        double elapsed_ms = ((double)(end - start) / CLOCKS_PER_SEC) * 1000.0;
        const char *status = (elapsed_ms <= benchmarks[i].target_ms) ? "PASS" : "SLOW";

        printf("[%s] %s: %.2f ms (target: %.2f ms)\n",
               status, benchmarks[i].name, elapsed_ms, benchmarks[i].target_ms);
    }

    return 0;
}
```

### Success Criteria

| Benchmark | Target | Description |
|-----------|--------|-------------|
| Tracer Bullet | < 1ms | 1-variable LP |
| Small LP | < 10ms | 10x10 problem |
| Medium LP | < 100ms | 100x100 problem |
| Netlib afiro | < 1s | 27 vars, 51 constraints |
| FTRAN/BTRAN | < 10ms | 1000 operations on 100-dim basis |
| Pricing | < 10ms | 1000 pricing operations |

---

## Structure Definitions

### Complete Structure Checklist

| Structure | Spec File | Header | Implementation |
|-----------|-----------|--------|----------------|
| CxfEnv | `docs/specs/structures/cxf_env.md` | `include/convexfeld/cxf_env.h` | `src/api/env.c` |
| CxfModel | `docs/specs/structures/cxf_model.md` | `include/convexfeld/cxf_model.h` | `src/api/model.c` |
| SparseMatrix | `docs/specs/structures/sparse_matrix.md` | `include/convexfeld/cxf_matrix.h` | `src/matrix/sparse_matrix.c` |
| SolverContext | `docs/specs/structures/solver_context.md` | `include/convexfeld/cxf_solver.h` | `src/solver_state/context.c` |
| BasisState | `docs/specs/structures/basis_state.md` | `include/convexfeld/cxf_basis.h` | `src/basis/basis_state.c` |
| EtaFactors | `docs/specs/structures/eta_factors.md` | `include/convexfeld/cxf_basis.h` | `src/basis/eta_factors.c` |
| PricingContext | `docs/specs/structures/pricing_context.md` | `include/convexfeld/cxf_pricing.h` | `src/pricing/context.c` |
| CallbackContext | `docs/specs/structures/callback_context.md` | `include/convexfeld/cxf_callback.h` | `src/callbacks/context.c` |

---

## Function Checklist

### Memory Management (9 functions)

- [ ] `cxf_malloc` - `docs/specs/functions/memory/cxf_malloc.md`
- [ ] `cxf_calloc` - `docs/specs/functions/memory/cxf_calloc.md`
- [ ] `cxf_realloc` - `docs/specs/functions/memory/cxf_realloc.md`
- [ ] `cxf_free` - `docs/specs/functions/memory/cxf_free.md`
- [ ] `cxf_vector_free` - `docs/specs/functions/memory/cxf_vector_free.md`
- [ ] `cxf_alloc_eta` - `docs/specs/functions/memory/cxf_alloc_eta.md`
- [ ] `cxf_free_solver_state` - `docs/specs/functions/memory/cxf_free_solver_state.md`
- [ ] `cxf_free_basis_state` - `docs/specs/functions/memory/cxf_free_basis_state.md`
- [ ] `cxf_free_callback_state` - `docs/specs/functions/memory/cxf_free_callback_state.md`

### Parameters (4 functions)

- [ ] `cxf_getdblparam` - `docs/specs/functions/parameters/cxf_getdblparam.md`
- [ ] `cxf_get_feasibility_tol` - `docs/specs/functions/parameters/cxf_get_feasibility_tol.md`
- [ ] `cxf_get_optimality_tol` - `docs/specs/functions/parameters/cxf_get_optimality_tol.md`
- [ ] `cxf_get_infinity` - `docs/specs/functions/parameters/cxf_get_infinity.md`

### Validation (2 functions)

- [ ] `cxf_validate_array` - `docs/specs/functions/validation/cxf_validate_array.md`
- [ ] `cxf_validate_vartypes` - `docs/specs/functions/validation/cxf_validate_vartypes.md`

### Error Handling (10 functions)

- [ ] `cxf_error` - `docs/specs/functions/error_logging/cxf_error.md`
- [ ] `cxf_errorlog` - `docs/specs/functions/error_logging/cxf_errorlog.md`
- [ ] `cxf_check_nan` - `docs/specs/functions/validation/cxf_check_nan.md`
- [ ] `cxf_check_nan_or_inf` - `docs/specs/functions/validation/cxf_check_nan_or_inf.md`
- [ ] `cxf_checkenv` - `docs/specs/functions/validation/cxf_checkenv.md`
- [ ] `cxf_pivot_check` - `docs/specs/functions/ratio_test/cxf_pivot_check.md`
- [ ] `cxf_special_check` - `docs/specs/functions/statistics/cxf_special_check.md`
- [ ] `cxf_check_model_flags1` - `docs/specs/functions/validation/cxf_check_model_flags1.md`
- [ ] `cxf_check_model_flags2` - `docs/specs/functions/validation/cxf_check_model_flags2.md`
- [ ] `cxf_check_terminate` - `docs/specs/functions/callbacks/cxf_check_terminate.md`

### Logging (5 functions)

- [ ] `cxf_log_printf` - `docs/specs/functions/error_logging/cxf_log_printf.md`
- [ ] `cxf_log10_wrapper` - `docs/specs/functions/utilities/cxf_log10_wrapper.md`
- [ ] `cxf_snprintf_wrapper` - `docs/specs/functions/utilities/cxf_snprintf_wrapper.md`
- [ ] `cxf_register_log_callback` - `docs/specs/functions/error_logging/cxf_register_log_callback.md`
- [ ] `cxf_get_logical_processors` - `docs/specs/functions/threading/cxf_get_logical_processors.md`

### Threading (7 functions)

- [ ] `cxf_get_threads` - `docs/specs/functions/threading/cxf_get_threads.md`
- [ ] `cxf_set_thread_count` - `docs/specs/functions/threading/cxf_set_thread_count.md`
- [ ] `cxf_get_physical_cores` - `docs/specs/functions/threading/cxf_get_physical_cores.md`
- [ ] `cxf_acquire_solve_lock` - `docs/specs/functions/threading/cxf_acquire_solve_lock.md`
- [ ] `cxf_release_solve_lock` - `docs/specs/functions/threading/cxf_release_solve_lock.md`
- [ ] `cxf_env_acquire_lock` - `docs/specs/functions/threading/cxf_env_acquire_lock.md`
- [ ] `cxf_leave_critical_section` - `docs/specs/functions/threading/cxf_leave_critical_section.md`

### Timing (5 functions)

- [ ] `cxf_get_timestamp` - `docs/specs/functions/timing/cxf_get_timestamp.md`
- [ ] `cxf_timing_start` - `docs/specs/functions/timing/cxf_timing_start.md`
- [ ] `cxf_timing_end` - `docs/specs/functions/timing/cxf_timing_end.md`
- [ ] `cxf_timing_pivot` - `docs/specs/functions/timing/cxf_timing_pivot.md`
- [ ] `cxf_timing_update` - `docs/specs/functions/timing/cxf_timing_update.md`

### Matrix Operations (7 functions)

- [ ] `cxf_matrix_multiply` - `docs/specs/functions/matrix/cxf_matrix_multiply.md`
- [ ] `cxf_dot_product` - `docs/specs/functions/matrix/cxf_dot_product.md`
- [ ] `cxf_vector_norm` - `docs/specs/functions/matrix/cxf_vector_norm.md`
- [ ] `cxf_build_row_major` - `docs/specs/functions/matrix/cxf_build_row_major.md`
- [ ] `cxf_prepare_row_data` - `docs/specs/functions/matrix/cxf_prepare_row_data.md`
- [ ] `cxf_finalize_row_data` - `docs/specs/functions/matrix/cxf_finalize_row_data.md`
- [ ] `cxf_sort_indices` - `docs/specs/functions/matrix/cxf_sort_indices.md`

### Model Analysis (6 functions)

- [ ] `cxf_is_mip_model` - `docs/specs/functions/validation/cxf_is_mip_model.md`
- [ ] `cxf_is_quadratic` - `docs/specs/functions/validation/cxf_is_quadratic.md`
- [ ] `cxf_is_socp` - `docs/specs/functions/validation/cxf_is_socp.md`
- [ ] `cxf_coefficient_stats` - `docs/specs/functions/statistics/cxf_coefficient_stats.md`
- [ ] `cxf_compute_coef_stats` - `docs/specs/functions/statistics/cxf_compute_coef_stats.md`
- [ ] `cxf_presolve_stats` - `docs/specs/functions/statistics/cxf_presolve_stats.md`

### Basis Operations (8 functions)

- [ ] `cxf_ftran` - `docs/specs/functions/basis/cxf_ftran.md`
- [ ] `cxf_btran` - `docs/specs/functions/basis/cxf_btran.md`
- [ ] `cxf_basis_refactor` - `docs/specs/functions/basis/cxf_basis_refactor.md`
- [ ] `cxf_basis_snapshot` - `docs/specs/functions/basis/cxf_basis_snapshot.md`
- [ ] `cxf_basis_diff` - `docs/specs/functions/basis/cxf_basis_diff.md`
- [ ] `cxf_basis_equal` - `docs/specs/functions/basis/cxf_basis_equal.md`
- [ ] `cxf_basis_validate` - `docs/specs/functions/basis/cxf_basis_validate.md`
- [ ] `cxf_basis_warm` - `docs/specs/functions/basis/cxf_basis_warm.md`

### Callbacks (6 functions)

- [ ] `cxf_init_callback_struct` - `docs/specs/functions/callbacks/cxf_init_callback_struct.md`
- [ ] `cxf_reset_callback_state` - `docs/specs/functions/callbacks/cxf_reset_callback_state.md`
- [ ] `cxf_pre_optimize_callback` - `docs/specs/functions/callbacks/cxf_pre_optimize_callback.md`
- [ ] `cxf_post_optimize_callback` - `docs/specs/functions/callbacks/cxf_post_optimize_callback.md`
- [ ] `cxf_callback_terminate` - `docs/specs/functions/callbacks/cxf_callback_terminate.md`
- [ ] `cxf_set_terminate` - `docs/specs/functions/callbacks/cxf_set_terminate.md`

### Solver State (4 functions)

- [ ] `cxf_init_solve_state` - `docs/specs/functions/memory/cxf_init_solve_state.md`
- [ ] `cxf_cleanup_solve_state` - `docs/specs/functions/memory/cxf_cleanup_solve_state.md`
- [ ] `cxf_cleanup_helper` - `docs/specs/functions/utilities/cxf_cleanup_helper.md`
- [ ] `cxf_extract_solution` - `docs/specs/functions/utilities/cxf_extract_solution.md`

### Pricing (6 functions)

- [ ] `cxf_pricing_init` - `docs/specs/functions/pricing/cxf_pricing_init.md`
- [ ] `cxf_pricing_candidates` - `docs/specs/functions/pricing/cxf_pricing_candidates.md`
- [ ] `cxf_pricing_steepest` - `docs/specs/functions/pricing/cxf_pricing_steepest.md`
- [ ] `cxf_pricing_update` - `docs/specs/functions/pricing/cxf_pricing_update.md`
- [ ] `cxf_pricing_invalidate` - `docs/specs/functions/pricing/cxf_pricing_invalidate.md`
- [ ] `cxf_pricing_step2` - `docs/specs/functions/pricing/cxf_pricing_step2.md`

### Simplex Core (21 functions)

- [ ] `cxf_solve_lp` - `docs/specs/functions/simplex/cxf_solve_lp.md`
- [ ] `cxf_simplex_init` - `docs/specs/functions/simplex/cxf_simplex_init.md`
- [ ] `cxf_simplex_setup` - `docs/specs/functions/simplex/cxf_simplex_setup.md`
- [ ] `cxf_simplex_preprocess` - `docs/specs/functions/simplex/cxf_simplex_preprocess.md`
- [ ] `cxf_simplex_crash` - `docs/specs/functions/simplex/cxf_simplex_crash.md`
- [ ] `cxf_simplex_iterate` - `docs/specs/functions/simplex/cxf_simplex_iterate.md`
- [ ] `cxf_simplex_step` - `docs/specs/functions/simplex/cxf_simplex_step.md`
- [ ] `cxf_simplex_step2` - `docs/specs/functions/simplex/cxf_simplex_step2.md`
- [ ] `cxf_simplex_step3` - `docs/specs/functions/simplex/cxf_simplex_step3.md`
- [ ] `cxf_simplex_refine` - `docs/specs/functions/simplex/cxf_simplex_refine.md`
- [ ] `cxf_simplex_perturbation` - `docs/specs/functions/simplex/cxf_simplex_perturbation.md`
- [ ] `cxf_simplex_unperturb` - `docs/specs/functions/simplex/cxf_simplex_unperturb.md`
- [ ] `cxf_simplex_phase_end` - `docs/specs/functions/simplex/cxf_simplex_phase_end.md`
- [ ] `cxf_simplex_post_iterate` - `docs/specs/functions/simplex/cxf_simplex_post_iterate.md`
- [ ] `cxf_simplex_final` - `docs/specs/functions/simplex/cxf_simplex_final.md`
- [ ] `cxf_simplex_cleanup` - `docs/specs/functions/simplex/cxf_simplex_cleanup.md`
- [ ] `cxf_pivot_primal` - `docs/specs/functions/pivot/cxf_pivot_primal.md`
- [ ] `cxf_pivot_bound` - `docs/specs/functions/ratio_test/cxf_pivot_bound.md`
- [ ] `cxf_pivot_special` - `docs/specs/functions/ratio_test/cxf_pivot_special.md`
- [ ] `cxf_pivot_with_eta` - `docs/specs/functions/basis/cxf_pivot_with_eta.md`
- [ ] `cxf_ratio_test` - `docs/specs/functions/ratio_test/cxf_ratio_test.md`

### Crossover (2 functions)

- [ ] `cxf_crossover` - `docs/specs/functions/crossover/cxf_crossover.md`
- [ ] `cxf_crossover_bounds` - `docs/specs/functions/crossover/cxf_crossover_bounds.md`

### Utilities (10 functions)

- [ ] `cxf_fix_variable` - `docs/specs/functions/pivot/cxf_fix_variable.md`
- [ ] `cxf_quadratic_adjust` - `docs/specs/functions/simplex/cxf_quadratic_adjust.md`
- [ ] `cxf_generate_seed` - `docs/specs/functions/threading/cxf_generate_seed.md`
- [ ] `cxf_floor_ceil_wrapper` - `docs/specs/functions/utilities/cxf_floor_ceil_wrapper.md`
- [ ] `cxf_misc_utility` - `docs/specs/functions/utilities/cxf_misc_utility.md`
- [ ] `cxf_is_multi_obj` - `docs/specs/functions/utilities/cxf_is_multi_obj.md`
- [ ] `cxf_get_genconstr_name` - `docs/specs/functions/utilities/cxf_get_genconstr_name.md`
- [ ] `cxf_get_qconstr_data` - `docs/specs/functions/utilities/cxf_get_qconstr_data.md`
- [ ] `cxf_count_genconstr_types` - `docs/specs/functions/utilities/cxf_count_genconstr_types.md`
- [ ] `cxf_timing_refactor` - `docs/specs/functions/basis/cxf_timing_refactor.md`

### Model API (30 functions)

- [ ] `cxf_loadenv` - Environment loader
- [ ] `cxf_emptyenv` - Empty environment creator
- [ ] `cxf_freeenv` - Environment destructor
- [ ] `cxf_newmodel` - Model constructor
- [ ] `cxf_freemodel` - Model destructor
- [ ] `cxf_copymodel` - Model copy
- [ ] `cxf_updatemodel` - Apply pending changes
- [ ] `cxf_addvar` - Add single variable
- [ ] `cxf_addvars` - Add multiple variables
- [ ] `cxf_delvars` - Delete variables
- [ ] `cxf_addconstr` - Add single constraint
- [ ] `cxf_addconstrs` - Add multiple constraints
- [ ] `cxf_addqpterms` - Add quadratic objective terms
- [ ] `cxf_addqconstr` - Add quadratic constraint
- [ ] `cxf_addgenconstrIndicator` - Add indicator constraint
- [ ] `cxf_chgcoeffs` - Change matrix coefficients
- [ ] `cxf_getconstrs` - Get constraint data
- [ ] `cxf_getcoeff` - Get single coefficient
- [ ] `cxf_optimize` - Optimize model
- [ ] `cxf_optimize_internal` - Internal optimizer
- [ ] `cxf_terminate` - Request termination
- [ ] `cxf_getintattr` - Get integer attribute
- [ ] `cxf_getdblattr` - Get double attribute
- [ ] `cxf_setintparam` - Set integer parameter
- [ ] `cxf_getintparam` - Get integer parameter
- [ ] `cxf_read` - Read model from file
- [ ] `cxf_write` - Write model to file
- [ ] `cxf_version` - Get version info
- [ ] `cxf_geterrormsg` - Get error message
- [ ] `cxf_setcallbackfunc` - Set callback function

---

## Parallelization Guide

### Milestone Dependencies

```
M0 (Setup)
 |
 v
M1 (Tracer Bullet)
 |
 v
M2 (Level 0: Memory, Parameters, Validation) <-- All 3 modules parallel
 |
 v
M3 (Level 1: Error, Logging, Threading) <-- All 3 modules parallel
 |
 v
M4 (Level 2: Matrix, Timing, Analysis) <-- All 3 modules parallel
 |
 v
M5 (Level 3: Basis, Callbacks, SolverState) <-- All 3 modules parallel
 |
 v
M6 (Level 4: Pricing) <-- Single module
 |
 v
M7 (Level 5: Simplex, Crossover, Utilities) <-- Simplex sequential; others parallel
 |
 v
M8 (Level 6: API) <-- All 30 functions parallel
```

### Within-Milestone Parallelism

| Milestone | Parallel Groups |
|-----------|-----------------|
| M2 | Memory (9), Parameters (4), Validation (2) - all parallel |
| M3 | Error (10), Logging (5), Threading (7) - all parallel |
| M4 | Matrix (7), Timing (5), Analysis (6) - all parallel |
| M5 | Basis (8), Callbacks (6), SolverState (4) - all parallel |
| M6 | Pricing (6) - all parallel |
| M7 | Simplex (21) mostly sequential; Crossover (2) + Utilities (10) parallel |
| M8 | All 30 API functions - all parallel |

### Recommended Agent Allocation

| Agent | Modules | Functions | Model |
|-------|---------|-----------|-------|
| Agent 1 | Memory, Parameters | 13 | Sonnet |
| Agent 2 | Validation, Error | 12 | Sonnet |
| Agent 3 | Logging, Threading | 12 | Sonnet |
| Agent 4 | Timing, Analysis | 11 | Sonnet |
| Agent 5 | Matrix | 7 | Sonnet |
| Agent 6 | **Basis** | **8** | **Opus** |
| Agent 7 | Callbacks, SolverState | 10 | Sonnet |
| Agent 8 | **Pricing** | **6** | **Opus** |
| Agent 9 | **Simplex Core** | **21** | **Opus** |
| Agent 10 | Crossover, Utilities | 12 | Sonnet |
| Agent 11 | API (Env, Model) | 10 | Sonnet |
| Agent 12 | API (Vars, Constraints) | 10 | Sonnet |
| Agent 13 | API (Optimize, Query, I/O) | 10 | Sonnet |

**Summary:** 107 functions Sonnet, 35 functions Opus (Basis + Pricing + Simplex Core)

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

---

*ConvexFeld LP Solver - C99 Implementation Plan*
*Based on published optimization literature*
*All code examples are C99 compliant*
