#include "flux_host_internal.h"

enum
{
	PROP_EMPTY = 0,
	PROP_LIVE  = 1,
	PROP_TOMB  = 2,
};

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

static uint32_t prop_hash(XtkCtrl control, XtkPropertyId id) {
	uint64_t x  = control ^ (( uint64_t ) id * UINT64_C(0x9e3779b97f4a7c15));
	x          ^= x >> 30;
	x          *= UINT64_C(0xbf58476d1ce4e5b9);
	x          ^= x >> 27;
	x          *= UINT64_C(0x94d049bb133111eb);
	x          ^= x >> 31;
	return ( uint32_t ) x;
}

static PropEntry *prop_probe(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	if (!R || !R->prop_cap || control == XTK_CTRL_INVALID) return NULL;
	uint32_t mask = R->prop_cap - 1u;
	uint32_t slot = prop_hash(control, id) & mask;
	for (uint32_t probe = 0; probe < R->prop_cap; probe++) {
		PropEntry *e = &R->props [slot];
		if (e->slot_state == PROP_EMPTY) return NULL;
		if (e->slot_state == PROP_LIVE && e->control == control && e->id == id) return e;
		slot = (slot + 1u) & mask;
	}
	return NULL;
}

PropEntry *flux_host_prop_find(FluxHostState *R, XtkCtrl control, XtkPropertyId id) {
	return prop_probe(R, control, id);
}

PropEntry const *flux_host_prop_findc(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	return prop_probe(R, control, id);
}

static bool prop_rehash(FluxHostState *R, uint32_t new_cap) {
	if (( size_t ) new_cap > SIZE_MAX / sizeof(PropEntry)) return false;
	PropEntry *table = ( PropEntry * ) xtk_alloc_zero(new_cap, sizeof(PropEntry));
	if (!table) return false;
	uint32_t mask = new_cap - 1u;
	uint32_t live = 0u;
	for (uint32_t i = 0; i < R->prop_cap; i++) {
		if (R->props [i].slot_state != PROP_LIVE) continue;
		uint32_t slot = prop_hash(R->props [i].control, R->props [i].id) & mask;
		while (table [slot].slot_state == PROP_LIVE) slot = (slot + 1u) & mask;
		table [slot] = R->props [i];
		live++;
	}
	xtk_alloc_free(R->props);
	R->props      = table;
	R->prop_cap   = new_cap;
	R->prop_count = live;
	R->prop_used  = live;
	return true;
}

static bool prop_ensure_cap(FluxHostState *R) {
	if (R->prop_cap && (R->prop_used + 1u) * 10u < R->prop_cap * 7u) return true;
	uint32_t next_cap = R->prop_cap ? R->prop_cap : 32u;
	if (R->prop_cap && (R->prop_count + 1u) * 2u >= R->prop_cap) {
		if (next_cap > UINT32_MAX / 2u) return false;
		next_cap *= 2u;
	}
	return prop_rehash(R, next_cap);
}

static void prop_erase(FluxHostState *R, PropEntry *e) {
	flux_host_value_clear(&e->value);
	e->live       = false;
	e->slot_state = PROP_TOMB;
	if (R->prop_count > 0u) R->prop_count--;
}

PropEntry *flux_host_prop_put(FluxHostState *R, XtkCtrl control, XtkPropertyId id) {
	if (!R || control == XTK_CTRL_INVALID || !prop_ensure_cap(R)) return NULL;
	uint32_t   mask  = R->prop_cap - 1u;
	uint32_t   slot  = prop_hash(control, id) & mask;
	PropEntry *avail = NULL;
	for (uint32_t probe = 0; probe < R->prop_cap; probe++) {
		PropEntry *e = &R->props [slot];
		if (e->slot_state == PROP_LIVE && e->control == control && e->id == id) return e;
		if (e->slot_state == PROP_TOMB && !avail) avail = e;
		if (e->slot_state == PROP_EMPTY) {
			PropEntry *dst = avail ? avail : e;
			if (!avail) R->prop_used++;
			*dst = (PropEntry) {.control = control, .id = id, .slot_state = PROP_LIVE, .live = true};
			R->prop_count++;
			return dst;
		}
		slot = (slot + 1u) & mask;
	}
	return NULL;
}

void flux_host_clear_props(FluxHostState *R, XtkCtrl control) {
	for (uint32_t i = 0; i < R->prop_cap; i++) {
		if (R->props [i].slot_state != PROP_LIVE || R->props [i].control != control) continue;
		prop_erase(R, &R->props [i]);
	}
}

void flux_host_rollback_prop(FluxHostState *R, UndoProp *prop) {
	PropEntry *e = flux_host_prop_find(R, prop->control, prop->id);
	if (!prop->had_value) {
		if (e) prop_erase(R, e);
		return;
	}
	if (!e) return;
	flux_host_value_clear(&e->value);
	e->value             = prop->old_value;
	e->change            = prop->old_change;
	prop->old_value.kind = XTK_VALUE_NONE;
}

bool flux_host_stale_echo(FluxHostState const *R, XtkCtrl control, XtkPropPatch const *patch) {
	if (patch->change.source != ( XtkSourceId ) control) return false;
	PropEntry const *e = flux_host_prop_findc(R, control, patch->id);
	return e && e->user_revision > patch->change.revision;
}

static PropEntry *prop_entry(FluxHostState *R, XtkCtrl control, XtkPropertyId id) {
	return flux_host_prop_put(R, control, id);
}

void flux_host_mark_user(FluxHostState *R, XtkEvent *event) {
	if (event->cause != XTK_EVENT_CAUSE_USER || event->change.source != XTK_SOURCE_INVALID) return;
	event->change = (XtkChangeStamp) {.source = ( XtkSourceId ) event->target, .revision = ++R->user_revision};
	XtkPropertyId ids [XTK_MIRROR_MAX];
	uint32_t      n = xtk_catalog_mirrors(flux_host_types_get(R, flux_host_as_node(event->target)), event->id, ids);
	for (uint32_t i = 0; i < n; i++) {
		PropEntry *e = prop_entry(R, event->target, ids [i]);
		if (e) e->user_revision = event->change.revision;
	}
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

	PropEntry *e = old ? old : flux_host_prop_put(R, control, patch->id);
	if (!e || !flux_host_value_clone(&e->value, &patch->value)) return XTK_BATCH_ERR_OOM;
	e->control    = control;
	e->id         = patch->id;
	e->change     = patch->change;
	e->slot_state = PROP_LIVE;
	e->live       = true;
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
