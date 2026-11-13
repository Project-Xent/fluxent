#include "controls/behavior/flux_repeat_timer.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_value.h"
#include "bridge/flux_internal.h"
#include "store/flux_node_data.h"
#include "render/flux_scroll_geom.h"

#include <math.h>
#include <stdlib.h>

#define SB_REPEAT_DELAY_MS    400u
#define SB_REPEAT_INTERVAL_MS 50u

typedef struct FluxScrollBarInputData {
	FluxScrollBarData   base;
	XentCtx            *ctx;
	FluxStore          *store;
	XentNodeId          node;

	double              drag_value;
	float               drag_origin;

	FluxRepeatTimer     repeat;
	FluxScrollEventType repeat_type;
} FluxScrollBarInputData;

static FluxScrollBarInputData *scroll_bar_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_SCROLL_BAR || !nd->component_data) return NULL;
	return ( FluxScrollBarInputData * ) nd->component_data;
}

static double scroll_bar_clamp(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void   scroll_bar_coerce(FluxScrollBarInputData *sb) {
	FluxScrollBarData *d     = &sb->base;
	double             value = scroll_bar_clamp(d->uncoerced_value, d->minimum, d->maximum);
	xent_sem_setvalue(sb->ctx, sb->node, ( float ) value, ( float ) d->minimum, ( float ) d->maximum);
	if (value == d->value) return;
	d->value = value;
	if (d->on_value_change) d->on_value_change(d->on_value_change_ctx, value);

	XtkHost *host = flux_store_host(sb->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = XTK_EVENT_CHANGE_F64,
	  .target         = ( XtkCtrl ) sb->node,
	  .source         = ( XtkCtrl ) sb->node,
	  .current_target = ( XtkCtrl ) sb->node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = {.kind = XTK_EVENT_PAYLOAD_F64, .u.f64 = value},
	};
	flux_host_route(host, &event);
}

static void scroll_bar_put_value(FluxScrollBarInputData *sb, double value) {
	sb->base.uncoerced_value = value;
	scroll_bar_coerce(sb);
}

static void scroll_bar_scroll_to(FluxScrollBarInputData *sb, double value, FluxScrollEventType type) {
	double before = sb->base.value;
	scroll_bar_put_value(sb, value);
	if (sb->base.value == before) return;
	if (sb->base.on_scroll) sb->base.on_scroll(sb->base.on_scroll_ctx, sb->base.value, type);
}

static void scroll_bar_step(FluxScrollBarInputData *sb, FluxScrollEventType type) {
	FluxScrollBarData *d       = &sb->base;
	bool               forward = type == FLUX_SCROLL_EVENT_SMALL_INCREMENT || type == FLUX_SCROLL_EVENT_LARGE_INCREMENT;
	bool               small   = type == FLUX_SCROLL_EVENT_SMALL_INCREMENT || type == FLUX_SCROLL_EVENT_SMALL_DECREMENT;
	double             change  = small ? d->small_change : d->large_change;
	double             target  = forward ? fmin(d->value + change, d->maximum) : fmax(d->value - change, d->minimum);
	scroll_bar_scroll_to(sb, target, type);
}

static void scroll_bar_repeat_tick(void *ctx) {
	FluxScrollBarInputData *sb = ( FluxScrollBarInputData * ) ctx;
	scroll_bar_step(sb, sb->repeat_type);
}

static void scroll_bar_begin_repeat(FluxScrollBarInputData *sb, FluxScrollEventType type) {
	sb->repeat_type = type;
	scroll_bar_step(sb, type);
	flux_repeat_timer_start(&sb->repeat, SB_REPEAT_DELAY_MS, SB_REPEAT_INTERVAL_MS, scroll_bar_repeat_tick, sb);
}

static void scroll_bar_release(FluxScrollBarInputData *sb) {
	if (!sb) return;
	flux_repeat_timer_stop(&sb->repeat);
	bool was_dragging          = sb->base.dragging;
	sb->base.dragging          = false;
	sb->base.dec_pressed       = 0;
	sb->base.inc_pressed       = 0;
	sb->base.large_dec_pressed = 0;
	sb->base.large_inc_pressed = 0;
	if (was_dragging && sb->base.on_scroll)
		sb->base.on_scroll(sb->base.on_scroll_ctx, sb->base.value, FLUX_SCROLL_EVENT_END_SCROLL);
}

static bool scroll_bar_geometry(FluxScrollBarInputData *sb, FluxRect *bar, FluxScrollBarParts *parts) {
	XentRect rect = {0};
	if (!xent_layout_rect(sb->ctx, sb->node, &rect)) return false;

	FluxScrollBarData const *d = &sb->base;
	*bar                       = (FluxRect) {0.0f, 0.0f, rect.w, rect.h};
	FluxScrollBarModel model   = {
	  ( float ) d->minimum, ( float ) d->maximum, ( float ) d->value, ( float ) d->viewport_size,
	  d->orientation == FLUX_SCROLLBAR_VERTICAL};
	float expanded = d->indicator_mode == FLUX_SCROLLBAR_INDICATOR_TOUCH ? 0.0f : 1.0f;
	flux_scroll_bar_parts(parts, bar, &model, expanded);
	return true;
}

static float scroll_bar_along(FluxScrollBarInputData const *sb, float x, float y) {
	return sb->base.orientation == FLUX_SCROLLBAR_VERTICAL ? y : x;
}

static void scroll_bar_drag_to(FluxScrollBarInputData *sb, float along) {
	FluxRect           bar   = {0};
	FluxScrollBarParts parts = {0};
	if (!scroll_bar_geometry(sb, &bar, &parts)) return;

	bool  vertical = sb->base.orientation == FLUX_SCROLLBAR_VERTICAL;
	float track    = vertical ? parts.track.h : parts.track.w;
	float thumb    = vertical ? parts.thumb.h : parts.thumb.w;
	float free     = track - thumb;
	if (free <= 0.0f) return;

	double range  = sb->base.maximum - sb->base.minimum;
	double offset = (( double ) (along - sb->drag_origin) / free) * range;
	if (isnan(offset) || isinf(offset)) return;
	scroll_bar_scroll_to(sb, sb->drag_value + offset, FLUX_SCROLL_EVENT_THUMB_TRACK);
}

static void scroll_bar_begin_drag(FluxScrollBarInputData *sb, float along) {
	sb->base.dragging = true;
	sb->drag_value    = sb->base.value;
	sb->drag_origin   = along;
}

static void scroll_bar_press_part(FluxScrollBarInputData *sb, FluxScrollBarParts const *parts, float x, float y) {
	if (flux_rect_contains(&parts->dec_btn, x, y)) {
		sb->base.dec_pressed = 1;
		scroll_bar_begin_repeat(sb, FLUX_SCROLL_EVENT_SMALL_DECREMENT);
		return;
	}
	if (flux_rect_contains(&parts->inc_btn, x, y)) {
		sb->base.inc_pressed = 1;
		scroll_bar_begin_repeat(sb, FLUX_SCROLL_EVENT_SMALL_INCREMENT);
		return;
	}
	if (parts->thumb_visible && flux_rect_contains(&parts->thumb, x, y)) {
		scroll_bar_begin_drag(sb, scroll_bar_along(sb, x, y));
		return;
	}
	if (flux_rect_contains(&parts->large_dec, x, y)) {
		sb->base.large_dec_pressed = 1;
		scroll_bar_begin_repeat(sb, FLUX_SCROLL_EVENT_LARGE_DECREMENT);
		return;
	}
	if (flux_rect_contains(&parts->large_inc, x, y)) {
		sb->base.large_inc_pressed = 1;
		scroll_bar_begin_repeat(sb, FLUX_SCROLL_EVENT_LARGE_INCREMENT);
	}
}

static void scroll_bar_on_pointer_down(void *ctx, float x, float y, int click_count) {
	( void ) click_count;
	FluxScrollBarInputData *sb = ( FluxScrollBarInputData * ) ctx;
	if (!sb || !sb->base.enabled) return;

	FluxRect           bar   = {0};
	FluxScrollBarParts parts = {0};
	if (!scroll_bar_geometry(sb, &bar, &parts)) return;
	scroll_bar_press_part(sb, &parts, x, y);
}

static void scroll_bar_on_pointer_move(void *ctx, float x, float y) {
	FluxScrollBarInputData *sb = ( FluxScrollBarInputData * ) ctx;
	if (!sb || !sb->base.dragging) return;
	scroll_bar_drag_to(sb, scroll_bar_along(sb, x, y));
}

static void scroll_bar_on_click(void *ctx) { scroll_bar_release(( FluxScrollBarInputData * ) ctx); }

static void scroll_bar_on_cancel(void *ctx) { scroll_bar_release(( FluxScrollBarInputData * ) ctx); }

static void scroll_bar_destroy(void *component_data) {
	FluxScrollBarInputData *sb = ( FluxScrollBarInputData * ) component_data;
	if (!sb) return;
	flux_repeat_timer_stop(&sb->repeat);
	free(sb);
}

XentNodeId flux_scroll_bar_init(FluxScrollBarDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SCROLL_BAR);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData           *nd = flux_store_find(info->store, node);
	FluxScrollBarInputData *sb = nd ? ( FluxScrollBarInputData * ) calloc(1, sizeof(FluxScrollBarInputData)) : NULL;
	if (!nd || !sb) {
		free(sb);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	sb->base.minimum                 = info->minimum;
	sb->base.maximum                 = info->maximum > info->minimum ? info->maximum : info->minimum;
	sb->base.uncoerced_value         = info->value;
	sb->base.value                   = scroll_bar_clamp(info->value, sb->base.minimum, sb->base.maximum);
	sb->base.viewport_size           = info->viewport_size;
	sb->base.small_change            = 1.0;
	sb->base.large_change            = 10.0;
	sb->base.orientation             = info->orientation;
	sb->base.indicator_mode          = FLUX_SCROLLBAR_INDICATOR_MOUSE;
	sb->base.enabled                 = true;
	sb->base.on_scroll               = info->on_scroll;
	sb->base.on_scroll_ctx           = info->userdata;
	sb->ctx                          = info->ctx;
	sb->store                        = info->store;
	sb->node                         = node;

	nd->component_data               = sb;
	nd->destroy_component_data       = scroll_bar_destroy;
	nd->behavior.on_pointer_down     = scroll_bar_on_pointer_down;
	nd->behavior.on_pointer_down_ctx = sb;
	nd->behavior.on_pointer_move     = scroll_bar_on_pointer_move;
	nd->behavior.on_pointer_move_ctx = sb;
	nd->behavior.on_click            = scroll_bar_on_click;
	nd->behavior.on_click_ctx        = sb;
	nd->behavior.on_cancel           = scroll_bar_on_cancel;
	nd->behavior.on_cancel_ctx       = sb;

	xent_sem_setvalue(
	  info->ctx, node, ( float ) sb->base.value, ( float ) sb->base.minimum, ( float ) sb->base.maximum
	);
	xent_set_focusable(info->ctx, node, false);
	sb->base.orientation
	  = info->orientation == FLUX_SCROLLBAR_VERTICAL ? FLUX_SCROLLBAR_HORIZONTAL : FLUX_SCROLLBAR_VERTICAL;
	flux_scroll_bar_setorientation(info->ctx, info->store, node, ( uint32_t ) info->orientation);
	return node;
}

void flux_scroll_bar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId id) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (!sb) return;
	sb->ctx   = ctx;
	sb->store = store;
	sb->node  = id;
}

