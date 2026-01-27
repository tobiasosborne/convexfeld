################################################################################
# ConvexFeld LP Solver - Root CMakeLists.txt
#
# Requirements:
#   - CMake 3.16+
#   - C99 compliant compiler (GCC, Clang)
#
# Build:
#   mkdir build && cd build
#   cmake ..
#   cmake --build .
#
# Test:
#   ctest
################################################################################

cmake_minimum_required(VERSION 3.16)

project(convexfeld
    VERSION 0.1.0
    DESCRIPTION "LP solver implementing the revised simplex method"
    LANGUAGES C
)

################################################################################
# C Standard Configuration
################################################################################

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

################################################################################
# Compiler Flags
################################################################################

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall
        -Wextra
        -Wpedantic
        -O2
    )

    # Additional warnings for better code quality
    add_compile_options(
        -Wconversion
        -Wshadow
        -Wdouble-promotion
        -Wformat=2
    )
endif()

# MSVC flags (if needed for future Windows support)
if(MSVC)
    add_compile_options(/W4 /O2)
endif()

################################################################################
# Build Options
################################################################################

option(BUILD_TESTING "Build test suite" ON)
option(BUILD_BENCHMARKS "Build benchmark suite" ON)

################################################################################
# Library Target
################################################################################

add_library(convexfeld STATIC)

target_include_directories(convexfeld
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Source files
target_sources(convexfeld PRIVATE
    # Memory module (M2.1.2, M2.1.3, M2.1.4)
    src/memory/alloc.c
    src/memory/vectors.c
    src/memory/state_cleanup.c
    # Matrix module (M1.3 stubs + M4.1.2 full + M4.1.3 multiply + M4.1.4 vectors + M4.1.5 row_major + M4.1.6 sort)
    src/matrix/sparse_stub.c
    src/matrix/sparse_matrix.c
    src/matrix/multiply.c
    src/matrix/vectors.c
    src/matrix/row_major.c
    src/matrix/sort.c
    # Basis module (M5.1.2-M5.1.8 complete + M7 pivot_eta)
    src/basis/basis_state.c
    src/basis/eta_factors.c
    src/basis/ftran.c
    src/basis/btran.c
    src/basis/pivot_eta.c
    src/basis/refactor.c
    src/basis/snapshot.c
    src/basis/warm.c
    src/basis/basis_stub.c
    # Simplex module (M7.1)
    src/simplex/solve_lp.c
    src/simplex/iterate.c
    src/simplex/step.c
    src/simplex/ratio_test.c
    src/simplex/context.c
    src/simplex/setup.c
    # Error module (M1.7 stubs + M3.1.2-M3.1.7)
    src/error/core.c
    src/error/nan_check.c
    src/error/env_check.c
    src/error/model_flags.c
    src/error/terminate.c
    src/error/pivot_check.c
    src/error/error_stub.c
    # Parameters module (M2.2.1)
    src/parameters/params.c
    # Logging module (M3.2.2, M3.2.3, M3.2.4)
    src/logging/output.c
    src/logging/format.c
    src/logging/system.c
    # Timing module (M4.2.2-M4.2.4)
    src/timing/timestamp.c
    src/timing/sections.c
    src/timing/operations.c
    # Analysis module (M4.3.2, M4.3.3, M4.3.4)
    src/analysis/model_type.c
    src/analysis/coef_stats.c
    src/analysis/presolve_stats.c
    # API module (M1.1, M1.2, M1.4 stubs + M8.1.4 constr stubs + M8.1.7 env + M8.1.8 model)
    src/api/env.c
    src/api/model.c
    src/api/model_stub.c
    src/api/constr_stub.c
    src/api/api_stub.c
    # Pricing module (M6.1.2-M6.1.7 + stubs)
    src/pricing/context.c
    src/pricing/init.c
    src/pricing/candidates.c
    src/pricing/steepest.c
    src/pricing/update.c
    src/pricing/phase.c
    src/pricing/pricing_stub.c
    # Callbacks module (M5.2.2+)
    src/callbacks/context.c
    src/callbacks/init.c
    src/callbacks/callback_stub.c
    # Validation module (M2.3.2)
    src/validation/arrays.c
    # Threading module (M3.3.1 stubs)
    src/threading/threading_stub.c
)

################################################################################
# Math Library Linking
################################################################################

# Link math library on Unix systems
if(UNIX)
    target_link_libraries(convexfeld PUBLIC m)
endif()

################################################################################
# Tests
################################################################################

if(BUILD_TESTING)
    enable_testing()
    add_subdirectory(tests)
endif()

################################################################################
# Benchmarks
################################################################################

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

################################################################################
# Installation (for future use)
################################################################################

# install(TARGETS convexfeld
#     LIBRARY DESTINATION lib
#     ARCHIVE DESTINATION lib
#     RUNTIME DESTINATION bin
#     INCLUDES DESTINATION include
# )
#
# install(DIRECTORY include/convexfeld
#     DESTINATION include
# )
