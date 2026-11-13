#include "controls/behavior/flux_slider.h"

#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_value.h"
#include "render/flux_slider_geom.h"
#include "store/flux_node_data.h"

#include <fluxent/flux_host.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <xtk/xtk_catalog.h>

static float                slider_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

static FluxSliderInputData *slider_input(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = store ? flux_store_find(store, id) : NULL;
	if (!nd || nd->component_type != FLUX_CONTROL_SLIDER) return NULL;
	return ( FluxSliderInputData * ) nd->component_data;
}

static float slider_closest_step(FluxSliderData const *sd, float step_delta, float from_value) {
	float num_steps = from_value / step_delta;
	float next_step = fminf(sd->max_value, ceilf(num_steps) * step_delta);
	float prev_step = fmaxf(sd->min_value, floorf(num_steps) * step_delta);
	return (next_step - from_value < from_value - prev_step) ? next_step : prev_step;
}

static float slider_snap_delta(FluxSliderData const *sd) {
	return sd->snaps_to == FLUX_SLIDER_SNAPS_TO_TICKS ? sd->tick_frequency : sd->step_frequency;
}

static float slider_value(FluxSliderData const *sd) {
	return sd->is_range ? sd->range_value [sd->active_thumb] : sd->current_value;
}

static float slider_clamp_thumb(FluxSliderData const *sd, int thumb, float value) {
	float lo = thumb == 0 ? sd->min_value : sd->range_value [0];
	float hi = thumb == 0 ? sd->range_value [1] : sd->max_value;
	return slider_clampf(value, lo, hi);
}

static void slider_set_live(FluxSliderData *sd, float value) {
	if (sd->is_range) sd->range_live [sd->active_thumb] = slider_clamp_thumb(sd, sd->active_thumb, value);
	else sd->intermediate_value = value;
}

static void slider_route(FluxSliderInputData *sid, XtkEventId id, XtkEventPayload payload) {
	XtkHost *host = flux_store_host(sid->store);
	if (!host) return;
	XtkEvent event = {
	  .id             = id,
	  .target         = ( XtkCtrl ) sid->node,
	  .source         = ( XtkCtrl ) sid->node,
	  .current_target = ( XtkCtrl ) sid->node,
	  .phase          = XTK_EVENT_PHASE_TARGET,
	  .cause          = XTK_EVENT_CAUSE_USER,
	  .payload        = payload,
	};
	flux_host_route(host, &event);
}

static void slider_put_value(FluxSliderInputData *sid, float value) {
	FluxSliderData *sd = &sid->base;
	if (fabsf(value - sd->current_value) < 1e-6f) return;
	sd->current_value = value;
	if (sd->on_change) sd->on_change(sd->on_change_ctx, value);
	slider_route(sid, XTK_EVENT_CHANGE_F32, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_F32, .u.f32 = value});
}

static void slider_put_thumb(FluxSliderInputData *sid, int thumb, float value) {
	FluxSliderData *sd = &sid->base;
	value              = slider_clamp_thumb(sd, thumb, value);
	if (fabsf(value - sd->range_value [thumb]) < 1e-6f) return;
	sd->range_value [thumb] = value;
	XtkEventPayload payload = {.kind = XTK_EVENT_PAYLOAD_RANGE};
	payload.u.range.start   = sd->range_value [0];
	payload.u.range.end     = sd->range_value [1];
	slider_route(sid, XTK_EVENT_CHANGE_RANGE, payload);
}

static void slider_commit(FluxSliderInputData *sid, float value) {
	if (sid->base.is_range) slider_put_thumb(sid, sid->base.active_thumb, value);
	else slider_put_value(sid, value);
}

