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

################################################################################
# Integration Tests
################################################################################

# M1.0: Tracer Bullet Test - proves end-to-end architecture
add_cxf_test(test_tracer_bullet integration/test_tracer_bullet.c)
