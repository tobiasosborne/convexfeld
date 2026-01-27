/**
 * @file convexfeld.h
 * @brief ConvexFeld LP Solver - Public API Header
 *
 * This is the main header file for the ConvexFeld LP solver.
 * Include this file to access all public API functions.
 *
 * @code
 * #include <convexfeld/convexfeld.h>
 *
 * int main(void) {
 *     CxfEnv *env = NULL;
 *     CxfModel *model = NULL;
 *
 *     cxf_loadenv(&env, NULL);
 *     cxf_newmodel(env, &model, "mymodel");
 *
 *     // Add variables and constraints...
 *
 *     cxf_optimize(model);
 *
 *     cxf_freemodel(model);
 *     cxf_freeenv(env);
 *     return 0;
 * }
 * @endcode
 */

#ifndef CONVEXFELD_H
#define CONVEXFELD_H

/*******************************************************************************
 * Core Types and Constants
 ******************************************************************************/

#include "cxf_types.h"

/*******************************************************************************
 * Data Structures
 ******************************************************************************/

#include "cxf_env.h"
#include "cxf_model.h"
#include "cxf_matrix.h"
#include "cxf_solver.h"
#include "cxf_basis.h"
#include "cxf_pricing.h"
#include "cxf_callback.h"
#include "cxf_utilities.h"

/*******************************************************************************
 * Version Information
 ******************************************************************************/

#define CXF_VERSION_MAJOR 0
#define CXF_VERSION_MINOR 1
#define CXF_VERSION_PATCH 0
#define CXF_VERSION_STRING "0.1.0"

/**
 * @brief Get version information.
 * @param majorP Output major version (may be NULL)
 * @param minorP Output minor version (may be NULL)
 * @param patchP Output patch version (may be NULL)
 */
void cxf_version(int *majorP, int *minorP, int *patchP);

/*******************************************************************************
 * Error Handling
 ******************************************************************************/

/**
 * @brief Get the last error message.
 * @param env Environment (may be NULL for generic message)
 * @return Error message string
 */
const char *cxf_geterrormsg(CxfEnv *env);

#endif /* CONVEXFELD_H */
