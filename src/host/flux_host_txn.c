#include "flux_host_internal.h"

#include "controls/factory/flux_control_build.h"
#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/behavior/flux_host_menu.h"

static bool
materialize(FluxHostState *R, XentNodeId node, XtkCtrlId type, XtkPropPatch const *props, uint32_t prop_count) {
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return false;
	FluxControlMount mount = {.env = &R->env, .props = props, .prop_count = prop_count};
	return flux_control_build(R->ctx, R->store, node, ( FluxControlType ) type, &mount);
}

bool flux_host_undo_push(Prepare *P, Undo u) {
	if (P->undo_count == P->undo_cap) {
		uint32_t cap = P->undo_cap ? P->undo_cap * 2u : 16u;
		Undo    *n   = ( Undo * ) xtk_alloc_resize(P->undos, sizeof(*n) * cap);
		if (!n) return false;
		P->undos    = n;
		P->undo_cap = cap;
	}
	P->undos [P->undo_count++] = u;
	return true;
}

bool flux_host_defer_destroy(Prepare *P, XentNodeId node) {
	if (P->destroy_count == P->destroy_cap) {
		uint32_t    cap = P->destroy_cap ? P->destroy_cap * 2u : 8u;
		XentNodeId *n   = ( XentNodeId * ) xtk_alloc_resize(P->deferred_destroy, sizeof(*n) * cap);
		if (!n) return false;
		P->deferred_destroy = n;
		P->destroy_cap      = cap;
	}
	P->deferred_destroy [P->destroy_count++] = node;
	return true;
}

bool flux_host_resolve_ref(Prepare const *P, XtkNodeRef ref, bool *ok, XtkCtrl *out) {
	*ok = false;
	if (out) *out = XTK_CTRL_INVALID;
	if (ref.kind == XTK_REF_NONE) {
		*ok = true;
		return true;
	}
	if (ref.kind == XTK_REF_EXISTING) {
		*ok = flux_host_node_live(P->R, ref.u.existing);
		if (out) *out = ref.u.existing;
		return true;
	}
	if (ref.kind == XTK_REF_LOCAL && ref.u.local < P->local_count && P->locals [ref.u.local] != XTK_CTRL_INVALID) {
		*ok = true;
		if (out) *out = P->locals [ref.u.local];
		return true;
	}
	return true;
}

static bool node_in_subtree(XentCtx const *ctx, XentNodeId node, XentNodeId root) {
	for (XentNodeId cur = node; cur != XENT_NODE_INVALID; cur = xent_node_parent(ctx, cur))
		if (cur == root) return true;
	return false;
}

static bool pending_destroy_subtree(Prepare const *P, XtkCtrl control) {
	if (control == XTK_CTRL_INVALID) return true;
	XentNodeId node = flux_host_as_node(control);
	for (uint32_t i = 0; i < P->destroy_count; i++) {
		XentNodeId root = P->deferred_destroy [i];
		if (node_in_subtree(P->R->ctx, node, root)) return true;
	}
	return false;
}

static void rollback_topo(FluxHostState *R, UndoTopo const *t) {
	if (t->old_parent == XENT_NODE_INVALID) {
		XentNodeId parent = xent_node_parent(R->ctx, t->child);
		if (parent != XENT_NODE_INVALID) xent_node_remove(R->ctx, parent, t->child);
		return;
	}
	xent_node_reparent(R->ctx, t->old_parent, t->child, t->old_next);
}

static void rollback_one(FluxHostState *R, Undo *u) {
	if (u->kind == UNDO_CREATE) {
		flux_host_purge_sidecar(R, u->u.created);
		flux_tree_drop(R->store, u->u.created);
		return;
	}
	if (u->kind == UNDO_PROP) {
		flux_host_rollback_prop(R, &u->u.prop);
		return;
	}
	if (u->kind == UNDO_TOPO) {
		rollback_topo(R, &u->u.topo);
		return;
	}
	if (u->kind == UNDO_SUB) flux_host_rollback_sub(R, &u->u.sub);
}

void flux_host_rollback(Prepare *P) {
	xtk_comp_abort(P->R->comp);
	for (uint32_t i = P->undo_count; i > 0u;) rollback_one(P->R, &P->undos [--i]);
	P->undo_count = 0u;
}

void flux_host_prepare_free(Prepare *P) {
	if (!P) return;
	xtk_alloc_free(P->locals);
	xtk_alloc_free(P->undos);
	xtk_alloc_free(P->deferred_destroy);
	xtk_alloc_free(P->sub_ids);
	if (P->invalidations != P->invalidation_inline) xtk_alloc_free(P->invalidations);
	xtk_alloc_free(P);
}

static XtkBatchError diag(XtkBatchDiag *d, XtkBatchError err, uint32_t op) {
	if (d) {
		d->error     = err;
		d->failed_op = op;
	}
	return err;
}

static bool value_ok(XtkValue const *v) {
	if (!v || v->kind == XTK_VALUE_NONE || v->kind > XTK_VALUE_BYTES) return false;
	if (v->kind == XTK_VALUE_STRING) return v->u.string.len == 0u || v->u.string.data != NULL;
	if (v->kind == XTK_VALUE_BYTES) return v->u.bytes.len == 0u || v->u.bytes.data != NULL;
	return true;
}

static XtkPropertyKind value_property_kind(XtkValueKind kind) {
	static XtkPropertyKind const kinds [] = {
	  [XTK_VALUE_BOOL]     = XTK_PROP_KIND_BOOL,
	  [XTK_VALUE_I32]      = XTK_PROP_KIND_I32,
	  [XTK_VALUE_F32]      = XTK_PROP_KIND_F32,
	  [XTK_VALUE_F64]      = XTK_PROP_KIND_F64,
	  [XTK_VALUE_ENUM]     = XTK_PROP_KIND_ENUM,
	  [XTK_VALUE_COLOR]    = XTK_PROP_KIND_COLOR,
	  [XTK_VALUE_STRING]   = XTK_PROP_KIND_STRING,
	  [XTK_VALUE_RESOURCE] = XTK_PROP_KIND_RESOURCE,
	  [XTK_VALUE_BYTES]    = XTK_PROP_KIND_BYTES,
	};
	if (( unsigned ) kind >= sizeof(kinds) / sizeof(kinds [0])) return 0;
	return kinds [kind];
}

