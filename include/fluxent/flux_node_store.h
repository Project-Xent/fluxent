/**
 * @file flux_node_store.h
 * @brief Node storage: XentNodeId -> FluxNodeData mapping.
 *
 * Advanced backend surface. FluxNodeData (retained visuals, interaction
 * state, event handlers) is backend-private; it appears here only as an
 * opaque handle.
 */
#ifndef FLUX_NODE_STORE_H
#define FLUX_NODE_STORE_H

#include "flux_types.h"
#include <xent/xent.h>
#include <xent/xent_display.h>
#include "flux_control_type.h"

typedef struct XtkHost      XtkHost;
typedef struct FluxNodeData FluxNodeData;
typedef struct FluxStore    FluxStore;

/** @brief Create a new node store with the specified initial capacity (grows as needed). */
XENT_NODISCARD FluxStore   *flux_store_create(uint32_t initial_capacity);

/**
 * @brief Bind the XentCtx this store's nodes live in.
 *
 * Lets store-only APIs (e.g. flux_*_set_enabled) reach context-scoped node
 * properties such as semantic enabled. Set once, at scene creation, before the
 * tree is built. Safe to call again (idempotent).
 */
bool                        flux_store_bind(FluxStore *store, XentCtx *ctx);

/** @brief The XentCtx bound via flux_store_bind (NULL if unbound). */
XentCtx                    *flux_store_ctx(FluxStore const *store);

/** @brief Destroy a node store and free all associated memory (NULL is safe). */
void                        flux_store_destroy(FluxStore *store);

/** @brief Look up node data by ID, or NULL if not found. */
FluxNodeData               *flux_store_find(FluxStore *store, XentNodeId id);

/** @brief Get or create node data for the given ID. */
FluxNodeData               *flux_store_ensure(FluxStore *store, XentNodeId id);

/** @brief Remove a node from the store. */
void                        flux_store_del(FluxStore *store, XentNodeId id);

/** @brief Notified for each destroyed node before its store data is freed. */
typedef void                (*FluxNodeRemovedFn)(void *userdata, XentNodeId id);

/**
 * @brief Register the single node-removed listener (NULL clears it).
 *
 * Fires once per node freed via xent_node_destroy / flux_tree_drop,
 * before the store entry (and component data) is released, so the listener
 * can drop its references and free node-owned platform resources. The node
 * is already invalid in xent when this fires; read identity from the store.
 */
void                        flux_store_ondel(FluxStore *store, FluxNodeRemovedFn fn, void *userdata);

/** @brief Associate the active XtkHost for event routing (optional). */
void                        flux_store_sethost(FluxStore *store, XtkHost *host);
/** @brief Host set via flux_store_sethost, or NULL. */
XtkHost                    *flux_store_host(FluxStore const *store);

/** Borrow a Component/HtmlView list; clears any store-owned list first. */
void                        flux_node_setdl(FluxStore *store, XentNodeId node, XentDl const *list);
/** Replace with a store-owned list (destroys the previous owned list). */
void                        flux_node_adoptdl(FluxStore *store, XentNodeId node, XentDl *list);
XentDl const               *flux_node_dl(FluxStore const *store, XentNodeId node);

/**
 * @brief Destroy a node and its entire subtree in one call.
 *
 * Detaches the node from its parent, then frees every node in the subtree:
 * each one's component data is destroyed through the store and the removed
 * listener (input/popup reference invalidation) fires per node. This is the
 * teardown path for page swaps and the declarative reconciler's unmount.
 * Requires flux_store_bind (FluxApp scenes do this).
 */
void                        flux_tree_drop(FluxStore *store, XentNodeId node);

/** @brief Get the number of nodes in the store. */
uint32_t                    flux_store_n(FluxStore const *store);

#endif
