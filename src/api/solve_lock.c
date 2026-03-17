/**
 * @file solve_lock.c
 * @brief Locale safety for optimization (threading_sync.md).
 *
 * Implements cxf_acquire_solve_lock / cxf_release_solve_lock.
 * These save the calling thread's LC_NUMERIC locale, switch to "C"
 * before optimization, and restore the original locale afterward.
 *
 * Despite the "lock" name, no mutex is involved. The name reflects
 * a conceptual locking of the numeric locale to "C" for the solve
 * duration (see threading_sync.md Module-Level Behavioral Notes).
 */

#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_types.h"

#include <locale.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Save current LC_NUMERIC locale and switch to "C".
 *
 * Per threading_sync.md:
 * - If env is NULL, return error.
 * - If env is already in an optimization context, no-op (return OK).
 * - If current locale is already "C", no-op (return OK).
 * - Otherwise: strdup the current locale, store on env->saved_locale,
 *   then setlocale(LC_NUMERIC, "C").
 *
 * @param env Environment controlling the optimization context
 * @return CXF_OK on success, error code on failure
 */
int cxf_acquire_solve_lock(CxfEnv *env) {
    const char *cur;

    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* If already in an optimization context, locale is already "C".
     * The outer call handles save/restore; skip redundant cycle. */
    if (env->optimizing) {
        return CXF_OK;
    }

    /* Query current locale (read-only; does not change it). */
    cur = setlocale(LC_NUMERIC, NULL);
    if (cur == NULL) {
        /* Defensive: should never happen on conforming systems. */
        setlocale(LC_NUMERIC, "C");
        return CXF_OK;
    }

    /* If already "C", no save/restore needed. */
    if (strcmp(cur, "C") == 0) {
        return CXF_OK;
    }

    /* Save current locale via heap copy. */
    env->saved_locale = strdup(cur);
    if (env->saved_locale == NULL) {
        /* OOM: still switch to "C" for safety, but report error. */
        setlocale(LC_NUMERIC, "C");
        return CXF_ERROR_OUT_OF_MEMORY;
    }

    /* Switch to "C" locale for consistent decimal formatting. */
    setlocale(LC_NUMERIC, "C");

    return CXF_OK;
}

/**
 * @brief Restore the saved LC_NUMERIC locale from the environment.
 *
 * Per threading_sync.md:
 * - If env is NULL, return error.
 * - If env->saved_locale is NULL (nothing was saved), no-op.
 * - Otherwise: restore, free, and clear the saved string.
 *
 * @param env Environment to restore
 * @return CXF_OK on success, error code on failure
 */
int cxf_release_solve_lock(CxfEnv *env) {
    if (env == NULL) {
        return CXF_ERROR_NULL_ARGUMENT;
    }

    /* If nothing was saved, acquire determined no change was needed. */
    if (env->saved_locale == NULL) {
        return CXF_OK;
    }

    /* Restore the original locale. */
    setlocale(LC_NUMERIC, env->saved_locale);

    /* Free and clear the saved string to prevent double-free. */
    free(env->saved_locale);
    env->saved_locale = NULL;

    return CXF_OK;
}
