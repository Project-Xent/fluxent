/**
 * @file flux_rating.c
 * @brief RatingControl behavior (RatingControl.cpp semantics): pointer scrub +
 * hover preview, click-to-commit with clear-on-reclick, and keyboard stepping.
 * Star geometry (size 16, spacing 8 by default) is kept exact so the pointer
 * percentage → ceil(pct*max) hit-testing matches WinUI. No pointer-over scale
 * "swell": WinUI's per-star scale expression is dead code in current
 * microsoft-ui-xaml — starsScaleFocalPoint is only ever written as the -100
 * rest sentinel (nothing updates it on pointer move), so the expression
 * evaluates to the 0.5 floor and WinUI stars render statically at 16dip too.
 */
#include "controls/factory/flux_factory.h"
#include "runtime/flux_str.h"
#include "fluxent/fluxent.h"

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

static FluxRatingData *rating_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_RATING || !nd->component_data) return NULL;
	return ( FluxRatingData * ) nd->component_data;
}

static double rating_strip_w(FluxRatingData const *r) {
	return ( double ) r->max_rating * r->star_size + ( double ) (r->max_rating - 1) * r->item_spacing;
}

static void rating_notify(FluxRatingData *r) {
	if (r->on_change) r->on_change(r->on_change_ctx, r->value, r->value < 0 ? 0 : ( int ) ceil(r->value));
}

/* CoerceValueBetweenMinAndMax (RatingControl.cpp): negatives collapse to the
 * -1 "unset" sentinel, (0, 1] pins to 1, and values above MaxRating clamp. */
static double rating_coerce(double v, int max_rating) {
	if (v < 0.0) return -1.0;
	if (v <= 1.0) return 1.0;
	if (v > max_rating) return max_rating;
	return v;
}

/* SetRatingTo's commit rules: not-clearable floors at 1; an EXACT reclick of
 * the current rating toggles off (a click on star 4 over a programmatic 3.5
 * sets 4, it does not clear), with originated_from_mouse gating the max-value
 * special-case; anything non-positive clears to the unset sentinel. */
static double rating_committed_value(FluxRatingData const *r, double v, double old, bool from_mouse) {
	if (!r->is_clear_enabled && v <= 0) return 1;
	if (v == old && r->is_clear_enabled && (v != ( double ) r->max_rating || from_mouse)) return -1;
	return v > 0 ? v : -1;
}

/* SetRatingTo (RatingControl.cpp): clamp, commit, and notify whenever the
 * guard passes, like the WinUI ValueChanged event. */
static void rating_set_to(FluxRatingData *r, double v, bool from_mouse) {
	double old = r->value;
	if (v > r->max_rating) v = r->max_rating;
	if (v < 0) v = 0;

	/* "You have no rating and you pressed left": nothing should happen. */
	if (old <= -1.0 && v == 0.0) return;

	r->value = rating_committed_value(r, v, old, from_mouse);
	rating_notify(r);
}

/* ChangeRatingBy (keyboard ±1). From unset, jump to initial_set_value; a
 * programmatic fraction is dropped before stepping (3.5 steps down to 3). */
static void rating_change_by(FluxRatingData *r, int delta) {
	if (!delta) return;
	double next;
	if (r->value <= -1.0) next = r->initial_set_value;
	else if (floor(r->value) != r->value) next = delta < 0 ? floor(r->value) : floor(r->value) + delta;
	else next = r->value + delta;
	rating_set_to(r, next, false);
}

/* Pointer X (node-local) → prospective whole-star value ceil(pct*max), 0 when
 * dragged left of the first star. The caption zone right of the strip is a
 * miss for a fresh press (WinUI's caption is outside the hit-test panel), but
 * a captured drag past the right edge still scrubs to max. */
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
	( void ) x; ( void ) y; ( void ) clicks;
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (!r->is_read_only) r->pointer_down = true;
}

static void rating_click(void *ctx) {
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (r->is_read_only) return;
	if (r->hover_value >= 0) rating_set_to(r, r->hover_value, true);
	r->pointer_down = false;
}

static void rating_hover_changed(void *ctx, bool hovered) {
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (hovered) return;
	if (r->pointer_down) return; /* keep scrubbing during a drag */
	r->pointer_over = false;
	r->hover_value  = -1;
}

