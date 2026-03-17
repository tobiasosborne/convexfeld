/**
 * @file cxf_env.h
 * @brief CxfEnv structure - optimization environment.
 *
 * The environment is the root context for all solver operations.
 * It manages configuration, logging, and serves as a factory for models.
 */

#ifndef CXF_ENV_H
#define CXF_ENV_H

#include "cxf_types.h"

/**
 * @brief Optimization environment structure.
 *
 * Encapsulates solver configuration, logging, and global resources.
 * All models must be associated with an environment.
 */
struct CxfEnv {
    uint32_t magic;           /**< Validation magic (CXF_ENV_MAGIC) */
    int active;               /**< 1 if environment is active, 0 otherwise */
    int error_code;           /**< Last error code (CxfErrorCode, always written) */
    char error_buffer[512];   /**< Last error message */

    /* Tolerances */
    double feasibility_tol;   /**< Primal feasibility tolerance */
    double optimality_tol;    /**< Dual optimality tolerance */
    double infinity;          /**< Infinity threshold (cached CXF_INFINITY) */

    /* Logging */
    int verbosity;            /**< Logging level: 0=silent, 1=normal, 2+=verbose */
    int output_flag;          /**< Master output control: 0=suppress, 1=enable */

    /* Termination flags */
    volatile int *terminate_flag_ptr; /**< External termination flag (fastest check) */
    volatile int terminate_flag;      /**< Primary termination flag */

    /* Algorithm selection */
    int method;               /**< Root LP method: -1=auto, 0=primal, 1=dual, 2=barrier, 3=concurrent, 4=det concurrent, 5=PDHG */

    /* Threading */
    int threads;              /**< User Threads parameter: 0=auto, >0=explicit cap */
    int license_thread_limit; /**< License thread limit: 0=unlimited, >0=hard cap */
    int logical_processors;   /**< Detected logical processors (cached at init) */
    int physical_cores;       /**< Detected physical cores (cached at init) */
    /* Refactorization parameters */
    int max_eta_count;        /**< Maximum eta vectors before forced refactor */
    int64_t max_eta_memory;   /**< Maximum eta memory before forced refactor */
    int refactor_interval;    /**< Iterations between routine refactorizations */

    /* Reference counting and versioning */
    int ref_count;            /**< Reference counter for environment lifetime */
    int version;              /**< Configuration version counter (incremented on param changes) */

    /* Session tracking */
    int session_ref;          /**< Session counter (incremented per optimize call) */
    uint64_t session_id;      /**< Unique ID for current session */

    /* State flags */
    int optimizing;           /**< 1 if optimization is in progress */
    int error_buf_locked;     /**< Prevents error buffer overwrites during nested errors */
    int anonymous_mode;       /**< Suppress variable/constraint name tracking */

    /* Locale safety (threading_sync.md) */
    char *saved_locale;       /**< Saved LC_NUMERIC locale string (heap, NULL when not held) */

    /* String parameters */
    char *logfile;            /**< Log file path (heap-allocated, NULL = none) */

    /* Log callback */
    void (*log_callback)(const char *msg, void *data); /**< User log callback */
    void *log_callback_data;  /**< User data for log callback */

    /* Optimization callback — per V2 spec (callback_state.md §Relationships),
     * the callback function pointer resides on the Environment, not CallbackState. */
    CxfCallbackFunc callback_func; /**< User optimization callback function */

    /* Optional owned structures (allocated on demand) */
    CallbackContext *callback_state; /**< Callback registration and tracking (may be NULL) */
    CxfEnv *master_env;       /**< Parent environment for copy/child environments (NULL for root) */
};

/*******************************************************************************
 * Environment Lifecycle API
 ******************************************************************************/

/**
 * @brief Create and initialize an environment.
 * @param envP Output pointer to created environment
 * @param logfilename Optional log file (NULL for stdout)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);

/**
 * @brief Create an inactive (unstarted) environment.
 *
 * Creates environment with active=0. Use cxf_startenv() to activate.
 * Useful for advanced configuration before activation.
 *
 * @param envP Output pointer to created environment
 * @param logfilename Optional log file (NULL for stdout)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_emptyenv(CxfEnv **envP, const char *logfilename);

/**
 * @brief Activate an inactive environment.
 *
 * Finalizes initialization and sets active=1. Only valid for
 * environments created with cxf_emptyenv().
 *
 * @param env Environment to activate
 * @return CXF_OK on success, error code otherwise
 */
int cxf_startenv(CxfEnv *env);

/**
 * @brief Free an environment and all associated resources.
 * @param env Environment to free (may be NULL)
 * @return CXF_OK on success, CXF_ERROR_INVALID_ARGUMENT if env is NULL
 */
int cxf_freeenv(CxfEnv *env);

/**
 * @brief Validate environment pointer and state.
 *
 * Checks for NULL and validates magic number.
 *
 * @param env Environment to validate
 * @return CXF_OK if valid, error code otherwise
 */
int cxf_checkenv(CxfEnv *env);

/**
 * @brief Validate environment pointer with two-level sentinel check.
 *
 * Per V2 input_validation.md: checks env non-null, validates primary
 * sentinel, follows root environment pointer and validates root sentinel.
 * Both sentinels must match for success.
 *
 * @param env Environment to validate
 * @return CXF_OK if valid, error code otherwise
 */
