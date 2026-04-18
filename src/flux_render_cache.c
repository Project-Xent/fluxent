#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include "flux_render_cache.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FLUX_RC_EMPTY    0
#define FLUX_RC_OCCUPIED 1
#define FLUX_RC_DELETED  2

typedef struct FluxCacheSlot {
	uint8_t        tag;
	uint64_t       key;
	FluxCacheEntry entry;
} FluxCacheSlot;

struct FluxRenderCache {
	FluxCacheSlot *slots;
	uint32_t       capacity;
	uint32_t       count;
	uint32_t       tombstones;
	uint32_t       max_entries;
	uint64_t       frame;
};

static uint32_t rc_hash(uint64_t key, uint32_t cap) {
	uint64_t h  = key;
	h          ^= h >> 33;
	h          *= 0xff51afd7ed558ccdull;
	h          ^= h >> 33;
	h          *= 0xc4ceb9fe1a85ec53ull;
	h          ^= h >> 33;
	return ( uint32_t ) (h & ( uint64_t ) (cap - 1));
}

static bool rc_needs_grow(FluxRenderCache const *cache) {
	return (cache->count + cache->tombstones) * 4 >= cache->capacity * 3;
}

static void release_entry_resources(FluxCacheEntry *e) {
		if (e->bg_brush) {
			ID2D1SolidColorBrush_Release(e->bg_brush);
			e->bg_brush = NULL;
		}
		if (e->border_brush) {
			ID2D1SolidColorBrush_Release(e->border_brush);
			e->border_brush = NULL;
		}
}

static bool rc_grow(FluxRenderCache *cache) {
	uint32_t new_cap = cache->capacity * 2;
	if (new_cap < 64) new_cap = 64;

	FluxCacheSlot *new_slots = ( FluxCacheSlot * ) calloc(new_cap, sizeof(FluxCacheSlot));
	if (!new_slots) return false;

		for (uint32_t i = 0; i < cache->capacity; i++) {
			if (cache->slots [i].tag != FLUX_RC_OCCUPIED) continue;
			uint32_t idx = rc_hash(cache->slots [i].key, new_cap);
				for (;;) {
						if (new_slots [idx].tag == FLUX_RC_EMPTY) {
							new_slots [idx].tag   = FLUX_RC_OCCUPIED;
							new_slots [idx].key   = cache->slots [i].key;
							new_slots [idx].entry = cache->slots [i].entry;
							break;
						}
					idx = (idx + 1) & (new_cap - 1);
				}
		}

	free(cache->slots);
	cache->slots      = new_slots;
	cache->capacity   = new_cap;
	cache->tombstones = 0;
	return true;
}

static FluxCacheSlot *rc_find(FluxRenderCache *cache, uint64_t key) {
	uint32_t idx = rc_hash(key, cache->capacity);
		for (;;) {
			FluxCacheSlot *s = &cache->slots [idx];
			if (s->tag == FLUX_RC_EMPTY) return NULL;
			if (s->tag == FLUX_RC_OCCUPIED && s->key == key) return s;
			idx = (idx + 1) & (cache->capacity - 1);
		}
}

FluxRenderCache *flux_render_cache_create(uint32_t max_entries) {
	FluxRenderCache *cache = ( FluxRenderCache * ) calloc(1, sizeof(*cache));
	if (!cache) return NULL;

	uint32_t cap = 64;
	while (cap < max_entries * 2) cap *= 2;

	cache->slots = ( FluxCacheSlot * ) calloc(cap, sizeof(FluxCacheSlot));
		if (!cache->slots) {
			free(cache);
			return NULL;
		}
	cache->capacity    = cap;
	cache->max_entries = max_entries > 0 ? max_entries : 256;
	return cache;
}

void flux_render_cache_destroy(FluxRenderCache *cache) {
	if (!cache) return;
	for (uint32_t i = 0; i < cache->capacity; i++)
		if (cache->slots [i].tag == FLUX_RC_OCCUPIED) release_entry_resources(&cache->slots [i].entry);
	free(cache->slots);
	free(cache);
}

void flux_render_cache_begin_frame(FluxRenderCache *cache) {
	if (cache) cache->frame++;
}

uint64_t        flux_render_cache_frame(FluxRenderCache const *cache) { return cache ? cache->frame : 0; }

FluxCacheEntry *flux_render_cache_get(FluxRenderCache *cache, uint64_t node_id) {
	if (!cache) return NULL;
	FluxCacheSlot *s = rc_find(cache, node_id);
	if (!s) return NULL;
	s->entry.last_frame = cache->frame;
	return &s->entry;
}

