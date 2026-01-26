/**
 * @file params.c
 * @brief Parameter getter functions for ConvexFeld.
 *
 * Provides access to solver configuration parameters:
 * - cxf_getdblparam: Generic double parameter getter
 * - Tolerance getters for inner-loop performance
 * - Infinity constant for unbounded value representation
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include <string.h>
#include <ctype.h>

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
 * @brief Get double parameter by name.
 *
 * Retrieves a double parameter value from the environment.
 * Supports case-insensitive parameter name matching.
 *
 * Known double parameters:
 * - FeasibilityTol: Primal feasibility tolerance
 * - OptimalityTol: Dual optimality tolerance
 * - Infinity: Infinity representation value
 *
 * @param env Environment to query
 * @param paramname Name of parameter
 * @param valueP Output pointer for value
 * @return CXF_OK on success, error code otherwise
 */
int cxf_getdblparam(CxfEnv *env, const char *paramname, double *valueP) {
    if (env == NULL || paramname == NULL || valueP == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* Check environment magic */
    if (env->magic != CXF_ENV_MAGIC || env->active == 0) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }

    /* Match known double parameters (case-insensitive) */
    if (strcasecmp_local(paramname, "FeasibilityTol") == 0) {
        *valueP = env->feasibility_tol;
        return CXF_OK;
    }
    if (strcasecmp_local(paramname, "OptimalityTol") == 0) {
        *valueP = env->optimality_tol;
        return CXF_OK;
    }
    if (strcasecmp_local(paramname, "Infinity") == 0) {
        *valueP = env->infinity;
        return CXF_OK;
    }

    /* Parameter not found */
    return CXF_ERROR_INVALID_ARGUMENT;
}

/**
 * @brief Get primal feasibility tolerance.
 *
 * Retrieves the feasibility tolerance used for constraint satisfaction
 * checks. Returns default value on any error to enable inner-loop usage
 * without error checking overhead.
 *
 * @param env Environment pointer (may be NULL)
 * @return Feasibility tolerance, default 1e-6 if env is NULL
 */
double cxf_get_feasibility_tol(CxfEnv *env) {
    if (env == NULL) {
        return CXF_FEASIBILITY_TOL;
    }
    return env->feasibility_tol;
}

/**
 * @brief Get dual optimality tolerance.
 *
 * Retrieves the optimality tolerance used for reduced cost checks
 * in simplex pricing. Returns default value on any error to enable
 * inner-loop usage without error checking overhead.
 *
 * @param env Environment pointer (may be NULL)
 * @return Optimality tolerance, default 1e-6 if env is NULL
 */
double cxf_get_optimality_tol(CxfEnv *env) {
    if (env == NULL) {
        return CXF_OPTIMALITY_TOL;
    }
    return env->optimality_tol;
}

/**
 * @brief Get infinity constant.
 *
 * Returns the finite constant (1e100) used to represent unbounded
 * values throughout ConvexFeld. Using a finite value avoids IEEE 754
 * infinity arithmetic issues (NaN propagation).
 *
 * @return Infinity constant (1e100)
 */
double cxf_get_infinity(void) {
    return CXF_INFINITY;
}
