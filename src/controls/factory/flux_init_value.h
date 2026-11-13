#ifndef FLUX_INIT_VALUE_H
#define FLUX_INIT_VALUE_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxSliderDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	float      min;
	float      max;
	float      value;
	void       (*on_change)(void *, float);
	void      *userdata;
} FluxSliderDesc;

typedef struct FluxProgressDesc {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId parent;
	float      value;
	float      max_value;
	float      min_value;
} FluxProgressDesc;

XentNodeId flux_slider_init(FluxSliderDesc const *info);

XentNodeId flux_progress_init(FluxProgressDesc const *info);

XentNodeId flux_progressring_init(FluxProgressDesc const *info);

typedef struct FluxRatingDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	double      value;
	int         max_rating;
	double      placeholder_value;
	int         initial_set_value;
	bool        is_clear_enabled;
	bool        is_read_only;
	char const *caption;
	double      item_spacing;
	double      star_size;
	uint32_t    set_glyph;
	uint32_t    unset_glyph;
	void        (*on_change)(void *ctx, double value, int stars);
	void       *userdata;
} FluxRatingDesc;

XentNodeId flux_rating_init(FluxRatingDesc const *info);
void       flux_rating_retarget(FluxStore *store, XentNodeId id);

void       flux_rating_set_value(FluxStore *store, XentNodeId id, double value);

void       flux_rating_set_max(FluxStore *store, XentNodeId id, int max_rating);

void       flux_rating_set_caption(FluxStore *store, XentNodeId id, char const *caption);

void       flux_rating_setreadonly(FluxStore *store, XentNodeId id, bool read_only);
void       flux_rating_setbehavior(
  FluxStore *store, XentNodeId id, double placeholder, int initial_value, bool clear_enabled
);

typedef struct FluxScrollBarDesc {
	XentCtx                 *ctx;
	FluxStore               *store;
	XentNodeId               parent;
	FluxScrollBarOrientation orientation;
	double                   minimum;
	double                   maximum;
	double                   value;
	double                   viewport_size;
	void                     (*on_scroll)(void *ctx, double value, FluxScrollEventType type);
	void                    *userdata;
} FluxScrollBarDesc;

XentNodeId flux_scroll_bar_init(FluxScrollBarDesc const *info);
void       flux_scroll_bar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId id);

void       flux_scroll_bar_setvalue(FluxStore *store, XentNodeId id, double value);
void       flux_scroll_bar_setminimum(FluxStore *store, XentNodeId id, double minimum);
void       flux_scroll_bar_setmaximum(FluxStore *store, XentNodeId id, double maximum);
void       flux_scroll_bar_setviewport(FluxStore *store, XentNodeId id, double viewport);
void       flux_scroll_bar_setorientation(XentCtx *ctx, FluxStore *store, XentNodeId id, uint32_t orientation);
void       flux_scroll_bar_setindicator(FluxStore *store, XentNodeId id, uint32_t indicator);
void       flux_scroll_bar_setsmallchange(FluxStore *store, XentNodeId id, float change);
void       flux_scroll_bar_setlargechange(FluxStore *store, XentNodeId id, float change);
void       flux_scroll_bar_setenabled(FluxStore *store, XentNodeId id, bool enabled);

#endif
