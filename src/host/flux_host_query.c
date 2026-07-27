#include "flux_host_internal.h"
#include <fluxent/flux_app.h>

static bool types_ensure(FluxHostState *R, uint32_t index) {
	if (index < R->types_cap) return true;
	uint32_t cap = R->types_cap ? R->types_cap * 2u : 64u;
	while (cap <= index) {
		if (cap > UINT32_MAX / 2u) return false;
		cap *= 2u;
	}
	XtkCtrlId *n = ( XtkCtrlId * ) xtk_alloc_resize(R->types, sizeof(*n) * cap);
	if (!n) return false;
	memset(n + R->types_cap, 0, sizeof(*n) * (cap - R->types_cap));
	R->types     = n;
	R->types_cap = cap;
	return true;
}

bool flux_host_types_set(FluxHostState *R, XentNodeId node, XtkCtrlId type) {
	uint32_t index = xent_node_index(node);
	if (!types_ensure(R, index)) return false;
	R->types [index] = type;
	return true;
}

XtkCtrlId flux_host_types_get(FluxHostState const *R, XentNodeId node) {
	uint32_t index = xent_node_index(node);
	if (index >= R->types_cap) return XTK_CTRL_ID_INVALID;
	return R->types [index];
}

static bool invalidate_ensure(FluxHostState *R, uint32_t index) {
	if (index < R->invalidate_cap) return true;
	uint32_t cap = R->invalidate_cap ? R->invalidate_cap * 2u : 64u;
	while (cap <= index) {
		if (cap > UINT32_MAX / 2u) return false;
		cap *= 2u;
	}
	uint32_t *n = ( uint32_t * ) xtk_alloc_resize(R->invalidate, sizeof(*n) * cap);
	if (!n) return false;
	memset(n + R->invalidate_cap, 0, sizeof(*n) * (cap - R->invalidate_cap));
	R->invalidate     = n;
	R->invalidate_cap = cap;
	return true;
}

bool flux_host_note_invalidate(FluxHostState *R, XtkCtrl control, uint32_t mask) {
	if (!mask) return true;
	if (control == XTK_CTRL_INVALID) {
		R->pending_invalidation |= mask;
		return true;
	}
	uint32_t index = xent_node_index(flux_host_as_node(control));
	if (!invalidate_ensure(R, index)) return false;
	R->invalidate [index]   |= mask;
	R->pending_invalidation |= mask;
	return true;
}

static PendingInvalidation *staged_invalidation(Prepare *P, XtkCtrl control) {
	for (uint32_t i = 0; i < P->invalidation_count; i++)
		if (P->invalidations [i].control == control) return P->invalidations + i;
	return NULL;
}

static bool grow_staged_invalidations(Prepare *P) {
	if (P->invalidation_count < P->invalidation_cap) return true;
	if (P->invalidation_cap > UINT32_MAX / 2u) return false;

	uint32_t             cap = P->invalidation_cap ? P->invalidation_cap * 2u : 8u;
	PendingInvalidation *next;
	if (P->invalidations == P->invalidation_inline) {
		next = ( PendingInvalidation * ) xtk_alloc(sizeof(*next) * cap);
		if (next) memcpy(next, P->invalidation_inline, sizeof(*next) * P->invalidation_count);
	}
	else next = ( PendingInvalidation * ) xtk_alloc_resize(P->invalidations, sizeof(*next) * cap);
	if (!next) return false;

	P->invalidations    = next;
	P->invalidation_cap = cap;
	return true;
}

bool flux_host_stage_invalidate(Prepare *P, XtkCtrl control, uint32_t mask) {
	if (!mask) return true;
	PendingInvalidation *old = staged_invalidation(P, control);
	if (old) {
		old->mask |= mask;
		return true;
	}
	if (!grow_staged_invalidations(P)) return false;
	P->invalidations [P->invalidation_count++] = (PendingInvalidation) {.control = control, .mask = mask};
	return true;
}

