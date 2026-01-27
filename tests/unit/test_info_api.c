/**
 * @file test_info_api.c
 * @brief Unit tests for Info API functions.
 *
 * Tests version retrieval, error message handling, and callback registration.
 */

#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include <string.h>

/* Extern declaration for cxf_version and cxf_setcallbackfunc */
extern void cxf_version(int *majorP, int *minorP, int *patchP);
extern const char *cxf_geterrormsg(CxfEnv *env);
extern int cxf_setcallbackfunc(CxfModel *model,
                               int (*cb)(CxfModel*, void*, int, void*),
                               void *usrdata);

/* Extern declaration for cxf_error to set error messages in tests */
extern void cxf_error(CxfEnv *env, const char *format, ...);

void setUp(void) {
}

void tearDown(void) {
}

/*******************************************************************************
 * cxf_version Tests
 ******************************************************************************/

void test_version_all_pointers_valid(void) {
    int major = -1, minor = -1, patch = -1;
    cxf_version(&major, &minor, &patch);
    TEST_ASSERT_EQUAL_INT(0, major);
    TEST_ASSERT_EQUAL_INT(1, minor);
    TEST_ASSERT_EQUAL_INT(0, patch);
}

void test_version_only_major_requested(void) {
    int major = -1;
    cxf_version(&major, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(0, major);
}

void test_version_only_minor_requested(void) {
    int minor = -1;
    cxf_version(NULL, &minor, NULL);
    TEST_ASSERT_EQUAL_INT(1, minor);
}

void test_version_only_patch_requested(void) {
    int patch = -1;
    cxf_version(NULL, NULL, &patch);
    TEST_ASSERT_EQUAL_INT(0, patch);
}

void test_version_all_null_pointers(void) {
    /* Should not crash */
    cxf_version(NULL, NULL, NULL);
    TEST_PASS();
}

/*******************************************************************************
 * cxf_geterrormsg Tests
 ******************************************************************************/

void test_geterrormsg_null_env_returns_empty(void) {
    /* core.c implementation returns "" for NULL env */
    const char *msg = cxf_geterrormsg(NULL);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("", msg);
}

void test_geterrormsg_invalid_magic_returns_buffer(void) {
    /* core.c implementation doesn't check magic, just returns buffer.
     * We initialize buffer to empty to test this case safely. */
    CxfEnv env;
    memset(&env, 0, sizeof(env));  /* Clear entire struct */
    env.magic = 0xDEADBEEF;  /* Wrong magic */
    env.error_buffer[0] = '\0';  /* Empty buffer */
    const char *msg = cxf_geterrormsg(&env);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("", msg);
}

void test_geterrormsg_valid_env_empty_buffer(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("", msg);
    cxf_freeenv(env);
}

void test_geterrormsg_valid_env_with_error(void) {
    CxfEnv *env = NULL;
    cxf_loadenv(&env, NULL);
    cxf_error(env, "Test error message");
    const char *msg = cxf_geterrormsg(env);
    TEST_ASSERT_NOT_NULL(msg);
    TEST_ASSERT_EQUAL_STRING("Test error message", msg);
    cxf_freeenv(env);
}

void test_geterrormsg_never_returns_null(void) {
    /* Even with invalid input, should never return NULL */
    const char *msg = cxf_geterrormsg(NULL);
    TEST_ASSERT_NOT_NULL(msg);
}

/*******************************************************************************
 * cxf_setcallbackfunc Tests
 ******************************************************************************/

/* Dummy callback function for testing */
static int dummy_callback(CxfModel *model, void *data, int where, void *extra) {
    (void)model;
    (void)data;
    (void)where;
    (void)extra;
    return 0;
}

void test_setcallbackfunc_null_model_returns_error(void) {
    int status = cxf_setcallbackfunc(NULL, dummy_callback, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, status);
}

void test_setcallbackfunc_null_callback_is_valid(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test");

    /* NULL callback means "disable callback" */
    int status = cxf_setcallbackfunc(model, NULL, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

void test_setcallbackfunc_valid_callback_returns_ok(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test");

    /* Stub should accept valid callback */
    int status = cxf_setcallbackfunc(model, dummy_callback, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

void test_setcallbackfunc_with_userdata(void) {
    CxfEnv *env = NULL;
    CxfModel *model = NULL;
    int userdata = 42;

    cxf_loadenv(&env, NULL);
    cxf_newmodel(env, &model, "test");

    /* Stub should accept callback with user data */
    int status = cxf_setcallbackfunc(model, dummy_callback, &userdata);
    TEST_ASSERT_EQUAL_INT(CXF_OK, status);

    cxf_freemodel(model);
    cxf_freeenv(env);
}

/*******************************************************************************
 * Test Runner
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    /* cxf_version tests */
    RUN_TEST(test_version_all_pointers_valid);
    RUN_TEST(test_version_only_major_requested);
    RUN_TEST(test_version_only_minor_requested);
    RUN_TEST(test_version_only_patch_requested);
    RUN_TEST(test_version_all_null_pointers);

    /* cxf_geterrormsg tests */
    RUN_TEST(test_geterrormsg_null_env_returns_empty);
    RUN_TEST(test_geterrormsg_invalid_magic_returns_buffer);
    RUN_TEST(test_geterrormsg_valid_env_empty_buffer);
    RUN_TEST(test_geterrormsg_valid_env_with_error);
    RUN_TEST(test_geterrormsg_never_returns_null);

    /* cxf_setcallbackfunc tests */
    RUN_TEST(test_setcallbackfunc_null_model_returns_error);
    RUN_TEST(test_setcallbackfunc_null_callback_is_valid);
    RUN_TEST(test_setcallbackfunc_valid_callback_returns_ok);
    RUN_TEST(test_setcallbackfunc_with_userdata);

    return UNITY_END();
}
