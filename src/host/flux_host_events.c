#include "flux_host_internal.h"
#include "flux_limit.h"

void flux_host_clear_subs(FluxHostState *R, XtkCtrl control) {
	for (uint32_t i = 0; i < R->sub_count; i++)
		if (R->subs [i].live && R->subs [i].target == control) R->subs [i].live = false;
}

void flux_host_rollback_sub(FluxHostState *R, UndoSub const *sub) {
	for (uint32_t s = 0; s < R->sub_count; s++) {
		if (R->subs [s].id != sub->id) continue;
		R->subs [s].live = sub->kind == SUB_UNDO_RESTORE;
		return;
	}
}

XtkBatchError flux_host_apply_subscribe(Prepare *P, XtkBatchSubscribe const *op) {
	bool    ok     = false;
	XtkCtrl target = XTK_CTRL_INVALID;
	if (!flux_host_resolve_ref(P, op->target, &ok, &target) || !ok || !flux_host_node_live(P->R, target))
		return XTK_BATCH_ERR_VALIDATION;
	if (op->token >= P->sub_id_count) return XTK_BATCH_ERR_VALIDATION;

	if (P->R->sub_count == P->R->sub_cap) {
		uint32_t  cap = P->R->sub_cap ? P->R->sub_cap * 2u : 8u;
		SubEntry *n   = ( SubEntry * ) xtk_alloc_resize(P->R->subs, sizeof(*n) * cap);
		if (!n) return XTK_BATCH_ERR_OOM;
		P->R->subs    = n;
		P->R->sub_cap = cap;
	}

	XtkSubId id = P->R->next_sub + 1u;
	if (!flux_host_undo_push(
		  P, (Undo) {
			   .kind = UNDO_SUB, .u.sub = {.id = id, .kind = SUB_UNDO_CANCEL_ADD}
    }
		))
		return XTK_BATCH_ERR_OOM;

	P->R->next_sub                 = id;
	P->R->subs [P->R->sub_count++] = (SubEntry) {
	  .id       = id,
	  .target   = target,
	  .event    = op->event,
	  .phases   = op->phases,
	  .fn       = op->fn,
	  .userdata = op->arg,
	  .live     = true};
	P->sub_ids [op->token] = id;
	return XTK_BATCH_OK;
}

XtkBatchError flux_host_apply_unsubscribe(Prepare *P, XtkSubId id) {
	for (uint32_t i = 0; i < P->R->sub_count; i++) {
		if (!P->R->subs [i].live || P->R->subs [i].id != id) continue;
		if (!flux_host_undo_push(
			  P, (Undo) {
				   .kind = UNDO_SUB, .u.sub = {.id = id, .kind = SUB_UNDO_RESTORE}
        }
			))
			return XTK_BATCH_ERR_OOM;
		P->R->subs [i].live = false;
		return XTK_BATCH_OK;
	}
	return XTK_BATCH_ERR_VALIDATION;
}

static uint32_t phase_mask_bit(XtkEventPhase phase) {
	if (phase == XTK_EVENT_PHASE_CAPTURE) return XTK_EVENT_PHASE_MASK_CAPTURE;
	if (phase == XTK_EVENT_PHASE_BUBBLE) return XTK_EVENT_PHASE_MASK_BUBBLE;
	return XTK_EVENT_PHASE_MASK_TARGET;
}

static bool sub_matches(SubEntry const *s, XtkEvent const *event) {
	if (!s->live || s->target != event->current_target || s->event != event->id) return false;
	return (s->phases & phase_mask_bit(event->phase)) != 0u;
}

static uint32_t invoke_sub(SubEntry const *s, XtkEvent const *event) {
	return s->fn ? s->fn(s->userdata, event) : XTK_EVENT_CONTINUE;
}

uint32_t flux_host_dispatch_at(FluxHostState *R, XtkEvent *event) {
	uint32_t flags = XTK_EVENT_CONTINUE;
	if (R->comp) {
		flags |= xtk_comp_dispatch(R->comp, &R->host, R->ctx, event->current_target, event);
		if (flags & XTK_EVENT_STOP_IMMEDIATE) return flags;
	}
	for (uint32_t i = 0; i < R->sub_count; i++) {
		if (!sub_matches(&R->subs [i], event)) continue;
		uint32_t d  = invoke_sub(&R->subs [i], event);
		flags      |= d;
		if (d & XTK_EVENT_STOP_IMMEDIATE) break;
	}
	return flags;
}

uint32_t flux_host_dispatch(XtkHost *host, XtkEvent const *event) {
	FluxHostState *R = flux_host_state(host);
	if (!R || !event) return XTK_EVENT_CONTINUE;
	XtkEvent copy = *event;
	return flux_host_dispatch_at(R, &copy);
}

static uint32_t ancestor_chain(FluxHostState *R, XtkCtrl target, XtkCtrl *out, uint32_t cap) {
	uint32_t count  = 0u;
	XtkCtrl  cursor = flux_host_parent(&R->host, target);
	while (cursor != XTK_CTRL_INVALID && count < cap) {
		out [count++] = cursor;
		cursor        = flux_host_parent(&R->host, cursor);
	}
	if (cursor != XTK_CTRL_INVALID && count == cap) flux_limit_exceeded(FLUX_LIMIT_EVENT_CHAIN);
	return count;
}

