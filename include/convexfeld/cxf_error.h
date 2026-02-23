/**
 * @file cxf_error.h
 * @brief 4-function error reporting model (P3.09).
 *
 * Two axes: {via Environment, via Model} x {custom message, predefined message}.
 * Error code is always written. Message follows buffer-empty/overwrite rules.
 */

#ifndef CXF_ERROR_H
#define CXF_ERROR_H

#include "cxf_types.h"

/* Forward declarations */
struct CxfEnv;
struct CxfModel;

/**
 * @brief Report error with custom message via Environment.
 *
 * Always writes error_code. Message written if overwrite==1 (and not locked)
 * or if error_buffer is empty.
 *
 * @param env        Environment (must not be NULL)
 * @param error_code Error code (CxfErrorCode)
 * @param overwrite  1=overwrite existing message, 0=empty-buffer-only
 * @param format     printf-style format string
 * @param ...        Format arguments
 */
void cxf_error_env(struct CxfEnv *env, int error_code, int overwrite,
                   const char *format, ...);

/**
 * @brief Report error with custom message via Model.
 *
 * Resolves to model->env and calls cxf_error_env.
 *
 * @param model      Model (must not be NULL, model->env must not be NULL)
 * @param error_code Error code (CxfErrorCode)
 * @param overwrite  1=overwrite existing message, 0=empty-buffer-only
 * @param format     printf-style format string
 * @param ...        Format arguments
 */
void cxf_error_model(struct CxfModel *model, int error_code, int overwrite,
                     const char *format, ...);

/**
 * @brief Report error with predefined message via Model.
 *
 * Looks up error_code in the message table. Uses empty-buffer-only rule
 * (except OOM always overwrites).
 *
 * @param model      Model (must not be NULL)
 * @param error_code Error code (CxfErrorCode)
 */
void cxf_set_error_message(struct CxfModel *model, int error_code);

/**
 * @brief Report error with predefined message via Environment.
 *
 * Looks up error_code in the message table. Uses empty-buffer-only rule
 * (except OOM always overwrites).
 *
 * @param env        Environment (must not be NULL)
 * @param error_code Error code (CxfErrorCode)
 */
void cxf_env_set_status(struct CxfEnv *env, int error_code);

/**
 * @brief Look up predefined message for an error code.
 *
 * @param error_code Error code (CxfErrorCode)
 * @return Static string for known codes, "Unknown error" otherwise
 */
const char *cxf_error_message(int error_code);

#endif /* CXF_ERROR_H */
