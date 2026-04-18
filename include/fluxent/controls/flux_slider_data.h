/**
 * @file flux_slider_data.h
 * @brief Data structure for FluxSlider control.
 */
#ifndef FLUX_SLIDER_DATA_H
#define FLUX_SLIDER_DATA_H

#include "../flux_types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════════
   FluxSliderData — Control-specific data for FluxSlider
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for a horizontal/vertical slider.
 *
 * The slider value ranges from min_value to max_value, changing in
 * increments of step. on_change is called when the user drags the thumb.
 */
typedef struct FluxSliderData {
	float     min_value;     /**< Minimum slider value */
	float     max_value;     /**< Maximum slider value */
	float     current_value; /**< Current position */
	float     step;          /**< Step increment (0 = continuous) */
	FluxColor track_color;   /**< Background track color */
	FluxColor fill_color;    /**< Filled portion color */
	FluxColor thumb_color;   /**< Thumb (handle) color */
	bool      enabled;       /**< Is slider interactive? */
	void      (*on_change)(void *ctx, float value);
	void     *on_change_ctx;
} FluxSliderData;

#ifdef __cplusplus
}
#endif

#endif /* FLUX_SLIDER_DATA_H */