static bool standard_property_ok(XtkCtrlId control, XtkPropPatch const *patch) {
	XtkPropDesc const *property = xtk_catalog_control_property(control, patch->id);
	return property && property->kind == value_property_kind(patch->value.kind);
}

static bool event_phases_ok(XtkEventId event, uint32_t phases) {
	if (!xtk_event_phases_valid(phases)) return false;
	XtkEventDesc const *descriptor = xtk_catalog_event(event);
	if (!descriptor) return true;
	uint32_t allowed = XTK_EVENT_PHASE_MASK_TARGET;
	if (descriptor->routing == XTK_EVENT_ROUTE_BUBBLE) allowed |= XTK_EVENT_PHASE_MASK_BUBBLE;
	if (descriptor->routing == XTK_EVENT_ROUTE_CAPTURE_BUBBLE) allowed = XTK_EVENT_PHASE_MASK_ALL;
	return (phases & ~allowed) == 0u;
}

typedef struct PreflightCtx {
	FluxHostState *R;
	XtkBatch      *batch;
	bool          *local_defined;
	bool          *local_destroyed;
	XtkCtrlId     *local_types;
	XtkCtrl       *destroyed;
	uint32_t       destroyed_count;
	uint32_t       destroyed_cap;
} PreflightCtx;

static void pf_ctx_dispose(PreflightCtx *ctx) {
	xtk_alloc_free(ctx->local_defined);
	xtk_alloc_free(ctx->local_destroyed);
	xtk_alloc_free(ctx->local_types);
	xtk_alloc_free(ctx->destroyed);
}

static bool pf_destroyed(PreflightCtx const *ctx, XtkCtrl control) {
	for (uint32_t i = 0; i < ctx->destroyed_count; i++)
		if (ctx->destroyed [i] == control) return true;
	return false;
}

static bool pf_mark_destroyed(PreflightCtx *ctx, XtkCtrl control) {
	if (control == XTK_CTRL_INVALID || pf_destroyed(ctx, control)) return true;
	if (ctx->destroyed_count == ctx->destroyed_cap) {
		uint32_t cap = ctx->destroyed_cap ? ctx->destroyed_cap * 2u : 8u;
		XtkCtrl *n   = ( XtkCtrl * ) xtk_alloc_resize(ctx->destroyed, sizeof(*n) * cap);
		if (!n) return false;
		ctx->destroyed     = n;
		ctx->destroyed_cap = cap;
	}
	ctx->destroyed [ctx->destroyed_count++] = control;
	return true;
}

static XtkCtrlId pf_ref_type(PreflightCtx const *ctx, XtkNodeRef ref) {
	if (ref.kind == XTK_REF_EXISTING) return flux_host_types_get(ctx->R, flux_host_as_node(ref.u.existing));
	if (ref.kind == XTK_REF_LOCAL) return ctx->local_types [ref.u.local];
	return XTK_CTRL_ID_INVALID;
}

static XtkBatchError pf_ref(PreflightCtx *ctx, XtkNodeRef ref, bool allow_none) {
	if (ref.kind == XTK_REF_NONE) return allow_none ? XTK_BATCH_OK : XTK_BATCH_ERR_INVALID_LOCAL;
	if (ref.kind == XTK_REF_EXISTING) {
		if (ref.u.existing == XTK_CTRL_INVALID
			|| !flux_host_node_live(ctx->R, ref.u.existing)
			|| pf_destroyed(ctx, ref.u.existing))
			return XTK_BATCH_ERR_INVALID_HANDLE;
		return XTK_BATCH_OK;
	}
	if (ref.kind != XTK_REF_LOCAL) return XTK_BATCH_ERR_INVALID_LOCAL;
	if (ref.u.local >= ctx->batch->next_local
		|| !ctx->local_defined [ref.u.local]
		|| ctx->local_destroyed [ref.u.local])
		return XTK_BATCH_ERR_INVALID_LOCAL;
	return XTK_BATCH_OK;
}

static XtkBatchError pf_refs(PreflightCtx *ctx, XtkNodeRef a, XtkNodeRef b, XtkNodeRef before) {
	XtkBatchError err = pf_ref(ctx, a, false);
	if (err != XTK_BATCH_OK) return err;
	err = pf_ref(ctx, b, false);
	if (err != XTK_BATCH_OK) return err;
	return pf_ref(ctx, before, true);
}

static bool prop_id_in_control(XtkCtrlDesc const *desc, XtkPropertyId id) {
	if (!desc || !desc->properties) return false;
	for (uint32_t i = 0; i < desc->property_count; i++)
		if (desc->properties [i] == id) return true;
	return false;
}

static XtkBatchError pf_component_patch(XtkCompType const *type, XtkPropPatch const *patch) {
	if (!value_ok(&patch->value) || !xtk_change_stamp_valid(patch->change)) return XTK_BATCH_ERR_INVALID_VALUE;
	if (patch->id == XTK_PROP_COMPONENT_PROPS) {
		if (patch->value.kind != XTK_VALUE_BYTES) return XTK_BATCH_ERR_INVALID_VALUE;
		if (patch->value.u.bytes.len != ( uint32_t ) type->props_size) return XTK_BATCH_ERR_INVALID_VALUE;
		return XTK_BATCH_OK;
	}
	if (!prop_id_in_control(type->control, patch->id)) return XTK_BATCH_ERR_INVALID_PROPERTY;
	XtkPropDesc const *pd = xtk_catalog_property(patch->id);
	if (!pd || pd->kind != value_property_kind(patch->value.kind)) return XTK_BATCH_ERR_INVALID_PROPERTY;
	return XTK_BATCH_OK;
}

static XtkBatchError pf_create_type(PreflightCtx *ctx, XtkCtrlId id, XtkCompType const **out) {
	*out = NULL;
	if (xtk_catalog_standard(id)) return XTK_BATCH_OK;
	if (!xtk_ctrl_is_component(id) || !ctx->R->comp) return XTK_BATCH_ERR_VALIDATION;
	*out = xtk_comp_lookup(ctx->R->comp, id);
	return *out ? XTK_BATCH_OK : XTK_BATCH_ERR_VALIDATION;
}

