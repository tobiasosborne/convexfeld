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

    /* Log callback */
    void (*log_callback)(const char *msg, void *data); /**< User log callback */
    void *log_callback_data;  /**< User data for log callback */

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

#endif /* CXF_ENV_H */
