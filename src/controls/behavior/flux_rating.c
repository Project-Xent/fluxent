#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "bridge/flux_internal.h"
#include "runtime/flux_str.h"
#include "controls/factory/flux_init_value.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

static void rating_data_destroy(void *component_data) {
	FluxRatingData *r = ( FluxRatingData * ) component_data;
	if (!r) return;
	flux_str_free(r->caption);
	free(r);
}

static FluxRatingData *rating_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_RATING || !nd->component_data) return NULL;
	return ( FluxRatingData * ) nd->component_data;
}

static double rating_strip_w(FluxRatingData const *r) {
	return ( double ) r->max_rating * r->star_size + ( double ) (r->max_rating - 1) * r->item_spacing;
}

static void rating_notify(FluxRatingData *r) {
	int    stars                          = r->value < 0 ? 0 : ( int ) ceil(r->value);
	double value                          = r->value;
	void   (*notify)(void *, double, int) = r->on_change;
	void  *notify_ctx                     = r->on_change_ctx;
	flux_input_route_event(
	  flux_store_host(r->store), r->node, XTK_EVENT_RATING_CHANGED,
	  (XtkEventPayload) {
		.kind     = XTK_EVENT_PAYLOAD_RATING,
		.u.rating = {value, stars},
	},
	  XTK_EVENT_CAUSE_USER
	);
	if (notify) notify(notify_ctx, value, stars);
}

static double rating_coerce(double v, int max_rating) {
	if (v < 0.0) return -1.0;
	if (v <= 1.0) return 1.0;
	if (v > max_rating) return max_rating;
	return v;
}

static double rating_committed_value(FluxRatingData const *r, double v, double old, bool from_mouse) {
	if (!r->is_clear_enabled && v <= 0) return 1;
	if (v == old && r->is_clear_enabled && (v != ( double ) r->max_rating || from_mouse)) return -1;
	return v > 0 ? v : -1;
}

static void rating_set_to(FluxRatingData *r, double v, bool from_mouse) {
	double old = r->value;
	if (v > r->max_rating) v = r->max_rating;
	if (v < 0) v = 0;

	if (old <= -1.0 && v == 0.0) return;

	r->value = rating_committed_value(r, v, old, from_mouse);
	rating_notify(r);
}

static void rating_change_by(FluxRatingData *r, int delta) {
	if (!delta) return;
	double next;
	if (r->value <= -1.0) next = r->initial_set_value;
	else if (floor(r->value) != r->value) next = delta < 0 ? floor(r->value) : floor(r->value) + delta;
	else next = r->value + delta;
	rating_set_to(r, next, false);
}

static double rating_value_at(FluxRatingData const *r, float x) {
	double w = rating_strip_w(r);
	if (( double ) x > w && !r->pointer_down) return -1.0;
	double pct = ( double ) x / w;
	if (pct < 0) pct = 0;
	double v = ceil(pct * r->max_rating);
	if (v > r->max_rating) v = r->max_rating;
	return v;
}

static void rating_pointer_move(void *ctx, float x, float y) {
	( void ) y;
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (r->is_read_only) return;
	r->pointer_over = true;
	r->hover_value  = rating_value_at(r, x);
}

static void rating_pointer_down(void *ctx, float x, float y, int clicks) {
	( void ) x;
	( void ) y;
	( void ) clicks;
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (!r->is_read_only) r->pointer_down = true;
}

static void rating_click(void *ctx) {
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (r->is_read_only) return;
	double value    = r->hover_value;
	r->pointer_down = false;
	if (value >= 0) rating_set_to(r, value, true);
}

static void rating_hover_changed(void *ctx, bool hovered) {
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (hovered) return;
	if (r->pointer_down) return;
	r->pointer_over = false;
	r->hover_value  = -1;
}

static void rating_cancel(void *ctx) {
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	r->pointer_down   = false;
	r->pointer_over   = false;
	r->hover_value    = -1;
}

static bool rating_key(void *ctx, FluxKey key, bool down) {
	if (!down) return false;
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (r->is_read_only) return false;
	switch (key) {
	case FLUX_KEY_LEFT  :
	case FLUX_KEY_DOWN  : rating_change_by(r, -1); return true;
	case FLUX_KEY_RIGHT :
	case FLUX_KEY_UP    : rating_change_by(r, +1); return true;
	case FLUX_KEY_HOME  : rating_set_to(r, 0, false); return true;
	case FLUX_KEY_END   : rating_set_to(r, r->max_rating, false); return true;
	default             : return false;
	}
}

