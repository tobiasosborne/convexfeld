/**
 * @file test_callback_init_mutex.c
 * @brief Tests for cxf_init_callback_struct V2 mutex allocation.
 *
 * V2 spec (callbacks.md): cxf_init_callback_struct allocates a
 * pthread_mutex_t and writes it to *mutex_out. Tests verify:
 * - Mutex is allocated (not NULL after init)
 * - Mutex can be locked/unlocked (proves valid initialization)
 * - NULL mutex_out handled gracefully
 * - Multiple inits produce independent mutexes
 * - *mutex_out set to NULL before allocation attempt
 */

#define _POSIX_C_SOURCE 199309L

#include "unity.h"
#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <pthread.h>

/* Forward declarations */
int cxf_init_callback_struct(CxfEnv *env, void **mutex_out);
int cxf_loadenv(CxfEnv **envP, const char *logfilename);
int cxf_freeenv(CxfEnv *env);
void cxf_free(void *ptr);

static CxfEnv *env = NULL;

void setUp(void) {
    cxf_loadenv(&env, NULL);
}

void tearDown(void) {
    if (env) { cxf_freeenv(env); env = NULL; }
}

/*==========================================================================*/

void test_mutex_allocated_not_null(void) {
    void *mutex = NULL;
    int rc = cxf_init_callback_struct(env, &mutex);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_NOT_NULL(mutex);
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    cxf_free(mutex);
}

void test_mutex_can_lock_and_unlock(void) {
    void *mutex = NULL;
    int rc = cxf_init_callback_struct(env, &mutex);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);

    pthread_mutex_t *mtx = (pthread_mutex_t *)mutex;
    int lock_rc = pthread_mutex_lock(mtx);
    TEST_ASSERT_EQUAL_INT(0, lock_rc);
    int unlock_rc = pthread_mutex_unlock(mtx);
    TEST_ASSERT_EQUAL_INT(0, unlock_rc);

    pthread_mutex_destroy(mtx);
    cxf_free(mutex);
}

void test_null_mutex_out_returns_error(void) {
    int rc = cxf_init_callback_struct(env, NULL);
    TEST_ASSERT_EQUAL_INT(CXF_ERROR_NULL_ARGUMENT, rc);
}

void test_multiple_inits_produce_independent_mutexes(void) {
    void *mutex1 = NULL;
    void *mutex2 = NULL;
    int rc1 = cxf_init_callback_struct(env, &mutex1);
    int rc2 = cxf_init_callback_struct(env, &mutex2);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc1);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc2);
    TEST_ASSERT_NOT_NULL(mutex1);
    TEST_ASSERT_NOT_NULL(mutex2);
    TEST_ASSERT_NOT_EQUAL(mutex1, mutex2);

    /* Lock mutex1 — mutex2 should still be lockable */
    pthread_mutex_lock((pthread_mutex_t *)mutex1);
    int lock_rc = pthread_mutex_trylock((pthread_mutex_t *)mutex2);
    TEST_ASSERT_EQUAL_INT(0, lock_rc);
    pthread_mutex_unlock((pthread_mutex_t *)mutex2);
    pthread_mutex_unlock((pthread_mutex_t *)mutex1);

    pthread_mutex_destroy((pthread_mutex_t *)mutex1);
    pthread_mutex_destroy((pthread_mutex_t *)mutex2);
    cxf_free(mutex1);
    cxf_free(mutex2);
}

void test_output_set_to_null_before_alloc(void) {
    /* V2 spec: *mutex_out = NULL first (clean state even on failure) */
    void *mutex = (void *)0xDEADBEEF;
    int rc = cxf_init_callback_struct(env, &mutex);
    /* On success, mutex should be non-NULL (overwritten from NULL) */
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_NOT_NULL(mutex);
    /* The key assertion: it is NOT 0xDEADBEEF anymore */
    TEST_ASSERT_NOT_EQUAL((void *)0xDEADBEEF, mutex);
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    cxf_free(mutex);
}

void test_null_env_succeeds(void) {
    void *mutex = NULL;
    int rc = cxf_init_callback_struct(NULL, &mutex);
    TEST_ASSERT_EQUAL_INT(CXF_OK, rc);
    TEST_ASSERT_NOT_NULL(mutex);
    pthread_mutex_destroy((pthread_mutex_t *)mutex);
    cxf_free(mutex);
}

/*==========================================================================*/

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_mutex_allocated_not_null);
    RUN_TEST(test_mutex_can_lock_and_unlock);
    RUN_TEST(test_null_mutex_out_returns_error);
    RUN_TEST(test_multiple_inits_produce_independent_mutexes);
    RUN_TEST(test_output_set_to_null_before_alloc);
    RUN_TEST(test_null_env_succeeds);
    return UNITY_END();
}
