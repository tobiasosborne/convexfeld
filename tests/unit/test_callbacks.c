/**
 * @file test_callbacks.c
 * @brief TDD tests for callback functions (M5.2.1, M5.2.2)
 *
 * Tests for callbacks module functions:
 * - CallbackContext lifecycle (M5.2.2)
 * - cxf_init_callback_struct
 * - cxf_set_terminate
 * - cxf_check_terminate
 * - cxf_callback_terminate
 * - cxf_reset_callback_state
 * - cxf_pre_optimize_callback
 * - cxf_post_optimize_callback
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_callback.h"
#include <string.h>
#include <math.h>

/*******************************************************************************
 * Forward declarations for callback functions (to be implemented)
 ******************************************************************************/

int cxf_init_callback_struct(CxfEnv *env, void *callbackSubStruct);
void cxf_set_terminate(CxfEnv *env);
int cxf_check_terminate(CxfEnv *env);
void cxf_callback_terminate(CxfModel *model);
void cxf_reset_callback_state(CxfEnv *env);
int cxf_pre_optimize_callback(CxfModel *model);
int cxf_post_optimize_callback(CxfModel *model);

/* API functions for setup */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
void cxf_freeenv(CxfEnv *env);
int cxf_newmodel(CxfEnv *env, CxfModel **modelP, const char *name);
void cxf_freemodel(CxfModel *model);

/*******************************************************************************
 * Test callback state
 ******************************************************************************/

static int callback_invocation_count;
static int callback_return_value;

static int test_callback(CxfModel *model, void *cbdata) {
    (void)model;
    (void)cbdata;
    callback_invocation_count++;
    return callback_return_value;
}

/*******************************************************************************
 * Test fixtures
 ******************************************************************************/

static CxfEnv *env = NULL;
static CxfModel *model = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test_model");
    callback_invocation_count = 0;
    callback_return_value = 0;
}

void tearDown(void) {
    if (model) { cxf_freemodel(model); model = NULL; }
    if (env) { cxf_freeenv(env); env = NULL; }
}

/*******************************************************************************
 * CallbackContext Lifecycle Tests (M5.2.2)
 ******************************************************************************/

void test_callback_create_returns_non_null(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    cxf_callback_free(ctx);
}

void test_callback_create_sets_magic(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_EQUAL_UINT32(CXF_CALLBACK_MAGIC, ctx->magic);
    TEST_ASSERT_EQUAL_UINT64(CXF_CALLBACK_MAGIC2, ctx->safety_magic);
    cxf_callback_free(ctx);
}

void test_callback_create_initializes_fields(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_NULL(ctx->callback_func);
    TEST_ASSERT_NULL(ctx->user_data);
    TEST_ASSERT_EQUAL_INT(0, ctx->terminate_requested);
    TEST_ASSERT_EQUAL_INT(0, ctx->enabled);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ctx->start_time);
    TEST_ASSERT_EQUAL_INT(0, ctx->iteration_count);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ctx->callback_calls);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ctx->callback_time);
    cxf_callback_free(ctx);
}

void test_callback_create_best_obj_is_infinity(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    TEST_ASSERT_TRUE(isinf(ctx->best_obj));
    cxf_callback_free(ctx);
}

