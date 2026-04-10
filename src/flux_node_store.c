#include "fluxent/flux_node_store.h"

#include <stdlib.h>
#include <string.h>

#define FLUX_NS_EMPTY    0
#define FLUX_NS_OCCUPIED 1
#define FLUX_NS_DELETED  2

typedef struct FluxNodeSlot {
    uint8_t       tag;
    XentNodeId    key;
    FluxNodeData  data;
} FluxNodeSlot;

struct FluxNodeStore {
    FluxNodeSlot *slots;
    uint32_t      capacity;
    uint32_t      count;
    uint32_t      tombstones;
};

static uint32_t flux_ns_hash(XentNodeId id, uint32_t cap) {
    uint32_t h = id;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h & (cap - 1);
}

static bool flux_ns_needs_grow(const FluxNodeStore *store) {
    return (store->count + store->tombstones) * 4 >= store->capacity * 3;
}

static bool flux_ns_grow(FluxNodeStore *store) {
    uint32_t new_cap = store->capacity * 2;
    if (new_cap < 64) {
        new_cap = 64;
    }

    FluxNodeSlot *new_slots = (FluxNodeSlot *)calloc(new_cap, sizeof(FluxNodeSlot));
    if (!new_slots) {
        return false;
    }

    for (uint32_t i = 0; i < store->capacity; i++) {
        if (store->slots[i].tag != FLUX_NS_OCCUPIED) {
            continue;
        }
        uint32_t idx = flux_ns_hash(store->slots[i].key, new_cap);
        for (;;) {
            if (new_slots[idx].tag == FLUX_NS_EMPTY) {
                new_slots[idx].tag  = FLUX_NS_OCCUPIED;
                new_slots[idx].key  = store->slots[i].key;
                new_slots[idx].data = store->slots[i].data;
                break;
            }
            idx = (idx + 1) & (new_cap - 1);
        }
    }

    free(store->slots);
    store->slots      = new_slots;
    store->capacity   = new_cap;
    store->tombstones = 0;
    return true;
}

static FluxNodeSlot *flux_ns_find(FluxNodeStore *store, XentNodeId id) {
    uint32_t idx = flux_ns_hash(id, store->capacity);
    for (;;) {
        FluxNodeSlot *s = &store->slots[idx];
        if (s->tag == FLUX_NS_EMPTY) {
            return NULL;
        }
        if (s->tag == FLUX_NS_OCCUPIED && s->key == id) {
            return s;
        }
        idx = (idx + 1) & (store->capacity - 1);
    }
}

static void flux_node_data_init(FluxNodeData *d, XentNodeId id) {
    memset(d, 0, sizeof(*d));
    d->node_id = id;
    d->visuals.opacity = 1.0f;
    d->state.enabled = 1;
    d->state.visible = 1;
}

FluxNodeStore *flux_node_store_create(uint32_t initial_capacity) {
    FluxNodeStore *store = (FluxNodeStore *)calloc(1, sizeof(*store));
    if (!store) {
        return NULL;
    }

    uint32_t cap = 64;
    while (cap < initial_capacity) {
        cap *= 2;
    }

    store->slots = (FluxNodeSlot *)calloc(cap, sizeof(FluxNodeSlot));
    if (!store->slots) {
        free(store);
        return NULL;
    }
    store->capacity = cap;
    return store;
}

void flux_node_store_destroy(FluxNodeStore *store) {
    if (!store) {
        return;
    }
    for (uint32_t i = 0; i < store->capacity; i++) {
        if (store->slots[i].tag == FLUX_NS_OCCUPIED) {
            free(store->slots[i].data.component_data);
        }
    }
    free(store->slots);
    free(store);
}

FluxNodeData *flux_node_store_get(FluxNodeStore *store, XentNodeId id) {
    if (!store || id == XENT_NODE_INVALID) {
        return NULL;
    }
    FluxNodeSlot *s = flux_ns_find(store, id);
    return s ? &s->data : NULL;
}

FluxNodeData *flux_node_store_get_or_create(FluxNodeStore *store, XentNodeId id) {
    if (!store || id == XENT_NODE_INVALID) {
        return NULL;
    }

    FluxNodeSlot *existing = flux_ns_find(store, id);
    if (existing) {
        return &existing->data;
    }

    if (flux_ns_needs_grow(store)) {
        if (!flux_ns_grow(store)) {
            return NULL;
        }
    }

    uint32_t idx = flux_ns_hash(id, store->capacity);
    for (;;) {
        FluxNodeSlot *s = &store->slots[idx];
        if (s->tag != FLUX_NS_OCCUPIED) {
            bool was_tombstone = (s->tag == FLUX_NS_DELETED);
            s->tag = FLUX_NS_OCCUPIED;
            s->key = id;
            flux_node_data_init(&s->data, id);
            store->count++;
            if (was_tombstone) {
                store->tombstones--;
            }
            return &s->data;
        }
        idx = (idx + 1) & (store->capacity - 1);
    }
}

void flux_node_store_remove(FluxNodeStore *store, XentNodeId id) {
    if (!store || id == XENT_NODE_INVALID) {
        return;
    }
    FluxNodeSlot *s = flux_ns_find(store, id);
    if (!s) {
        return;
    }
    free(s->data.component_data);
    s->data.component_data = NULL;
    s->tag = FLUX_NS_DELETED;
    store->count--;
    store->tombstones++;
}

uint32_t flux_node_store_count(const FluxNodeStore *store) {
    return store ? store->count : 0;
}

void flux_node_store_attach_userdata(FluxNodeStore *store, XentContext *ctx) {
    if (!store || !ctx) {
        return;
    }
    for (uint32_t i = 0; i < store->capacity; i++) {
        if (store->slots[i].tag != FLUX_NS_OCCUPIED) {
            continue;
        }
        FluxNodeData *d = &store->slots[i].data;
        xent_set_userdata(ctx, d->node_id, d);
    }
}