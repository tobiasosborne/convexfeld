/**
 * @file threading_stub.c
 * @brief Stub implementations for threading functions (M3.3.1)
 *
 * TDD stub implementations for threading module.
 * Most functions have been extracted to dedicated files:
 * - locks.c: cxf_env_acquire_lock, cxf_leave_critical_section,
 *            cxf_acquire_solve_lock, cxf_release_solve_lock
 * - config.c: cxf_get_threads, cxf_set_thread_count
 * - cpu.c: cxf_get_physical_cores (cxf_get_logical_processors in logging/system.c)
 * - seed.c: cxf_generate_seed
 *
 * This file is kept for any remaining stub functions that may be needed.
 */

#include "convexfeld/cxf_types.h"

/* All functions have been extracted to dedicated files */
