#include "fluxent/flux_node_store.h"
#include "store/flux_node_data.h"
#include "runtime/flux_str.h"

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

struct FluxStore {
	FluxNodeSlot     *slots;
	uint32_t          capacity;
	uint32_t          count;
	uint32_t          tombstones;
	XentCtx          *ctx;        /**< Context the nodes live in; bound at scene creation. */
	XentObsId         observer;
	FluxNodeRemovedFn removed_fn; /**< Notified before a destroyed node's data is freed. */
	void             *removed_userdata;
	XtkHost          *host;
};

static uint32_t ns_hash(XentNodeId id, uint32_t cap) {
	uint64_t h  = id;
	h          ^= h >> 33;
	h          *= 0xff51afd7ed558ccdull;
	h          ^= h >> 33;
	return ( uint32_t ) h & (cap - 1);
}

static bool ns_needs_grow(FluxStore const *store) {
	return (store->count + store->tombstones) * 4 >= store->capacity * 3;
}

static bool ns_needs_rehash(FluxStore const *store) {
	return store->tombstones > store->count && store->tombstones > 16;
}

static void ns_probe_next(uint32_t *idx, uint32_t *step, uint32_t cap) {
	(*step)++;
	*idx = (*idx + *step) & (cap - 1);
}

static void ns_insert_rehashed_slot(FluxNodeSlot *slots, uint32_t cap, FluxNodeSlot const *src) {
	uint32_t idx  = ns_hash(src->key, cap);
	uint32_t step = 0;
	while (slots [idx].tag != FLUX_NS_EMPTY) ns_probe_next(&idx, &step, cap);

	slots [idx].tag  = FLUX_NS_OCCUPIED;
	slots [idx].key  = src->key;
	slots [idx].data = src->data;
}

static bool ns_rehash(FluxStore *store, uint32_t new_cap) {
	if (new_cap < 64) new_cap = 64;

	FluxNodeSlot *new_slots = ( FluxNodeSlot * ) calloc(new_cap, sizeof(FluxNodeSlot));
	if (!new_slots) return false;

	for (uint32_t i = 0; i < store->capacity; i++)
		if (store->slots [i].tag == FLUX_NS_OCCUPIED) ns_insert_rehashed_slot(new_slots, new_cap, &store->slots [i]);

	free(store->slots);
	store->slots      = new_slots;
	store->capacity   = new_cap;
	store->tombstones = 0;
	return true;
}

static bool ns_grow(FluxStore *store) { return ns_rehash(store, store->capacity * 2); }

static bool ns_prepare_insert(FluxStore *store) {
	if (ns_needs_grow(store)) return ns_grow(store);
	if (ns_needs_rehash(store)) return ns_rehash(store, store->capacity);
	return true;
}

static void node_data_clear_display_list(FluxNodeData *d) {
	if (!d) return;
	if (d->display_list_owned && d->display_list) xent_dl_free(( XentDl * ) d->display_list);
	d->display_list       = NULL;
	d->display_list_owned = false;
}

static void node_data_destroy_component(FluxNodeData *d) {
	if (!d) return;

	flux_str_free(d->tooltip_text);
	d->tooltip_text = NULL;
	node_data_clear_display_list(d);

	if (!d->component_data) return;

	if (d->destroy_component_data) d->destroy_component_data(d->component_data);

	d->component_data         = NULL;
	d->destroy_component_data = NULL;
	d->component_type         = FLUX_CONTROL_CONTAINER;
}

static FluxNodeSlot *ns_find(FluxStore *store, XentNodeId id) {
	uint32_t idx  = ns_hash(id, store->capacity);
	uint32_t step = 0;
	for (;;) {
		FluxNodeSlot *s = &store->slots [idx];
		if (s->tag == FLUX_NS_EMPTY) return NULL;
		if (s->tag == FLUX_NS_OCCUPIED && s->key == id) return s;
		ns_probe_next(&idx, &step, store->capacity);
	}
}

static void node_data_init(FluxNodeData *d, XentNodeId id) {
	memset(d, 0, sizeof(*d));
	d->node_id         = id;
	d->visuals.opacity = 1.0f;
	d->render_scale    = 1.0f;
	d->render_opacity  = 1.0f;
	d->state.visible   = 1;
	d->component_type  = FLUX_CONTROL_CONTAINER;
}

static FluxNodeSlot *ns_find_insert_slot(FluxStore *store, XentNodeId id, bool *was_tombstone) {
	uint32_t      idx        = ns_hash(id, store->capacity);
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
		ns_probe_next(&idx, &step, store->capacity);
	}
}

static FluxNodeData *ns_occupy_slot(FluxStore *store, FluxNodeSlot *slot, XentNodeId id, bool was_tombstone) {
	slot->tag = FLUX_NS_OCCUPIED;
	slot->key = id;
	node_data_init(&slot->data, id);
	store->count++;
	if (was_tombstone) store->tombstones--;
	return &slot->data;
}

