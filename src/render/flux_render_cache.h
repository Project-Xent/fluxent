/**
 * @file flux_render_cache.h
 * @brief Per-node render state caching and animation tweening.
 *
 * FluxRenderCache maintains D2D resources (brushes) and animation state
 * for each node. This avoids recreating brushes every frame and provides
 * smooth transitions for hover, press, focus, and other state changes.
 *
 * ## Lifecycle
 *
 * 1. Create cache with `flux_render_cache_create(max_entries)`
 * 2. Each frame:
 *    - Call `flux_render_cache_begin_frame()` to advance frame counter
 *    - Use `flux_render_cache_get_or_create()` to access node entries
 *    - Periodically call `flux_render_cache_evict_lru()` to prune stale entries
 * 3. Destroy with `flux_render_cache_destroy()`
 *
 * ## Animation Tweens
 *
 * Each cache entry contains FluxTween and FluxColorTween structs for
 * animating common properties. Renderers update these tweens each frame
 * to interpolate between current and target values.
 *
 * ## Thread Safety
 *
 * Not thread-safe. All access must be from the render thread.
 *
 * ## D2D Brush Caching
 *
 * Use `flux_render_cache_brush()` to get or create solid color brushes.
 * The cache tracks the last RGBA value and only recreates the brush if
 * the color changes.
 */
#ifndef FLUX_RENDER_CACHE_H
#define FLUX_RENDER_CACHE_H

#include "fluxent/flux_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct ID2D1DeviceContext   ID2D1DeviceContext;
typedef struct ID2D1SolidColorBrush ID2D1SolidColorBrush;

/**
 * @brief Single-value animation tween.
 *
 * Interpolates a float value from start_val to target over duration.
 * Set target and duration, then read current each frame.
 */
typedef struct FluxTween {
	float  current;     /**< Current interpolated value (read this) */
	float  target;      /**< Target value to animate toward */
	float  start_val;   /**< Value at animation start */
	double start_time;  /**< Monotonic time when animation began */
	double duration;    /**< Animation duration in seconds */
	bool   active;      /**< Animation is in progress */
	bool   initialized; /**< Tween has been set up at least once */
} FluxTween;

/**
 * @brief Color animation tween (RGBA packed as uint32).
 *
 * Interpolates each channel independently. Uses premultiplied alpha.
 */
typedef struct FluxColorTween {
	uint32_t current_rgba; /**< Current interpolated color */
	uint32_t target_rgba;  /**< Target color */
	uint32_t start_rgba;   /**< Color at animation start */
	double   start_time;
	double   duration;
	bool     active;
	bool     initialized;
} FluxColorTween;

/**
 * @brief Cached render state for a single node.
 *
 * Contains D2D brushes and animation tweens. Entries are keyed by node_id
 * and evicted based on last_frame (LRU policy).
 */
typedef struct FluxCacheEntry {
	uint64_t              node_id;            /**< Node this entry belongs to */
	uint64_t              last_frame;         /**< Frame count when last accessed */

	ID2D1SolidColorBrush *bg_brush;           /**< Background fill brush */
	ID2D1SolidColorBrush *border_brush;       /**< Border stroke brush */
	uint32_t              cached_bg_rgba;     /**< RGBA used to create bg_brush */
	uint32_t              cached_border_rgba; /**< RGBA used to create border_brush */
	float                 cached_corner_radius;
	float                 cached_border_width;

	FluxTween             hover_anim;    /**< 0.0 = not hovered, 1.0 = fully hovered */
	FluxTween             press_anim;    /**< 0.0 = not pressed, 1.0 = fully pressed */
	FluxTween             focus_anim;    /**< 0.0 = not focused, 1.0 = focused */
	FluxTween             check_anim;    /**< Checkbox/Toggle check state */
	FluxTween             slider_anim;   /**< Slider thumb position */
	FluxTween             progress_anim; /**< Progress bar fill */
	FluxTween             scroll_anim_x; /**< Horizontal scroll position */
	FluxTween             scroll_anim_y; /**< Vertical scroll position */
	FluxColorTween        color_anim;    /**< Generic color transition */
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

/**
 * @brief Create a render cache.
 * @param max_entries Initial hash table capacity.
 * @return New cache instance, or NULL on failure.
 */
FluxRenderCache      *flux_render_cache_create(uint32_t max_entries);

/**
 * @brief Destroy a render cache and release all D2D resources.
 * @param cache Cache to destroy (NULL is safe).
 */
void                  flux_render_cache_destroy(FluxRenderCache *cache);

/**
 * @brief Begin a new frame, incrementing the frame counter.
 *
 * Call this once per frame before accessing any cache entries.
 * @param cache Cache instance.
 */
void                  flux_render_cache_begin_frame(FluxRenderCache *cache);

/**
 * @brief Get the current frame number.
 * @param cache Cache instance.
 * @return Current frame count.
 */
uint64_t              flux_render_cache_frame(FluxRenderCache const *cache);

/**
 * @brief Look up a cache entry by node ID.
 * @param cache Cache instance.
 * @param node_id Node ID to find.
 * @return Entry pointer, or NULL if not found.
 */
FluxCacheEntry       *flux_render_cache_get(FluxRenderCache *cache, uint64_t node_id);

/**
 * @brief Get or create a cache entry for a node.
 *
 * Updates last_frame to the current frame. Creates a zero-initialized
 * entry if one doesn't exist.
 *
 * @param cache Cache instance.
 * @param node_id Node ID.
 * @return Entry pointer (never NULL unless allocation fails).
 */
FluxCacheEntry       *flux_render_cache_get_or_create(FluxRenderCache *cache, uint64_t node_id);

/**
 * @brief Remove a cache entry and release its D2D resources.
 * @param cache Cache instance.
 * @param node_id Node ID to remove.
 */
void                  flux_render_cache_remove(FluxRenderCache *cache, uint64_t node_id);

/**
 * @brief Remove all entries and release all D2D resources.
 * @param cache Cache instance.
 */
void                  flux_render_cache_clear(FluxRenderCache *cache);

/**
 * @brief Get the number of entries in the cache.
 * @param cache Cache instance.
 * @return Entry count.
 */
uint32_t              flux_render_cache_count(FluxRenderCache const *cache);

/**
 * @brief Evict entries not accessed in recent frames.
 *
 * Removes entries where (current_frame - last_frame) > age_threshold.
 * Call periodically to prevent unbounded memory growth.
 *
 * @param cache Cache instance.
 * @param age_threshold Maximum frames since last access.
 */
void                  flux_render_cache_evict_lru(FluxRenderCache *cache, uint64_t age_threshold);

/**
 * @brief Get or create a solid color brush.
 *
 * Caches the brush in the specified slot. Only recreates if the color
 * changes. Uses the node_id to access the cache entry.
 *
 * @param cache Cache instance.
 * @param dc D2D device context for brush creation.
 * @param node_id Node ID (for cache entry lookup).
 * @param color Desired brush color.
 * @param cached_rgba Pointer to cached RGBA value (updated on change).
 * @param slot Pointer to brush pointer (updated on change).
 * @return The brush (caller does NOT own it; do not Release).
 */
ID2D1SolidColorBrush *flux_render_cache_brush(FluxRenderBrushRequest const *request);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_RENDER_CACHE_H */