static XtkBatchError pf_create_patch(XtkBatchCreate const *op, XtkCompType const *ctype, XtkPropPatch const *patch) {
	if (ctype) return pf_component_patch(ctype, patch);
	if (!xtk_catalog_control_property(op->type, patch->id)) return XTK_BATCH_ERR_INVALID_PROPERTY;
	if (!value_ok(&patch->value) || !xtk_change_stamp_valid(patch->change)) return XTK_BATCH_ERR_INVALID_VALUE;
	if (!standard_property_ok(op->type, patch)) return XTK_BATCH_ERR_INVALID_PROPERTY;
	return flux_host_can_project(op->type, patch) ? XTK_BATCH_OK : XTK_BATCH_ERR_BACKEND;
}

static XtkBatchError pf_create_props(XtkBatchCreate const *op, XtkCompType const *ctype) {
	for (uint32_t i = 0; i < op->prop_count; i++) {
		XtkBatchError err = pf_create_patch(op, ctype, &op->props [i]);
		if (err != XTK_BATCH_OK) return err;
	}
	return XTK_BATCH_OK;
}

static XtkBatchError pf_create(PreflightCtx *ctx, XtkBatchCreate const *op) {
	XtkCompType const *ctype = NULL;
	XtkBatchError      err   = pf_create_type(ctx, op->type, &ctype);
	if (err != XTK_BATCH_OK) return err;
	err = pf_ref(ctx, op->parent, true);
	if (err != XTK_BATCH_OK) return err;
	err = pf_ref(ctx, op->before, true);
	if (err != XTK_BATCH_OK) return err;
	err = pf_create_props(op, ctype);
	if (err != XTK_BATCH_OK) return err;
	if (op->local >= ctx->batch->next_local) return XTK_BATCH_ERR_DUPLICATE_LOCAL;
	ctx->local_defined [op->local] = true;
	ctx->local_types [op->local]   = op->type;
	return XTK_BATCH_OK;
}

static XtkBatchError pf_standard_patch(XtkCtrlId type, XtkPropPatch const *patch) {
	if (!xtk_catalog_control_property(type, patch->id)) return XTK_BATCH_ERR_INVALID_PROPERTY;
	if (!xtk_catalog_standard(type)) return XTK_BATCH_OK;
	if (!standard_property_ok(type, patch)) return XTK_BATCH_ERR_INVALID_PROPERTY;
	return flux_host_can_project(type, patch) ? XTK_BATCH_OK : XTK_BATCH_ERR_BACKEND;
}

static XtkBatchError pf_set_prop(PreflightCtx *ctx, XtkBatchSetProp const *op) {
	XtkBatchError err = pf_ref(ctx, op->target, false);
	if (err != XTK_BATCH_OK) return err;
	if (op->patch.id == XTK_PROP_ID_INVALID || !value_ok(&op->patch.value) || !xtk_change_stamp_valid(op->patch.change))
		return XTK_BATCH_ERR_INVALID_VALUE;
	XtkCtrlId type = pf_ref_type(ctx, op->target);
	if (xtk_ctrl_is_component(type)) {
		XtkCompType const *ctype = ctx->R->comp ? xtk_comp_lookup(ctx->R->comp, type) : NULL;
		if (!ctype) return XTK_BATCH_ERR_VALIDATION;
		return pf_component_patch(ctype, &op->patch);
	}
	return pf_standard_patch(type, &op->patch);
}

static XtkBatchError pf_destroy(PreflightCtx *ctx, XtkNodeRef target) {
	XtkBatchError err = pf_ref(ctx, target, false);
	if (err != XTK_BATCH_OK) return err;
	if (target.kind == XTK_REF_LOCAL) {
		ctx->local_destroyed [target.u.local] = true;
		return XTK_BATCH_OK;
	}
	return pf_mark_destroyed(ctx, target.u.existing) ? XTK_BATCH_OK : XTK_BATCH_ERR_OOM;
}

static XtkBatchError pf_subscribe(PreflightCtx *ctx, XtkBatchSubscribe const *op) {
	XtkBatchError err = pf_ref(ctx, op->target, false);
	if (err != XTK_BATCH_OK) return err;
	if (!op->fn || !event_phases_ok(op->event, op->phases) || op->token >= ctx->batch->next_sub)
		return XTK_BATCH_ERR_VALIDATION;
	XtkCtrlId type = pf_ref_type(ctx, op->target);
	if (xtk_catalog_standard(type) && !xtk_catalog_control_event(type, op->event)) return XTK_BATCH_ERR_VALIDATION;
	return XTK_BATCH_OK;
}

static XtkBatchError pf_unsubscribe(PreflightCtx *ctx, XtkSubId id) {
	for (uint32_t i = 0; i < ctx->R->sub_count; i++)
		if (ctx->R->subs [i].live && ctx->R->subs [i].id == id) return XTK_BATCH_OK;
	return XTK_BATCH_ERR_INVALID_HANDLE;
}

static XtkBatchError pf_op(PreflightCtx *ctx, XtkBatchOp const *op) {
	if (op->kind == XTK_BATCH_OP_CREATE) return pf_create(ctx, &op->u.create);
	if (op->kind == XTK_BATCH_OP_DESTROY) return pf_destroy(ctx, op->u.destroy.target);
	if (op->kind == XTK_BATCH_OP_SET_PROP) return pf_set_prop(ctx, &op->u.set_prop);
	if (op->kind == XTK_BATCH_OP_INSERT)
		return pf_refs(ctx, op->u.insert.parent, op->u.insert.child, op->u.insert.before);
	if (op->kind == XTK_BATCH_OP_MOVE) return pf_refs(ctx, op->u.move.parent, op->u.move.child, op->u.move.before);
	if (op->kind == XTK_BATCH_OP_REPARENT)
		return pf_refs(ctx, op->u.reparent.new_parent, op->u.reparent.child, op->u.reparent.before);
	if (op->kind == XTK_BATCH_OP_SUBSCRIBE) return pf_subscribe(ctx, &op->u.subscribe);
	if (op->kind == XTK_BATCH_OP_UNSUBSCRIBE) return pf_unsubscribe(ctx, op->u.unsubscribe.id);
	return XTK_BATCH_ERR_VALIDATION;
}

