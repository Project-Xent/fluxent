#include "controls/behavior/flux_repeat_timer.h"
#include "controls/factory/flux_factory.h"
#include "bridge/flux_internal.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_str.h"

#include "render/flux_engine.h"
#include "controls/factory/flux_init_basic.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

typedef struct FluxRepeatButtonInputData {
	FluxRepeatButtonData base;
	XentNodeId           node;
	FluxRepeatTimer      timer;
	FluxWindow          *window;
	XentCtx             *ctx;
	FluxStore           *store;
	bool                 pointer_inside;
	bool                 repeat_active;
} FluxRepeatButtonInputData;

static void repeat_notify(FluxRepeatButtonInputData *rb) {
	XtkHost *host = flux_store_host(rb->store);
	if (host) {
		flux_input_route_invoke(host, rb->node, XTK_EVENT_CAUSE_USER);
		return;
	}
	if (rb->base.on_click) rb->base.on_click(rb->base.on_click_ctx);
}

static void repeat_tick(void *ctx) {
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (rb->repeat_active) repeat_notify(rb);
}

static void repeat_start_timer(FluxRepeatButtonInputData *rb) {
	uint32_t delay    = rb->base.repeat_delay_ms;
	uint32_t interval = rb->base.repeat_interval_ms;
	flux_repeat_timer_start(&rb->timer, delay < 10 ? 400 : delay, interval < 10 ? 50 : interval, repeat_tick, rb);
}

static void repeat_stop_timer(FluxRepeatButtonInputData *rb) {
	if (!rb) return;
	flux_repeat_timer_stop(&rb->timer);
	rb->pointer_inside = false;
	rb->repeat_active  = false;
}

static void repeat_on_pointer_down(void *ctx, float x, float y, int click_count) {
	( void ) x;
	( void ) y;
	( void ) click_count;
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (!rb) return;

	rb->pointer_inside = true;
	rb->repeat_active  = true;
	repeat_start_timer(rb);
	repeat_notify(rb);
}

static void repeat_on_pointer_move(void *ctx, float local_x, float local_y) {
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (!rb || !rb->ctx) return;

	XentRect rect = {0};
	xent_layout_rect(rb->ctx, rb->node, &rect);
	bool inside = local_x >= 0.0f && local_x < rect.w && local_y >= 0.0f && local_y < rect.h;
	if (!inside) {
		if (rb->pointer_inside) flux_repeat_timer_stop(&rb->timer);
		rb->pointer_inside = false;
		return;
	}

	if (rb->pointer_inside) return;

	rb->pointer_inside = true;
	if (!rb->repeat_active || flux_repeat_timer_running(&rb->timer)) return;

	repeat_start_timer(rb);
}

static void repeat_on_blur(void *ctx) { repeat_stop_timer(( FluxRepeatButtonInputData * ) ctx); }

static void repeat_on_pointer_up(void *ctx) { repeat_stop_timer(( FluxRepeatButtonInputData * ) ctx); }

static bool repeat_on_key(void *ctx, FluxKey key, bool down) {
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (!rb || key != FLUX_KEY_SPACE) return false;
	if (!down) {
		repeat_stop_timer(rb);
		return true;
	}
	if (rb->repeat_active) return true;
	rb->repeat_active = true;
	repeat_start_timer(rb);
	repeat_notify(rb);
	return true;
}

static void repeat_destroy(void *component_data) {
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) component_data;
	if (!rb) return;
	repeat_stop_timer(rb);
	flux_str_free(rb->base.label);
	flux_str_free(rb->base.icon_name);
	free(rb);
}

XentNodeId flux_repeat_init(FluxButtonDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_REPEAT_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData              *nd = flux_store_find(info->store, node);
	FluxRepeatButtonInputData *rb = nd ? ( FluxRepeatButtonInputData * ) calloc(1, sizeof(*rb)) : NULL;
	if (!nd || !rb) {
		free(rb);
		return node;
	}

	rb->base.label                   = flux_str_dup(info->label);
	rb->base.font_size               = 14.0f;
	rb->base.style                   = FLUX_BUTTON_STANDARD;
	rb->base.repeat_delay_ms         = 400;
	rb->base.repeat_interval_ms      = 50;
	rb->base.on_click                = info->on_click;
	rb->base.on_click_ctx            = info->userdata;
	rb->node                         = node;
	rb->ctx                          = info->ctx;
	rb->store                        = info->store;

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
	nd->behavior.on_key              = repeat_on_key;
	nd->behavior.on_key_ctx          = rb;

	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_BUTTON);
	if (info->label) xent_sem_setlabel(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, info->label, {11.0f, 5.0f, 11.0f, 6.0f},
         {0.0f, 32.0f},
         {NAN, 32.0f}
    });

	return node;
}

void flux_repeat_retarget(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->component_type != FLUX_CONTROL_REPEAT_BUTTON || !nd->component_data) return;
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) nd->component_data;
	rb->node                      = node;
	rb->store                     = store;
}