bool flux_host_stage_prop_invalidation(Prepare *P, XtkCtrl control, XtkPropertyId id) {
	XtkPropDesc const *prop = xtk_catalog_property(id);
	return !prop || flux_host_stage_invalidate(P, control, prop->invalidates);
}

bool flux_host_stage_lifecycle(Prepare *P, XtkCtrl control, bool create) {
	XtkCtrlId          type = flux_host_types_get(P->R, flux_host_as_node(control));
	XtkCtrlDesc const *desc = xtk_catalog_control(type);
	if (!desc && P->R->comp) desc = xtk_comp_desc(P->R->comp, type);
	if (!desc) return true;
	return flux_host_stage_invalidate(P, control, create ? desc->create_invalidates : desc->destroy_invalidates);
}

bool flux_host_prepare_invalidations(Prepare *P) {
	uint32_t max_index = 0u;
	for (uint32_t i = 0; i < P->invalidation_count; i++) {
		XtkCtrl control = P->invalidations [i].control;
		if (control == XTK_CTRL_INVALID) continue;
		uint32_t index = xent_node_index(flux_host_as_node(control));
		if (index > max_index) max_index = index;
	}
	return max_index == 0u || invalidate_ensure(P->R, max_index);
}

void flux_host_publish_invalidations(Prepare *P) {
	for (uint32_t i = 0; i < P->invalidation_count; i++) {
		PendingInvalidation const *entry  = &P->invalidations [i];
		P->R->pending_invalidation       |= entry->mask;
		if (!flux_host_node_live(P->R, entry->control)) continue;
		uint32_t index            = xent_node_index(flux_host_as_node(entry->control));
		P->R->invalidate [index] |= entry->mask;
	}
}

static void purge_sidecar_node(FluxHostState *R, XtkCtrl control) {
	if (R->focus == control) R->focus = XTK_CTRL_INVALID;
	if (R->comp) xtk_comp_dropinst(R->comp, control);
	flux_host_clear_props(R, control);
	flux_host_clear_subs(R, control);
	uint32_t index = xent_node_index(flux_host_as_node(control));
	if (index < R->invalidate_cap) R->invalidate [index] = 0u;
}

static void purge_subtree_walk(FluxHostState *R, XentNodeId node) {
	for (XentNodeId child = xent_node_first(R->ctx, node); child != XENT_NODE_INVALID;
	  child               = xent_node_next(R->ctx, child))
		purge_subtree_walk(R, child);
	purge_sidecar_node(R, flux_host_as_control(node));
}

void flux_host_purge_sidecar(FluxHostState *R, XentNodeId root) { purge_subtree_walk(R, root); }

bool flux_host_is_live(XtkHost const *host, XtkCtrl control) {
	FluxHostState const *R = flux_host_state_c(host);
	return R && flux_host_node_live(R, control);
}

uint64_t flux_host_revision(XtkHost const *host) {
	FluxHostState const *R = flux_host_state_c(host);
	return R ? R->revision : 0u;
}

XtkCtrl flux_host_scene_root(XtkHost const *host) {
	FluxHostState const *R = flux_host_state_c(host);
	return R ? flux_host_as_control(R->root) : XTK_CTRL_INVALID;
}

XtkCtrlId flux_host_control_type(XtkHost const *host, XtkCtrl control) {
	FluxHostState const *R = flux_host_state_c(host);
	if (!R || !flux_host_node_live(R, control)) return XTK_CTRL_ID_INVALID;
	return flux_host_types_get(R, flux_host_as_node(control));
}

XtkCtrl flux_host_parent(XtkHost const *host, XtkCtrl control) {
	FluxHostState const *R = flux_host_state_c(host);
	if (!R || !flux_host_node_live(R, control)) return XTK_CTRL_INVALID;
	return flux_host_as_control(xent_node_parent(R->ctx, flux_host_as_node(control)));
}

