################################################################################
# ConvexFeld - Benchmark Suite CMakeLists.txt
################################################################################

################################################################################
# Benchmark Helper Function
################################################################################

# Function to create benchmark executables with standard configuration
function(add_cxf_benchmark bench_name)
    add_executable(${bench_name} ${ARGN})
    target_link_libraries(${bench_name}
        PRIVATE
            convexfeld
    )
endfunction()

################################################################################
# Benchmarks
################################################################################

# M1.8: Tracer Bullet Benchmark
add_cxf_benchmark(bench_tracer bench_tracer.c)

# Netlib LP Benchmark Suite
add_cxf_benchmark(bench_netlib bench_netlib.c)
