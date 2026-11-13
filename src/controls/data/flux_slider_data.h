#ifndef FLUX_SLIDER_DATA_H
#define FLUX_SLIDER_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>

typedef enum FluxSliderSnapsTo
{
	FLUX_SLIDER_SNAPS_TO_STEP_VALUES = 0,
	FLUX_SLIDER_SNAPS_TO_TICKS,
} FluxSliderSnapsTo;

typedef XtkTickPlacement FluxTickPlacement;

#define FLUX_TICK_INLINE       XTK_TICK_INLINE
#define FLUX_TICK_TOP_LEFT     XTK_TICK_TOP_LEFT
#define FLUX_TICK_BOTTOM_RIGHT XTK_TICK_BOTTOM_RIGHT
#define FLUX_TICK_OUTSIDE      XTK_TICK_OUTSIDE
#define FLUX_TICK_NONE         XTK_TICK_NONE

typedef struct FluxSliderData {
	float             min_value;
	float             max_value;
	float             current_value;
	float             intermediate_value;
	float             step_frequency;
	float             tick_frequency;
	float             small_change;
	float             large_change;
	FluxSliderSnapsTo snaps_to;
	FluxTickPlacement tick_placement;
	FluxColor         track_color;
	FluxColor         fill_color;
	bool              vertical;
	bool              color_picker;
	bool              is_range;
	float             range_value [2];
	float             range_live [2];
	uint8_t           active_thumb;
	bool              thumb_held;
	FluxColor         thumb_color;
	void              (*on_change)(void *ctx, float value);
	void             *on_change_ctx;
} FluxSliderData;

#endif