XtkCtrl flux_host_first_child(XtkHost const *host, XtkCtrl control) {
	FluxHostState const *R = flux_host_state_c(host);
	if (!R || !flux_host_node_live(R, control)) return XTK_CTRL_INVALID;
	return flux_host_as_control(xent_node_first(R->ctx, flux_host_as_node(control)));
}

XtkCtrl flux_host_next_sibling(XtkHost const *host, XtkCtrl control) {
	FluxHostState const *R = flux_host_state_c(host);
	if (!R || !flux_host_node_live(R, control)) return XTK_CTRL_INVALID;
	return flux_host_as_control(xent_node_next(R->ctx, flux_host_as_node(control)));
}

XtkCtrlDesc const *flux_host_descriptor(XtkHost const *host, XtkCtrlId type) {
	XtkCtrlDesc const *d = xtk_catalog_control(type);
	if (d) return d;
	FluxHostState const *R = flux_host_state_c(host);
	return R && R->comp ? xtk_comp_desc(R->comp, type) : NULL;
}

bool flux_host_res_valid(XtkHost const *host, XtkResource resource, XtkResourceTypeId type) {
	( void ) host;
	( void ) type;
	return resource != XTK_RESOURCE_INVALID;
}

bool flux_host_resource_retain(XtkHost *host, XtkResource resource, XtkResourceTypeId type) {
	( void ) host;
	( void ) type;
	return resource != XTK_RESOURCE_INVALID;
}

void flux_host_resource_release(XtkHost *host, XtkResource resource, XtkResourceTypeId type) {
	( void ) host;
	( void ) resource;
	( void ) type;
}

uint32_t flux_host_invalidation(XtkHost const *host, XtkCtrl control) {
	FluxHostState const *R = flux_host_state_c(host);
	if (!R || !flux_host_node_live(R, control)) return XTK_INVALIDATE_NONE;
	uint32_t index = xent_node_index(flux_host_as_node(control));
	if (index >= R->invalidate_cap) return XTK_INVALIDATE_NONE;
	return R->invalidate [index];
}

uint32_t flux_host_take_invalidations(XtkHost *host) {
	FluxHostState *R = flux_host_state(host);
	if (!R) return XTK_INVALIDATE_NONE;
	uint32_t mask           = R->pending_invalidation;
	R->pending_invalidation = 0u;
	return mask;
}

static XtkHost *create_failed(FluxHostState *R) {
	if (R->store && R->root != XENT_NODE_INVALID) flux_tree_drop(R->store, R->root);
	if (R->store) flux_store_destroy(R->store);
	xtk_comp_destroy(R->comp);
	if (R->ctx) xent_ctx_destroy(R->ctx);
	xtk_alloc_free(R->invalidate);
	xtk_alloc_free(R->types);
	xtk_alloc_free(R);
	return NULL;
}

XtkHost *flux_host_create(void) {
	FluxHostState *R = ( FluxHostState * ) xtk_alloc_zero(1, sizeof(FluxHostState));
	if (!R) return NULL;
	R->comp = xtk_comp_create();
	R->ctx  = xent_ctx_create(NULL);
	if (!R->ctx || !R->comp) return create_failed(R);
	R->store = flux_store_create(64);
	if (!R->store || !flux_store_bind(R->store, R->ctx)) return create_failed(R);
	R->root = flux_factory_create_node(R->ctx, R->store, XENT_NODE_INVALID, FLUX_CONTROL_CONTAINER);
	if (R->root == XENT_NODE_INVALID) return create_failed(R);
	if (!flux_host_types_set(R, R->root, XTK_CTRL_CONTAINER)) return create_failed(R);
	R->owns_tree = true;
	R->host.vt   = &flux_host_vtable;
	R->host.impl = R;
	R->revision  = 1u;
	R->next_sub  = 0u;
	R->focus     = XTK_CTRL_INVALID;
	flux_store_sethost(R->store, &R->host);
	XtkCtrlDesc const *root_desc = xtk_catalog_control(XTK_CTRL_CONTAINER);
	if (!root_desc || !flux_host_note_invalidate(R, flux_host_as_control(R->root), root_desc->create_invalidates))
		return create_failed(R);
	return &R->host;
}

