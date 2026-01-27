/**
 * @file bench_tracer.c
 * @brief Tracer Bullet Benchmark - M1.8
 *
 * Benchmarks the tracer bullet to establish baseline performance.
 * Runs 10,000 iterations and reports microseconds per iteration.
 *
 * Target: < 1ms per iteration (< 1000 us/iteration)
 */

#include <stdio.h>
#include <time.h>
#include "convexfeld/convexfeld.h"

#define ITERATIONS 10000
#define TARGET_US_PER_ITER 1000.0

int main(void) {
    clock_t start, end;
    double cpu_time_sec;
    double us_per_iter;
    int i;

    printf("ConvexFeld Tracer Bullet Benchmark\n");
    printf("===================================\n");
    printf("Iterations: %d\n", ITERATIONS);
    printf("Target: < %.0f us/iteration\n\n", TARGET_US_PER_ITER);

    start = clock();

    for (i = 0; i < ITERATIONS; i++) {
        CxfEnv *env = NULL;
        CxfModel *model = NULL;

        cxf_loadenv(&env, NULL);
        cxf_newmodel(env, &model, "bench", 0, NULL, NULL, NULL, NULL, NULL);
        cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, CXF_INFINITY, CXF_CONTINUOUS, "x");
        cxf_optimize(model);
        cxf_freemodel(model);
        cxf_freeenv(env);
    }

    end = clock();

    cpu_time_sec = ((double)(end - start)) / CLOCKS_PER_SEC;
    us_per_iter = (cpu_time_sec * 1e6) / ITERATIONS;

    printf("Results:\n");
    printf("  Total time:  %.3f sec\n", cpu_time_sec);
    printf("  Per iteration: %.3f us\n", us_per_iter);
    printf("  Status: %s\n",
           us_per_iter < TARGET_US_PER_ITER ? "PASS" : "SLOW");

    return us_per_iter < TARGET_US_PER_ITER ? 0 : 1;
}