static int slider_pick_thumb(FluxSliderData const *sd, FluxSliderLine const *line, float px, float py) {
	float pos   = line->vertical ? py : px;
	float range = fmaxf(sd->max_value - sd->min_value, 1e-6f);
	float at0   = flux_slider_along(line, (sd->range_value [0] - sd->min_value) / range);
	float at1   = flux_slider_along(line, (sd->range_value [1] - sd->min_value) / range);
	float d0    = fabsf(pos - at0);
	float d1    = fabsf(pos - at1);
	if (d0 != d1) return d1 < d0 ? 1 : 0;
	return (line->vertical ? pos < at0 : pos > at0) ? 1 : 0;
}

static bool slider_begin_drag(FluxSliderInputData *sid, FluxSliderLine const *line, float x, float y) {
	FluxSliderData *sd = &sid->base;
	sid->tip_keyboard  = false;
	if (sd->is_range) sd->active_thumb = ( uint8_t ) slider_pick_thumb(sd, line, x, y);

	float range          = sd->max_value - sd->min_value;
	float value          = slider_value(sd);
	float center         = flux_slider_along(line, (value - sd->min_value) / fmaxf(range, 1e-6f));
	float main           = line->vertical ? y : x;
	float cross          = line->vertical ? x : y;
	float radius         = sd->color_picker ? 9.0f : FLUX_SLIDER_THUMB_OUTER;
	bool  on_thumb       = fabsf(main - center) <= radius && fabsf(cross - line->cross) <= radius;

	sid->drag_origin     = main;
	sid->drag_value      = value;
	sid->drag_from_thumb = on_thumb;
	sd->thumb_held       = true;
	if (on_thumb) slider_set_live(sd, value);
	return on_thumb;
}

static float slider_drag_live(FluxSliderInputData const *sid, FluxSliderLine const *line, float x, float y) {
	FluxSliderData const *sd    = &sid->base;
	float                 range = sd->max_value - sd->min_value;
	if (!sid->drag_from_thumb) return sd->min_value + flux_slider_fraction_at(line, x, y) * range;

	float change = sd->vertical ? sid->drag_origin - y : x - sid->drag_origin;
	float offset = change * range / fmaxf(line->length, 1.0f);
	return slider_clampf(sid->drag_value + offset, sd->min_value, sd->max_value);
}

static void slider_move_trampoline(void *ctx, float local_x, float local_y) {
	FluxSliderInputData *sid  = ( FluxSliderInputData * ) ctx;
	FluxSliderData      *sd   = &sid->base;

	XentRect             rect = {0};
	if (!xent_layout_rect(sid->ctx, sid->node, &rect)) return;

	FluxSliderLine line = flux_slider_line(0.0f, 0.0f, rect.w, rect.h, sd->vertical);
	if (sd->color_picker) {
		line.start  = 9.0f;
		line.length = fmaxf(0.0f, rect.w - 18.0f);
	}
	if (!sd->thumb_held && slider_begin_drag(sid, &line, local_x, local_y)) return;

	float live = slider_drag_live(sid, &line, local_x, local_y);
	slider_set_live(sd, live);

	float delta = slider_snap_delta(sd);
	slider_commit(sid, delta > 0.0f ? slider_closest_step(sd, delta, live) : live);
}

static void slider_release(void *ctx) {
	FluxSliderInputData *sid = ( FluxSliderInputData * ) ctx;
	if (sid) sid->base.thumb_held = false;
}

static void slider_step(FluxSliderInputData *sid, bool forward) {
	FluxSliderData *sd         = &sid->base;
	float           step_delta = sd->snaps_to == FLUX_SLIDER_SNAPS_TO_TICKS ? sd->tick_frequency : sd->small_change;
	if (step_delta <= 0.0f) return;

	float value = slider_value(sd);
	float closest;
	if (!forward && fabsf(value - sd->max_value) < 1e-6f && fabsf(remainderf(value, step_delta)) > 1e-6f)
		closest = floorf(value / step_delta) * step_delta;
	else closest = slider_closest_step(sd, step_delta, forward ? value + step_delta : value - step_delta);

	slider_set_live(sd, closest);
	slider_commit(sid, closest);
}

