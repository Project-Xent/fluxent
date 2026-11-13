#ifndef FLUX_SLIDER_H
#define FLUX_SLIDER_H

#include "controls/data/flux_slider_data.h"
#include "fluxent/flux_node_store.h"

typedef struct FluxSliderInputData {
	FluxSliderData base;
	XentCtx       *ctx;
	FluxStore     *store;
	XentNodeId     node;
	float          drag_origin;
	float          drag_value;
	bool           drag_from_thumb;
	bool           tip_keyboard;
} FluxSliderInputData;

void flux_slider_retarget(XentCtx *ctx, FluxStore *store, XentNodeId dst);

void flux_slider_setvertical(FluxStore *store, XentNodeId id, bool vertical);

void flux_slider_setrange(FluxStore *store, XentNodeId id, bool is_range);

void flux_slider_setthumb(FluxStore *store, XentNodeId id, int thumb, float value);

bool flux_slider_thumb(FluxStore *store, XentNodeId id, int thumb, float *value, float *min, float *max);

bool flux_slider_userthumb(FluxStore *store, XentNodeId id, int thumb, float value);

#endif
