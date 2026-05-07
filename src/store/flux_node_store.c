#include "fluxent/flux_node_store.h"

#include <stdlib.h>
#include <string.h>

#define FLUX_NS_EMPTY    0
#define FLUX_NS_OCCUPIED 1
#define FLUX_NS_DELETED  2

typedef struct FluxNodeSlot {
	uint8_t      tag;
	XentNodeId   key;
	FluxNodeData data;
} FluxNodeSlot;

struct FluxNodeStore {
	FluxNodeSlot       *slots;
	uint32_t            capacity;
	uint32_t            count;
	uint32_t            tombstones;
	FluxControlRenderer renderers [XENT_CONTROL_CUSTOM + 1];
};

static uint32_t flux_ns_hash(XentNodeId id, uint32_t cap) {
	uint32_t h  = id;
	h          ^= h >> 16;
	h          *= 0x45d9f3b;
	h          ^= h >> 16;
	return h & (cap - 1);
}

static bool flux_ns_needs_grow(FluxNodeStore const *store) {
	return (store->count + store->tombstones) * 4 >= store->capacity * 3;
}

static bool flux_ns_needs_rehash(FluxNodeStore const *store) {
	return store->tombstones > store->count && store->tombstones > 16;
}

static void flux_ns_probe_next(uint32_t *idx, uint32_t *step, uint32_t cap) {
	(*step)++;
	*idx = (*idx + *step) & (cap - 1);
}

static void flux_ns_insert_rehashed_slot(FluxNodeSlot *slots, uint32_t cap, FluxNodeSlot const *src) {
	uint32_t idx  = flux_ns_hash(src->key, cap);
	uint32_t step = 0;
	while (slots [idx].tag != FLUX_NS_EMPTY) flux_ns_probe_next(&idx, &step, cap);

	slots [idx].tag  = FLUX_NS_OCCUPIED;
	slots [idx].key  = src->key;
	slots [idx].data = src->data;
}

static bool flux_ns_rehash(FluxNodeStore *store, uint32_t new_cap) {
	if (new_cap < 64) new_cap = 64;

	FluxNodeSlot *new_slots = ( FluxNodeSlot * ) calloc(new_cap, sizeof(FluxNodeSlot));
	if (!new_slots) return false;

	for (uint32_t i = 0; i < store->capacity; i++)
		if (store->slots [i].tag == FLUX_NS_OCCUPIED)
			flux_ns_insert_rehashed_slot(new_slots, new_cap, &store->slots [i]);

	free(store->slots);
	store->slots      = new_slots;
	store->capacity   = new_cap;
	store->tombstones = 0;
	return true;
}

static bool flux_ns_grow(FluxNodeStore *store) { return flux_ns_rehash(store, store->capacity * 2); }

static bool flux_ns_prepare_insert(FluxNodeStore *store) {
	if (flux_ns_needs_grow(store)) return flux_ns_grow(store);
	if (flux_ns_needs_rehash(store)) return flux_ns_rehash(store, store->capacity);
	return true;
}

static void flux_node_data_destroy_component(FluxNodeData *d) {
	if (!d || !d->component_data) return;

	if (d->destroy_component_data) d->destroy_component_data(d->component_data);

	d->component_data         = NULL;
	d->destroy_component_data = NULL;
	d->component_type         = XENT_CONTROL_CONTAINER;
}

static FluxNodeSlot *flux_ns_find(FluxNodeStore *store, XentNodeId id) {
	uint32_t idx  = flux_ns_hash(id, store->capacity);
	uint32_t step = 0;
	for (;;) {
		FluxNodeSlot *s = &store->slots [idx];
		if (s->tag == FLUX_NS_EMPTY) return NULL;
		if (s->tag == FLUX_NS_OCCUPIED && s->key == id) return s;
		flux_ns_probe_next(&idx, &step, store->capacity);
	}
}

static void flux_node_data_init(FluxNodeData *d, XentNodeId id) {
	memset(d, 0, sizeof(*d));
	d->node_id         = id;
	d->visuals.opacity = 1.0f;
	d->state.enabled   = 1;
	d->state.visible   = 1;
	d->component_type  = XENT_CONTROL_CONTAINER;
}

