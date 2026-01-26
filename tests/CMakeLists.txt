################################################################################
# ConvexFeld - Test Suite CMakeLists.txt
################################################################################

################################################################################
# Unity Test Framework
################################################################################

# Unity will be added in M0.3: Setup Unity Test Framework
# add_library(unity STATIC unity/unity.c)
# target_include_directories(unity PUBLIC unity)

################################################################################
# Test Helper Function
################################################################################

# Function to create test executables with standard configuration
function(add_cxf_test test_name)
    add_executable(${test_name} ${ARGN})
    target_link_libraries(${test_name}
        PRIVATE
            # unity      # Uncomment after Unity setup
            convexfeld
    )
    target_include_directories(${test_name}
        PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/unity
    )
    add_test(NAME ${test_name} COMMAND ${test_name})
endfunction()

################################################################################
# Unit Tests
################################################################################

# Unit tests will be added as modules are implemented
# add_subdirectory(unit)

################################################################################
# Integration Tests
################################################################################

# Integration tests will be added starting with M1.0: Tracer Bullet Test
# add_subdirectory(integration)
