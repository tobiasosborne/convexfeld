/**
 * @file clear_pending.c
 * @brief Deep free of pending modifications buffer (state_cleanup_buffers.md)
 *
 * Performs a complete teardown of the pending modifications buffer,
 * freeing all sub-structures and setting the caller's pointer to NULL.
 *
 * NOTE: The PendingBuffer struct does not exist yet. The model currently
 * stores pending_buffer as void*. This stub frees the void* and nulls
 * the pointer. When a typed PendingBuffer is added, this function must
 * be extended to deep-free all sub-structures per the spec.
 *
 * Spec: docs/specs-v2/specs/modules/state_cleanup_buffers.md
 *       Function: cxf_clear_pending_buffer
 */

#include "convexfeld/cxf_env.h"
#include "memory_internal.h"

#include <stddef.h>

/**
 * @brief Deep free the pending modifications buffer.
 *
 * Per spec: takes env (memory context) and a pointer-to-pointer so we
 * can null the caller's handle after freeing.
 *
 * Current implementation: the model has only a void* pending_buffer
 * with no typed PendingBuffer sub-structures. We free it and null.
 *
 * When PendingBuffer gains sub-structures (pending linear constraints,
 * variable additions, coefficient changes, etc.), each must be
 * iterated and deep-freed here before freeing the buffer itself.
 *
 * @param env  Environment providing memory context (unused until
 *             env-scoped allocation is implemented; must be non-NULL
 *             per spec precondition, but we tolerate NULL gracefully)
 * @param bufferPtr  Pointer to the model's pending buffer pointer.
 *                   NULL is a no-op. *bufferPtr may also be NULL.
 */
void cxf_clear_pending_buffer(CxfEnv *env, void **bufferPtr) {
    (void)env;  /* env reserved for future env-scoped allocation */

    if (bufferPtr == NULL) {
        return;
    }

    if (*bufferPtr == NULL) {
        return;
    }

    /*
     * TODO: When PendingBuffer gains typed sub-structures, iterate and
     * deep-free each category here:
     *   - pending linear constraint changes
     *   - pending range constraint changes
     *   - pending variable additions
     *   - pending SOS constraint data
     *   - pending indicator constraint data
     *   - pending general constraint data
     *   - pending quadratic constraint data
     *   - pending quadratic objective terms
     *   - pending cone constraint data
     *   - deletion masks (vars, constrs, quadratic)
     *   - name change arrays
     *   - variable type change arrays
     */

    cxf_free(*bufferPtr);
    *bufferPtr = NULL;
}
