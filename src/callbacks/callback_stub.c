/**
 * @file callback_stub.c
 * @brief Stub implementations for callback functions (M5.2.3-M5.2.5)
 *
 * These stubs allow TDD tests to link. Functions will be replaced
 * with full implementations in subsequent milestones.
 *
 * Note: cxf_check_terminate, cxf_terminate, cxf_clear_terminate are
 * already implemented in src/error/terminate.c (M3.1.6).
 */

#include "convexfeld/cxf_types.h"
#include "convexfeld/cxf_env.h"
#include "convexfeld/cxf_model.h"
#include "convexfeld/cxf_callback.h"

/*============================================================================
 * M5.2.3: cxf_init_callback_struct, cxf_reset_callback_state
 * NOTE: Moved to src/callbacks/init.c
 *===========================================================================*/

/*============================================================================
 * M5.2.5: Termination functions (callback-specific)
 * NOTE: Moved to src/callbacks/terminate.c
 *===========================================================================*/

/*============================================================================
 * M5.2.4: Pre/post optimize callbacks
 * NOTE: Moved to src/callbacks/invoke.c
 *===========================================================================*/