static FluxNodeSlot *flux_ns_find_insert_slot(FluxNodeStore *store, XentNodeId id, bool *was_tombstone) {
	uint32_t      idx        = flux_ns_hash(id, store->capacity);
	uint32_t      step       = 0;
	FluxNodeSlot *first_dead = NULL;
	for (;;) {
		FluxNodeSlot *s = &store->slots [idx];
		if (s->tag == FLUX_NS_DELETED && !first_dead) first_dead = s;
		if (s->tag == FLUX_NS_EMPTY) {
			FluxNodeSlot *dst = first_dead ? first_dead : s;
			*was_tombstone    = dst->tag == FLUX_NS_DELETED;
			return dst;
		}
		flux_ns_probe_next(&idx, &step, store->capacity);
	}
}

static FluxNodeData *flux_ns_occupy_slot(FluxNodeStore *store, FluxNodeSlot *slot, XentNodeId id, bool was_tombstone) {
	slot->tag = FLUX_NS_OCCUPIED;
	slot->key = id;
	flux_node_data_init(&slot->data, id);
	store->count++;
	if (was_tombstone) store->tombstones--;
	return &slot->data;
}

FluxNodeStore *flux_node_store_create(uint32_t initial_capacity) {
	FluxNodeStore *store = ( FluxNodeStore * ) calloc(1, sizeof(*store));
	if (!store) return NULL;

	uint32_t cap = 64;
	while (cap < initial_capacity) cap *= 2;

	store->slots = ( FluxNodeSlot * ) calloc(cap, sizeof(FluxNodeSlot));
	if (!store->slots) {
		free(store);
		return NULL;
	}
	store->capacity = cap;
	return store;
}

void flux_node_store_destroy(FluxNodeStore *store) {
	if (!store) return;
	for (uint32_t i = 0; i < store->capacity; i++)
		if (store->slots [i].tag == FLUX_NS_OCCUPIED) flux_node_data_destroy_component(&store->slots [i].data);
	free(store->slots);
	free(store);
}

FluxNodeData *flux_node_store_get(FluxNodeStore *store, XentNodeId id) {
	if (!store || id == XENT_NODE_INVALID) return NULL;
	FluxNodeSlot *s = flux_ns_find(store, id);
	return s ? &s->data : NULL;
}

FluxNodeData *flux_node_store_get_or_create(FluxNodeStore *store, XentNodeId id) {
	if (!store || id == XENT_NODE_INVALID) return NULL;

	FluxNodeSlot *existing = flux_ns_find(store, id);
	if (existing) return &existing->data;
	if (!flux_ns_prepare_insert(store)) return NULL;

	bool          was_tombstone = false;
	FluxNodeSlot *slot          = flux_ns_find_insert_slot(store, id, &was_tombstone);
	return flux_ns_occupy_slot(store, slot, id, was_tombstone);
}

void flux_node_store_remove(FluxNodeStore *store, XentNodeId id) {
	if (!store || id == XENT_NODE_INVALID) return;
	FluxNodeSlot *s = flux_ns_find(store, id);
	if (!s) return;
	flux_node_data_destroy_component(&s->data);
	s->tag = FLUX_NS_DELETED;
	store->count--;
	store->tombstones++;
}

uint32_t flux_node_store_count(FluxNodeStore const *store) { return store ? store->count : 0; }

void     flux_node_store_attach_userdata(FluxNodeStore *store, XentContext *ctx) {
	if (!store || !ctx) return;
	for (uint32_t i = 0; i < store->capacity; i++) {
		if (store->slots [i].tag != FLUX_NS_OCCUPIED) continue;
		FluxNodeData *d = &store->slots [i].data;
		if (d->component_data) d->component_type = xent_get_control_type(ctx, d->node_id);
		xent_set_userdata(ctx, d->node_id, d);
	}
}

void flux_node_store_register_renderer(
  FluxNodeStore *store, XentControlType type,
  void (*draw)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *),
  void (*draw_overlay)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *)
) {
	if (!store || ( uint32_t ) type > XENT_CONTROL_CUSTOM) return;
	store->renderers [type].draw         = draw;
	store->renderers [type].draw_overlay = draw_overlay;
}

FluxControlRenderer const *flux_node_store_get_renderer(FluxNodeStore const *store, XentControlType type) {
	if (!store || ( uint32_t ) type > XENT_CONTROL_CUSTOM) return NULL;
	return &store->renderers [type];
}
