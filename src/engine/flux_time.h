/**
 * @file flux_time.h
 * @brief High-precision timing utilities using QueryPerformanceCounter.
 *
 * ## Usage
 *
 * ```c
 * int64_t prev = 0;
 * while (running) {
 *     int64_t now = flux_perf_now();
 *     float dt = flux_compute_dt(prev, now);
 *     prev = now;
 *     // animate with dt...
 * }
 * ```
 */
#ifndef FLUX_TIME_H
#define FLUX_TIME_H

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Get the QPC frequency (ticks per second).
 * @return Frequency (cached after first call).
 */
int64_t flux_perf_freq(void);

/**
 * @brief Get the current QPC tick count.
 * @return Current ticks.
 */
int64_t flux_perf_now(void);

/**
 * @brief Convert ticks to seconds.
 * @param ticks QPC tick count.
 * @return Seconds (monotonic time since boot).
 */
double  flux_perf_seconds(int64_t ticks);

/**
 * @brief Compute delta time between two tick values.
 *
 * Clamps result to [0, 0.1] seconds to avoid huge jumps on
 * first frame or after debugger stalls.
 *
 * @param prev Previous frame's tick count (0 = first frame).
 * @param now Current frame's tick count.
 * @return Delta time in seconds.
 */
float   flux_compute_dt(int64_t prev, int64_t now);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_TIME_H */
