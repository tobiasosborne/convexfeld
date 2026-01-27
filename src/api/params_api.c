/**
 * @file params_api.c
 * @brief Parameter API implementation (M8.1.8)
 *
 * Functions for getting and setting integer parameters in CxfEnv.
 */

#include <string.h>
#include "convexfeld/cxf_env.h"

/* Forward declare validation function */
extern int cxf_checkenv(CxfEnv *env);

/**
 * @brief Set an integer parameter.
 *
 * @param env Environment to modify
 * @param paramname Parameter name (case-sensitive)
 * @param newvalue New value to set
 * @return CXF_OK on success, error code otherwise
 */
int cxf_setintparam(CxfEnv *env, const char *paramname, int newvalue) {
    int status;

    /* Validate environment */
    status = cxf_checkenv(env);
    if (status != CXF_OK) {
        return status;
    }

    /* Validate paramname */
    if (paramname == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* OutputFlag: 0 or 1 */
    if (strcmp(paramname, "OutputFlag") == 0) {
        if (newvalue != 0 && newvalue != 1) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        env->output_flag = newvalue;
        return CXF_OK;
    }

    /* Verbosity: 0-2 */
    if (strcmp(paramname, "Verbosity") == 0) {
        if (newvalue < 0 || newvalue > 2) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        env->verbosity = newvalue;
        return CXF_OK;
    }

    /* RefactorInterval: 1-10000 */
    if (strcmp(paramname, "RefactorInterval") == 0) {
        if (newvalue < 1 || newvalue > 10000) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        env->refactor_interval = newvalue;
        return CXF_OK;
    }

    /* MaxEtaCount: 10-1000 */
    if (strcmp(paramname, "MaxEtaCount") == 0) {
        if (newvalue < 10 || newvalue > 1000) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        env->max_eta_count = newvalue;
        return CXF_OK;
    }

    /* Unknown parameter */
    return CXF_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief Get an integer parameter.
 *
 * @param env Environment to query
 * @param paramname Parameter name (case-sensitive)
 * @param valueP Output pointer for value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getintparam(CxfEnv *env, const char *paramname, int *valueP) {
    int status;

    /* Validate environment */
    status = cxf_checkenv(env);
    if (status != CXF_OK) {
        return status;
    }

    /* Validate arguments */
    if (paramname == NULL || valueP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* OutputFlag */
    if (strcmp(paramname, "OutputFlag") == 0) {
        *valueP = env->output_flag;
        return CXF_OK;
    }

    /* Verbosity */
    if (strcmp(paramname, "Verbosity") == 0) {
        *valueP = env->verbosity;
        return CXF_OK;
    }

    /* RefactorInterval */
    if (strcmp(paramname, "RefactorInterval") == 0) {
        *valueP = env->refactor_interval;
        return CXF_OK;
    }

    /* MaxEtaCount */
    if (strcmp(paramname, "MaxEtaCount") == 0) {
        *valueP = env->max_eta_count;
        return CXF_OK;
    }

    /* Unknown parameter */
    return CXF_ERROR_INVALID_ARGUMENT;
}
