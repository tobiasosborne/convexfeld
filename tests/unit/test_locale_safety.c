/**
 * @file test_locale_safety.c
 * @brief Tests for locale safety in cxf_optimize (solve_entry.md Steps 3/12).
 *
 * Verifies that cxf_optimize saves LC_NUMERIC, switches to "C" during solve,
 * and restores the original locale on all exit paths.
 */

#include <locale.h>
#include <string.h>
#include "unity.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_callback.h"

/* Test fixture */
static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    cxf_freeenv(env);
    env = NULL;
}

/* Try to set a non-C LC_NUMERIC locale.  Returns 1 on success, 0 if none. */
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

/*******************************************************************************
 * Locale restored after successful optimize
 ******************************************************************************/

void test_locale_restored_after_optimize(void) {
    if (!try_set_non_c_locale()) {
        TEST_IGNORE_MESSAGE("No non-C locale available on this system");
        return;
    }

    /* Capture the non-C locale name */
    const char *before = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_NOT_NULL(before);
    char saved[128];
    strncpy(saved, before, sizeof(saved) - 1);
    saved[sizeof(saved) - 1] = '\0';

    /* Verify we are NOT in "C" locale */
    TEST_ASSERT_NOT_EQUAL(0, strcmp(saved, "C"));

    /* Build a simple model and optimize */
    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "locale_test", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Locale must be restored to the non-C value */
    const char *after = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL_STRING(saved, after);

    cxf_freemodel(model);

    /* Restore to C for remaining tests */
    setlocale(LC_NUMERIC, "C");
}

/*******************************************************************************
 * Locale restored after optimize on empty model
 ******************************************************************************/

void test_locale_restored_after_empty_model(void) {
    if (!try_set_non_c_locale()) {
        TEST_IGNORE_MESSAGE("No non-C locale available on this system");
        return;
    }

    const char *before = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_NOT_NULL(before);
    char saved[128];
    strncpy(saved, before, sizeof(saved) - 1);
    saved[sizeof(saved) - 1] = '\0';

    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "empty_locale", 0, NULL, NULL, NULL, NULL, NULL);

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* Must be restored even for an empty model */
    const char *after = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL_STRING(saved, after);

    cxf_freemodel(model);
    setlocale(LC_NUMERIC, "C");
}

/*******************************************************************************
 * C locale is active during optimization (callback verification)
 ******************************************************************************/

static char cb_locale_during[128] = {0};
static int cb_locale_count = 0;

static int capture_locale_cb(CxfModel *model, void *cbdata,
                             int where, void *usrdata) {
    (void)model;
    (void)cbdata;
    (void)where;
    (void)usrdata;
    const char *loc = setlocale(LC_NUMERIC, NULL);
    if (loc != NULL && cb_locale_count == 0) {
        strncpy(cb_locale_during, loc, sizeof(cb_locale_during) - 1);
        cb_locale_during[sizeof(cb_locale_during) - 1] = '\0';
    }
    cb_locale_count++;
    return 0;
}

void test_c_locale_active_during_optimize(void) {
    if (!try_set_non_c_locale()) {
        TEST_IGNORE_MESSAGE("No non-C locale available on this system");
        return;
    }

    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "during_locale", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    cxf_setcallbackfunc(model, capture_locale_cb, NULL);
    cb_locale_during[0] = '\0';
    cb_locale_count = 0;

    cxf_optimize(model);

    /* During optimization the locale must be "C" */
    if (cb_locale_count > 0) {
        TEST_ASSERT_EQUAL_STRING("C", cb_locale_during);
    }

    cxf_freemodel(model);
    setlocale(LC_NUMERIC, "C");
}

/*******************************************************************************
 * Default C locale preserved when caller had no special locale
 ******************************************************************************/

void test_default_c_locale_preserved(void) {
    setlocale(LC_NUMERIC, "C");

    CxfModel *model = NULL;
    cxf_newmodel(env, &model, "default_locale", 0, NULL, NULL, NULL, NULL, NULL);
    cxf_addvar(model, 0, NULL, NULL, 1.0, 0.0, 10.0, 'C', "x");

    int rc = cxf_optimize(model);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    /* "C" locale must still be active */
    const char *after = setlocale(LC_NUMERIC, NULL);
    TEST_ASSERT_NOT_NULL(after);
    TEST_ASSERT_EQUAL_STRING("C", after);

    cxf_freemodel(model);
}

/*******************************************************************************
 * Main
 ******************************************************************************/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_locale_restored_after_optimize);
    RUN_TEST(test_locale_restored_after_empty_model);
    RUN_TEST(test_c_locale_active_during_optimize);
    RUN_TEST(test_default_c_locale_preserved);

    return UNITY_END();
}
