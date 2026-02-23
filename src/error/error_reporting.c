/**
 * @file error_reporting.c
 * @brief 4-function error reporting model (P3.09).
 *
 * Implements the 2x2 error reporting matrix:
 *   cxf_error_env       — custom message via Environment
 *   cxf_error_model     — custom message via Model
 *   cxf_set_error_message — predefined message via Model
 *   cxf_env_set_status  — predefined message via Environment
 *
 * Spec: docs/specs-v2/specs/integration/error_propagation.md
 */

#include "convexfeld/cxf_error.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/*---------------------------------------------------------------------------
 * Predefined error message table
 *---------------------------------------------------------------------------*/

const char *cxf_error_message(int error_code) {
    switch (error_code) {
    case 10001: return "Out of memory";
    case 10002: return "NULL argument";
    case 10003: return "Invalid argument";
    case 10004: return "Unknown attribute";
    case 10005: return "Data not available";
    case 10006: return "Index out of range";
    case 10007: return "Unknown parameter";
    case 10008: return "Value out of range";
    case 10009: return "No license";
    case 10010: return "Size limit exceeded";
    case 10011: return "Callback error";
    case 10012: return "File read error";
    case 10013: return "File write error";
    case 10014: return "Numerical error";
    case 10015: return "IIS: model not infeasible";
    case 10016: return "Not valid for MIP models";
    case 10017: return "Optimization in progress";
    case 10018: return "Duplicate indices";
    case 10019: return "Node file I/O error";
    case 10020: return "Q matrix not positive semi-definite";
    case 10021: return "Non-convex QCP equality constraint";
    case 10022: return "Network error";
    case 10023: return "Job rejected by server";
    case 10024: return "Feature not supported";
    case 10025: return "Matrix exceeds 2 billion nonzeros";
    case 10026: return "Invalid piecewise-linear objective";
    case 10027: return "UpdateMode modification attempted";
    case 10028: return "Cloud computing error";
    case 10029: return "Model modification made model invalid";
    case 10030: return "Client-server worker error";
    case 10031: return "Multi-type model tuning attempted";
    case 10032: return "Authentication/permission failure";
    case 20001: return "Element not in model";
    case 20002: return "Failed to create model";
    case 20003: return "Internal error";
    default:    return "Unknown error";
    }
}

/*---------------------------------------------------------------------------
 * Internal: write message to env error_buffer
 *---------------------------------------------------------------------------*/

static void write_error(CxfEnv *env, int error_code, int overwrite,
                        const char *msg) {
    /* Error code is ALWAYS written */
    env->error_code = error_code;

    /* OOM always overwrites (critical diagnostic override) */
    if (error_code == CXF_ERROR_OUT_OF_MEMORY) {
        snprintf(env->error_buffer, sizeof(env->error_buffer), "%s", msg);
        return;
    }

    /* Overwrite mode: write if flag set and buffer not locked */
    if (overwrite && !env->error_buf_locked) {
        snprintf(env->error_buffer, sizeof(env->error_buffer), "%s", msg);
        return;
    }

    /* Empty-buffer-only: write only if buffer is empty */
    if (env->error_buffer[0] == '\0') {
        snprintf(env->error_buffer, sizeof(env->error_buffer), "%s", msg);
    }
}

/*---------------------------------------------------------------------------
 * Public API
 *---------------------------------------------------------------------------*/

void cxf_error_env(CxfEnv *env, int error_code, int overwrite,
                   const char *format, ...) {
    if (!env) return;
    char buf[512];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    write_error(env, error_code, overwrite, buf);
}

void cxf_error_model(CxfModel *model, int error_code, int overwrite,
                     const char *format, ...) {
    if (!model || !model->env) return;
    char buf[512];
    va_list ap;
    va_start(ap, format);
    vsnprintf(buf, sizeof(buf), format, ap);
    va_end(ap);
    write_error(model->env, error_code, overwrite, buf);
}

void cxf_set_error_message(CxfModel *model, int error_code) {
    if (!model || !model->env) return;
    write_error(model->env, error_code, 0, cxf_error_message(error_code));
}

void cxf_env_set_status(CxfEnv *env, int error_code) {
    if (!env) return;
    write_error(env, error_code, 0, cxf_error_message(error_code));
}