XtkHost *flux_host_adopt(XentCtx *ctx, FluxStore *store, XentNodeId root) {
	if (!ctx || !store || root == XENT_NODE_INVALID) return NULL;
	if (!flux_store_ctx(store) && !flux_store_bind(store, ctx)) return NULL;
	FluxHostState *R = ( FluxHostState * ) xtk_alloc_zero(1, sizeof(FluxHostState));
	if (!R) return NULL;
	R->comp = xtk_comp_create();
	if (!R->comp) {
		xtk_alloc_free(R);
		return NULL;
	}
	R->ctx       = ctx;
	R->store     = store;
	R->root      = root;
	R->owns_tree = false;
	if (!flux_host_types_set(R, root, XTK_CTRL_CONTAINER)) {
		xtk_comp_destroy(R->comp);
		xtk_alloc_free(R);
		return NULL;
	}
	R->host.vt   = &flux_host_vtable;
	R->host.impl = R;
	R->revision  = 1u;
	R->focus     = XTK_CTRL_INVALID;
	flux_store_sethost(R->store, &R->host);
	return &R->host;
}

void flux_host_bind_app(XtkHost *host, FluxApp *app) {
	FluxHostState *R = flux_host_state(host);
	if (!R) return;
	R->env
	  = app
	    ? (
			FluxControlEnv
		  ) {.app = app, .window = flux_app_window(app), .text = flux_app_text(app), .theme = flux_app_theme(app), .input = flux_app_input(app), .overlay = flux_app_overlay(app)}
	    : (FluxControlEnv) {0};
}

FluxStore *flux_host_store(XtkHost *host) {
	FluxHostState *R = flux_host_state(host);
	return R ? R->store : NULL;
}

XentCtx *flux_host_context(XtkHost *host) {
	FluxHostState *R = flux_host_state(host);
	return R ? R->ctx : NULL;
}

void flux_host_destroy(XtkHost *host) {
	FluxHostState *R = flux_host_state(host);
	if (!R) return;
	for (uint32_t i = 0; i < R->prop_count; i++)
		if (R->props [i].live) flux_host_value_clear(&R->props [i].value);
	xtk_alloc_free(R->props);
	xtk_alloc_free(R->subs);
	xtk_alloc_free(R->types);
	xtk_alloc_free(R->invalidate);
	xtk_comp_destroy(R->comp);
	if (R->owns_tree) {
		flux_store_destroy(R->store);
		xent_ctx_destroy(R->ctx);
	}
	xtk_alloc_free(R);
}

bool flux_host_register_component(XtkHost *host, XtkCompType const *type) {
	FluxHostState *R = flux_host_state(host);
	return R && R->comp && xtk_comp_register(R->comp, type);
}

bool flux_host_set_providers(XtkHost *host, XtkProviderDesc const *providers, uint32_t count) {
	FluxHostState *R = flux_host_state(host);
	return R && R->comp && xtk_comp_setproviders(R->comp, providers, count);
}

XtkHostVtable const flux_host_vtable = {
  .abi         = XTK_HOST_ABI_VERSION,
  .preflight   = flux_host_preflight,
  .prepare     = flux_host_prepare,
  .commit      = flux_host_commit,
  .abort       = flux_host_abort,
  .live        = flux_host_is_live,
  .revision    = flux_host_revision,
  .root        = flux_host_scene_root,
  .type        = flux_host_control_type,
  .parent      = flux_host_parent,
  .first       = flux_host_first_child,
  .next        = flux_host_next_sibling,
  .desc        = flux_host_descriptor,
  .prop        = flux_host_get_property,
  .res_valid   = flux_host_res_valid,
  .res_retain  = flux_host_resource_retain,
  .res_release = flux_host_resource_release,
  .dispatch    = flux_host_dispatch,
};