static uint32_t route_capture(FluxHostState *R, XtkEvent *event, XtkCtrl const *chain, uint32_t depth) {
	uint32_t flags = XTK_EVENT_CONTINUE;
	event->phase   = XTK_EVENT_PHASE_CAPTURE;
	for (uint32_t i = depth; i > 0u;) {
		event->current_target  = chain [--i];
		flags                 |= flux_host_dispatch_at(R, event);
		if (flags & (XTK_EVENT_STOP_PROPAGATION | XTK_EVENT_STOP_IMMEDIATE)) break;
	}
	return flags;
}

static uint32_t route_bubble(FluxHostState *R, XtkEvent *event, XtkCtrl const *chain, uint32_t depth) {
	uint32_t flags = XTK_EVENT_CONTINUE;
	event->phase   = XTK_EVENT_PHASE_BUBBLE;
	for (uint32_t i = 0; i < depth; i++) {
		event->current_target  = chain [i];
		flags                 |= flux_host_dispatch_at(R, event);
		if (flags & (XTK_EVENT_STOP_PROPAGATION | XTK_EVENT_STOP_IMMEDIATE)) break;
	}
	return flags;
}

static void route_begin(FluxHostState *R, XtkEvent *event, uint32_t depth) {
	event->scene_revision = R->revision;
	if (event->source == XTK_CTRL_INVALID) event->source = event->target;
}

static uint32_t route_target(FluxHostState *R, XtkEvent *event) {
	event->phase          = XTK_EVENT_PHASE_TARGET;
	event->current_target = event->target;
	return flux_host_dispatch_at(R, event);
}

uint32_t flux_host_route(XtkHost *host, XtkEvent *event) {
	FluxHostState *R = flux_host_state(host);
	if (!R || !event || !flux_host_is_live(host, event->target)) return XTK_EVENT_CONTINUE;
	XtkEventDesc const *desc    = xtk_catalog_event(event->id);
	XtkEventRouting     routing = desc ? desc->routing : XTK_EVENT_ROUTE_TARGET;
	XtkCtrl             chain [64];
	uint32_t            depth = ancestor_chain(R, event->target, chain, 64u);
	uint32_t            flags = XTK_EVENT_CONTINUE;
	route_begin(R, event, depth);
	if (routing == XTK_EVENT_ROUTE_CAPTURE_BUBBLE) {
		flags |= route_capture(R, event, chain, depth);
		if (flags & (XTK_EVENT_STOP_PROPAGATION | XTK_EVENT_STOP_IMMEDIATE)) return flags;
	}
	flags |= route_target(R, event);
	if (flags & (XTK_EVENT_STOP_PROPAGATION | XTK_EVENT_STOP_IMMEDIATE)) return flags;
	if (routing == XTK_EVENT_ROUTE_BUBBLE || routing == XTK_EVENT_ROUTE_CAPTURE_BUBBLE)
		flags |= route_bubble(R, event, chain, depth);
	return flags;
}

static bool host_subscribe_push(FluxHostState *R, SubEntry entry, XtkSubId *out_id) {
	if (R->sub_count == R->sub_cap) {
		uint32_t  cap = R->sub_cap ? R->sub_cap * 2u : 8u;
		SubEntry *n   = ( SubEntry * ) xtk_alloc_resize(R->subs, sizeof(*n) * cap);
		if (!n) return false;
		R->subs    = n;
		R->sub_cap = cap;
	}
	XtkSubId id              = R->next_sub + 1u;
	R->next_sub              = id;
	R->subs [R->sub_count++] = (SubEntry) {
	  .id       = id,
	  .target   = entry.target,
	  .event    = entry.event,
	  .phases   = entry.phases,
	  .fn       = entry.fn,
	  .userdata = entry.userdata,
	  .live     = true,
	};
	if (out_id) *out_id = id;
	return true;
}

XtkSubId
flux_host_subscribe(XtkHost *host, XtkCtrl target, XtkEventId event, uint32_t phases, XtkEventFn fn, void *userdata) {
	FluxHostState *R = flux_host_state(host);
	if (!R || !flux_host_node_live(R, target) || !xtk_event_phasesvalid(phases)) return XTK_SUBSCRIPTION_INVALID;
	XtkSubId id = XTK_SUBSCRIPTION_INVALID;
	if (!host_subscribe_push(
		  R, (SubEntry) {.target = target, .event = event, .phases = phases, .fn = fn, .userdata = userdata}, &id
		))
		return XTK_SUBSCRIPTION_INVALID;
	return id;
}

void flux_host_unsubscribe(XtkHost *host, XtkSubId id) {
	FluxHostState *R = flux_host_state(host);
	if (!R || id == XTK_SUBSCRIPTION_INVALID) return;
	for (uint32_t i = 0; i < R->sub_count; i++) {
		if (!R->subs [i].live || R->subs [i].id != id) continue;
		R->subs [i].live = false;
		return;
	}
}

bool flux_host_setfocus(XtkHost *host, XtkCtrl control) {
	FluxHostState *R = flux_host_state(host);
	if (!R) return false;
	if (control != XTK_CTRL_INVALID && !flux_host_is_live(host, control)) return false;
	if (!flux_host_note_invalidate(R, control, XTK_INVALIDATE_ACCESS | XTK_INVALIDATE_SEMANTICS)) return false;
	R->focus = control;
	return true;
}

XtkCtrl flux_host_focus(XtkHost const *host) {
	FluxHostState const *R = flux_host_state_c(host);
	return R ? R->focus : XTK_CTRL_INVALID;
}
