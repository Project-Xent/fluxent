#ifndef FLUX_NODE_STORE_H
#define FLUX_NODE_STORE_H

#include "flux_types.h"
#include <xent/xent.h>
#include <xent/xent_display.h>
#include "flux_control_type.h"

typedef struct XtkHost      XtkHost;
typedef struct FluxNodeData FluxNodeData;
typedef struct FluxStore    FluxStore;

XENT_NODISCARD FluxStore   *flux_store_create(uint32_t initial_capacity);

bool                        flux_store_bind(FluxStore *store, XentCtx *ctx);

XentCtx                    *flux_store_ctx(FluxStore const *store);

void                        flux_store_destroy(FluxStore *store);

FluxNodeData               *flux_store_find(FluxStore *store, XentNodeId id);

FluxNodeData               *flux_store_ensure(FluxStore *store, XentNodeId id);

void                        flux_store_del(FluxStore *store, XentNodeId id);

typedef void                (*FluxNodeRemovedFn)(void *userdata, XentNodeId id);

void                        flux_store_ondel(FluxStore *store, FluxNodeRemovedFn fn, void *userdata);

void                        flux_store_sethost(FluxStore *store, XtkHost *host);
XtkHost                    *flux_store_host(FluxStore const *store);

void                        flux_node_setdl(FluxStore *store, XentNodeId node, XentDl const *list);
void                        flux_node_adoptdl(FluxStore *store, XentNodeId node, XentDl *list);
XentDl const               *flux_node_dl(FluxStore const *store, XentNodeId node);

void                        flux_tree_drop(FluxStore *store, XentNodeId node);

uint32_t                    flux_store_n(FluxStore const *store);

#endif
