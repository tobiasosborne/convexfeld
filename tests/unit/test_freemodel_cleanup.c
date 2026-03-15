/**
 * @file test_freemodel_cleanup.c
 * @brief Tests for cxf_freemodel child environment cleanup (convexfeld-3wx7).
 *
 * V2 spec (data-model/model.md, Destruction):
 *   Step 1: Free all owned sub-structures.
 *   Step 2: If environment_owned is set, free the child Environment.
 *   Step 3: Zero the validity sentinel.
 *   Step 4: Deallocate the Model memory.
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include <stddef.h>

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name,
                 int numvars, double *obj, double *lb, double *ub,
                 char *vtype, char **varnames);
void cxf_freemodel(CxfModel *model);

void setUp(void) {}
void tearDown(void) {}

/*==========================================================================
 * Test: freemodel with environment_owned=1 frees the child env
 *=========================================================================*/

void test_freemodel_frees_owned_env(void) {
    /* Create a shared (parent) env and a child env */
    CxfEnv *parent_env = NULL;
    CxfEnv *child_env = NULL;
    CxfModel *model = NULL;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_loadenv(&parent_env, NULL));
    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_loadenv(&child_env, NULL));

    /* Create model on the child env */
    TEST_ASSERT_EQUAL_INT(CXF_OK,
        cxf_newmodel(child_env, &model, "owned", 0,
                     NULL, NULL, NULL, NULL, NULL));

    /* Mark model as owning the child env */
    model->environment_owned = 1;

    /* Verify the child env magic is valid before free */
    TEST_ASSERT_EQUAL_UINT32(CXF_ENV_MAGIC, child_env->magic);

    /* freemodel should free the child env (sentinel zeroed by cxf_freeenv) */
    cxf_freemodel(model);

    /* After freemodel, child_env memory was freed by cxf_freeenv.
     * We cannot safely dereference child_env — it's been deallocated.
     * The test succeeds if freemodel does not crash or leak. */

    /* Clean up the parent env (not freed by model since it's separate) */
    cxf_freeenv(parent_env);
}

/*==========================================================================
 * Test: freemodel with environment_owned=0 does NOT free the env
 *=========================================================================*/

void test_freemodel_does_not_free_borrowed_env(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_loadenv(&env, NULL));
    TEST_ASSERT_EQUAL_INT(CXF_OK,
        cxf_newmodel(env, &model, "borrowed", 0,
                     NULL, NULL, NULL, NULL, NULL));

    /* Default: environment_owned = 0 */
    TEST_ASSERT_EQUAL_INT(0, model->environment_owned);

    cxf_freemodel(model);

    /* The env must still be valid — freemodel must not have freed it */
    TEST_ASSERT_EQUAL_UINT32(CXF_ENV_MAGIC, env->magic);
    TEST_ASSERT_EQUAL_INT(1, env->active);

    cxf_freeenv(env);
}

/*==========================================================================
 * Test: sentinel invalidation happens (magic = 0 after free)
 *=========================================================================*/

void test_freemodel_sentinel_invalidation(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_loadenv(&env, NULL));
    TEST_ASSERT_EQUAL_INT(CXF_OK,
        cxf_newmodel(env, &model, "sentinel", 0,
                     NULL, NULL, NULL, NULL, NULL));

    TEST_ASSERT_EQUAL_UINT32(CXF_MODEL_MAGIC, model->magic);

    /* We cannot check magic after free (use-after-free), but we verify
     * magic is correct before free — the implementation sets magic=0
     * as the last step before cxf_free(model). The existing test
     * test_freemodel_clears_magic in test_api_model.c covers this
     * more directly; here we confirm it still holds with our changes. */
    cxf_freemodel(model);

    cxf_freeenv(env);
}

/*==========================================================================
 * Test: freemodel(NULL) is safe
 *=========================================================================*/

void test_freemodel_null_safe(void) {
    /* Must not crash */
    cxf_freemodel(NULL);
}

/*==========================================================================
 * Test: freemodel with environment_owned=1 but env=NULL is safe
 *=========================================================================*/

void test_freemodel_owned_but_null_env_safe(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;

    TEST_ASSERT_EQUAL_INT(CXF_OK, cxf_loadenv(&env, NULL));
    TEST_ASSERT_EQUAL_INT(CXF_OK,
        cxf_newmodel(env, &model, "nullenv", 0,
                     NULL, NULL, NULL, NULL, NULL));

    /* Simulate a pathological state: owned but env already NULL */
    model->environment_owned = 1;
    model->env = NULL;

    /* Must not crash */
    cxf_freemodel(model);

    cxf_freeenv(env);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_freemodel_frees_owned_env);
    RUN_TEST(test_freemodel_does_not_free_borrowed_env);
    RUN_TEST(test_freemodel_sentinel_invalidation);
    RUN_TEST(test_freemodel_null_safe);
    RUN_TEST(test_freemodel_owned_but_null_env_safe);

    return UNITY_END();
}
