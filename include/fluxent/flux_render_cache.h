#ifndef FLUX_RENDER_CACHE_H
#define FLUX_RENDER_CACHE_H

#include "flux_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ID2D1DeviceContext ID2D1DeviceContext;
typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;

typedef struct FluxTween {
    float  current;
    float  target;
    float  start_val;
    double start_time;
    double duration;
    bool   active;
    bool   initialized;
} FluxTween;

typedef struct FluxColorTween {
    uint32_t current_rgba;
    uint32_t target_rgba;
    uint32_t start_rgba;
    double   start_time;
    double   duration;
    bool     active;
    bool     initialized;
} FluxColorTween;

typedef struct FluxCacheEntry {
    uint64_t            node_id;
    uint64_t            last_frame;
    ID2D1SolidColorBrush *bg_brush;
    ID2D1SolidColorBrush *border_brush;
    uint32_t            cached_bg_rgba;
    uint32_t            cached_border_rgba;
    float               cached_corner_radius;
    float               cached_border_width;
    FluxTween           hover_anim;
    FluxTween           press_anim;
    FluxTween           focus_anim;
    FluxTween           check_anim;
    FluxTween           slider_anim;
    FluxColorTween      color_anim;
} FluxCacheEntry;

typedef struct FluxRenderCache FluxRenderCache;

FluxRenderCache    *flux_render_cache_create(uint32_t max_entries);
void                flux_render_cache_destroy(FluxRenderCache *cache);

void                flux_render_cache_begin_frame(FluxRenderCache *cache);
uint64_t            flux_render_cache_frame(const FluxRenderCache *cache);

FluxCacheEntry     *flux_render_cache_get(FluxRenderCache *cache, uint64_t node_id);
FluxCacheEntry     *flux_render_cache_get_or_create(FluxRenderCache *cache, uint64_t node_id);
void                flux_render_cache_remove(FluxRenderCache *cache, uint64_t node_id);
void                flux_render_cache_clear(FluxRenderCache *cache);
uint32_t            flux_render_cache_count(const FluxRenderCache *cache);

void                flux_render_cache_evict_lru(FluxRenderCache *cache, uint64_t age_threshold);

ID2D1SolidColorBrush *flux_render_cache_brush(FluxRenderCache *cache,
                                               ID2D1DeviceContext *dc,
                                               uint64_t node_id,
                                               FluxColor color,
                                               uint32_t *cached_rgba,
                                               ID2D1SolidColorBrush **slot);

#ifdef __cplusplus
}
#endif

#endif