/**
 * @file test_solve_lock.c
 * @brief Tests for cxf_acquire_solve_lock / cxf_release_solve_lock
 *        (threading_sync.md locale safety).
 *
 * Verifies:
 * - Acquire switches LC_NUMERIC to "C"
 * - Release restores original locale
 * - NULL env returns error
 * - Already-"C" locale is a no-op (no saved_locale allocated)
 * - Already-optimizing env is a no-op (nested call safe)
 * - Double release is safe (idempotent)
 */

#include <locale.h>
#include <string.h>
#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    /* Ensure C locale is restored for subsequent tests */
    setlocale(LC_NUMERIC, "C");
    cxf_freeenv(env);
    env = NULL;
}

/*******************************************************************************
 * NULL env returns error
 ******************************************************************************/

void test_acquire_null_env(void) {
    int rc = cxf_acquire_solve_lock(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_release_null_env(void) {
    int rc = cxf_release_solve_lock(NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

/*******************************************************************************
 * Acquire switches to "C", release restores original
 ******************************************************************************/

/* Try to set a non-C LC_NUMERIC locale. Returns 1 on success, 0 if none. */
static int try_set_non_c_locale(void) {
    const char *candidates[] = {
        "de_DE.UTF-8", "fr_FR.UTF-8", "de_DE", "fr_FR",
        "es_ES.UTF-8", "it_IT.UTF-8", NULL
    };
    for (int i = 0; candidates[i] != NULL; i++) {
        if (setlocale(LC_NUMERIC, candidates[i]) != NULL) {
            return 1;
        }
    }
    return 0;
}

void test_acquire_switches_to_c(void) {
    if (!try_set_non_c_locale()) {
        TEST_IGNORE_MESSAGE("No non-C locale available on this system");
        return;
    }

    const char *before = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_NOT_NULL(before);
    TEST_ASSERT_NOT_EQUAL(0, strcmp(before, "C"));

    int rc = cxf_acquire_solve_lock(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    const char *during = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_NOT_NULL(during);
    TEST_ASSERT_EQUAL_STRING("C", during);

    /* saved_locale should be non-NULL */
    TEST_ASSERT_NOT_NULL(env->saved_locale);

    cxf_release_solve_lock(env);
}

void test_release_restores_original(void) {
    if (!try_set_non_c_locale()) {
        TEST_IGNORE_MESSAGE("No non-C locale available on this system");
        return;
    }

    char saved[128];
    const char *before = setlocale(LC_NUMERIC, NULL);
    strncpy(saved, before, sizeof(saved) - 1);
    saved[sizeof(saved) - 1] = '\0';

    cxf_acquire_solve_lock(env);

    int rc = cxf_release_solve_lock(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    const char *after = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_EQUAL_STRING(saved, after);

    /* saved_locale must be cleared after release */
    TEST_ASSERT_NULL(env->saved_locale);
}

/*******************************************************************************
 * Already "C" locale: no-op, no allocation
 ******************************************************************************/

void test_acquire_already_c_is_noop(void) {
    setlocale(LC_NUMERIC, "C");

    int rc = cxf_acquire_solve_lock(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Nothing saved because already "C" */
    TEST_ASSERT_NULL(env->saved_locale);

    /* Release is also a no-op */
    rc = cxf_release_solve_lock(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    const char *after = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_EQUAL_STRING("C", after);
}

/*******************************************************************************
 * Nested call: optimizing flag makes acquire a no-op
 ******************************************************************************/

void test_acquire_while_optimizing_is_noop(void) {
    if (!try_set_non_c_locale()) {
        TEST_IGNORE_MESSAGE("No non-C locale available on this system");
        return;
    }

    /* Simulate outer optimize already holding the locale lock */
    env->optimizing = 1;

    int rc = cxf_acquire_solve_lock(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Nothing saved because optimizing was already set */
    TEST_ASSERT_NULL(env->saved_locale);

    env->optimizing = 0;
    setlocale(LC_NUMERIC, "C");
}

/*******************************************************************************
 * Double release is safe (idempotent)
 ******************************************************************************/

void test_double_release_is_safe(void) {
    if (!try_set_non_c_locale()) {
        TEST_IGNORE_MESSAGE("No non-C locale available on this system");
        return;
    }

    cxf_acquire_solve_lock(env);

    int rc1 = cxf_release_solve_lock(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc1);
    TEST_ASSERT_NULL(env->saved_locale);

    /* Second release should be safe (no-op) */
    int rc2 = cxf_release_solve_lock(env);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc2);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_acquire_null_env);
    RUN_TEST(test_release_null_env);
    RUN_TEST(test_acquire_switches_to_c);
    RUN_TEST(test_release_restores_original);
    RUN_TEST(test_acquire_already_c_is_noop);
    RUN_TEST(test_acquire_while_optimizing_is_noop);
    RUN_TEST(test_double_release_is_safe);

    return UNITY_END();
}
