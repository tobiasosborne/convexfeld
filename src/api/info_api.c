/**
 * @file info_api.c
 * @brief Info API implementation - version and error message functions.
 *
 * Provides library version information, error message retrieval,
 * and callback registration (stub).
 */

#include <string.h>
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_env.h"

/* Extern declarations for validation functions */
extern int cxf_checkenv(CxfEnv *env);
extern int cxf_checkmodel(CxfModel *model);

/* Version constants */
#define CXF_VERSION_MAJOR 0
#define CXF_VERSION_MINOR 1
#define CXF_VERSION_PATCH 0

/**
 * @brief Get library version information.
 *
 * Returns the ConvexFeld library version as three integers.
 * All pointers are optional (may be NULL).
 *
 * @param majorP Output major version (may be NULL)
 * @param minorP Output minor version (may be NULL)
 * @param patchP Output patch version (may be NULL)
 */
void cxf_version(int *majorP, int *minorP, int *patchP) {
    if (majorP != NULL) {
        *majorP = CXF_VERSION_MAJOR;
    }
    if (minorP != NULL) {
        *minorP = CXF_VERSION_MINOR;
    }
    if (patchP != NULL) {
        *patchP = CXF_VERSION_PATCH;
    }
}

/* cxf_geterrormsg is implemented in src/error/core.c */

/**
 * @brief Register callback function (STUB).
 *
 * Stub implementation for callback registration.
 * Full implementation deferred until callback infrastructure is complete.
 *
 * @param model Model to attach callback to
 * @param cb Callback function (NULL to disable)
 * @param usrdata User data pointer
 * @return CXF_OK on success, error code otherwise
 */
int cxf_setcallbackfunc(CxfModel *model,
                        int (*cb)(CxfModel*, void*, int, void*),
                        void *usrdata) {
    (void)usrdata;  /* Unused in stub */

    if (model == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* If callback is NULL, that means disable callback */
    if (cb == NULL) {
        return CXF_OK;
    }

    /* Stub: Just validate and return success.
     * Full implementation will store cb and usrdata in model.
     * Note: Model structure does not yet have callback_func and
     * callback_data fields. */
    return CXF_OK;
}
