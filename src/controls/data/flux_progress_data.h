/**
 * @file flux_progress_data.h
 * @brief Data structures for FluxProgressBar and FluxProgressRing controls.
 */
#ifndef FLUX_PROGRESS_DATA_H
#define FLUX_PROGRESS_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>

/**
 * @brief Configuration for a linear progress bar.
 *
 * When indeterminate is true, the bar shows a looping animation
 * rather than a specific percentage.
 */
typedef struct FluxProgressData {
	float     value;         /**< Current progress value */
	float     min_value;     /**< Minimum value (0% mark) */
	float     max_value;     /**< Maximum value (100% mark) */
	FluxColor fill_color;    /**< Filled portion color */
	FluxColor track_color;   /**< Background track color */
	bool      indeterminate; /**< Show indeterminate animation */
} FluxProgressData;

/**
 * @brief Configuration for a circular progress ring.
 *
 * Like FluxProgressData but rendered as a circle/arc.
 */
typedef struct FluxProgressRingData {
	float     value;         /**< Current progress value */
	float     min_value;     /**< Minimum value (0% mark) */
	float     max_value;     /**< Maximum value (100% mark) */
	FluxColor fill_color;    /**< Arc fill color */
	FluxColor track_color;   /**< Background ring color */
	bool      indeterminate; /**< Show indeterminate animation */
} FluxProgressRingData;


#endif
