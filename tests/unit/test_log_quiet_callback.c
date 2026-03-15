/**
 * @file test_log_quiet_callback.c
 * @brief Tests: callbacks receive messages even when output_flag == 0
 *
 * V2 spec references:
 *   logging.md §Verbosity Control:
 *     "Verbosity == 0 (quiet mode): Console and log file output are
 *      suppressed. [...] User callbacks remain active if registered."
 *   callback_protocol.md §Parameters:
 *     "OutputFlag: Controls whether console logging occurs, but does
 *      NOT affect callback invocation."
 *
 * Issue: convexfeld-ir53
 */

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <string.h>

/* Forward declarations */
void cxf_log_printf(CxfEnv *env, int level, const char *format, ...);
int cxf_register_log_callback(CxfEnv *env,
                               void (*callback)(const char *msg, void *data),
                               void *data);
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);

/* Callback state */
static char cb_msg[256];
static int cb_count;
static void *cb_data_received;

static void capture_callback(const char *msg, void *data) {
    strncpy(cb_msg, msg, sizeof(cb_msg) - 1);
    cb_msg[sizeof(cb_msg) - 1] = '\0';
    cb_count++;
    cb_data_received = data;
}

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
    cb_msg[0] = '\0';
    cb_count = 0;
    cb_data_received = NULL;
}

void tearDown(void) {
    if (env) {
        cxf_freeenv(env);
        env = NULL;
    }
}

/*============================================================================
 * Quiet mode: output_flag == 0, callback registered
 *===========================================================================*/

void test_quiet_mode_callback_receives_message(void) {
    cxf_register_log_callback(env, capture_callback, NULL);
    env->output_flag = 0;
    cxf_log_printf(env, 0, "quiet but heard");
    TEST_ASSERT_EQUAL_INT(1, cb_count);
    TEST_ASSERT_EQUAL_STRING("quiet but heard", cb_msg);
}

void test_quiet_mode_callback_receives_formatted(void) {
    cxf_register_log_callback(env, capture_callback, NULL);
    env->output_flag = 0;
    cxf_log_printf(env, 0, "iter=%d obj=%.2f", 7, 3.14);
    TEST_ASSERT_EQUAL_INT(1, cb_count);
    TEST_ASSERT_EQUAL_STRING("iter=7 obj=3.14", cb_msg);
}

void test_quiet_mode_callback_receives_userdata(void) {
    int sentinel = 42;
    cxf_register_log_callback(env, capture_callback, &sentinel);
    env->output_flag = 0;
    cxf_log_printf(env, 0, "data check");
    TEST_ASSERT_EQUAL_PTR(&sentinel, cb_data_received);
}

void test_quiet_mode_multiple_messages(void) {
    cxf_register_log_callback(env, capture_callback, NULL);
    env->output_flag = 0;
    cxf_log_printf(env, 0, "first");
    cxf_log_printf(env, 0, "second");
    cxf_log_printf(env, 0, "third");
    TEST_ASSERT_EQUAL_INT(3, cb_count);
    TEST_ASSERT_EQUAL_STRING("third", cb_msg);
}

void test_quiet_mode_no_callback_no_crash(void) {
    /* No callback registered, output_flag = 0, should not crash */
    env->output_flag = 0;
    cxf_log_printf(env, 0, "silently dropped");
    TEST_ASSERT_EQUAL_INT(0, cb_count);
}

/*============================================================================
 * Verbosity filtering still applies independently of output_flag
 *===========================================================================*/

void test_quiet_mode_verbosity_still_filters(void) {
    cxf_register_log_callback(env, capture_callback, NULL);
    env->output_flag = 0;
    env->verbosity = 0;
    /* level=1 message should be filtered by verbosity even though
     * callback would otherwise be active */
    cxf_log_printf(env, 1, "filtered by verbosity");
    TEST_ASSERT_EQUAL_INT(0, cb_count);
}

void test_enabled_mode_callback_still_works(void) {
    /* Sanity: output_flag=1 still delivers to callback */
    cxf_register_log_callback(env, capture_callback, NULL);
    env->output_flag = 1;
    cxf_log_printf(env, 0, "enabled mode");
    TEST_ASSERT_EQUAL_INT(1, cb_count);
    TEST_ASSERT_EQUAL_STRING("enabled mode", cb_msg);
}

/*============================================================================
 * Main
 *===========================================================================*/

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_quiet_mode_callback_receives_message);
    RUN_TEST(test_quiet_mode_callback_receives_formatted);
    RUN_TEST(test_quiet_mode_callback_receives_userdata);
    RUN_TEST(test_quiet_mode_multiple_messages);
    RUN_TEST(test_quiet_mode_no_callback_no_crash);
    RUN_TEST(test_quiet_mode_verbosity_still_filters);
    RUN_TEST(test_enabled_mode_callback_still_works);
    return UNITY_END();
}
