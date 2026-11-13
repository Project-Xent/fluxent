#ifndef FLUX_SCROLL_BAR_DATA_H
#define FLUX_SCROLL_BAR_DATA_H

#include <stdbool.h>
#include <stdint.h>

typedef enum FluxScrollBarOrientation
{
	FLUX_SCROLLBAR_VERTICAL   = 0,
	FLUX_SCROLLBAR_HORIZONTAL = 1,
} FluxScrollBarOrientation;

typedef enum FluxScrollBarIndicator
{
	FLUX_SCROLLBAR_INDICATOR_NONE = 0,
	FLUX_SCROLLBAR_INDICATOR_TOUCH,
	FLUX_SCROLLBAR_INDICATOR_MOUSE,
} FluxScrollBarIndicator;

typedef enum FluxScrollEventType
{
	FLUX_SCROLL_EVENT_SMALL_DECREMENT = 0,
	FLUX_SCROLL_EVENT_SMALL_INCREMENT,
	FLUX_SCROLL_EVENT_LARGE_DECREMENT,
	FLUX_SCROLL_EVENT_LARGE_INCREMENT,
	FLUX_SCROLL_EVENT_THUMB_POSITION,
	FLUX_SCROLL_EVENT_THUMB_TRACK,
	FLUX_SCROLL_EVENT_FIRST,
	FLUX_SCROLL_EVENT_LAST,
	FLUX_SCROLL_EVENT_END_SCROLL,
} FluxScrollEventType;

typedef struct FluxScrollBarData {
	double                   value;
	double                   uncoerced_value;
	double                   minimum;
	double                   maximum;
	double                   viewport_size;
	double                   small_change;
	double                   large_change;
	FluxScrollBarOrientation orientation;
	FluxScrollBarIndicator   indicator_mode;
	bool                     enabled;

	bool                     dragging;
	uint8_t                  dec_pressed;
	uint8_t                  inc_pressed;
	uint8_t                  large_dec_pressed;
	uint8_t                  large_inc_pressed;

	void                     (*on_scroll)(void *ctx, double value, FluxScrollEventType type);
	void                    *on_scroll_ctx;
	void                     (*on_value_change)(void *ctx, double value);
	void                    *on_value_change_ctx;
} FluxScrollBarData;

#endif