int cxf_check_env(CxfEnv *env);

/*******************************************************************************
 * Environment Accessor API
 ******************************************************************************/

/**
 * @brief Set the termination flag for an environment.
 *
 * Signals the solver to terminate at the next opportunity.
 *
 * @param env Environment to terminate
 * @return CXF_OK on success, error code otherwise
 */
int cxf_terminate(CxfEnv *env);

/**
 * @brief Reset the termination flag.
 *
 * Clears the termination flag to allow a new optimization.
 *
 * @param env Environment to reset
 * @return CXF_OK on success, error code otherwise
 */
int cxf_reset_terminate(CxfEnv *env);

/**
 * @brief Get the last error message.
 *
 * @param env Environment to query
 * @return Pointer to error message (never NULL, may be empty string)
 */
const char *cxf_geterrormsg(CxfEnv *env);

/**
 * @brief Clear the error message buffer.
 *
 * @param env Environment to clear
 * @return CXF_OK on success, error code otherwise
 */
int cxf_clearerrormsg(CxfEnv *env);

/**
 * @brief Set the callback context for an environment.
 *
 * Transfers ownership of the callback context to the environment.
 *
 * @param env Environment to modify
 * @param ctx Callback context (may be NULL to clear)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_set_callback_context(CxfEnv *env, CallbackContext *ctx);

/**
 * @brief Get the callback context for an environment.
 *
 * @param env Environment to query
 * @return Callback context (may be NULL if not set)
 */
CallbackContext *cxf_get_callback_context(CxfEnv *env);

/**
 * @brief Clear error buffer state for a new API operation.
 *
 * Per V2 threading_sync.md: resets error_code and error_buffer unless
 * error_buf_locked is set (nested error handling in progress).
 * Despite the name, no mutex is acquired.
 *
 * @param env Environment to clear (NULL-safe, silent return)
 */
void cxf_env_acquire_lock(CxfEnv *env);

/*******************************************************************************
 * Locale Safety API (threading_sync.md)
 ******************************************************************************/

/**
 * @brief Save current LC_NUMERIC locale and switch to "C".
 *
 * Ensures consistent decimal point formatting during optimization.
 * Stores the saved locale string on env->saved_locale via strdup.
 * If already in "C" locale or env is NULL, this is a no-op.
 *
 * @param env Environment to modify
 * @return CXF_OK on success, error code on failure
 */
int cxf_acquire_solve_lock(CxfEnv *env);

/**
 * @brief Restore the saved LC_NUMERIC locale from the environment.
 *
 * Restores the locale saved by cxf_acquire_solve_lock, frees the
 * saved string, and clears the pointer. No-op if nothing was saved.
 *
 * @param env Environment to restore
 * @return CXF_OK on success, error code on failure
 */
int cxf_release_solve_lock(CxfEnv *env);

/*******************************************************************************
 * Parameter API
 ******************************************************************************/

/**
 * @brief Set an integer parameter value.
 *
 * Supported parameters: OutputFlag, Verbosity, RefactorInterval, MaxEtaCount.
 *
 * @param env Environment to modify
 * @param paramname Parameter name (case-sensitive)
 * @param newvalue New value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_setintparam(CxfEnv *env, const char *paramname, int newvalue);

/**
 * @brief Get an integer parameter value.
 *
 * @param env Environment to query
 * @param paramname Parameter name (case-sensitive)
 * @param valueP Output pointer for value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getintparam(CxfEnv *env, const char *paramname, int *valueP);

/**
 * @brief Set a double parameter value.
 *
 * Supported: FeasibilityTol (1e-9..1e-2), OptimalityTol (1e-9..1e-2),
 * Infinity (1e15..1e30). Case-insensitive matching.
 *
 * @param env Environment to modify
 * @param paramname Parameter name (case-insensitive)
 * @param newvalue New value to set
 * @return CXF_OK on success, error code otherwise
 */
int cxf_setdblparam(CxfEnv *env, const char *paramname, double newvalue);

/**
 * @brief Get a double parameter value.
 *
 * @param env Environment to query
 * @param paramname Parameter name (case-insensitive)
 * @param valueP Output pointer for value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getdblparam(CxfEnv *env, const char *paramname, double *valueP);

/**
 * @brief Set a string parameter value.
 *
 * Supported: LogFile. Case-insensitive matching.
 * The string is copied internally (caller retains ownership of newvalue).
 *
 * @param env Environment to modify
 * @param paramname Parameter name (case-insensitive)
 * @param newvalue New value (may be NULL to clear)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_setstringparam(CxfEnv *env, const char *paramname,
                       const char *newvalue);

/**
 * @brief Get a string parameter value.
 *
 * Copies the current value into the caller's buffer, respecting bufsize.
 *
 * @param env Environment to query
 * @param paramname Parameter name (case-insensitive)
 * @param valueP Output buffer for value
 * @param bufsize Size of output buffer in bytes
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getstringparam(CxfEnv *env, const char *paramname,
                       char *valueP, int bufsize);

#endif /* CXF_ENV_H */