static void slider_jump_to(FluxSliderInputData *sid, float value) {
	slider_set_live(&sid->base, value);
	slider_commit(sid, value);
}

static bool slider_dispatch_key(FluxSliderInputData *sid, FluxKey key) {
	FluxSliderData *sd = &sid->base;
	switch (key) {
	case FLUX_KEY_LEFT  :
	case FLUX_KEY_DOWN  : slider_step(sid, false); return true;
	case FLUX_KEY_RIGHT :
	case FLUX_KEY_UP    : slider_step(sid, true); return true;
	case FLUX_KEY_HOME  : slider_jump_to(sid, sd->min_value); return true;
	case FLUX_KEY_END   : slider_jump_to(sid, sd->max_value); return true;
	default             : return false;
	}
}

static bool slider_on_key(void *ctx, FluxKey key, bool down) {
	FluxSliderInputData *sid = ( FluxSliderInputData * ) ctx;
	if (!down || !slider_dispatch_key(sid, key)) return false;
	sid->tip_keyboard = true;
	return true;
}

static void slider_on_blur(void *ctx) { (( FluxSliderInputData * ) ctx)->tip_keyboard = false; }

static int  slider_tip_decimals(float step) {
	int places = 0;
	for (; places < 4 && fabsf(step - roundf(step)) > 0.00001f; ++places) step *= 10.0f;
	return places;
}

static bool slider_auto_tooltip(void *ctx, char *text, size_t cap, FluxRect *local, FluxPlacement *placement) {
	FluxSliderInputData *sid = ( FluxSliderInputData * ) ctx;
	FluxSliderData      *sd  = &sid->base;
	XentRect             r   = {0};
	if (sd->color_picker) return false;
	if (!(sd->thumb_held || sid->tip_keyboard) || !xent_layout_rect(sid->ctx, sid->node, &r)) return false;
	float          held  = sd->is_range ? sd->range_live [sd->active_thumb] : sd->intermediate_value;
	float          value = sd->thumb_held ? held : slider_value(sd);
	FluxSliderLine line  = flux_slider_line(0.0f, 0.0f, r.w, r.h, sd->vertical);
	float          range = fmaxf(sd->max_value - sd->min_value, 1e-6f);
	float          along = flux_slider_along(&line, (value - sd->min_value) / range);
	float          o     = FLUX_SLIDER_THUMB_OUTER;
	*local               = sd->vertical ? (FluxRect) {line.cross - o, along - o, o * 2.0f, o * 2.0f}
	                                    : (FluxRect) {along - o, line.cross - o, o * 2.0f, o * 2.0f};
	*placement           = sd->vertical ? FLUX_PLACEMENT_LEFT : FLUX_PLACEMENT_TOP;
	snprintf(text, cap, "%.*f", slider_tip_decimals(sd->step_frequency), value);
	return true;
}

static bool slider_on_tab(void *ctx, bool shift, bool entering) {
	FluxSliderData *sd = &(( FluxSliderInputData * ) ctx)->base;
	if (!sd->is_range) return false;
	if (entering) {
		sd->active_thumb = shift ? 1u : 0u;
		return false;
	}
	int next = ( int ) sd->active_thumb + (shift ? -1 : 1);
	if (next < 0 || next > 1) return false;
	sd->active_thumb = ( uint8_t ) next;
	return true;
}

static void slider_wire(FluxNodeData *nd, FluxSliderInputData *sid) {
	nd->component_data               = &sid->base;
	nd->destroy_component_data       = free;
	nd->behavior.on_pointer_move     = slider_move_trampoline;
	nd->behavior.on_pointer_move_ctx = sid;
	nd->behavior.on_click            = slider_release;
	nd->behavior.on_click_ctx        = sid;
	nd->behavior.on_cancel           = slider_release;
	nd->behavior.on_cancel_ctx       = sid;
	nd->behavior.on_key              = slider_on_key;
	nd->behavior.on_key_ctx          = sid;
	nd->behavior.on_tab              = slider_on_tab;
	nd->behavior.on_tab_ctx          = sid;
	nd->behavior.on_blur             = slider_on_blur;
	nd->behavior.on_blur_ctx         = sid;
	nd->behavior.auto_tooltip        = slider_auto_tooltip;
	nd->behavior.auto_tooltip_ctx    = sid;
}

