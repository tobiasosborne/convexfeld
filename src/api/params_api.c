/**
 * @file params_api.c
 * @brief Parameter API implementation (M8.1.8)
 *
 * Functions for getting and setting integer parameters in CxfEnv.
 */

#include <string.h>
#include <ctype.h>
#include "convexfeld/cxf_env.h"

/* Forward declare validation function */
extern int cxf_checkenv(CxfEnv *env);

/** @brief Case-insensitive string comparison helper. */
static int strcasecmp_local(const char *s1, const char *s2) {
    while (*s1 && *s2) {
        int c1 = tolower((unsigned char)*s1);
        int c2 = tolower((unsigned char)*s2);
        if (c1 != c2) return c1 - c2;
        s1++;
        s2++;
    }
    return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
}

/**
 * @brief Set an integer parameter.
 *
 * @param env Environment to modify
 * @param paramname Parameter name (case-insensitive)
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
    if (strcasecmp_local(paramname, "OutputFlag") == 0) {
        if (newvalue != 0 && newvalue != 1) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        env->output_flag = newvalue;
        return CXF_OK;
    }

    /* Verbosity: 0-2 */
    if (strcasecmp_local(paramname, "Verbosity") == 0) {
        if (newvalue < 0 || newvalue > 2) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        env->verbosity = newvalue;
        return CXF_OK;
    }

    /* RefactorInterval: 1-10000 */
    if (strcasecmp_local(paramname, "RefactorInterval") == 0) {
        if (newvalue < 1 || newvalue > 10000) {
            return CXF_ERROR_INVALID_ARGUMENT;
        }
        env->refactor_interval = newvalue;
        return CXF_OK;
    }

    /* MaxEtaCount: 10-1000 */
    if (strcasecmp_local(paramname, "MaxEtaCount") == 0) {
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
 * @param paramname Parameter name (case-insensitive)
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
    if (strcasecmp_local(paramname, "OutputFlag") == 0) {
        *valueP = env->output_flag;
        return CXF_OK;
    }

    /* Verbosity */
    if (strcasecmp_local(paramname, "Verbosity") == 0) {
        *valueP = env->verbosity;
        return CXF_OK;
    }

    /* RefactorInterval */
    if (strcasecmp_local(paramname, "RefactorInterval") == 0) {
        *valueP = env->refactor_interval;
        return CXF_OK;
    }

    /* MaxEtaCount */
    if (strcasecmp_local(paramname, "MaxEtaCount") == 0) {
        *valueP = env->max_eta_count;
        return CXF_OK;
    }

    /* Unknown parameter */
    return CXF_ERROR_INVALID_ARGUMENT;
}
