/**
 * @file flux_slider_data.h
 * @brief Data structure for FluxSlider control (WinUI 3 Slider semantics).
 */
#ifndef FLUX_SLIDER_DATA_H
#define FLUX_SLIDER_DATA_H

#include "../flux_types.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief What the value snaps to while dragging (WinUI SliderSnapsTo). */
typedef enum FluxSliderSnapsTo
{
	FLUX_SLIDER_SNAPS_TO_STEP_VALUES = 0, /**< Snap to multiples of step_frequency (WinUI default). */
	FLUX_SLIDER_SNAPS_TO_TICKS,           /**< Snap to multiples of tick_frequency. */
} FluxSliderSnapsTo;

/** @brief Where tick marks render (WinUI TickPlacement; zero = the WinUI default). */
typedef enum FluxTickPlacement
{
	FLUX_TICK_INLINE = 0,   /**< On the track itself (WinUI default). */
	FLUX_TICK_TOP_LEFT,     /**< Above a horizontal slider / left of a vertical one. */
	FLUX_TICK_BOTTOM_RIGHT, /**< Below a horizontal slider / right of a vertical one. */
	FLUX_TICK_OUTSIDE,      /**< Both sides. */
	FLUX_TICK_NONE,         /**< Never draw ticks. */
} FluxTickPlacement;

/**
 * @brief Configuration for a horizontal slider, mirroring WinUI Slider.
 *
 * current_value (WinUI Value) always sits on a snap multiple; while the thumb
 * is held, intermediate_value tracks the pointer continuously and the thumb is
 * drawn from it, snapping back to current_value on release (WinUI
 * IntermediateValue). Tick marks render only when tick_frequency > 0.
 */
typedef struct FluxSliderData {
	float             min_value;          /**< Minimum slider value. */
	float             max_value;          /**< Maximum slider value (WinUI default 100). */
	float             current_value;      /**< WinUI Value: snapped to step/tick multiples. */
	float             intermediate_value; /**< WinUI IntermediateValue: thumb position while dragging. */
	float             step_frequency;     /**< Snap grid for SNAPS_TO_STEP_VALUES (WinUI default 1). */
	float             tick_frequency;     /**< Tick mark interval; 0 = no ticks (WinUI default). */
	float             small_change;       /**< Arrow-key delta (WinUI default 1). */
	float             large_change;       /**< PageUp/Down delta (WinUI default 10). */
	FluxSliderSnapsTo snaps_to;
	FluxTickPlacement tick_placement;
	FluxColor         track_color;                          /**< Background track color */
	FluxColor         fill_color;                           /**< Filled portion color */
	FluxColor         thumb_color;                          /**< Thumb (handle) color */
	void              (*on_change)(void *ctx, float value); /**< Invoked when current_value changes. */
	void             *on_change_ctx;
} FluxSliderData;

#ifdef __cplusplus
}
#endif

#endif