XtkBatchError flux_host_preflight(XtkHost *host, XtkBatch *batch, XtkBatchDiag *diagnostic) {
	FluxHostState *R = flux_host_state(host);
	if (!R || !batch) return diag(diagnostic, XTK_BATCH_ERR_VALIDATION, UINT32_MAX);
	if (batch->revision != R->revision) return diag(diagnostic, XTK_BATCH_ERR_STALE_REVISION, UINT32_MAX);

	XtkBatchError structural = xtk_batch_preflight(batch, diagnostic);
	if (structural != XTK_BATCH_OK) return structural;

	PreflightCtx ctx = {.R = R, .batch = batch};
	if (batch->next_local) {
		ctx.local_defined   = ( bool * ) xtk_alloc_zero(batch->next_local, sizeof(bool));
		ctx.local_destroyed = ( bool * ) xtk_alloc_zero(batch->next_local, sizeof(bool));
		ctx.local_types     = ( XtkCtrlId * ) xtk_alloc_zero(batch->next_local, sizeof(XtkCtrlId));
		if (!ctx.local_defined || !ctx.local_destroyed || !ctx.local_types) {
			pf_ctx_dispose(&ctx);
			return diag(diagnostic, XTK_BATCH_ERR_OOM, UINT32_MAX);
		}
	}

	for (uint32_t i = 0; i < batch->count; i++) {
		XtkBatchError err = pf_op(&ctx, &batch->ops [i]);
		if (err != XTK_BATCH_OK) {
			pf_ctx_dispose(&ctx);
			return diag(diagnostic, err, i);
		}
	}

	pf_ctx_dispose(&ctx);
	return diag(diagnostic, XTK_BATCH_OK, UINT32_MAX);
}

XtkBatchError flux_host_prepare(XtkHost *host, XtkBatch *batch, XtkPrepare **out_prepare, XtkBatchDiag *diagnostic) {
	*out_prepare      = NULL;
	XtkBatchError err = flux_host_preflight(host, batch, diagnostic);
	if (err != XTK_BATCH_OK) return err;

	FluxHostState *R = flux_host_state(host);
	Prepare       *P = ( Prepare * ) xtk_alloc_zero(1, sizeof(Prepare));
	if (!P) return diag(diagnostic, XTK_BATCH_ERR_OOM, UINT32_MAX);
	P->R                = R;
	P->batch            = batch;
	P->local_count      = batch->next_local;
	P->sub_id_count     = batch->next_sub;
	P->invalidations    = P->invalidation_inline;
	P->invalidation_cap = FLUX_HOST_INLINE_INVALIDATIONS;
	if (P->local_count) {
		P->locals = ( XtkCtrl * ) xtk_alloc_zero(P->local_count, sizeof(XtkCtrl));
		if (!P->locals) {
			flux_host_prepare_free(P);
			return diag(diagnostic, XTK_BATCH_ERR_OOM, UINT32_MAX);
		}
	}
	if (P->sub_id_count) {
		P->sub_ids = ( XtkSubId * ) xtk_alloc_zero(P->sub_id_count, sizeof(XtkSubId));
		if (!P->sub_ids) {
			flux_host_prepare_free(P);
			return diag(diagnostic, XTK_BATCH_ERR_OOM, UINT32_MAX);
		}
	}
	if (R->comp) xtk_comp_lockproviders(R->comp);
	*out_prepare = ( XtkPrepare * ) P;
	return diag(diagnostic, XTK_BATCH_OK, UINT32_MAX);
}

static XtkBatchError capture_topo(Prepare *P, XentNodeId child) {
	XentNodeId parent = xent_node_parent(P->R->ctx, child);
	Undo       u      = {
	  .kind   = UNDO_TOPO,
	  .u.topo = {
				 .child      = child,
				 .old_parent = parent,
				 .old_next   = parent == XENT_NODE_INVALID ? XENT_NODE_INVALID : xent_node_next(P->R->ctx, child),
				 }
    };
	return flux_host_undo_push(P, u) ? XTK_BATCH_OK : XTK_BATCH_ERR_OOM;
}

static bool is_list_control(XtkCtrlId type) {
	return type == XTK_CTRL_LIST
	    || type == XTK_CTRL_LISTBOX
	    || type == XTK_CTRL_GRIDVIEW
	    || type == XTK_CTRL_REPEATER
	    || type == XTK_CTRL_ITEMSVIEW;
}

typedef XentNodeId (*ContentNodeFn)(FluxStore *store, XentNodeId node);

typedef struct ContentRoute {
	XtkCtrlId     type;
	ContentNodeFn node;
} ContentRoute;

/* Public children attach under backend content hosts, not the control root. */
static XentNodeId content_parent(FluxHostState *R, XentNodeId parent) {
	static ContentRoute const routes [] = {
	  {XTK_CTRL_LIST,      flux_list_content_node   },
	  {XTK_CTRL_LISTBOX,   flux_list_content_node   },
	  {XTK_CTRL_GRIDVIEW,  flux_list_content_node   },
	  {XTK_CTRL_REPEATER,  flux_list_content_node   },
	  {XTK_CTRL_ITEMSVIEW, flux_list_content_node   },
	  {XTK_CTRL_NAV,       flux_nav_content_node    },
	  {XTK_CTRL_EXPANDER,  flux_expbody_node        },
	  {XTK_CTRL_DIALOG,    flux_dialogbody_node     },
	  {XTK_CTRL_FLIP,      flux_flip_content_node   },
	  {XTK_CTRL_REFRESH,   flux_refresh_content_node},
	  {XTK_CTRL_TABS,      flux_tabs_content_node   },
	  {XTK_CTRL_MENUBAR,   flux_menubar_content_node},
	};
	XtkCtrlId type = flux_host_types_get(R, parent);
	for (uint32_t i = 0; i < sizeof(routes) / sizeof(routes [0]); i++) {
		if (routes [i].type != type) continue;
		XentNodeId host = routes [i].node(R->store, parent);
		return host != XENT_NODE_INVALID ? host : parent;
	}
	return parent;
}

