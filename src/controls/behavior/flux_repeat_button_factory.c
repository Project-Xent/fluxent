#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"

#include "fluxent/flux_engine.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <windows.h>

typedef struct FluxRepeatButtonInputData {
	FluxRepeatButtonData              base;
	XentNodeId                        node;
	UINT_PTR                          timer_id;
	FluxWindow                       *window;
	XentContext                      *ctx;
	bool                              pointer_inside;
	bool                              repeat_active;
	struct FluxRepeatButtonInputData *timer_next;
} FluxRepeatButtonInputData;

static FluxRepeatButtonInputData *g_repeat_timers = NULL;

static FluxRepeatButtonInputData *repeat_find_timer(UINT_PTR id) {
	for (FluxRepeatButtonInputData *rb = g_repeat_timers; rb; rb = rb->timer_next)
		if (rb->timer_id == id) return rb;
	return NULL;
}

static void repeat_track_timer(FluxRepeatButtonInputData *rb) {
	if (!rb || rb->timer_next || g_repeat_timers == rb) return;
	rb->timer_next  = g_repeat_timers;
	g_repeat_timers = rb;
}

static void repeat_untrack_timer(FluxRepeatButtonInputData *rb) {
	FluxRepeatButtonInputData **link = &g_repeat_timers;
	while (*link && *link != rb) link = &(*link)->timer_next;
	if (*link) *link = rb->timer_next;
	rb->timer_next = NULL;
}

static void CALLBACK repeat_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed) {
	( void ) hwnd;
	( void ) msg;
	( void ) elapsed;
	FluxRepeatButtonInputData *rb = repeat_find_timer(id);
	if (!rb || !rb->repeat_active) return;
	if (rb->base.on_click) rb->base.on_click(rb->base.on_click_ctx);

	uint32_t interval = rb->base.repeat_interval_ms;
	if (interval < 10) interval = 50;
	repeat_untrack_timer(rb);
	KillTimer(NULL, rb->timer_id);
	rb->timer_id = SetTimer(NULL, 0, interval, repeat_timer_proc);
	if (rb->timer_id) repeat_track_timer(rb);
}

static void repeat_start_timer(FluxRepeatButtonInputData *rb, uint32_t delay_ms) {
	if (!rb || rb->timer_id) return;
	rb->timer_id = SetTimer(NULL, 0, delay_ms, repeat_timer_proc);
	if (rb->timer_id) repeat_track_timer(rb);
}

static void repeat_stop_timer(FluxRepeatButtonInputData *rb) {
	if (!rb) return;
	if (rb->timer_id) {
		repeat_untrack_timer(rb);
		KillTimer(NULL, rb->timer_id);
		rb->timer_id = 0;
	}
	rb->pointer_inside = false;
	rb->repeat_active  = false;
}

static void repeat_pause_timer(FluxRepeatButtonInputData *rb) {
	if (!rb || !rb->timer_id) return;
	repeat_untrack_timer(rb);
	KillTimer(NULL, rb->timer_id);
	rb->timer_id = 0;
}

static void repeat_on_pointer_down(void *ctx, float x, float y, int click_count) {
	( void ) x;
	( void ) y;
	( void ) click_count;
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (!rb) return;
	if (rb->base.on_click) rb->base.on_click(rb->base.on_click_ctx);

	rb->pointer_inside = true;
	rb->repeat_active  = true;
	uint32_t delay     = rb->base.repeat_delay_ms;
	if (delay < 10) delay = 400;
	repeat_start_timer(rb, delay);
}

static void repeat_on_pointer_move(void *ctx, float local_x, float local_y) {
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (!rb || !rb->ctx) return;

	XentRect rect = {0};
	xent_get_layout_rect(rb->ctx, rb->node, &rect);
	bool inside = local_x >= 0.0f && local_x < rect.width && local_y >= 0.0f && local_y < rect.height;
	if (!inside) {
		if (rb->pointer_inside) repeat_pause_timer(rb);
		rb->pointer_inside = false;
		return;
	}

	if (rb->pointer_inside) return;

	rb->pointer_inside = true;
	if (!rb->repeat_active || rb->timer_id) return;

	uint32_t interval = rb->base.repeat_interval_ms;
	repeat_start_timer(rb, interval < 10 ? 50 : interval);
}

static void repeat_on_blur(void *ctx) { repeat_stop_timer(( FluxRepeatButtonInputData * ) ctx); }

static void repeat_on_pointer_up(void *ctx) { repeat_stop_timer(( FluxRepeatButtonInputData * ) ctx); }

static void repeat_destroy(void *component_data) {
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) component_data;
	repeat_stop_timer(rb);
	free(rb);
}

XentNodeId flux_create_repeat_button(FluxButtonCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_REPEAT_BUTTON, flux_draw_button, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, XENT_CONTROL_REPEAT_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData              *nd = flux_node_store_get(info->store, node);
	FluxRepeatButtonInputData *rb = nd ? ( FluxRepeatButtonInputData * ) calloc(1, sizeof(*rb)) : NULL;
	if (!nd || !rb) {
		free(rb);
		return node;
	}

	rb->base.label                   = info->label;
	rb->base.font_size               = 14.0f;
	rb->base.style                   = FLUX_BUTTON_STANDARD;
	rb->base.enabled                 = true;
	rb->base.repeat_delay_ms         = 400;
	rb->base.repeat_interval_ms      = 50;
	rb->base.on_click                = info->on_click;
	rb->base.on_click_ctx            = info->userdata;
	rb->node                         = node;
	rb->ctx                          = info->ctx;

	nd->component_data               = &rb->base;
	nd->destroy_component_data       = repeat_destroy;
	nd->behavior.on_click            = repeat_on_pointer_up;
	nd->behavior.on_click_ctx        = rb;
	nd->behavior.on_pointer_down     = repeat_on_pointer_down;
	nd->behavior.on_pointer_down_ctx = rb;
	nd->behavior.on_pointer_move     = repeat_on_pointer_move;
	nd->behavior.on_pointer_move_ctx = rb;
	nd->behavior.on_cancel           = repeat_on_pointer_up;
	nd->behavior.on_cancel_ctx       = rb;
	nd->behavior.on_blur             = repeat_on_blur;
	nd->behavior.on_blur_ctx         = rb;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	return node;
}