FluxStore *flux_store_create(uint32_t initial_capacity) {
	FluxStore *store = ( FluxStore * ) calloc(1, sizeof(*store));
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

void flux_store_destroy(FluxStore *store) {
	if (!store) return;
	if (store->ctx && store->observer != XENT_NODE_OBSERVER_INVALID) xent_node_delobs(store->ctx, store->observer);
	for (uint32_t i = 0; i < store->capacity; i++)
		if (store->slots [i].tag == FLUX_NS_OCCUPIED) node_data_destroy_component(&store->slots [i].data);
	free(store->slots);
	free(store);
}

FluxNodeData *flux_store_find(FluxStore *store, XentNodeId id) {
	if (!store || id == XENT_NODE_INVALID) return NULL;
	FluxNodeSlot *s = ns_find(store, id);
	return s ? &s->data : NULL;
}

FluxNodeData *flux_store_ensure(FluxStore *store, XentNodeId id) {
	if (!store || id == XENT_NODE_INVALID) return NULL;
	/* Creation requires a live Core handle; destroy-path remove stays stale-safe. */
	if (!store->ctx || !xent_node_valid(store->ctx, id)) return NULL;

	FluxNodeSlot *existing = ns_find(store, id);
	if (existing) return &existing->data;
	if (!ns_prepare_insert(store)) return NULL;

	bool          was_tombstone = false;
	FluxNodeSlot *slot          = ns_find_insert_slot(store, id, &was_tombstone);
	return ns_occupy_slot(store, slot, id, was_tombstone);
}

void flux_store_del(FluxStore *store, XentNodeId id) {
	if (!store || id == XENT_NODE_INVALID) return;
	FluxNodeSlot *s = ns_find(store, id);
	if (!s) return;
	node_data_destroy_component(&s->data);
	s->tag = FLUX_NS_DELETED;
	store->count--;
	store->tombstones++;
}

uint32_t    flux_store_n(FluxStore const *store) { return store ? store->count : 0; }

/* xent invokes this for every node freed by xent_node_destroy, including each
 * node of a destroyed subtree. The node is already dead in xent at this point,
 * so consumers must read control identity from the store data, not the layout
 * tree. Listener runs first: it may still need the component data (e.g. the
 * input boundary releasing a scroll node's DirectManipulation viewport). */
static void ns_on_node_lifecycle(XentCtx *ctx, XentNodeEvent const *lifecycle, void *userdata) {
	( void ) ctx;
	if (lifecycle->event != XENT_NODE_EVENT_DESTROY) return;

	FluxStore *store = ( FluxStore * ) userdata;
	if (store->removed_fn) store->removed_fn(store->removed_userdata, lifecycle->node);
	flux_store_del(store, lifecycle->node);
}

static void node_store_unbind_context(FluxStore *store) {
	if (!store->ctx) return;
	if (store->observer != XENT_NODE_OBSERVER_INVALID) xent_node_delobs(store->ctx, store->observer);
	store->ctx      = NULL;
	store->observer = XENT_NODE_OBSERVER_INVALID;
}

bool flux_store_bind(FluxStore *store, XentCtx *ctx) {
	if (!store) return false;
	if (store->ctx == ctx) return !ctx || store->observer != XENT_NODE_OBSERVER_INVALID;

	node_store_unbind_context(store);
	if (!ctx) return true;

	XentNodeObs observer = {.notify = ns_on_node_lifecycle, .userdata = store};
	XentObsId   id       = xent_node_addobs(ctx, &observer);
	if (id == XENT_NODE_OBSERVER_INVALID) return false;

	store->ctx      = ctx;
	store->observer = id;
	return true;
}

void flux_store_ondel(FluxStore *store, FluxNodeRemovedFn fn, void *userdata) {
	if (!store) return;
	store->removed_fn       = fn;
	store->removed_userdata = userdata;
}

void flux_store_sethost(FluxStore *store, XtkHost *host) {
	if (!store) return;
	store->host = host;
}

XtkHost *flux_store_host(FluxStore const *store) { return store ? store->host : NULL; }

void     flux_tree_drop(FluxStore *store, XentNodeId node) {
	XentCtx *ctx = flux_store_ctx(store);
	if (!ctx || node == XENT_NODE_INVALID) return;
	xent_node_destroy(ctx, node);
}

XentCtx *flux_store_ctx(FluxStore const *store) { return store ? store->ctx : NULL; }

void     flux_node_setdl(FluxStore *store, XentNodeId node, XentDl const *list) {
	FluxNodeData *nd = flux_store_ensure(store, node);
	if (!nd) return;
	node_data_clear_display_list(nd);
	nd->display_list       = list;
	nd->display_list_owned = false;
}

void flux_node_adoptdl(FluxStore *store, XentNodeId node, XentDl *list) {
	FluxNodeData *nd = flux_store_ensure(store, node);
	if (!nd) {
		xent_dl_free(list);
		return;
	}
	node_data_clear_display_list(nd);
	nd->display_list       = list;
	nd->display_list_owned = list != NULL;
}

XentDl const *flux_node_dl(FluxStore const *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(( FluxStore * ) store, node);
	return nd ? nd->display_list : NULL;
}