static bool stage_place_invalidations(Prepare *P, XentNodeId parent, XentNodeId previous) {
	uint32_t mask = XTK_INVALIDATE_LAYOUT | XTK_INVALIDATE_HIT_TEST | XTK_INVALIDATE_PAINT;
	if (!flux_host_stage_invalidate(P, flux_host_as_control(parent), mask)) return false;
	if (previous == XENT_NODE_INVALID || previous == parent) return true;
	return flux_host_stage_invalidate(P, flux_host_as_control(previous), mask);
}

static XtkBatchError place(Prepare *P, XentNodeId parent, XentNodeId child, XentNodeId before) {
	parent                = content_parent(P->R, parent);
	XtkBatchError cap_err = capture_topo(P, child);
	if (cap_err != XTK_BATCH_OK) return cap_err;
	XentNodeId cur = xent_node_parent(P->R->ctx, child);
	bool       ok  = false;
	if (cur == XENT_NODE_INVALID) ok = xent_node_insert(P->R->ctx, parent, child, before);
	else if (cur == parent) ok = xent_node_move(P->R->ctx, parent, child, before);
	else ok = xent_node_reparent(P->R->ctx, parent, child, before);
	/* Tree diverged from what preflight validated (e.g. before left the parent). */
	if (!ok) return XTK_BATCH_ERR_VALIDATION;
	/* LIST_ITEM seeds with no parent; bind owner once under the content host. */
	if (flux_host_types_get(P->R, child) == XTK_CTRL_LISTITEM) fluxlistitem_bind_owner(P->R->store, child);
	return stage_place_invalidations(P, parent, cur) ? XTK_BATCH_OK : XTK_BATCH_ERR_OOM;
}

static XtkBatchError store_create_props(Prepare *P, XentNodeId node, XtkPropPatch const *props, uint32_t prop_count) {
	for (uint32_t i = 0; i < prop_count; i++) {
		XtkBatchError err = flux_host_apply_prop(P, flux_host_as_control(node), &props [i]);
		if (err != XTK_BATCH_OK) return err;
	}
	return XTK_BATCH_OK;
}

static XtkBatchError create_node(
  Prepare *P, XtkCtrlId type, XtkLocalId local, XtkPropPatch const *props, uint32_t prop_count, XentNodeId *out
) {
	XentNodeId node = flux_factory_create_node(P->R->ctx, P->R->store, XENT_NODE_INVALID, ( FluxControlType ) type);
	if (node == XENT_NODE_INVALID) return XTK_BATCH_ERR_OOM;
	if (!flux_host_types_set(P->R, node, type)
		|| !flux_host_undo_push(P, (Undo) {.kind = UNDO_CREATE, .u.created = node}))
	{
		flux_factory_fail_node(P->R->ctx, P->R->store, node);
		return XTK_BATCH_ERR_OOM;
	}
	P->locals [local] = flux_host_as_control(node);
	*out              = node;
	XtkBatchError err = store_create_props(P, node, props, prop_count);
	if (err != XTK_BATCH_OK) return err;
	if (!materialize(P->R, node, type, props, prop_count)) return XTK_BATCH_ERR_OOM;
	return XTK_BATCH_OK;
}

static XtkBatchError place_refs(Prepare *P, XtkNodeRef parent_ref, XtkNodeRef before_ref, XentNodeId child) {
	bool    ok     = false;
	XtkCtrl parent = XTK_CTRL_INVALID;
	XtkCtrl before = XTK_CTRL_INVALID;
	if (!flux_host_resolve_ref(P, parent_ref, &ok, &parent) || !ok) return XTK_BATCH_ERR_VALIDATION;
	if (!flux_host_resolve_ref(P, before_ref, &ok, &before) || !ok) return XTK_BATCH_ERR_VALIDATION;
	XentNodeId parent_node = parent == XTK_CTRL_INVALID ? P->R->root : flux_host_as_node(parent);
	XentNodeId before_node = before == XTK_CTRL_INVALID ? XENT_NODE_INVALID : flux_host_as_node(before);
	return place(P, parent_node, child, before_node);
}

static XtkBatchError create_child_root(Prepare *P, XentNodeId parent, XentNodeId *out) {
	XentNodeId node = flux_factory_create_node(P->R->ctx, P->R->store, XENT_NODE_INVALID, FLUX_CONTROL_CONTAINER);
	if (node == XENT_NODE_INVALID) return XTK_BATCH_ERR_OOM;
	if (!flux_host_types_set(P->R, node, XTK_CTRL_CONTAINER)
		|| !flux_host_undo_push(P, (Undo) {.kind = UNDO_CREATE, .u.created = node}))
	{
		flux_factory_fail_node(P->R->ctx, P->R->store, node);
		return XTK_BATCH_ERR_OOM;
	}
	if (!xent_node_append(P->R->ctx, parent, node)) return XTK_BATCH_ERR_VALIDATION;
	*out = node;
	return XTK_BATCH_OK;
}

static XtkPropPatch const *find_component_props(XtkBatchCreate const *op) {
	for (uint32_t i = 0; i < op->prop_count; i++)
		if (op->props [i].id == XTK_PROP_COMPONENT_PROPS) return &op->props [i];
	return NULL;
}

static XtkBatchError create_component_roots(Prepare *P, XentNodeId node, XentNodeId roots [2]) {
	XtkBatchError err = create_child_root(P, node, &roots [0]);
	if (err != XTK_BATCH_OK) return err;
	return create_child_root(P, node, &roots [1]);
}

static XtkBatchError component_error(XtkCompErr error) {
	if (error == XTK_COMPONENT_OK) return XTK_BATCH_OK;
	if (error == XTK_COMPONENT_ERR_OOM) return XTK_BATCH_ERR_OOM;
	if (error == XTK_COMPONENT_ERR_INVALID_PROPS) return XTK_BATCH_ERR_VALIDATION;
	return XTK_BATCH_ERR_BACKEND;
}

static XtkBatchError stage_component_mount(
  Prepare *P, XtkBatchCreate const *op, XtkCompType const *type, XentNodeId node, XentNodeId const roots [2]
) {
	XtkPropPatch const *props = find_component_props(op);
	void const         *bytes = props ? props->value.u.bytes.data : NULL;
	uint32_t            len   = props ? props->value.u.bytes.len : 0u;
	return component_error(xtk_comp_prepmount(
	  P->R->comp, &P->R->host, P->R->ctx, flux_host_as_control(node), flux_host_as_control(roots [0]),
	  flux_host_as_control(roots [1]), type, bytes, len
	));
}

