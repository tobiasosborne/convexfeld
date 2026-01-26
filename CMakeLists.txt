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
    # Memory module (M2.1.2, M2.1.3)
    src/memory/alloc.c
    src/memory/vectors.c
    # Matrix module (M1.3 stubs + M4.1.2 full + M4.1.3 multiply + M4.1.4 vectors)
    src/matrix/sparse_stub.c
    src/matrix/sparse_matrix.c
    src/matrix/multiply.c
    src/matrix/vectors.c
    # Basis module (M5.1.2 BasisState, M5.1.3 EtaFactors, M5.1.4 ftran + remaining stubs)
    src/basis/basis_state.c
    src/basis/eta_factors.c
    src/basis/ftran.c
    src/basis/basis_stub.c
    # Simplex module (M1.5 stubs)
    src/simplex/solve_lp_stub.c
    # Error module (M1.7 stubs)
    src/error/error_stub.c
    # API module (M1.1, M1.2, M1.4 stubs)
    src/api/env_stub.c
    src/api/model_stub.c
    src/api/api_stub.c
    # Pricing module (M6.1.2 context + M6.1.1 stubs)
    src/pricing/context.c
    src/pricing/pricing_stub.c
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