void flux_scroll_bar_setvalue(FluxStore *store, XentNodeId id, double value) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (sb) scroll_bar_put_value(sb, value);
}

void flux_scroll_bar_setminimum(FluxStore *store, XentNodeId id, double minimum) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (!sb) return;
	sb->base.minimum = minimum;
	if (sb->base.maximum < minimum) sb->base.maximum = minimum;
	scroll_bar_coerce(sb);
}

void flux_scroll_bar_setmaximum(FluxStore *store, XentNodeId id, double maximum) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (!sb) return;
	sb->base.maximum = maximum > sb->base.minimum ? maximum : sb->base.minimum;
	scroll_bar_coerce(sb);
}

void flux_scroll_bar_setviewport(FluxStore *store, XentNodeId id, double viewport) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (sb) sb->base.viewport_size = viewport > 0.0 ? viewport : 0.0;
}

void flux_scroll_bar_setorientation(XentCtx *ctx, FluxStore *store, XentNodeId id, uint32_t orientation) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (!sb || !ctx) return;
	FluxScrollBarOrientation want
	  = orientation == FLUX_SCROLLBAR_HORIZONTAL ? FLUX_SCROLLBAR_HORIZONTAL : FLUX_SCROLLBAR_VERTICAL;
	bool turned          = want != sb->base.orientation;
	sb->base.orientation = want;

	bool vertical        = want == FLUX_SCROLLBAR_VERTICAL;
	if (turned) {
		if (vertical) xent_seth(ctx, id, NAN);
		else xent_setw(ctx, id, NAN);
	}
	if (vertical) xent_setw(ctx, id, FLUX_SCROLLBAR_FULL_SIZE);
	else xent_seth(ctx, id, FLUX_SCROLLBAR_FULL_SIZE);
}

void flux_scroll_bar_setindicator(FluxStore *store, XentNodeId id, uint32_t indicator) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (!sb || indicator > FLUX_SCROLLBAR_INDICATOR_MOUSE) return;
	sb->base.indicator_mode = ( FluxScrollBarIndicator ) indicator;
}

void flux_scroll_bar_setsmallchange(FluxStore *store, XentNodeId id, float change) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (sb) sb->base.small_change = change;
}

void flux_scroll_bar_setlargechange(FluxStore *store, XentNodeId id, float change) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (sb) sb->base.large_change = change;
}

void flux_scroll_bar_setenabled(FluxStore *store, XentNodeId id, bool enabled) {
	FluxScrollBarInputData *sb = scroll_bar_data(store, id);
	if (!sb) return;
	sb->base.enabled = enabled;
	if (!enabled) scroll_bar_release(sb);
}
