#include "app.h"

static XtkMsg event_pair_msg(XtkMsg msg, XtkEventPayload const *p) {
	switch (p->kind) {
	case XTK_EVENT_PAYLOAD_QUERY :
		msg.ptr = ( void * ) p->u.query.text;
		msg.i   = p->u.query.index;
		break;
	case XTK_EVENT_PAYLOAD_RATING :
		msg.d = p->u.rating.value;
		msg.i = p->u.rating.count;
		break;
	case XTK_EVENT_PAYLOAD_INDEX_BOOL :
		msg.i = p->u.index_bool.index;
		msg.b = p->u.index_bool.value;
		break;
	case XTK_EVENT_PAYLOAD_RANGE :
		msg.f = p->u.range.start;
		msg.d = p->u.range.end;
		break;
	default : break;
	}
	return msg;
}

static XtkMsg event_payload_msg(XtkMsg msg, XtkEventPayload const *p) {
	switch (p->kind) {
	case XTK_EVENT_PAYLOAD_BOOL   : msg.b = p->u.b; break;
	case XTK_EVENT_PAYLOAD_I32    : msg.i = p->u.i32; break;
	case XTK_EVENT_PAYLOAD_F32    : msg.f = p->u.f32; break;
	case XTK_EVENT_PAYLOAD_F64    : msg.d = p->u.f64; break;
	case XTK_EVENT_PAYLOAD_STRING : msg.ptr = ( void * ) p->u.string.data; break;
	default                       : msg = event_pair_msg(msg, p); break;
	}
	return msg;
}

static XtkMsg event_msg(GalleryBind const *bind, XtkEvent const *event) {
	XtkMsg msg = event_payload_msg((XtkMsg) {.tag = bind->tag, .i = bind->i}, &event->payload);
	if (event->id == XTK_EVENT_TREE_SELECT) msg.i = msg.i >= 0 ? msg.i + 1 : 0;
	return msg;
}

void     gallery_post(GalleryApp *g, XtkMsg msg) { g->dispatch(g, msg); }

uint32_t page_post(void *arg, XtkEvent const *event) {
	GalleryBind *bind = ( GalleryBind * ) arg;
	if (!bind || !event) return XTK_EVENT_CONTINUE;
	bind->app->dispatch(bind->app, event_msg(bind, event));
	return XTK_EVENT_HANDLED;
}

XtkCtrl gallery_result_ctrl(XtkBatchResult const *result, XtkLocalId local) {
	if (local == XTK_LOCAL_INVALID) return XTK_CTRL_INVALID;
	for (uint32_t i = 0; i < result->control_count; i++)
		if (result->controls [i].local == local) return result->controls [i].control;
	return XTK_CTRL_INVALID;
}
