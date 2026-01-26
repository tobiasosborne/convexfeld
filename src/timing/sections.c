/**
 * @file sections.c
 * @brief Timing section functions (M4.2.3)
 *
 * Functions for profiling code sections:
 * - cxf_timing_start: Record start timestamp
 * - cxf_timing_end: Calculate elapsed time and update stats
 * - cxf_timing_update: Accumulate timing statistics
 */

#include "convexfeld/cxf_timing.h"
#include <stddef.h>

/**
 * @brief Record start timestamp for timing measurement.
 *
 * Captures the current high-resolution timestamp to mark the beginning
 * of a timed section. The timestamp can later be compared with an end
 * timestamp to calculate elapsed time.
 *
 * @param timing Pointer to timing state (may be NULL)
 */
void cxf_timing_start(TimingState *timing) {
    if (timing == NULL) {
        return;
    }
    timing->start_time = cxf_get_timestamp();
}

/**
 * @brief Record end timestamp and update section statistics.
 *
 * Calculates elapsed time since the corresponding start call and
 * accumulates statistics for the current timing section.
 *
 * @param timing Pointer to timing state (may be NULL)
 */
void cxf_timing_end(TimingState *timing) {
    if (timing == NULL) {
        return;
    }

    double end_time = cxf_get_timestamp();
    timing->elapsed = end_time - timing->start_time;

    /* Validate section index before updating */
    int section = timing->current_section;
    if (section < 0 || section >= CXF_MAX_TIMING_SECTIONS) {
        return;  /* Invalid section, skip stats update */
    }

    /* Accumulate into section */
    timing->total_time[section] += timing->elapsed;
    timing->operation_count[section]++;
    timing->last_elapsed[section] = timing->elapsed;

    /* Update average */
    if (timing->operation_count[section] > 0) {
        timing->avg_time[section] =
            timing->total_time[section] / timing->operation_count[section];
    }
}

/**
 * @brief Update timing statistics for a specific category.
 *
 * Accumulates the current elapsed time into the specified category's
 * totals, increments the operation count, and recalculates the average.
 *
 * @param timing Pointer to timing state (may be NULL)
 * @param category Category/section index (0 to CXF_MAX_TIMING_SECTIONS-1)
 */
void cxf_timing_update(TimingState *timing, int category) {
    if (timing == NULL) {
        return;
    }

    /* Validate category index */
    if (category < 0 || category >= CXF_MAX_TIMING_SECTIONS) {
        return;
    }

    /* Accumulate into category */
    timing->total_time[category] += timing->elapsed;
    timing->operation_count[category]++;
    timing->last_elapsed[category] = timing->elapsed;

    /* Update average */
    if (timing->operation_count[category] > 0) {
        timing->avg_time[category] =
            timing->total_time[category] / timing->operation_count[category];
    }

    /* Update iteration rate for category 0 (total) */
    if (category == 0 && timing->total_time[0] > 0.0) {
        timing->iteration_rate =
            (double)timing->operation_count[0] / timing->total_time[0];
    }
}