static void rating_cancel(void *ctx) {
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	r->pointer_down = false;
	r->pointer_over = false;
	r->hover_value  = -1;
}

static bool rating_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	FluxRatingData *r = ( FluxRatingData * ) ctx;
	if (r->is_read_only) return false;
	switch (vk) {
	case VK_LEFT :
	case VK_DOWN  : rating_change_by(r, -1); return true;
	case VK_RIGHT :
	case VK_UP    : rating_change_by(r, +1); return true;
	case VK_HOME  : rating_set_to(r, 0, false); return true;
	case VK_END   : rating_set_to(r, r->max_rating, false); return true;
	default       : return false;
	}
}

XentNodeId flux_create_rating(FluxRatingCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_RATING);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_node_store_get(info->store, node);
	FluxRatingData *r  = nd ? ( FluxRatingData * ) calloc(1, sizeof(*r)) : NULL;
	if (!nd || !r) {
		free(r);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}
	r->max_rating        = info->max_rating > 0 ? info->max_rating : 5;
	r->value             = info->value > 0.0 ? rating_coerce(info->value, r->max_rating) : -1.0; /* <=0 = unset */
	r->placeholder_value = info->placeholder_value != 0.0 ? info->placeholder_value : -1.0;
	r->initial_set_value = info->initial_set_value > 0 ? info->initial_set_value : 1;
	r->is_clear_enabled  = info->is_clear_enabled;
	r->is_read_only      = info->is_read_only;
	r->item_spacing      = info->item_spacing > 0.0 ? info->item_spacing : 8.0;
	r->star_size         = info->star_size > 0.0 ? info->star_size : 16.0;
	r->set_glyph         = info->set_glyph ? info->set_glyph : 0xE735;
	r->unset_glyph       = info->unset_glyph ? info->unset_glyph : 0xE734;
	r->caption           = info->caption ? flux_str_dup(info->caption) : NULL;
	r->hover_value       = -1.0;
	r->on_change         = info->on_change;
	r->on_change_ctx     = info->userdata;

	nd->component_data         = r;
	nd->destroy_component_data = rating_data_destroy;
	if (!r->is_read_only) {
		nd->behavior.on_pointer_move     = rating_pointer_move;
		nd->behavior.on_pointer_move_ctx = r;
		nd->behavior.on_pointer_down     = rating_pointer_down;
		nd->behavior.on_pointer_down_ctx = r;
		nd->behavior.on_click            = rating_click;
		nd->behavior.on_click_ctx        = r;
		nd->behavior.on_hover_changed    = rating_hover_changed;
		nd->behavior.on_hover_changed_ctx = r;
		nd->behavior.on_cancel           = rating_cancel;
		nd->behavior.on_cancel_ctx       = r;
		nd->behavior.on_key              = rating_key;
		nd->behavior.on_key_ctx          = r;
		xent_set_focusable(info->ctx, node, true);
	}
	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);

	/* CalculateTotalRatingControlWidth: the strip plus, when a caption is set,
	 * the 12dip caption spacing and the caption's own text width. */
	float w = ( float ) rating_strip_w(r);
	if (r->caption && r->caption [0]) {
		XentTextMetrics tm = {0};
		if (xent_measure_text(
		      info->ctx,
		      &(XentTextMeasureRequest) {
				.text              = r->caption,
				.font_size         = 12.0f,
				.line_break_policy = XENT_LINE_BREAK_NO_WRAP,
				.width_mode        = XENT_MEASURE_UNDEFINED},
		      &tm
		    ))
			w += FLUX_RATING_CAPTION_GAP + ceilf(tm.width);
	}
	xent_set_size(info->ctx, node, (XentSize) {w, 32.0f});
	return node;
}

void flux_rating_set_value(FluxNodeStore *store, XentNodeId id, double value) {
	FluxRatingData *r = rating_data(store, id);
	if (r) r->value = value > 0.0 ? rating_coerce(value, r->max_rating) : -1.0;
}

void flux_rating_set_read_only(FluxNodeStore *store, XentNodeId id, bool ro) {
	FluxRatingData *r = rating_data(store, id);
	if (r) r->is_read_only = ro;
}