static XtkBatchError apply_create_component(Prepare *P, XtkBatchCreate const *op, XtkLocalId local) {
	XtkCompType const *type = xtk_comp_lookup(P->R->comp, op->type);
	if (!type) return XTK_BATCH_ERR_VALIDATION;
	XentNodeId    node;
	XtkBatchError err = create_node(P, op->type, local, op->props, op->prop_count, &node);
	if (err != XTK_BATCH_OK) return err;
	err = place_refs(P, op->parent, op->before, node);
	if (err != XTK_BATCH_OK) return err;
	XentNodeId roots [2] = {XENT_NODE_INVALID, XENT_NODE_INVALID};
	err                  = create_component_roots(P, node, roots);
	if (err != XTK_BATCH_OK) return err;
	err = stage_component_mount(P, op, type, node, roots);
	if (err != XTK_BATCH_OK) return err;
	return flux_host_stage_lifecycle(P, flux_host_as_control(node), true) ? XTK_BATCH_OK : XTK_BATCH_ERR_OOM;
}

static XtkBatchError apply_create(Prepare *P, XtkBatchCreate const *op, XtkLocalId local) {
	if (xtk_ctrl_is_component(op->type)) return apply_create_component(P, op, local);
	XentNodeId    node;
	XtkBatchError err = create_node(P, op->type, local, op->props, op->prop_count, &node);
	if (err != XTK_BATCH_OK) return err;
	err = place_refs(P, op->parent, op->before, node);
	if (err != XTK_BATCH_OK) return err;
	return flux_host_stage_lifecycle(P, flux_host_as_control(node), true) ? XTK_BATCH_OK : XTK_BATCH_ERR_OOM;
}

static XtkBatchError apply_topo_op(Prepare *P, XtkNodeRef pref, XtkNodeRef cref, XtkNodeRef bref) {
	bool    ok     = false;
	XtkCtrl parent = XTK_CTRL_INVALID;
	XtkCtrl child  = XTK_CTRL_INVALID;
	XtkCtrl before = XTK_CTRL_INVALID;
	if (!flux_host_resolve_ref(P, pref, &ok, &parent) || !ok) return XTK_BATCH_ERR_VALIDATION;
	if (!flux_host_resolve_ref(P, cref, &ok, &child) || !ok) return XTK_BATCH_ERR_VALIDATION;
	if (!flux_host_resolve_ref(P, bref, &ok, &before) || !ok) return XTK_BATCH_ERR_VALIDATION;
	XentNodeId p = parent == XTK_CTRL_INVALID ? P->R->root : flux_host_as_node(parent);
	XentNodeId b = before == XTK_CTRL_INVALID ? XENT_NODE_INVALID : flux_host_as_node(before);
	return place(P, p, flux_host_as_node(child), b);
}

static XtkBatchError apply_destroy(Prepare *P, XtkBatchOp const *op) {
	bool    ok     = false;
	XtkCtrl target = XTK_CTRL_INVALID;
	if (!flux_host_resolve_ref(P, op->u.destroy.target, &ok, &target) || !ok || !flux_host_node_live(P->R, target))
		return XTK_BATCH_ERR_VALIDATION;
	if (!flux_host_defer_destroy(P, flux_host_as_node(target))) return XTK_BATCH_ERR_OOM;
	return flux_host_stage_lifecycle(P, target, false) ? XTK_BATCH_OK : XTK_BATCH_ERR_OOM;
}

static XtkBatchError apply_set_prop(Prepare *P, XtkBatchOp const *op) {
	bool    ok     = false;
	XtkCtrl target = XTK_CTRL_INVALID;
	if (!flux_host_resolve_ref(P, op->u.set_prop.target, &ok, &target) || !ok) return XTK_BATCH_ERR_VALIDATION;
	XtkCtrlId type = flux_host_types_get(P->R, flux_host_as_node(target));
	if (op->u.set_prop.patch.id == XTK_PROP_COMPONENT_PROPS && xtk_ctrl_is_component(type)) {
		XtkValue const *v = &op->u.set_prop.patch.value;
		return component_error(
		  xtk_comp_prepupd(P->R->comp, &P->R->host, P->R->ctx, target, v->u.bytes.data, v->u.bytes.len)
		);
	}
	return flux_host_apply_prop(P, target, &op->u.set_prop.patch);
}

static XtkBatchError apply_op(Prepare *P, XtkBatchOp const *op) {
	if (op->kind == XTK_BATCH_OP_CREATE) return apply_create(P, &op->u.create, op->u.create.local);
	if (op->kind == XTK_BATCH_OP_DESTROY) return apply_destroy(P, op);
	if (op->kind == XTK_BATCH_OP_SET_PROP) return apply_set_prop(P, op);
	if (op->kind == XTK_BATCH_OP_INSERT)
		return apply_topo_op(P, op->u.insert.parent, op->u.insert.child, op->u.insert.before);
	if (op->kind == XTK_BATCH_OP_MOVE) return apply_topo_op(P, op->u.move.parent, op->u.move.child, op->u.move.before);
	if (op->kind == XTK_BATCH_OP_REPARENT)
		return apply_topo_op(P, op->u.reparent.new_parent, op->u.reparent.child, op->u.reparent.before);
	if (op->kind == XTK_BATCH_OP_SUBSCRIBE) return flux_host_apply_subscribe(P, &op->u.subscribe);
	if (op->kind == XTK_BATCH_OP_UNSUBSCRIBE) return flux_host_apply_unsubscribe(P, op->u.unsubscribe.id);
	return XTK_BATCH_ERR_VALIDATION;
}

static void finish_destroys(Prepare *P) {
	for (uint32_t i = 0; i < P->destroy_count; i++) {
		XentNodeId node = P->deferred_destroy [i];
		if (!xent_node_valid(P->R->ctx, node)) continue;
		XtkCtrl control = flux_host_as_control(node);
		flux_control_unmount(P->R->store, node, ( FluxControlType ) flux_host_types_get(P->R, node));
		if (P->R->comp && xtk_comp_hasinst(P->R->comp, control))
			xtk_comp_unmount(P->R->comp, &P->R->host, P->R->ctx, control);
		flux_host_purge_sidecar(P->R, node);
		flux_tree_drop(P->R->store, node);
	}
}

