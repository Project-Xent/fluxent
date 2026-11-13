#ifndef FLUX_RENDER_CACHE_H
#define FLUX_RENDER_CACHE_H

#include "fluxent/flux_types.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct ID2D1DeviceContext   ID2D1DeviceContext;
typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;

typedef struct FluxTween {
	float  current;
	float  target;
	float  start_val;
	double start_time;
	double duration;
	bool   active;
	bool   initialized;
	float  (*easing)(float);
} FluxTween;

typedef struct FluxColorTween {
	uint32_t current_rgba;
	uint32_t target_rgba;
	uint32_t start_rgba;
	double   start_time;
	double   duration;
	bool     active;
	bool     initialized;
	float    (*easing)(float);
} FluxColorTween;

typedef struct FluxVisualState {
	uint32_t state;
	uint32_t prev;
	double   since;
	float    aux [2];
	bool     initialized;
} FluxVisualState;

typedef struct FluxCacheEntry {
	uint64_t              node_id;
	uint64_t              last_frame;

	ID2D1SolidColorBrush *bg_brush;
	ID2D1SolidColorBrush *border_brush;
	uint32_t              cached_bg_rgba;
	uint32_t              cached_border_rgba;
	float                 cached_corner_radius;
	float                 cached_border_width;

	FluxTween             hover_anim;
	FluxTween             press_anim;
	FluxTween             focus_anim;
	FluxTween             check_anim;
	FluxTween             slider_anim;
	FluxTween             progress_anim;
	FluxTween             scroll_anim_x;
	FluxTween             scroll_anim_y;
	FluxColorTween        color_anim;
	FluxVisualState       vstate;

	double                caret_blink_epoch;
	float                 caret_blink_index;
	bool                  caret_blink_initialized;
} FluxCacheEntry;

typedef struct FluxRenderCache FluxRenderCache;

typedef struct FluxRenderBrushRequest {
	FluxRenderCache       *cache;
	ID2D1DeviceContext    *dc;
	uint64_t               node_id;
	FluxColor              color;
	uint32_t              *cached_rgba;
	ID2D1SolidColorBrush **slot;
} FluxRenderBrushRequest;

FluxRenderCache *flux_render_cache_create(uint32_t max_entries);

void             flux_render_cache_destroy(FluxRenderCache *cache);

void             flux_rcache_begin(FluxRenderCache *cache);

FluxCacheEntry  *flux_render_cache_get(FluxRenderCache *cache, uint64_t node_id);

FluxCacheEntry  *flux_rcache_ensure(FluxRenderCache *cache, uint64_t node_id);

void             flux_render_cache_remove(FluxRenderCache *cache, uint64_t node_id);

void             flux_rcache_evict(FluxRenderCache *cache, uint64_t age_threshold);

#endif
