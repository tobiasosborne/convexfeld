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

# M8.1.4: API Tests - Constraints
add_cxf_test(test_api_constrs unit/test_api_constrs.c)

# M8.1.5: API Tests - Optimize
add_cxf_test(test_api_optimize unit/test_api_optimize.c)

# M8.1.6: API Tests - Queries
add_cxf_test(test_api_query unit/test_api_query.c)

# M3.2.1: Logging tests
add_cxf_test(test_logging unit/test_logging.c)
target_link_libraries(test_logging PRIVATE m)  # For math functions

# M3.1.7: Pivot validation tests
add_cxf_test(test_pivot_check unit/test_pivot_check.c)
target_link_libraries(test_pivot_check PRIVATE m)  # For math functions

# M2.2.1: Parameters tests
add_cxf_test(test_parameters unit/test_parameters.c)

# M7.1.1: Simplex Tests - Basic
add_cxf_test(test_simplex_basic unit/test_simplex_basic.c)

# M5.2.1, M5.2.2: Callbacks tests
add_cxf_test(test_callbacks unit/test_callbacks.c)
target_link_libraries(test_callbacks PRIVATE m)  # For math functions (isinf)

# M7.1.2: Simplex Tests - Iteration
add_cxf_test(test_simplex_iteration unit/test_simplex_iteration.c)

# M2.3.1: Validation tests
add_cxf_test(test_validation unit/test_validation.c)
target_link_libraries(test_validation PRIVATE m)  # For math functions (NAN, INFINITY)

# M3.3.1: Threading tests
add_cxf_test(test_threading unit/test_threading.c)

# M7.1.3: Simplex Tests - Edge Cases
add_cxf_test(test_simplex_edge unit/test_simplex_edge.c)

# M5.1: Pivot with Eta tests
add_cxf_test(test_pivot_eta unit/test_pivot_eta.c)
target_link_libraries(test_pivot_eta PRIVATE m)  # For math functions

# M7.1.6: Simplex Setup and Preprocess tests
add_cxf_test(test_simplex_setup unit/test_simplex_setup.c)
target_link_libraries(test_simplex_setup PRIVATE m)  # For math functions

################################################################################
# Integration Tests
################################################################################

# M1.0: Tracer Bullet Test - proves end-to-end architecture
add_cxf_test(test_tracer_bullet integration/test_tracer_bullet.c)
