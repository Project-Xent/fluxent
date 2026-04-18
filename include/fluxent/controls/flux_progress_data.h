/**
 * @file flux_progress_data.h
 * @brief Data structures for FluxProgressBar and FluxProgressRing controls.
 */
#ifndef FLUX_PROGRESS_DATA_H
#define FLUX_PROGRESS_DATA_H

#include "../flux_types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════════
   FluxProgressData — Control-specific data for FluxProgressBar
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for a linear progress bar.
 *
 * When indeterminate is true, the bar shows a looping animation
 * rather than a specific percentage.
 */
typedef struct FluxProgressData {
	float     value;         /**< Current progress value */
	float     max_value;     /**< Maximum value (100% mark) */
	FluxColor fill_color;    /**< Filled portion color */
	FluxColor track_color;   /**< Background track color */
	bool      indeterminate; /**< Show indeterminate animation */
} FluxProgressData;

/* ═══════════════════════════════════════════════════════════════════════
   FluxProgressRingData — Control-specific data for FluxProgressRing
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for a circular progress ring.
 *
 * Like FluxProgressData but rendered as a circle/arc.
 */
typedef struct FluxProgressRingData {
	float     value;         /**< Current progress value */
	float     max_value;     /**< Maximum value (100% mark) */
	FluxColor fill_color;    /**< Arc fill color */
	FluxColor track_color;   /**< Background ring color */
	bool      indeterminate; /**< Show indeterminate animation */
} FluxProgressRingData;

#ifdef __cplusplus
}
#endif

#endif /* FLUX_PROGRESS_DATA_H */
