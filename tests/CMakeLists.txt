################################################################################
# ConvexFeld - Test Suite CMakeLists.txt
################################################################################

################################################################################
# Unity Test Framework
################################################################################

add_library(unity STATIC
    unity/unity.c
)

target_include_directories(unity PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/unity
)

# Unity configuration options
target_compile_definitions(unity PUBLIC
    UNITY_INCLUDE_DOUBLE
    UNITY_DOUBLE_PRECISION=1e-12
)

################################################################################
# Test Helper Function
################################################################################

# Function to create test executables with standard configuration
function(add_cxf_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name}
        PRIVATE
            unity
            convexfeld
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

################################################################################
# Unit Tests
################################################################################

# Smoke test to verify Unity works
add_cxf_test(test_smoke unit/test_smoke.c)

# M2.1.1: Memory management tests
add_cxf_test(test_memory unit/test_memory.c)

# M4.1.1: Matrix operations tests
add_cxf_test(test_matrix unit/test_matrix.c)
target_link_libraries(test_matrix PRIVATE m)  # For math functions

# M5.1.1: Basis operations tests
add_cxf_test(test_basis unit/test_basis.c)
target_link_libraries(test_basis PRIVATE m)  # For math functions

# M6.1.1: Pricing operations tests
add_cxf_test(test_pricing unit/test_pricing.c)
target_link_libraries(test_pricing PRIVATE m)  # For math functions

# M8.1.1: API Tests - Environment
add_cxf_test(test_api_env unit/test_api_env.c)

# M8.1.2: API Tests - Model
add_cxf_test(test_api_model unit/test_api_model.c)

# M8.1.3: API Tests - Variables
add_cxf_test(test_api_vars unit/test_api_vars.c)

# M2.1.3: Vector memory management tests
add_cxf_test(test_memory_vectors unit/test_memory_vectors.c)

# M3.1.1: Error handling tests
add_cxf_test(test_error unit/test_error.c)
target_link_libraries(test_error PRIVATE m)  # For math functions (NAN, INFINITY)

# M4.2.1: Timing tests
add_cxf_test(test_timing unit/test_timing.c)
target_link_libraries(test_timing PRIVATE m)  # For math functions

# M4.3.1-M4.3.2: Analysis tests
add_cxf_test(test_analysis unit/test_analysis.c)

################################################################################
# Integration Tests
################################################################################

# M1.0: Tracer Bullet Test - proves end-to-end architecture
add_cxf_test(test_tracer_bullet integration/test_tracer_bullet.c)