FluxCacheEntry *flux_render_cache_get_or_create(FluxRenderCache *cache, uint64_t node_id) {
	if (!cache) return NULL;

	FluxCacheSlot *existing = rc_find(cache, node_id);
		if (existing) {
			existing->entry.last_frame = cache->frame;
			return &existing->entry;
		}

	if (cache->count >= cache->max_entries) flux_render_cache_evict_lru(cache, 120);

		if (rc_needs_grow(cache)) {
			if (!rc_grow(cache)) return NULL;
		}

	uint32_t idx = rc_hash(node_id, cache->capacity);
		for (;;) {
			FluxCacheSlot *s = &cache->slots [idx];
				if (s->tag != FLUX_RC_OCCUPIED) {
					bool was_tombstone = (s->tag == FLUX_RC_DELETED);
					s->tag             = FLUX_RC_OCCUPIED;
					s->key             = node_id;
					memset(&s->entry, 0, sizeof(s->entry));
					s->entry.node_id    = node_id;
					s->entry.last_frame = cache->frame;
					cache->count++;
					if (was_tombstone) cache->tombstones--;
					return &s->entry;
				}
			idx = (idx + 1) & (cache->capacity - 1);
		}
}

void flux_render_cache_remove(FluxRenderCache *cache, uint64_t node_id) {
	if (!cache) return;
	FluxCacheSlot *s = rc_find(cache, node_id);
	if (!s) return;
	release_entry_resources(&s->entry);
	s->tag = FLUX_RC_DELETED;
	cache->count--;
	cache->tombstones++;
}

void flux_render_cache_clear(FluxRenderCache *cache) {
	if (!cache) return;
		for (uint32_t i = 0; i < cache->capacity; i++) {
			if (cache->slots [i].tag == FLUX_RC_OCCUPIED) release_entry_resources(&cache->slots [i].entry);
			cache->slots [i].tag = FLUX_RC_EMPTY;
		}
	cache->count      = 0;
	cache->tombstones = 0;
}

uint32_t flux_render_cache_count(FluxRenderCache const *cache) { return cache ? cache->count : 0; }

void     flux_render_cache_evict_lru(FluxRenderCache *cache, uint64_t age_threshold) {
	if (!cache) return;
		for (uint32_t i = 0; i < cache->capacity; i++) {
			if (cache->slots [i].tag != FLUX_RC_OCCUPIED) continue;
				if (cache->frame - cache->slots [i].entry.last_frame > age_threshold) {
					release_entry_resources(&cache->slots [i].entry);
					cache->slots [i].tag = FLUX_RC_DELETED;
					cache->count--;
					cache->tombstones++;
				}
		}

		while (cache->count > cache->max_entries) {
			uint64_t oldest_frame = cache->frame;
			uint32_t oldest_idx   = cache->capacity;

				for (uint32_t i = 0; i < cache->capacity; i++) {
						if (cache->slots [i].tag == FLUX_RC_OCCUPIED
							&& cache->slots [i].entry.last_frame < oldest_frame)
						{
							oldest_frame = cache->slots [i].entry.last_frame;
							oldest_idx   = i;
						}
				}

			if (oldest_idx >= cache->capacity) break;

			release_entry_resources(&cache->slots [oldest_idx].entry);
			cache->slots [oldest_idx].tag = FLUX_RC_DELETED;
			cache->count--;
			cache->tombstones++;
		}
}

ID2D1SolidColorBrush *flux_render_cache_brush(
  FluxRenderCache *cache, ID2D1DeviceContext *dc, uint64_t node_id, FluxColor color, uint32_t *cached_rgba,
  ID2D1SolidColorBrush **slot
) {
	if (!cache || !dc || !cached_rgba || !slot) return NULL;

	if (*slot && *cached_rgba == color.rgba) return *slot;

		if (*slot) {
			ID2D1SolidColorBrush_Release(*slot);
			*slot = NULL;
		}

	D2D1_COLOR_F          cf = flux_to_d2d_color(color);
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = 1.0f;
	bp.transform._11 = 1.0f;
	bp.transform._12 = 0.0f;
	bp.transform._21 = 0.0f;
	bp.transform._22 = 1.0f;
	bp.transform._31 = 0.0f;
	bp.transform._32 = 0.0f;

	HRESULT hr       = ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) dc, &cf, &bp, slot);
	if (FAILED(hr)) return NULL;

	*cached_rgba = color.rgba;
	( void ) node_id;
	return *slot;
}
