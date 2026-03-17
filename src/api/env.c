/**
 * @file env.c
 * @brief CxfEnv lifecycle and accessor functions (M8.1.7)
 */

#include <string.h>
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_callback.h"

#include "../memory/memory_internal.h"

/* Internal CPU detection helpers (from logging/system.c and threading/cpu.c) */
int cxf_detect_logical_processors(void);
int cxf_detect_physical_cores(void);

/* Default values for refactorization parameters */
#define DEFAULT_MAX_ETA_COUNT     100
#define DEFAULT_MAX_ETA_MEMORY    (1024 * 1024)  /* 1 MB */
#define DEFAULT_REFACTOR_INTERVAL 50

/** Initialize common environment fields. */
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

    /* Algorithm selection default: -1 = auto */
    env->method = -1;

    /* Threading defaults: 0 = auto, no license limit */
    env->threads = 0;
    env->license_thread_limit = 0;

    /* Detect and cache CPU topology at init time (V2 threading_sync.md) */
    env->logical_processors = cxf_detect_logical_processors();
    env->physical_cores = cxf_detect_physical_cores();

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

    /* String parameters */
    env->logfile = NULL;

    /* Log callback (none by default) */
    env->log_callback = NULL;
    env->log_callback_data = NULL;

    /* Optimization callback — V2: resides on Environment */
    env->callback_func = NULL;

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

    /* Free string parameters */
    if (env->logfile != NULL) {
        cxf_free(env->logfile);
        env->logfile = NULL;
    }

    /* Free owned callback context if present */
    if (env->callback_state != NULL) {
        cxf_callback_free(env->callback_state);
        env->callback_state = NULL;
    }
    env->callback_func = NULL;  /* V2: callback_func on Environment */

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
