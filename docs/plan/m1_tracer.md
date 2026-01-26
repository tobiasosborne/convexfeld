# Milestone 1: Tracer Bullet

**Goal:** Minimal end-to-end test from API to simplex core
**Parallelizable:** Steps 1.1-1.8 after Step 1.0
**Spec Reference:** End-to-end integration demonstrating all layers

This milestone proves every layer works together by solving a trivial 1-variable LP.

---

## Step 1.0: Tracer Bullet Test (FIRST)

**LOC:** ~80
**File:** `tests/integration/test_tracer_bullet.c`

```c
#include "unity.h"
#include "convexfeld/convexfeld.h"

void setUp(void) {}
void tearDown(void) {}

void test_tracer_bullet_1var_lp(void) {
    /* Solve: min x subject to x >= 0 */
    /* Expected: x* = 0, obj* = 0 */

    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int status;
    double objval;

    status = cxf_loadenv(&env, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    status = cxf_newmodel(env, &model, "tracer");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    /* Add variable: lb=0, ub=inf, obj=1.0, type='C', name="x" */
    status = cxf_addvar(model, 0.0, CXF_INFINITY, 1.0, CXF_CONTINUOUS, "x");
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    status = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    int opt_status;
    status = cxf_getintattr(model, "Status", &opt_status);
    TEST_ASSERT_EQUAL_INT(CXF_OPTIMAL, opt_status);

    status = cxf_getdblattr(model, "ObjVal", &objval);
    TEST_ASSERT_DOUBLE_WITHIN(1e-6, 0.0, objval);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_tracer_bullet_1var_lp);
    return UNITY_END();
}
```

---

## Step 1.1: Stub CxfEnv Structure

**LOC:** ~100
**Files:** `include/convexfeld/cxf_env.h`, `src/api/env_stub.c`
**Spec:** `docs/specs/structures/cxf_env.md`

---

## Step 1.2: Stub CxfModel Structure

**LOC:** ~120
**Files:** `include/convexfeld/cxf_model.h`, `src/api/model_stub.c`
**Spec:** `docs/specs/structures/cxf_model.md`

---

## Step 1.3: Stub SparseMatrix Structure

**LOC:** ~80
**Files:** `include/convexfeld/cxf_matrix.h`, `src/matrix/sparse_stub.c`
**Spec:** `docs/specs/structures/sparse_matrix.md`

---

## Step 1.4: Stub API Functions

**LOC:** ~150
**File:** `src/api/api_stub.c`
**Spec:** `docs/specs/modules/17_model_api.md`

Implement minimal stubs for:
- `cxf_loadenv()` - Allocate and initialize CxfEnv
- `cxf_freeenv()` - Free CxfEnv
- `cxf_newmodel()` - Allocate and initialize CxfModel
- `cxf_freemodel()` - Free CxfModel
- `cxf_addvar()` - Add variable to model arrays
- `cxf_optimize()` - Call simplex stub
- `cxf_getintattr()` - Return status
- `cxf_getdblattr()` - Return objective value

---

## Step 1.5: Stub Simplex Entry

**LOC:** ~100
**File:** `src/simplex/solve_lp_stub.c`
**Spec:** `docs/specs/functions/simplex/cxf_solve_lp.md`

```c
/* Stub for tracer bullet: trivial 1-var LP */
int cxf_solve_lp(CxfModel *model) {
    if (model == NULL) return CXF_ERROR_NULL_ARGUMENT;

    /* For unconstrained 1-var LP: optimal at lower bound */
    if (model->num_vars == 1 && model->num_constrs == 0) {
        double x_opt = model->lb[0];  /* Optimal at lower bound for min */
        if (model->obj_coeffs[0] < 0) {
            x_opt = model->ub[0];     /* Optimal at upper bound if c < 0 */
        }
        model->solution[0] = x_opt;
        model->obj_val = model->obj_coeffs[0] * x_opt;
        model->status = CXF_OPTIMAL;
        return CXF_OK;
    }
    return CXF_ERROR_DATA_NOT_AVAILABLE;
}
```

---

## Step 1.6: Stub Memory Functions

**LOC:** ~60
**File:** `src/memory/alloc_stub.c`
**Specs:** `docs/specs/functions/memory/cxf_malloc.md`, `cxf_free.md`

---

## Step 1.7: Stub Error Functions

**LOC:** ~60
**File:** `src/error/error_stub.c`
**Spec:** `docs/specs/functions/error_logging/cxf_error.md`

---

## Step 1.8: Tracer Bullet Benchmark

**LOC:** ~50
**File:** `benchmarks/bench_tracer.c`

```c
#include <stdio.h>
#include <time.h>
#include "convexfeld/convexfeld.h"

#define ITERATIONS 10000

int main(void) {
    clock_t start, end;
    start = clock();
    for (int i = 0; i < ITERATIONS; i++) {
        CxfEnv *env;
        CxfModel *model;
        cxf_loadenv(&env, NULL);
        cxf_newmodel(env, &model, "bench");
        cxf_addvar(model, 0.0, CXF_INFINITY, 1.0, 'C', "x");
        cxf_optimize(model);
        cxf_freemodel(model);
        cxf_freeenv(env);
    }
    end = clock();
    double cpu_time = ((double)(end - start)) / CLOCKS_PER_SEC;
    printf("Tracer bullet: %.3f us/iter\n", (cpu_time * 1e6) / ITERATIONS);
    return 0;
}
```

**Milestone 1 Complete When:** `ctest -R tracer_bullet` passes
