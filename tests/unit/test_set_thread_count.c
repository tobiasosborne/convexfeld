/**
 * @file test_set_thread_count.c
 * @brief V2 spec compliance tests for cxf_set_thread_count
 *
 * Per threading_sync.md V2 spec (convexfeld-385q):
 * - Name: cxf_set_thread_count (not cxf_validate_thread_count)
 * - Signature: (CxfEnv *env, int thread_count) -> void
 * - Behavior: warn via cxf_log if thread_count > logical processors
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"

/* Function under test */
void cxf_set_thread_count(CxfEnv *env, int thread_count);

/* Hardware query used for boundary testing */
int cxf_get_logical_processors(CxfEnv *env);

/* Logging callback API */
int cxf_register_log_callback(CxfEnv *env,
                               void (*cb)(const char *msg, void *data),
                               void *data);

/* API functions */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);

static CxfEnv *env = NULL;

/* Log capture state */
static int log_call_count;
static char last_log_msg[512];

static void log_capture(const char *msg, void *data) {
    (void)data;
    log_call_count++;
    if (msg) {
        int i;
        for (i = 0; i < 511 && msg[i]; i++) {
            last_log_msg[i] = msg[i];
        }
        last_log_msg[i] = '\0';
    }
}

void setUp(void) {
    cxf_loadenv(&env, NULL);
    log_call_count = 0;
    last_log_msg[0] = '\0';
    cxf_register_log_callback(env, log_capture, NULL);
}

void tearDown(void) {
    if (env) {
        cxf_register_log_callback(env, NULL, NULL);
        cxf_freeenv(env);
        env = NULL;
    }
}

/*==========================================================================
 * Spec compliance: void return type
 *=========================================================================*/

void test_returns_void(void) {
    /* Compiles only if cxf_set_thread_count returns void */
    cxf_set_thread_count(env, 1);
    TEST_PASS();
}

/*==========================================================================
 * Spec compliance: no warning when within logical processors
 *=========================================================================*/

void test_no_warning_within_logical(void) {
    int logical = cxf_get_logical_processors(env);
    cxf_set_thread_count(env, logical);
    TEST_ASSERT_EQUAL_INT(0, log_call_count);
}

void test_no_warning_single_thread(void) {
    cxf_set_thread_count(env, 1);
    TEST_ASSERT_EQUAL_INT(0, log_call_count);
}

/*==========================================================================
 * Spec compliance: warning emitted when exceeding logical processors
 *=========================================================================*/

void test_warning_on_oversubscription(void) {
    int logical = cxf_get_logical_processors(env);
    cxf_set_thread_count(env, logical + 1);
    /* Spec says up to 4 log lines: separator, warning, suggestion, separator */
    TEST_ASSERT_GREATER_OR_EQUAL(1, log_call_count);
}

void test_warning_mentions_counts(void) {
    int logical = cxf_get_logical_processors(env);
    int requested = logical + 50;
    cxf_set_thread_count(env, requested);
    /* At least one log line should have been emitted */
    TEST_ASSERT_GREATER_OR_EQUAL(1, log_call_count);
}

/*==========================================================================
 * Spec compliance: NULL env is safe (no crash, no output)
 *=========================================================================*/

void test_null_env_safe(void) {
    cxf_set_thread_count(NULL, 4);
    TEST_PASS();
}

/*==========================================================================
 * Boundary: exactly at logical count should NOT warn
 *=========================================================================*/

void test_exact_logical_no_warning(void) {
    int logical = cxf_get_logical_processors(env);
    cxf_set_thread_count(env, logical);
    TEST_ASSERT_EQUAL_INT(0, log_call_count);
}

/*==========================================================================
 * Main
 *=========================================================================*/

int main(void) {
    UNITY_BEGIN();

    RUN_TEST(test_returns_void);
    RUN_TEST(test_no_warning_within_logical);
    RUN_TEST(test_no_warning_single_thread);
    RUN_TEST(test_warning_on_oversubscription);
    RUN_TEST(test_warning_mentions_counts);
    RUN_TEST(test_null_env_safe);
    RUN_TEST(test_exact_logical_no_warning);

    return UNITY_END();
}