void test_callback_free_null_safe(void) {
    cxf_callback_free(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_callback_validate_returns_ok_for_valid(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    int status = cxf_callback_validate(ctx);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    cxf_callback_free(ctx);
}

void test_callback_validate_null_returns_error(void) {
    int status = cxf_callback_validate(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_callback_validate_bad_magic_returns_error(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    ctx->magic = 0xDEADBEEF;
    int status = cxf_callback_validate(ctx);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
    /* Restore magic for proper free */
    ctx->magic = CXF_CALLBACK_MAGIC;
    cxf_callback_free(ctx);
}

void test_callback_validate_bad_safety_magic_returns_error(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    ctx->safety_magic = 0xDEADBEEFDEADBEEFULL;
    int status = cxf_callback_validate(ctx);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
    /* Restore magic for proper free */
    ctx->safety_magic = CXF_CALLBACK_MAGIC2;
    cxf_callback_free(ctx);
}

void test_callback_reset_stats_clears_counters(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    /* Set some values */
    ctx->callback_calls = 100.0;
    ctx->callback_time = 5.5;
    ctx->iteration_count = 50;
    ctx->best_obj = 42.0;
    ctx->start_time = 1234567890.0;
    ctx->terminate_requested = 1;

    int status = cxf_callback_reset_stats(ctx);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ctx->callback_calls);
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ctx->callback_time);
    TEST_ASSERT_EQUAL_INT(0, ctx->iteration_count);
    TEST_ASSERT_TRUE(isinf(ctx->best_obj));
    TEST_ASSERT_EQUAL_DOUBLE(0.0, ctx->start_time);
    TEST_ASSERT_EQUAL_INT(0, ctx->terminate_requested);
    cxf_callback_free(ctx);
}

void test_callback_reset_stats_preserves_registration(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    /* Set registration info */
    ctx->callback_func = (CxfCallbackFunc)0x12345678;
    ctx->user_data = (void *)0x87654321;
    ctx->enabled = 1;

    int status = cxf_callback_reset_stats(ctx);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    /* Registration preserved */
    TEST_ASSERT_EQUAL_PTR((void *)0x12345678, ctx->callback_func);
    TEST_ASSERT_EQUAL_PTR((void *)0x87654321, ctx->user_data);
    TEST_ASSERT_EQUAL_INT(1, ctx->enabled);
    cxf_callback_free(ctx);
}

void test_callback_reset_stats_null_returns_error(void) {
    int status = cxf_callback_reset_stats(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_callback_reset_stats_invalid_magic_returns_error(void) {
    CallbackContext *ctx = cxf_callback_create();
    TEST_ASSERT_NOT_NULL(ctx);
    ctx->magic = 0;
    int status = cxf_callback_reset_stats(ctx);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_INVALID_ARGUMENT, status);
    ctx->magic = CXF_CALLBACK_MAGIC;
    cxf_callback_free(ctx);
}

/*******************************************************************************
 * cxf_init_callback_struct Tests
 ******************************************************************************/

void test_init_callback_struct_zeroes_memory(void) {
    char buffer[48];
    memset(buffer, 0xFF, sizeof(buffer));  /* Fill with non-zero */
    int status = cxf_init_callback_struct(env, buffer);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);
    for (int i = 0; i < 48; i++) {
        TEST_ASSERT_EQUAL_UINT8(0, buffer[i]);
    }
}

void test_init_callback_struct_null_pointer_returns_error(void) {
    int status = cxf_init_callback_struct(env, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_init_callback_struct_null_env_succeeds(void) {
    char buffer[48];
    int status = cxf_init_callback_struct(NULL, buffer);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);  /* env unused per spec */
}

/*******************************************************************************
 * cxf_set_terminate Tests
 ******************************************************************************/

void test_set_terminate_sets_flag(void) {
    env->terminate_flag = 0;
    cxf_set_terminate(env);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
}

void test_set_terminate_null_env_safe(void) {
    cxf_set_terminate(NULL);  /* Should not crash */
    TEST_PASS();
}

void test_set_terminate_idempotent(void) {
    env->terminate_flag = 0;
    cxf_set_terminate(env);
    cxf_set_terminate(env);  /* Call twice */
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
}

/*******************************************************************************
 * cxf_check_terminate Tests
 ******************************************************************************/

void test_check_terminate_returns_zero_when_clear(void) {
    env->terminate_flag = 0;
    int result = cxf_check_terminate(env);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_check_terminate_returns_one_when_set(void) {
    env->terminate_flag = 1;
    int result = cxf_check_terminate(env);
    TEST_ASSERT_EQUAL_INT(1, result);
}

void test_check_terminate_null_env_returns_zero(void) {
    int result = cxf_check_terminate(NULL);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*******************************************************************************
 * cxf_callback_terminate Tests
 ******************************************************************************/

void test_callback_terminate_sets_env_flag(void) {
    env->terminate_flag = 0;
    cxf_callback_terminate(model);
    TEST_ASSERT_EQUAL_INT(1, env->terminate_flag);
}

void test_callback_terminate_null_model_safe(void) {
    cxf_callback_terminate(NULL);  /* Should not crash */
    TEST_PASS();
}

/*******************************************************************************
 * cxf_reset_callback_state Tests
 ******************************************************************************/

void test_reset_callback_state_null_env_safe(void) {
    cxf_reset_callback_state(NULL);  /* Should not crash */
    TEST_PASS();
}

/*******************************************************************************
 * cxf_pre_optimize_callback Tests
 ******************************************************************************/

void test_pre_optimize_callback_null_env_returns_success(void) {
    CxfModel temp_model;
    memset(&temp_model, 0, sizeof(temp_model));
    temp_model.env = NULL;
    int result = cxf_pre_optimize_callback(&temp_model);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_pre_optimize_callback_no_callback_returns_success(void) {
    int result = cxf_pre_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*******************************************************************************
 * cxf_post_optimize_callback Tests
 ******************************************************************************/

void test_post_optimize_callback_null_env_returns_success(void) {
    CxfModel temp_model;
    memset(&temp_model, 0, sizeof(temp_model));
    temp_model.env = NULL;
    int result = cxf_post_optimize_callback(&temp_model);
    TEST_ASSERT_EQUAL_INT(0, result);
}

void test_post_optimize_callback_no_callback_returns_success(void) {
    int result = cxf_post_optimize_callback(model);
    TEST_ASSERT_EQUAL_INT(0, result);
}

/*******************************************************************************
 * Main test runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* CallbackContext lifecycle tests (M5.2.2) */
    RUN_TEST(test_callback_create_returns_non_null);
    RUN_TEST(test_callback_create_sets_magic);
    RUN_TEST(test_callback_create_initializes_fields);
    RUN_TEST(test_callback_create_best_obj_is_infinity);
    RUN_TEST(test_callback_free_null_safe);
    RUN_TEST(test_callback_validate_returns_ok_for_valid);
    RUN_TEST(test_callback_validate_null_returns_error);
    RUN_TEST(test_callback_validate_bad_magic_returns_error);
    RUN_TEST(test_callback_validate_bad_safety_magic_returns_error);
    RUN_TEST(test_callback_reset_stats_clears_counters);
    RUN_TEST(test_callback_reset_stats_preserves_registration);
    RUN_TEST(test_callback_reset_stats_null_returns_error);
    RUN_TEST(test_callback_reset_stats_invalid_magic_returns_error);

    /* cxf_init_callback_struct tests */
    RUN_TEST(test_init_callback_struct_zeroes_memory);
    RUN_TEST(test_init_callback_struct_null_pointer_returns_error);
    RUN_TEST(test_init_callback_struct_null_env_succeeds);

    /* cxf_set_terminate tests */
    RUN_TEST(test_set_terminate_sets_flag);
    RUN_TEST(test_set_terminate_null_env_safe);
    RUN_TEST(test_set_terminate_idempotent);

    /* cxf_check_terminate tests */
    RUN_TEST(test_check_terminate_returns_zero_when_clear);
    RUN_TEST(test_check_terminate_returns_one_when_set);
    RUN_TEST(test_check_terminate_null_env_returns_zero);

    /* cxf_callback_terminate tests */
    RUN_TEST(test_callback_terminate_sets_env_flag);
    RUN_TEST(test_callback_terminate_null_model_safe);

    /* cxf_reset_callback_state tests */
    RUN_TEST(test_reset_callback_state_null_env_safe);

    /* cxf_pre_optimize_callback tests */
    RUN_TEST(test_pre_optimize_callback_null_env_returns_success);
    RUN_TEST(test_pre_optimize_callback_no_callback_returns_success);

    /* cxf_post_optimize_callback tests */
    RUN_TEST(test_post_optimize_callback_null_env_returns_success);
    RUN_TEST(test_post_optimize_callback_no_callback_returns_success);

    return UNITY_END();
}
