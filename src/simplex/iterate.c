/**
 * @file iterate.c
 * @brief Iteration progress logging (v2 P3.20, P6.6)
 *
 * Reports iteration progress to the log and invokes the external
 * monitoring callback. Time-throttled to at most one log message
 * per second to avoid flooding.
 *
 * Spec: simplex_iteration.md Step 4
 * Beads: s6pw
 */

#include "convexfeld/cxf_solver.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_callback.h"
#include "convexfeld/cxf_timing.h"
#include "convexfeld/cxf_types.h"
#include <stdio.h>

/* Time throttle: minimum seconds between log messages */
#define LOG_INTERVAL_SEC 1.0

/**
 * @brief Report iteration progress (v2 P3.20 Phase 2, P6.6).
 *
 * Steps per spec:
 *   1. If logging disabled on model, skip to step 4.
 *   2. Time throttle: at most one message per second.
 *   3. Format and emit progress message.
 *   4. Invoke external monitoring callback unconditionally.
 */
void cxf_log_iteration_progress(CxfModel *model, SolverState *state) {
    if (model == NULL || state == NULL) return;

    CxfEnv *env = model->env;
    int iter = state->iteration;

    /* Steps 1-3: Logging (if enabled and time-throttled) */
    if (env != NULL && env->output_flag && env->verbosity >= 1) {
        double now = cxf_get_elapsed_time();

        if (iter == 0 || (now - state->last_log_time) >= LOG_INTERVAL_SEC) {
            state->last_log_time = now;

            if (env->log_callback) {
                char buf[128];
                snprintf(buf, sizeof(buf),
                         "Iter %d: phase=%d obj=%.6e",
                         iter, state->phase, state->obj_value);
                env->log_callback(buf, env->log_callback_data);
            }
        }
    }

    /* Step 4: Invoke monitoring callback unconditionally.
     * This enables GUI/external monitoring even when logging disabled. */
    if (env != NULL && env->callback_state != NULL) {
        CallbackContext *cb = env->callback_state;
        if (cb->enabled && cb->callback_func != NULL) {
            cb->iteration_count = iter;
            cb->best_obj = state->obj_value;
            int result = cb->callback_func(model, cb, CXF_CB_POLLING,
                                           cb->user_data);
            cb->callback_calls += 1.0;
            if (result != 0) {
                /* Callback requested termination */
                if (env->terminate_flag_ptr)
                    *(env->terminate_flag_ptr) = 1;
                env->terminate_flag = 1;
            }
        }
    }
}
