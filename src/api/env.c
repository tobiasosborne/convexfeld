/**
 * @file env.c
 * @brief Full CxfEnv structure implementation (M8.1.7)
 *
 * Implements environment lifecycle and accessor functions.
 */

#include <string.h>
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_callback.h"

/* Forward declare memory functions */
extern void *cxf_calloc(size_t count, size_t size);
extern void cxf_free(void *ptr);

/* Default values for refactorization parameters */
#define DEFAULT_MAX_ETA_COUNT     100
#define DEFAULT_MAX_ETA_MEMORY    (1024 * 1024)  /* 1 MB */
#define DEFAULT_REFACTOR_INTERVAL 50

/**
 * @brief Internal helper to initialize common environment fields.
 *
 * @param env Environment to initialize
 * @param logfilename Log file name (ignored in current implementation)
 * @param set_active 1 to set active=1, 0 for inactive environment
 */
static void cxf_env_init_fields(CxfEnv *env, const char *logfilename, int set_active) {
    (void)logfilename;  /* Reserved for future log file support */

    env->magic = CXF_ENV_MAGIC;
    env->active = set_active;
    env->error_buffer[0] = '\0';

    /* Default tolerances */
    env->feasibility_tol = CXF_FEASIBILITY_TOL;
    env->optimality_tol = CXF_OPTIMALITY_TOL;
    env->infinity = CXF_INFINITY;

    /* Logging defaults */
    env->verbosity = 1;
    env->output_flag = 1;

    /* Termination flags */
    env->terminate_flag_ptr = NULL;
    env->terminate_flag = 0;

    /* Refactorization defaults */
    env->max_eta_count = DEFAULT_MAX_ETA_COUNT;
    env->max_eta_memory = DEFAULT_MAX_ETA_MEMORY;
    env->refactor_interval = DEFAULT_REFACTOR_INTERVAL;

    /* Reference counting and versioning */
    env->ref_count = 1;
    env->version = 0;

    /* Session tracking */
    env->session_ref = 0;
    env->session_id = 0;

    /* State flags */
    env->optimizing = 0;
    env->error_buf_locked = 0;
    env->anonymous_mode = 0;

    /* Log callback (none by default) */
    env->log_callback = NULL;
    env->log_callback_data = NULL;

    /* Optional structures (NULL until needed) */
    env->callback_state = NULL;
    env->master_env = NULL;
}

int cxf_loadenv(CxfEnv **envP, const char *logfilename) {
    CxfEnv *env;

    if (envP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    *envP = NULL;

    env = (CxfEnv *)cxf_calloc(1, sizeof(CxfEnv));
    if (env == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    cxf_env_init_fields(env, logfilename, 1);  /* active=1 */

    *envP = env;
    return CXF_OK;
}

int cxf_emptyenv(CxfEnv **envP, const char *logfilename) {
    CxfEnv *env;

    if (envP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    *envP = NULL;

    env = (CxfEnv *)cxf_calloc(1, sizeof(CxfEnv));
    if (env == NULL) {
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    cxf_env_init_fields(env, logfilename, 0);  /* active=0 */

    *envP = env;
    return CXF_OK;
}

int cxf_startenv(CxfEnv *env) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (env->magic != CXF_ENV_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    if (env->active == 1) {
        return CXF_ERROR_INVALID_ARGUMENT;  /* Already active */
    }

    env->active = 1;
    return CXF_OK;
}

int cxf_freeenv(CxfEnv *env) {
    if (env == NULL) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Free owned callback context if present */
    if (env->callback_state != NULL) {
        cxf_callback_free(env->callback_state);
        env->callback_state = NULL;
    }

    /* Note: Models are NOT owned by the environment.
     * The application must free models before freeing the environment.
     * This is consistent with the spec's "Models must be freed before environment". */

    /* Mark as inactive before freeing */
    env->active = 0;
    env->magic = 0;

    cxf_free(env);
    return CXF_OK;
}

/* Note: cxf_terminate and cxf_reset_terminate are in src/error/terminate.c */
/* Note: cxf_geterrormsg is in src/error/core.c */

int cxf_clearerrormsg(CxfEnv *env) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (env->magic != CXF_ENV_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    env->error_buffer[0] = '\0';
    return CXF_OK;
}

int cxf_set_callback_context(CxfEnv *env, CallbackContext *ctx) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (env->magic != CXF_ENV_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Free existing callback context if present */
    if (env->callback_state != NULL && env->callback_state != ctx) {
        cxf_callback_free(env->callback_state);
    }

    env->callback_state = ctx;
    return CXF_OK;
}

CallbackContext *cxf_get_callback_context(CxfEnv *env) {
    if (env == NULL || env->magic != CXF_ENV_MAGIC) {
        return NULL;
    }
    return env->callback_state;
}
