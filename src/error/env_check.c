/**
 * @file env_check.c
 * @brief Environment validation functions (M3.1.4)
 *
 * Validates that environment pointers are properly initialized
 * before proceeding with API operations.
 */

#include "convexfeld/cxf_env.h"

/**
 * @brief Validate environment pointer and state.
 *
 * Performs fast validation of the environment:
 * 1. Checks for NULL pointer
 * 2. Validates magic number (confirms proper initialization)
 *
 * This is a critical guard function called at the entry point of
 * almost every API function.
 *
 * @param env Environment pointer to validate
 * @return CXF_OK if valid, error code otherwise
 */
int cxf_checkenv(CxfEnv *env) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }
    if (env->magic != CXF_ENV_MAGIC) {
        return CXF_ERROR_INVALID_ARGUMENT;
    }
    return CXF_OK;
}
