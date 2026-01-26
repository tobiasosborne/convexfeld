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
    double infinity;          /**< Infinity threshold */

    /* Logging */
    int verbosity;            /**< Logging level: 0=silent, 1=normal, 2+=verbose */
    int output_flag;          /**< Master output control: 0=suppress, 1=enable */

    /* Reference counting */
    int ref_count;            /**< Reference counter for environment lifetime */
};

/*******************************************************************************
 * Environment API (stubs for tracer bullet)
 ******************************************************************************/

/**
 * @brief Create and initialize an environment.
 * @param envP Output pointer to created environment
 * @param logfilename Optional log file (NULL for stdout)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_loadenv(CxfEnv **envP, const char *logfilename);

/**
 * @brief Free an environment and all associated resources.
 * @param env Environment to free (may be NULL)
 */
void cxf_freeenv(CxfEnv *env);

#endif /* CXF_ENV_H */
