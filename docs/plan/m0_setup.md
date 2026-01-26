# Milestone 0: Project Setup

**Goal:** Initialize C99 project structure
**Parallelizable:** No (must complete first)
**LOC Total:** ~300

---

## Step 0.1: Create CMakeLists.txt

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

---

## Step 0.2: Create Core Types Header

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

---

## Step 0.3: Setup Unity Test Framework

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

---

## Step 0.4: Create Module Headers (Stubs)

**LOC:** ~20 each, ~160 total
**Files:** All header files in `include/convexfeld/`

Create stub headers for all 8 structures with forward declarations:
- `cxf_env.h`
- `cxf_model.h`
- `cxf_matrix.h`
- `cxf_solver.h`
- `cxf_basis.h`
- `cxf_pricing.h`
- `cxf_callback.h`
- `convexfeld.h`
