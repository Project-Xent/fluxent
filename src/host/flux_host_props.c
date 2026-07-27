#include "flux_host_internal.h"

static bool clone_string(XtkValue *dst, XtkValue const *src) {
	uint32_t len = src->u.string.len;
	char    *dup = ( char * ) xtk_alloc(( size_t ) len + 1u);
	if (!dup) {
		dst->u.string.data = NULL;
		return false;
	}
	if (len) memcpy(dup, src->u.string.data, len);
	dup [len]          = '\0';
	dst->u.string.data = dup;
	return true;
}

static bool clone_bytes(XtkValue *dst, XtkValue const *src) {
	uint32_t len = src->u.bytes.len;
	uint8_t *dup = len ? ( uint8_t * ) xtk_alloc(len) : NULL;
	if (len && !dup) {
		dst->u.bytes.data = NULL;
		return false;
	}
	if (len) memcpy(dup, src->u.bytes.data, len);
	dst->u.bytes.data = dup;
	return true;
}

void flux_host_value_clear(XtkValue *v) {
	if (!v) return;
	if (v->kind == XTK_VALUE_STRING) xtk_alloc_free(v->u.string.data);
	if (v->kind == XTK_VALUE_BYTES) xtk_alloc_free(v->u.bytes.data);
	*v = (XtkValue) {.kind = XTK_VALUE_NONE};
}

bool flux_host_value_clone(XtkValue *dst, XtkValue const *src) {
	*dst = *src;
	if (src->kind == XTK_VALUE_STRING) return clone_string(dst, src);
	if (src->kind == XTK_VALUE_BYTES) return clone_bytes(dst, src);
	return true;
}

PropEntry *flux_host_prop_find(FluxHostState *R, XtkCtrl control, XtkPropertyId id) {
	for (uint32_t i = 0; i < R->prop_count; i++)
		if (R->props [i].live && R->props [i].control == control && R->props [i].id == id) return &R->props [i];
	return NULL;
}

PropEntry const *flux_host_prop_findc(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	for (uint32_t i = 0; i < R->prop_count; i++)
		if (R->props [i].live && R->props [i].control == control && R->props [i].id == id) return &R->props [i];
	return NULL;
}

PropEntry *flux_host_prop_add(FluxHostState *R) {
	if (R->prop_count == R->prop_cap) {
		uint32_t   cap = R->prop_cap ? R->prop_cap * 2u : 16u;
		PropEntry *n   = ( PropEntry * ) xtk_alloc_resize(R->props, sizeof(*n) * cap);
		if (!n) return NULL;
		R->props    = n;
		R->prop_cap = cap;
	}
	PropEntry *e = &R->props [R->prop_count++];
	*e           = (PropEntry) {.live = true};
	return e;
}

void flux_host_clear_props(FluxHostState *R, XtkCtrl control) {
	for (uint32_t i = 0; i < R->prop_count; i++) {
		if (!R->props [i].live || R->props [i].control != control) continue;
		flux_host_value_clear(&R->props [i].value);
		R->props [i].live = false;
	}
}

void flux_host_rollback_prop(FluxHostState *R, UndoProp *prop) {
	PropEntry *e = flux_host_prop_find(R, prop->control, prop->id);
	if (!prop->had_value) {
		if (e) {
			flux_host_value_clear(&e->value);
			e->live = false;
		}
		return;
	}
	if (!e) return;
	flux_host_value_clear(&e->value);
	e->value             = prop->old_value;
	e->change            = prop->old_change;
	prop->old_value.kind = XTK_VALUE_NONE;
}

XtkBatchError flux_host_apply_prop(Prepare *P, XtkCtrl control, XtkPropPatch const *patch) {
	FluxHostState *R   = P->R;
	PropEntry     *old = flux_host_prop_find(R, control, patch->id);
	Undo           u   = {
	  .kind = UNDO_PROP, .u.prop = {.control = control, .id = patch->id, .had_value = old != NULL}
    };
	if (old) {
		u.u.prop.old_value  = old->value;
		u.u.prop.old_change = old->change;
		old->value.kind     = XTK_VALUE_NONE;
	}
	if (!flux_host_undo_push(P, u)) {
		if (old) old->value = u.u.prop.old_value;
		return XTK_BATCH_ERR_OOM;
	}

	PropEntry *e = old ? old : flux_host_prop_add(R);
	if (!e || !flux_host_value_clone(&e->value, &patch->value)) return XTK_BATCH_ERR_OOM;
	e->control = control;
	e->id      = patch->id;
	e->change  = patch->change;
	e->live    = true;
	return flux_host_stage_prop_invalidation(P, control, patch->id) ? XTK_BATCH_OK : XTK_BATCH_ERR_OOM;
}

bool flux_host_value_view(XtkValue const *v, XtkValueView *out) {
	if (v->kind == XTK_VALUE_NONE || v->kind > XTK_VALUE_BYTES) return false;
	out->kind = v->kind;
	memcpy(&out->u, &v->u, sizeof(out->u));
	return true;
}

bool flux_host_get_property(
  XtkHost const *host, XtkCtrl control, XtkPropertyId property, XtkValueView *out_value, XtkChangeStamp *out_change
) {
	FluxHostState const *R = flux_host_state_c(host);
	if (!R || !out_value || !flux_host_is_live(host, control)) return false;
	if (property == XTK_PROP_COMPONENT_PROPS && R->comp) {
		size_t      sz   = 0u;
		void const *blob = xtk_comp_props(R->comp, control, &sz);
		if (!blob && sz != 0u) return false;
		if (out_change) *out_change = (XtkChangeStamp) {0};
		*out_value = (XtkValueView) {
		  .kind = XTK_VALUE_BYTES, .u.bytes = {.data = ( uint8_t const * ) blob, .len = ( uint32_t ) sz}
        };
		return xtk_comp_hasinst(R->comp, control);
	}
	PropEntry const *e = flux_host_prop_findc(R, control, property);
	if (!e) return false;
	if (out_change) *out_change = e->change;
	return flux_host_value_view(&e->value, out_value);
}
