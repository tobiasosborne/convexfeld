# Parallelization Guide

## Milestone Dependencies

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

---

## Within-Milestone Parallelism

| Milestone | Parallel Groups |
|-----------|-----------------|
| M2 | Memory (9), Parameters (4), Validation (2) - all parallel |
| M3 | Error (10), Logging (5), Threading (7) - all parallel |
| M4 | Matrix (7), Timing (5), Analysis (6) - all parallel |
| M5 | Basis (8), Callbacks (6), SolverState (4) - all parallel |
| M6 | Pricing (6) - all parallel |
| M7 | Simplex (21) mostly sequential; Crossover (2) + Utilities (10) parallel |
| M8 | All 30 API functions - all parallel |

---

## Recommended Agent Allocation

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

## Benchmarks

### Success Criteria

| Benchmark | Target | Description |
|-----------|--------|-------------|
| Tracer Bullet | < 1ms | 1-variable LP |
| Small LP | < 10ms | 10x10 problem |
| Medium LP | < 100ms | 100x100 problem |
| Netlib afiro | < 1s | 27 vars, 51 constraints |
| FTRAN/BTRAN | < 10ms | 1000 operations on 100-dim basis |
| Pricing | < 10ms | 1000 pricing operations |

### Benchmark Suite Template

```c
#include <stdio.h>
#include <time.h>
#include "convexfeld/convexfeld.h"

typedef struct {
    const char *name;
    int (*func)(void);
    double target_ms;
} Benchmark;

int main(void) {
    Benchmark benchmarks[] = {
        {"Tracer Bullet (1 var)", bench_tracer_bullet, 1.0},
        {"Small LP (10x10)", bench_small_lp, 10.0},
        {"Medium LP (100x100)", bench_medium_lp, 100.0},
        {"Netlib afiro", bench_netlib_afiro, 1000.0},
        {"FTRAN/BTRAN ops", bench_ftran_btran, 10.0},
        {"Pricing operations", bench_pricing, 10.0},
    };

    printf("ConvexFeld Benchmark Suite\n");
    printf("==========================\n\n");

    int num = sizeof(benchmarks) / sizeof(Benchmark);
    for (int i = 0; i < num; i++) {
        clock_t start = clock();
        benchmarks[i].func();
        double elapsed_ms = ((double)(clock() - start) / CLOCKS_PER_SEC) * 1000.0;
        const char *status = (elapsed_ms <= benchmarks[i].target_ms) ? "PASS" : "SLOW";
        printf("[%s] %s: %.2f ms (target: %.2f ms)\n",
               status, benchmarks[i].name, elapsed_ms, benchmarks[i].target_ms);
    }
    return 0;
}
```
