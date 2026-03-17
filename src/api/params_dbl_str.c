/**
 * @file params_dbl_str.c
 * @brief Double and string parameter API implementation.
 *
 * Functions for getting/setting double and string parameters in CxfEnv.
 * Ranges per V2 parameters_defaults.md Section 2 & 9.
 */

#include <string.h>
#include <ctype.h>
#include "convexfeld/cxf_env.h"
#include "../memory/memory_internal.h"

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
 * @brief Set a double parameter.
 *
 * @param env Environment to modify
 * @param paramname Parameter name (case-insensitive)
 * @param newvalue New value to set
 * @return CXF_OK on success, error code otherwise
 */
int cxf_setdblparam(CxfEnv *env, const char *paramname, double newvalue) {
    int status;

    status = cxf_checkenv(env);
    if (status != CXF_OK) {
        return status;
    }
    if (paramname == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* FeasibilityTol: 1e-9 .. 1e-2 (V2 parameters_defaults.md Section 2) */
    if (strcasecmp_local(paramname, "FeasibilityTol") == 0) {
        if (newvalue < 1e-9 || newvalue > 1e-2) {
            return CXF_ERROR_VALUE_OUT_OF_RANGE;
        }
        env->feasibility_tol = newvalue;
        return CXF_OK;
    }

    /* OptimalityTol: 1e-9 .. 1e-2 (V2 parameters_defaults.md Section 2) */
    if (strcasecmp_local(paramname, "OptimalityTol") == 0) {
        if (newvalue < 1e-9 || newvalue > 1e-2) {
            return CXF_ERROR_VALUE_OUT_OF_RANGE;
        }
        env->optimality_tol = newvalue;
        return CXF_OK;
    }

    /* Infinity: 1e15 .. 1e30 */
    if (strcasecmp_local(paramname, "Infinity") == 0) {
        if (newvalue < 1e15 || newvalue > 1e30) {
            return CXF_ERROR_VALUE_OUT_OF_RANGE;
        }
        env->infinity = newvalue;
        return CXF_OK;
    }

    return CXF_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief Set a string parameter.
 *
 * @param env Environment to modify
 * @param paramname Parameter name (case-insensitive)
 * @param newvalue New value (NULL clears the parameter)
 * @return CXF_OK on success, error code otherwise
 */
int cxf_setstringparam(CxfEnv *env, const char *paramname,
                       const char *newvalue) {
    int status;

    status = cxf_checkenv(env);
    if (status != CXF_OK) {
        return status;
    }
    if (paramname == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* LogFile (V2 parameters_defaults.md Section 9) */
    if (strcasecmp_local(paramname, "LogFile") == 0) {
        char *copy = NULL;
        if (newvalue != NULL && newvalue[0] != '\0') {
            size_t len = strlen(newvalue);
            copy = (char *)cxf_malloc(len + 1);
            if (copy == NULL) {
                return CXF_ERROR_OUT_OF_MEMORY;
            }
            memcpy(copy, newvalue, len + 1);
        }
        if (env->logfile != NULL) {
            cxf_free(env->logfile);
        }
        env->logfile = copy;
        return CXF_OK;
    }

    return CXF_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief Get a string parameter.
 *
 * @param env Environment to query
 * @param paramname Parameter name (case-insensitive)
 * @param valueP Output buffer
 * @param bufsize Size of output buffer in bytes
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getstringparam(CxfEnv *env, const char *paramname,
                       char *valueP, int bufsize) {
    int status;

    status = cxf_checkenv(env);
    if (status != CXF_OK) {
        return status;
    }
    if (paramname == NULL || valueP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (bufsize <= 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* LogFile */
    if (strcasecmp_local(paramname, "LogFile") == 0) {
        const char *src = (env->logfile != NULL) ? env->logfile : "";
        size_t len = strlen(src);
        if (len >= (size_t)bufsize) {
            len = (size_t)(bufsize - 1);
        }
        memcpy(valueP, src, len);
        valueP[len] = '\0';
        return CXF_OK;
    }

    return CXF_ERROR_INVALID_ARGUMENT;
}