static void note_list_sync(Prepare *P, XentNodeId list, XentNodeId *lists, uint32_t *count, uint32_t cap) {
	if (list == XENT_NODE_INVALID || !is_list_control(flux_host_types_get(P->R, list))) return;
	for (uint32_t i = 0; i < *count; i++)
		if (lists [i] == list) return;
	if (*count < cap) lists [(*count)++] = list;
}

static XentNodeId list_root_from_node(Prepare *P, XentNodeId node) {
	XtkCtrlId type = flux_host_types_get(P->R, node);
	if (is_list_control(type)) return node;
	if (type != XTK_CTRL_LISTITEM) return XENT_NODE_INVALID;
	XentNodeId parent = xent_node_parent(P->R->ctx, node);
	XentNodeId scroll = parent != XENT_NODE_INVALID ? xent_node_parent(P->R->ctx, parent) : XENT_NODE_INVALID;
	return scroll != XENT_NODE_INVALID ? xent_node_parent(P->R->ctx, scroll) : XENT_NODE_INVALID;
}

static void note_list_node(Prepare *P, XentNodeId node, XentNodeId *lists, uint32_t *count, uint32_t cap) {
	note_list_sync(P, list_root_from_node(P, node), lists, count, cap);
}

static bool topology_child_ref(XtkBatchOp const *op, XtkNodeRef *out) {
	if (op->kind == XTK_BATCH_OP_MOVE) {
		*out = op->u.move.child;
		return true;
	}
	if (op->kind == XTK_BATCH_OP_INSERT) {
		*out = op->u.insert.child;
		return true;
	}
	if (op->kind == XTK_BATCH_OP_REPARENT) {
		*out = op->u.reparent.child;
		return true;
	}
	return false;
}

static void note_list_op(Prepare *P, XtkBatchOp const *op, XentNodeId *lists, uint32_t *count, uint32_t cap) {
	if (op->kind == XTK_BATCH_OP_CREATE) {
		XtkCtrl control = P->locals [op->u.create.local];
		if (control != XTK_CTRL_INVALID) note_list_node(P, flux_host_as_node(control), lists, count, cap);
		return;
	}
	XtkNodeRef ref;
	if (op->kind == XTK_BATCH_OP_SET_PROP) ref = op->u.set_prop.target;
	else if (!topology_child_ref(op, &ref)) return;
	bool    ok      = false;
	XtkCtrl control = XTK_CTRL_INVALID;
	if (flux_host_resolve_ref(P, ref, &ok, &control) && ok)
		note_list_node(P, flux_host_as_node(control), lists, count, cap);
}

static void sync_list_places(Prepare *P) {
	XentNodeId lists [64];
	uint32_t   count = 0u;
	for (uint32_t i = 0; i < P->batch->count; i++) note_list_op(P, &P->batch->ops [i], lists, &count, 64u);
	for (uint32_t i = 0; i < count; i++) flux_list_apply_pending(P->R->store, lists [i]);
}

static void note_compound(XentNodeId *seen, uint32_t *n, uint32_t cap, XentNodeId id) {
	if (id == XENT_NODE_INVALID || *n >= cap) return;
	for (uint32_t i = 0; i < *n; i++)
		if (seen [i] == id) return;
	seen [(*n)++] = id;
}

static bool op_topo_ref(XtkBatchOp const *op, XtkNodeRef *out) {
	if (op->kind == XTK_BATCH_OP_SET_PROP) {
		*out = op->u.set_prop.target;
		return true;
	}
	if (op->kind == XTK_BATCH_OP_MOVE) {
		*out = op->u.move.child;
		return true;
	}
	if (op->kind == XTK_BATCH_OP_INSERT) {
		*out = op->u.insert.child;
		return true;
	}
	if (op->kind == XTK_BATCH_OP_REPARENT) {
		*out = op->u.reparent.child;
		return true;
	}
	return false;
}

static XentNodeId op_touched_node(Prepare *P, XtkBatchOp const *op) {
	if (op->kind == XTK_BATCH_OP_CREATE) {
		XtkCtrl c = P->locals [op->u.create.local];
		return c == XTK_CTRL_INVALID ? XENT_NODE_INVALID : flux_host_as_node(c);
	}
	XtkNodeRef ref;
	if (!op_topo_ref(op, &ref)) return XENT_NODE_INVALID;
	bool    ok     = false;
	XtkCtrl target = XTK_CTRL_INVALID;
	if (!flux_host_resolve_ref(P, ref, &ok, &target) || !ok) return XENT_NODE_INVALID;
	return flux_host_as_node(target);
}

typedef struct CompoundBuckets {
	XentNodeId radios [32];
	XentNodeId menu_flies [16];
	XentNodeId menu_bars [16];
	uint32_t   n_radio;
	uint32_t   n_menu_fly;
	uint32_t   n_menu_bar;
} CompoundBuckets;

static void note_radio_parent(Prepare *P, CompoundBuckets *b, XentNodeId node) {
	XentNodeId parent = xent_node_parent(P->R->ctx, node);
	if (parent != XENT_NODE_INVALID && flux_host_types_get(P->R, parent) == XTK_CTRL_RADIOGROUP)
		note_compound(b->radios, &b->n_radio, 32u, parent);
}

static void note_menu_fly(Prepare *P, CompoundBuckets *b, XentNodeId fly) {
	note_compound(b->menu_flies, &b->n_menu_fly, 16u, fly);
	XentNodeId parent = flux_host_menufly_parent(P->R->store, fly);
	if (parent != XENT_NODE_INVALID && flux_host_types_get(P->R, parent) == XTK_CTRL_MENUBAR)
		note_compound(b->menu_bars, &b->n_menu_bar, 16u, parent);
}

static void note_compound_touch(Prepare *P, CompoundBuckets *b, XentNodeId node) {
	XtkCtrlId type = flux_host_types_get(P->R, node);
	if (type == XTK_CTRL_MENUFLY) {
		note_menu_fly(P, b, node);
		return;
	}
	if (type == XTK_CTRL_MENUITEM) {
		note_menu_fly(P, b, xent_node_parent(P->R->ctx, node));
		return;
	}
	if (type == XTK_CTRL_MENUBAR) {
		note_compound(b->menu_bars, &b->n_menu_bar, 16u, node);
		return;
	}
	if (type == XTK_CTRL_RADIOGROUP) {
		note_compound(b->radios, &b->n_radio, 32u, node);
		return;
	}
	if (type == XTK_CTRL_RADIO) {
		note_radio_parent(P, b, node);
		return;
	}
}

