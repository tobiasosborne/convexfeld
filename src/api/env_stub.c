/**
 * @file env_stub.c
 * @brief Stub environment functions for tracer bullet.
 *
 * Minimal implementation of cxf_loadenv and cxf_freeenv.
 * Full implementation with parameter management comes later.
 */

#include <string.h>
#include "convexfeld/cxf_env.h"

/* Forward declare memory functions */
extern void *cxf_calloc(size_t count, size_t size);
extern void cxf_free(void *ptr);

/**
 * @brief Create and initialize an environment (stub).
 *
 * Allocates a CxfEnv structure and initializes it with default values.
 * The logfilename parameter is ignored in this stub.
 *
 * @param envP Output pointer to created environment
 * @param logfilename Optional log file (ignored in stub)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_loadenv(CxfEnv **envP, const char *logfilename) {
    CxfEnv *env;

    (void)logfilename;  /* Unused in stub */

    if (envP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    *envP = NULL;

    env = (CxfEnv *)cxf_calloc(1, sizeof(CxfEnv));
    if (env == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Initialize fields */
    env->magic = CXF_ENV_MAGIC;
    env->active = 1;
    env->error_buffer[0] = '\0';

    /* Default tolerances */
    env->feasibility_tol = CXF_FEASIBILITY_TOL;
    env->optimality_tol = CXF_OPTIMALITY_TOL;
    env->infinity = CXF_INFINITY;

    /* Logging defaults */
    env->verbosity = 1;
    env->output_flag = 1;

    /* Reference counting */
    env->ref_count = 1;

    /* Log callback (none by default) */
    env->log_callback = NULL;
    env->log_callback_data = NULL;

    *envP = env;
    return CXF_OK;
}

/**
 * @brief Free an environment and all associated resources (stub).
 *
 * @param env Environment to free (may be NULL)
 */
void cxf_freeenv(CxfEnv *env) {
    if (env == NULL) {
        return;
    }

    /* Mark as inactive before freeing */
    env->active = 0;
    env->magic = 0;

    cxf_free(env);
}