XentNodeId flux_slider_init(FluxSliderDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SLIDER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_sem_setvalue(info->ctx, node, info->value, info->min, info->max);

	FluxNodeData        *nd  = flux_store_find(info->store, node);
	FluxSliderInputData *sid = nd ? ( FluxSliderInputData * ) calloc(1, sizeof(FluxSliderInputData)) : NULL;
	if (!nd || !sid) {
		free(sid);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}
	sid->base = (FluxSliderData) {
	  .min_value          = info->min,
	  .max_value          = info->max,
	  .current_value      = info->value,
	  .intermediate_value = info->value,
	  .step_frequency     = 1.0f,
	  .small_change       = 1.0f,
	  .large_change       = 10.0f,
	  .snaps_to           = FLUX_SLIDER_SNAPS_TO_STEP_VALUES,
	  .tick_placement     = FLUX_TICK_INLINE,
	  .range_value        = {info->min, info->max},
	  .range_live         = {info->min, info->max},
	  .on_change          = info->on_change,
	  .on_change_ctx      = info->userdata,
	};
	sid->ctx   = info->ctx;
	sid->store = info->store;
	sid->node  = node;
	slider_wire(nd, sid);

	xent_set_focusable(info->ctx, node, true);
	xent_setsize(info->ctx, node, (XentSize) {NAN, 32.0f});

	return node;
}

void flux_slider_retarget(XentCtx *ctx, FluxStore *store, XentNodeId dst) {
	FluxSliderInputData *sid = slider_input(store, dst);
	if (!sid) return;
	sid->node  = dst;
	sid->ctx   = ctx;
	sid->store = store;
	xent_setsize(ctx, dst, (XentSize) {sid->base.vertical ? 32.0f : NAN, sid->base.vertical ? NAN : 32.0f});
	xent_set_focusable(ctx, dst, true);
}

void flux_slider_setvertical(FluxStore *store, XentNodeId id, bool vertical) {
	FluxSliderInputData *sid = slider_input(store, id);
	if (sid) sid->base.vertical = vertical;
}

void flux_slider_setrange(FluxStore *store, XentNodeId id, bool is_range) {
	FluxSliderInputData *sid = slider_input(store, id);
	if (sid) sid->base.is_range = is_range;
}

void flux_slider_setthumb(FluxStore *store, XentNodeId id, int thumb, float value) {
	FluxSliderInputData *sid = slider_input(store, id);
	if (!sid || thumb < 0 || thumb > 1) return;
	FluxSliderData *sd      = &sid->base;
	float           v       = slider_clampf(value, sd->min_value, sd->max_value);
	sd->range_value [thumb] = v;
	if (!sd->thumb_held) sd->range_live [thumb] = v;
}

bool flux_slider_thumb(FluxStore *store, XentNodeId id, int thumb, float *value, float *min, float *max) {
	FluxSliderInputData *sid = slider_input(store, id);
	if (!sid || thumb < 0 || thumb > 1) return false;
	*value = sid->base.range_value [thumb];
	*min   = sid->base.min_value;
	*max   = sid->base.max_value;
	return true;
}

bool flux_slider_userthumb(FluxStore *store, XentNodeId id, int thumb, float value) {
	FluxSliderInputData *sid = slider_input(store, id);
	if (!sid || thumb < 0 || thumb > 1) return false;
	sid->base.active_thumb = ( uint8_t ) thumb;
	slider_set_live(&sid->base, value);
	slider_commit(sid, value);
	return true;
}