static void sync_compound_groups(Prepare *P) {
	CompoundBuckets b = {0};
	for (uint32_t i = 0; i < P->batch->count; i++) {
		XentNodeId node = op_touched_node(P, &P->batch->ops [i]);
		if (node != XENT_NODE_INVALID) note_compound_touch(P, &b, node);
	}
	for (uint32_t i = 0; i < b.n_radio; i++) flux_radiogroup_sync_children(P->R->store, b.radios [i]);
	for (uint32_t i = 0; i < b.n_menu_fly; i++) flux_host_menu_sync_fly(P->R->store, b.menu_flies [i]);
	for (uint32_t i = 0; i < b.n_menu_bar; i++) flux_host_menu_sync_bar(P->R->store, b.menu_bars [i]);
}

static void count_result_slots(XtkBatch const *batch, uint32_t *creates, uint32_t *subs) {
	*creates = 0u;
	*subs    = 0u;
	for (uint32_t i = 0; i < batch->count; i++) {
		*creates += batch->ops [i].kind == XTK_BATCH_OP_CREATE;
		*subs    += batch->ops [i].kind == XTK_BATCH_OP_SUBSCRIBE;
	}
}

static XtkCtrl mapped_created_control(Prepare const *P, XtkBatchCreate const *create) {
	XtkCtrl mapped = P->locals [create->local];
	return pending_destroy_subtree(P, mapped) ? XTK_CTRL_INVALID : mapped;
}

static void fill_mappings(Prepare const *P, XtkBatchResult *result) {
	uint32_t ci = 0u, si = 0u;
	for (uint32_t i = 0; i < P->batch->count; i++) {
		XtkBatchOp const *op = &P->batch->ops [i];
		if (op->kind == XTK_BATCH_OP_CREATE) {
			XtkCtrl mapped          = mapped_created_control(P, &op->u.create);
			result->controls [ci++] = (XtkControlMapping) {.local = op->u.create.local, .control = mapped};
			continue;
		}
		if (op->kind != XTK_BATCH_OP_SUBSCRIBE) continue;
		result->subscriptions [si++]
		  = (XtkSubMapping) {.token = op->u.subscribe.token, .id = P->sub_ids [op->u.subscribe.token]};
	}
}

static XtkBatchResult *fill_result(Prepare *P) {
	uint32_t creates = 0u, subs = 0u;
	count_result_slots(P->batch, &creates, &subs);
	XtkBatchResult *result = xtk_batch_result_create(creates, subs);
	if (!result) return NULL;
	fill_mappings(P, result);
	return result;
}

static void project_patch(Prepare *P, XtkCtrl control, XtkPropPatch const *patch) {
	if (!flux_host_node_live(P->R, control) || pending_destroy_subtree(P, control)) return;
	( void ) flux_host_apply_native(P->R, control, patch);
}

static void project_create_props(Prepare *P, XtkBatchCreate const *create) {
	XtkCtrl control = P->locals [create->local];
	for (uint32_t i = 0; i < create->prop_count; i++) {
		XtkPropPatch const *patch = &create->props [i];
		if (flux_control_mount_consumed(( FluxControlType ) create->type, patch->id)) continue;
		project_patch(P, control, patch);
	}
}

static void project_set_prop(Prepare *P, XtkBatchSetProp const *set) {
	bool    ok      = false;
	XtkCtrl control = XTK_CTRL_INVALID;
	if (!flux_host_resolve_ref(P, set->target, &ok, &control) || !ok) return;
	project_patch(P, control, &set->patch);
}

static void project_native_props(Prepare *P) {
	for (uint32_t i = 0; i < P->batch->count; i++) {
		XtkBatchOp const *op = &P->batch->ops [i];
		if (op->kind == XTK_BATCH_OP_CREATE) project_create_props(P, &op->u.create);
		if (op->kind == XTK_BATCH_OP_SET_PROP) project_set_prop(P, &op->u.set_prop);
	}
}

XtkBatchResult *flux_host_commit(XtkHost *host, XtkPrepare *prepare, XtkBatchDiag *diagnostic) {
	( void ) host;
	Prepare *P = ( Prepare * ) prepare;
	if (!P) return NULL;

	for (uint32_t i = 0; i < P->batch->count; i++) {
		XtkBatchError err = apply_op(P, &P->batch->ops [i]);
		if (err == XTK_BATCH_OK) continue;
		flux_host_rollback(P);
		diag(diagnostic, err, i);
		flux_host_prepare_free(P);
		return NULL;
	}

	XtkBatchResult *result = fill_result(P);
	if (!result) {
		flux_host_rollback(P);
		diag(diagnostic, XTK_BATCH_ERR_OOM, UINT32_MAX);
		flux_host_prepare_free(P);
		return NULL;
	}

	if (!flux_host_prepare_invalidations(P)) {
		xtk_batch_result_destroy(result);
		flux_host_rollback(P);
		diag(diagnostic, XTK_BATCH_ERR_OOM, UINT32_MAX);
		flux_host_prepare_free(P);
		return NULL;
	}

	XtkCompErr component_commit = xtk_comp_commit(P->R->comp);
	if (component_commit != XTK_COMPONENT_OK) {
		xtk_batch_result_destroy(result);
		flux_host_rollback(P);
		diag(diagnostic, component_error(component_commit), UINT32_MAX);
		flux_host_prepare_free(P);
		return NULL;
	}

	project_native_props(P);
	finish_destroys(P);
	sync_list_places(P);
	sync_compound_groups(P);
	flux_host_publish_invalidations(P);
	P->R->revision += 1u;
	P->undo_count   = 0u;
	flux_host_prepare_free(P);
	return result;
}

void flux_host_abort(XtkHost *host, XtkPrepare *prepare) {
	( void ) host;
	Prepare *P = ( Prepare * ) prepare;
	if (!P) return;
	flux_host_rollback(P);
	flux_host_prepare_free(P);
}