static void rating_fill_defaults(FluxRatingData *r, FluxRatingDesc const *info) {
	r->store             = info->store;
	r->max_rating        = info->max_rating > 0 ? info->max_rating : 5;
	r->value             = info->value > 0.0 ? rating_coerce(info->value, r->max_rating) : -1.0;
	r->placeholder_value = info->placeholder_value != 0.0 ? info->placeholder_value : -1.0;
	r->initial_set_value = info->initial_set_value > 0 ? info->initial_set_value : 1;
	r->is_clear_enabled  = info->is_clear_enabled;
	r->is_read_only      = info->is_read_only;
	r->item_spacing      = info->item_spacing > 0.0 ? info->item_spacing : 8.0;
	r->star_size         = info->star_size > 0.0 ? info->star_size : 16.0;
	r->set_glyph         = info->set_glyph ? info->set_glyph : 0xe735;
	r->unset_glyph       = info->unset_glyph ? info->unset_glyph : 0xe734;
	r->caption           = info->caption ? flux_str_dup(info->caption) : NULL;
	r->hover_value       = -1.0;
	r->on_change         = info->on_change;
	r->on_change_ctx     = info->userdata;
}

static void rating_wire_interactive(XentCtx *ctx, XentNodeId node, FluxNodeData *nd, FluxRatingData *r) {
	if (r->is_read_only) return;
	nd->behavior.on_pointer_move      = rating_pointer_move;
	nd->behavior.on_pointer_move_ctx  = r;
	nd->behavior.on_pointer_down      = rating_pointer_down;
	nd->behavior.on_pointer_down_ctx  = r;
	nd->behavior.on_click             = rating_click;
	nd->behavior.on_click_ctx         = r;
	nd->behavior.on_hover_changed     = rating_hover_changed;
	nd->behavior.on_hover_changed_ctx = r;
	nd->behavior.on_cancel            = rating_cancel;
	nd->behavior.on_cancel_ctx        = r;
	nd->behavior.on_key               = rating_key;
	nd->behavior.on_key_ctx           = r;
	xent_set_focusable(ctx, node, true);
}

static float rating_preferred_width(XentCtx *ctx, FluxRatingData const *r) {
	float w = ( float ) rating_strip_w(r);
	if (!r->caption || !r->caption [0]) return w;
	XentTextMetrics tm = {0};
	if (!xent_text_measure(
		  ctx,
		  &(XentTextMeasureReq) {
			.text              = r->caption,
			.font_size         = 12.0f,
			.line_break_policy = XENT_LINEBREAK_NOWRAP,
			.width_mode        = XENT_MEASURE_UNDEFINED},
		  &tm
		))
		return w;
	return w + FLUX_RATING_CAPTION_GAP + ceilf(tm.width);
}

XentNodeId flux_rating_init(FluxRatingDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_RATING);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_store_find(info->store, node);
	FluxRatingData *r  = nd ? ( FluxRatingData * ) calloc(1, sizeof(*r)) : NULL;
	if (!nd || !r) {
		free(r);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}
	rating_fill_defaults(r, info);
	r->node                    = node;
	nd->component_data         = r;
	nd->destroy_component_data = rating_data_destroy;
	rating_wire_interactive(info->ctx, node, nd, r);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_BUTTON);
	xent_setsize(info->ctx, node, (XentSize) {rating_preferred_width(info->ctx, r), 32.0f});
	return node;
}

void flux_rating_retarget(FluxStore *store, XentNodeId id) {
	FluxRatingData *r = rating_data(store, id);
	if (!r) return;
	r->store = store;
	r->node  = id;
}

void flux_rating_set_value(FluxStore *store, XentNodeId id, double value) {
	FluxRatingData *r = rating_data(store, id);
	if (r) r->value = value > 0.0 ? rating_coerce(value, r->max_rating) : -1.0;
}

void flux_rating_set_max(FluxStore *store, XentNodeId id, int max_rating) {
	FluxRatingData *r = rating_data(store, id);
	if (!r) return;
	r->max_rating = max_rating > 0 ? max_rating : 5;
	xent_setsize(flux_store_ctx(store), id, (XentSize) {rating_preferred_width(flux_store_ctx(store), r), 32.0f});
}

void flux_rating_set_caption(FluxStore *store, XentNodeId id, char const *caption) {
	FluxRatingData *r = rating_data(store, id);
	if (!r) return;
	flux_str_free(r->caption);
	r->caption = caption && caption [0] ? flux_str_dup(caption) : NULL;
	xent_setsize(flux_store_ctx(store), id, (XentSize) {rating_preferred_width(flux_store_ctx(store), r), 32.0f});
}

void flux_rating_setreadonly(FluxStore *store, XentNodeId id, bool ro) {
	FluxRatingData *r = rating_data(store, id);
	if (r) r->is_read_only = ro;
}

void flux_rating_setbehavior(
  FluxStore *store, XentNodeId id, double placeholder, int initial_value, bool clear_enabled
) {
	FluxRatingData *r = rating_data(store, id);
	if (!r) return;
	r->placeholder_value = placeholder;
	r->initial_set_value = initial_value > 0 ? initial_value : 1;
	r->is_clear_enabled  = clear_enabled;
}
