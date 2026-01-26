/**
 * @file callback_stub.c
 * @brief Stub implementations for callback functions (M5.2.3-M5.2.5)
 *
 * These stubs allow TDD tests to link. Functions will be replaced
 * with full implementations in subsequent milestones.
 *
 * Note: cxf_check_terminate, cxf_terminate, cxf_clear_terminate are
 * already implemented in src/error/terminate.c (M3.1.6).
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_callback.h"
#include <string.h>

/*============================================================================
 * M5.2.3: cxf_init_callback_struct, cxf_reset_callback_state
 * NOTE: Moved to src/callbacks/init.c
 *===========================================================================*/

/*============================================================================
 * M5.2.5: Termination functions (callback-specific)
 *
 * Note: cxf_check_terminate is in src/error/terminate.c
 * cxf_set_terminate is an alias/wrapper for cxf_terminate
 *===========================================================================*/

/**
 * @brief Set termination flag in environment (alias for cxf_terminate).
 *
 * @param env Environment to mark for termination.
 */
void cxf_set_terminate(CxfEnv *env) {
    if (env == NULL) {
        return;
    }
    env->terminate_flag = 1;
}

/**
 * @brief Request termination from within a callback.
 *
 * @param model Model being optimized.
 */
void cxf_callback_terminate(CxfModel *model) {
    if (model == NULL || model->env == NULL) {
        return;
    }
    model->env->terminate_flag = 1;
}

/*============================================================================
 * M5.2.4: Pre/post optimize callbacks
 *===========================================================================*/

/**
 * @brief Execute pre-optimization callback.
 *
 * @param model Model about to be optimized.
 * @return 0 on success, non-zero to abort optimization.
 */
int cxf_pre_optimize_callback(CxfModel *model) {
    if (model == NULL || model->env == NULL) {
        return 0;  /* No callback, success */
    }
    /* Stub: no actual callback invocation yet */
    return 0;
}

/**
 * @brief Execute post-optimization callback.
 *
 * @param model Model that was optimized.
 * @return 0 on success.
 */
int cxf_post_optimize_callback(CxfModel *model) {
    if (model == NULL || model->env == NULL) {
        return 0;  /* No callback, success */
    }
    /* Stub: no actual callback invocation yet */
    return 0;
}
