/**
 * @file cxf_timing.h
 * @brief Timing state structure and function declarations.
 *
 * Provides timing utilities for profiling solver operations.
 */

#ifndef CXF_TIMING_H
#define CXF_TIMING_H

/* Maximum timing sections */
#define CXF_MAX_TIMING_SECTIONS 8

/**
 * @brief Timing state structure for profiling.
 *
 * Tracks timing statistics across multiple categories/sections.
 * Each section can independently accumulate time and operation counts.
 */
typedef struct TimingState {
    double start_time;        /**< Start timestamp (seconds) */
    double elapsed;           /**< Last computed elapsed time */
    int current_section;      /**< Active timing section (0-7) */

    /* Per-section statistics */
    double total_time[CXF_MAX_TIMING_SECTIONS];     /**< Accumulated time */
    int operation_count[CXF_MAX_TIMING_SECTIONS];   /**< Operation count */
    double last_elapsed[CXF_MAX_TIMING_SECTIONS];   /**< Last elapsed time */
    double avg_time[CXF_MAX_TIMING_SECTIONS];       /**< Average time */

    double iteration_rate;    /**< Overall iterations per second */
} TimingState;

/**
 * @brief Get current high-resolution timestamp.
 * @return Current timestamp in seconds
 */
double cxf_get_timestamp(void);

/**
 * @brief Record start timestamp for timing measurement.
 * @param timing Pointer to timing state (may be NULL)
 */
void cxf_timing_start(TimingState *timing);

/**
 * @brief Record end timestamp and update section statistics.
 * @param timing Pointer to timing state (may be NULL)
 */
void cxf_timing_end(TimingState *timing);

/**
 * @brief Update timing statistics for a specific category.
 * @param timing Pointer to timing state (may be NULL)
 * @param category Category/section index (0 to CXF_MAX_TIMING_SECTIONS-1)
 */
void cxf_timing_update(TimingState *timing, int category);

#endif /* CXF_TIMING_H */
