#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <cd2d.h>
#include "flux_render_cache.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define FLUX_RC_EMPTY            0
#define FLUX_RC_OCCUPIED         1
#define FLUX_RC_DELETED          2

#define FLUX_RC_EVICT_AGE_FRAMES 120

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

static void rc_insert_rehashed_slot(FluxCacheSlot *slots, uint32_t cap, FluxCacheSlot const *src) {
	uint32_t idx = rc_hash(src->key, cap);
	while (slots [idx].tag != FLUX_RC_EMPTY) idx = (idx + 1) & (cap - 1);

	slots [idx].tag   = FLUX_RC_OCCUPIED;
	slots [idx].key   = src->key;
	slots [idx].entry = src->entry;
}

static bool rc_grow(FluxRenderCache *cache) {
	uint32_t new_cap = cache->capacity * 2;
	if (new_cap < 64) new_cap = 64;

	FluxCacheSlot *new_slots = ( FluxCacheSlot * ) calloc(new_cap, sizeof(FluxCacheSlot));
	if (!new_slots) return false;

	for (uint32_t i = 0; i < cache->capacity; i++)
		if (cache->slots [i].tag == FLUX_RC_OCCUPIED) rc_insert_rehashed_slot(new_slots, new_cap, &cache->slots [i]);

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

void flux_rcache_begin(FluxRenderCache *cache) {
	if (cache) cache->frame++;
}

FluxCacheEntry *flux_render_cache_get(FluxRenderCache *cache, uint64_t node_id) {
	if (!cache) return NULL;
	FluxCacheSlot *s = rc_find(cache, node_id);
	if (!s) return NULL;
	s->entry.last_frame = cache->frame;
	return &s->entry;
}

static bool rc_prepare_insert(FluxRenderCache *cache) {
	if (cache->count >= cache->max_entries) flux_rcache_evict(cache, FLUX_RC_EVICT_AGE_FRAMES);
	if (!rc_needs_grow(cache)) return true;
	return rc_grow(cache);
}

static FluxCacheSlot *rc_find_insert_slot(FluxRenderCache *cache, uint64_t key) {
	uint32_t idx = rc_hash(key, cache->capacity);
	while (cache->slots [idx].tag == FLUX_RC_OCCUPIED) idx = (idx + 1) & (cache->capacity - 1);
	return &cache->slots [idx];
}

static FluxCacheEntry *rc_insert_new_entry(FluxRenderCache *cache, uint64_t node_id) {
	FluxCacheSlot *s             = rc_find_insert_slot(cache, node_id);
	bool           was_tombstone = (s->tag == FLUX_RC_DELETED);
	s->tag                       = FLUX_RC_OCCUPIED;
	s->key                       = node_id;
	memset(&s->entry, 0, sizeof(s->entry));
	s->entry.node_id    = node_id;
	s->entry.last_frame = cache->frame;
	cache->count++;
	if (was_tombstone) cache->tombstones--;
	return &s->entry;
}

FluxCacheEntry *flux_rcache_ensure(FluxRenderCache *cache, uint64_t node_id) {
	if (!cache) return NULL;

	FluxCacheSlot *existing = rc_find(cache, node_id);
	if (existing) {
		existing->entry.last_frame = cache->frame;
		return &existing->entry;
	}

	if (!rc_prepare_insert(cache)) return NULL;
	return rc_insert_new_entry(cache, node_id);
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

static void rc_delete_slot(FluxRenderCache *cache, uint32_t idx) {
	release_entry_resources(&cache->slots [idx].entry);
	cache->slots [idx].tag = FLUX_RC_DELETED;
	cache->count--;
	cache->tombstones++;
}

static void rc_evict_by_age(FluxRenderCache *cache, uint64_t age_threshold) {
	for (uint32_t i = 0; i < cache->capacity; i++) {
		if (cache->slots [i].tag != FLUX_RC_OCCUPIED) continue;
		if (cache->frame - cache->slots [i].entry.last_frame <= age_threshold) continue;
		rc_delete_slot(cache, i);
	}
}

static uint32_t rc_oldest_slot(FluxRenderCache const *cache) {
	uint64_t oldest_frame = cache->frame;
	uint32_t oldest_idx   = cache->capacity;

	for (uint32_t i = 0; i < cache->capacity; i++) {
		if (cache->slots [i].tag != FLUX_RC_OCCUPIED) continue;
		if (cache->slots [i].entry.last_frame >= oldest_frame) continue;
		oldest_frame = cache->slots [i].entry.last_frame;
		oldest_idx   = i;
	}
	return oldest_idx;
}

void flux_rcache_evict(FluxRenderCache *cache, uint64_t age_threshold) {
	if (!cache) return;
	rc_evict_by_age(cache, age_threshold);
	while (cache->count > cache->max_entries) {
		uint32_t oldest_idx = rc_oldest_slot(cache);
		if (oldest_idx >= cache->capacity) break;
		rc_delete_slot(cache, oldest_idx);
	}
}
