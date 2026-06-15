#include "controls/factory/flux_factory.h"
#include "controls/behavior/flux_hyperlink_action.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_str.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_engine.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static float inline fclampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

void flux_button_data_destroy(void *component_data) {
	FluxButtonData *bd = ( FluxButtonData * ) component_data;
	if (!bd) return;
	flux_str_free(bd->label);
	flux_str_free(bd->icon_name);
	free(bd);
}

static void text_data_destroy(void *component_data) {
	FluxTextData *td = ( FluxTextData * ) component_data;
	if (!td) return;
	flux_str_free(td->content);
	flux_str_free(td->font_family);
	free(td);
}

static void checkbox_data_destroy(void *component_data) {
	FluxCheckboxData *cd = ( FluxCheckboxData * ) component_data;
	if (!cd) return;
	flux_str_free(cd->label);
	free(cd);
}

static void hyperlink_data_destroy(void *component_data) {
	FluxHyperlinkData *hd = ( FluxHyperlinkData * ) component_data;
	if (!hd) return;
	flux_str_free(hd->label);
	flux_str_free(hd->url);
	flux_str_free(hd->icon_name);
	free(hd);
}

static void image_data_destroy(void *component_data) {
	FluxImageData *im = ( FluxImageData * ) component_data;
	if (!im) return;
	flux_str_free(im->source);
	free(im);
}

static void info_badge_data_destroy(void *component_data) {
	FluxInfoBadgeData *bd = ( FluxInfoBadgeData * ) component_data;
	if (!bd) return;
	flux_str_free(bd->icon_name);
	free(bd);
}

void flux_leaf_default_metrics(FluxLeafMetrics const *m) {
	if (m->label && m->label [0]) {
		xent_set_text(m->ctx, m->node, m->label);
		xent_set_font_size(m->ctx, m->node, 14.0f);
		xent_set_padding(m->ctx, m->node, m->padding);
	}
	else if (!isnan(m->fallback.w) || !isnan(m->fallback.h)) xent_set_size(m->ctx, m->node, m->fallback);
	if (m->min_size.w > 0.0f || m->min_size.h > 0.0f) xent_set_min_size(m->ctx, m->node, m->min_size);
}

XentNodeId flux_factory_create_node(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, FluxControlType type) {
	XentNodeId node = xent_create_node(ctx);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	flux_set_control_type(ctx, node, type);

	if (parent != XENT_NODE_INVALID) xent_append_child(ctx, parent, node);

	FluxNodeData *nd = flux_node_store_get_or_create(store, node);
	if (nd) nd->component_type = type;
	xent_set_userdata(ctx, node, nd);

	return node;
}

XentNodeId flux_create_button(FluxButtonCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_node_store_get(info->store, node);
	FluxButtonData *bd = nd ? ( FluxButtonData * ) calloc(1, sizeof(FluxButtonData)) : NULL;
	if (!nd || !bd) {
		free(bd);
		return node;
	}
	bd->label                  = flux_str_dup(info->label);
	bd->font_size              = 14.0f;
	bd->style                  = FLUX_BUTTON_STANDARD;
	bd->on_click               = info->on_click;
	bd->on_click_ctx           = info->userdata;

	nd->component_data         = bd;
	nd->destroy_component_data = flux_button_data_destroy;
	nd->behavior.on_click      = info->on_click;
	nd->behavior.on_click_ctx  = info->userdata;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, info->label, {11.0f, 5.0f, 11.0f, 6.0f},
         {0.0f, 32.0f},
         {NAN, 32.0f}
    });

	return node;
}

XentNodeId flux_create_dropdown_button(FluxDropDownButtonCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_DROPDOWN_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_node_store_get(info->store, node);
	FluxButtonData *bd = nd ? ( FluxButtonData * ) calloc(1, sizeof(FluxButtonData)) : NULL;
	if (!nd || !bd) {
		free(bd);
		return node;
	}
	bd->label                  = flux_str_dup(info->label);
	bd->icon_name              = flux_str_dup(info->icon_name);
	bd->font_size              = 14.0f;
	bd->style                  = FLUX_BUTTON_STANDARD;

	nd->component_data         = bd;
	nd->destroy_component_data = flux_button_data_destroy;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	/* Right inset reserves the chevron box (12) + spacing (8) after the label. */
	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, info->label, {11.0f, 5.0f, 31.0f, 6.0f},
         {0.0f, 32.0f},
         {32.0f, 32.0f}
    });

	return node;
}

XentNodeId flux_create_text(FluxTextCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TEXT);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_text(info->ctx, node, info->content ? info->content : "");
	if (info->font_size > 0.0f) xent_set_font_size(info->ctx, node, info->font_size);
	xent_set_font_weight(info->ctx, node, flux_font_weight_numeric(FLUX_FONT_REGULAR));
	xent_set_text_line_break_policy(info->ctx, node, XENT_LINE_BREAK_WORD_WRAP);

	FluxNodeData *nd = flux_node_store_get(info->store, node);
	FluxTextData *td = nd ? ( FluxTextData * ) calloc(1, sizeof(FluxTextData)) : NULL;
	if (!nd || !td) {
		free(td);
		return node;
	}
	td->content                = flux_str_dup(info->content);
	td->font_size              = info->font_size > 0.0f ? info->font_size : 14.0f;
	td->font_weight            = FLUX_FONT_REGULAR;
	td->alignment              = FLUX_TEXT_LEFT;
	td->vertical_alignment     = FLUX_TEXT_TOP;
	td->wrap                   = true;

	nd->component_data         = td;
	nd->destroy_component_data = text_data_destroy;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_TEXT);
	if (info->content) xent_set_semantic_label(info->ctx, node, info->content);

	return node;
}

typedef struct FluxSliderInputData {
	FluxSliderData base;
	XentContext   *ctx;
	XentNodeId     node;
} FluxSliderInputData;

/* Closest snap-grid multiple to fromValue (WinUI Slider::GetClosestStep): grid
 * multiples are absolute (counted from zero, not from min); the candidates are
 * clamped into [min, max] and ties resolve to the previous step. */
static float slider_closest_step(FluxSliderData const *sd, float step_delta, float from_value) {
	float num_steps = from_value / step_delta;
	float next_step = fminf(sd->max_value, ceilf(num_steps) * step_delta);
	float prev_step = fmaxf(sd->min_value, floorf(num_steps) * step_delta);
	return (next_step - from_value < from_value - prev_step) ? next_step : prev_step;
}

/* The grid the value snaps to while dragging (WinUI SnapsTo). */
static float slider_snap_delta(FluxSliderData const *sd) {
	return sd->snaps_to == FLUX_SLIDER_SNAPS_TO_TICKS ? sd->tick_frequency : sd->step_frequency;
}

static void slider_put_value(FluxSliderData *sd, float value) {
	if (fabsf(value - sd->current_value) < 1e-6f) return;
	sd->current_value = value;
	if (sd->on_change) sd->on_change(sd->on_change_ctx, value);
}

/* Pointer press/drag (WinUI Slider::MoveThumbToPoint + OnThumbDragDelta):
 * IntermediateValue tracks the pointer continuously (the thumb is drawn from
 * it while pressed), Value snaps to the closest step/tick multiple. */
static void slider_move_trampoline(void *ctx, float local_x, float local_y) {
	( void ) local_y;
	FluxSliderInputData *sid  = ( FluxSliderInputData * ) ctx;
	FluxSliderData      *sd   = &sid->base;

	XentRect             rect = {0};
	if (!xent_get_layout_rect(sid->ctx, sid->node, &rect)) return;

	float thumb_half       = 10.0f;
	float track_w          = fmaxf(rect.w - thumb_half * 2.0f, 1.0f);
	float pct              = fclampf((local_x - thumb_half) / track_w, 0.0f, 1.0f);

	float intermediate     = sd->min_value + pct * (sd->max_value - sd->min_value);
	sd->intermediate_value = intermediate;

	float delta            = slider_snap_delta(sd);
	slider_put_value(sd, delta > 0.0f ? slider_closest_step(sd, delta, intermediate) : intermediate);
}

/* Move Value one step in the given direction (WinUI Slider::Step). SnapsTo=Ticks
 * ignores SmallChange in favour of TickFrequency and rounds LargeChange to a
 * tick multiple; stepping back from a max that is off the grid lands on the last
 * multiple before it instead of skipping it. */
static void slider_step(FluxSliderData *sd, bool use_small_change, bool forward) {
	float step_delta;
	if (sd->snaps_to == FLUX_SLIDER_SNAPS_TO_TICKS) {
		if (sd->tick_frequency <= 0.0f) return;
		step_delta = use_small_change ? sd->tick_frequency
		                              : floorf(sd->large_change / sd->tick_frequency + 0.5f) * sd->tick_frequency;
	}
	else step_delta = use_small_change ? sd->small_change : sd->large_change;
	if (step_delta <= 0.0f) return;

	float value = sd->current_value;
	float closest;
	if (!forward && fabsf(value - sd->max_value) < 1e-6f && fabsf(remainderf(value, step_delta)) > 1e-6f)
		closest = floorf(value / step_delta) * step_delta;
	else closest = slider_closest_step(sd, step_delta, forward ? value + step_delta : value - step_delta);

	sd->intermediate_value = closest;
	slider_put_value(sd, closest);
}

static void slider_jump_to(FluxSliderData *sd, float value) {
	sd->intermediate_value = value;
	slider_put_value(sd, value);
}

static bool slider_dispatch_key(FluxSliderData *sd, unsigned int vk) {
	switch (vk) {
	case VK_LEFT :
	case VK_DOWN  : slider_step(sd, true, false); return true;
	case VK_RIGHT :
	case VK_UP    : slider_step(sd, true, true); return true;
	case VK_PRIOR : slider_step(sd, false, true); return true;
	case VK_NEXT  : slider_step(sd, false, false); return true;
	case VK_HOME  : slider_jump_to(sd, sd->min_value); return true;
	case VK_END   : slider_jump_to(sd, sd->max_value); return true;
	default : return false;
	}
}

static bool slider_on_key(void *ctx, unsigned int vk, bool down) {
	if (!down) return false;
	return slider_dispatch_key(&(( FluxSliderInputData * ) ctx)->base, vk);
}

XentNodeId flux_create_slider(FluxSliderCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SLIDER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_value(info->ctx, node, info->value, info->min, info->max);

	FluxNodeData        *nd  = flux_node_store_get(info->store, node);
	FluxSliderInputData *sid = nd ? ( FluxSliderInputData * ) calloc(1, sizeof(FluxSliderInputData)) : NULL;
	if (!nd || !sid) {
		free(sid);
		return node;
	}
	/* WinUI Slider defaults (Slider_Partial.h / DependencyProperty.cpp):
	 * StepFrequency 1, SmallChange 1, LargeChange 10, TickFrequency 0,
	 * SnapsTo StepValues, TickPlacement Inline. */
	sid->base.min_value              = info->min;
	sid->base.max_value              = info->max;
	sid->base.current_value          = info->value;
	sid->base.intermediate_value     = info->value;
	sid->base.step_frequency         = 1.0f;
	sid->base.small_change           = 1.0f;
	sid->base.large_change           = 10.0f;
	sid->base.tick_frequency         = 0.0f;
	sid->base.snaps_to               = FLUX_SLIDER_SNAPS_TO_STEP_VALUES;
	sid->base.tick_placement         = FLUX_TICK_INLINE;
	sid->base.on_change              = info->on_change;
	sid->base.on_change_ctx          = info->userdata;
	sid->ctx                         = info->ctx;
	sid->node                        = node;

	nd->component_data               = &sid->base;
	nd->destroy_component_data       = free;
	nd->behavior.on_pointer_move     = slider_move_trampoline;
	nd->behavior.on_pointer_move_ctx = sid;
	nd->behavior.on_key              = slider_on_key;
	nd->behavior.on_key_ctx          = sid;

	xent_set_focusable(info->ctx, node, true);
	xent_set_size(info->ctx, node, (XentSize) {NAN, 32.0f});

	return node;
}

static void checkbox_click_trampoline(void *ctx) {
	FluxCheckboxData *cd = ( FluxCheckboxData * ) ctx;
	if (!cd) return;
	cd->state = (cd->state == FLUX_CHECK_CHECKED) ? FLUX_CHECK_UNCHECKED : FLUX_CHECK_CHECKED;
	if (cd->on_change) cd->on_change(cd->on_change_ctx, cd->state);
}

/* Label insets follow the draw code: checkbox box 20 + gap 12; radio ring 20 +
 * gap 8; switch draws a bare 40x20 track and no label. */
static void toggle_default_metrics(FluxToggleCreateInfo const *info, XentNodeId node, FluxControlType type) {
	static struct {
		XentInsets padding;
		XentSize   fallback;
	} const metrics [] = {
	  [FLUX_CONTROL_CHECKBOX] = {{32.0f, 6.0f, 0.0f, 6.0f}, {20.0f, 32.0f}},
	  [FLUX_CONTROL_RADIO]    = {{28.0f, 6.0f, 0.0f, 6.0f}, {20.0f, 32.0f}},
	  [FLUX_CONTROL_SWITCH]   = {{0.0f, 0.0f, 0.0f, 0.0f},  {40.0f, 32.0f}},
	};

	char const *label = type == FLUX_CONTROL_SWITCH ? NULL : info->label;
	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, label, metrics [type].padding, {0.0f, 32.0f},
           metrics [type].fallback
    });
}

static XentNodeId create_toggle_node(FluxToggleCreateInfo const *info, FluxControlType type) {
	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, type);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_checked(info->ctx, node, info->checked ? 1 : 0);
	toggle_default_metrics(info, node, type);

	FluxNodeData     *nd = flux_node_store_get(info->store, node);
	FluxCheckboxData *cd = nd ? ( FluxCheckboxData * ) calloc(1, sizeof(FluxCheckboxData)) : NULL;
	if (!nd || !cd) {
		free(cd);
		return node;
	}
	cd->label                  = flux_str_dup(info->label);
	cd->state                  = info->checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
	cd->on_change              = info->on_change;
	cd->on_change_ctx          = info->userdata;

	nd->component_data         = cd;
	nd->destroy_component_data = checkbox_data_destroy;
	nd->behavior.on_click      = checkbox_click_trampoline;
	nd->behavior.on_click_ctx  = cd;

	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	return node;
}

XentNodeId flux_create_checkbox(FluxToggleCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	return create_toggle_node(info, FLUX_CONTROL_CHECKBOX);
}

XentNodeId flux_create_radio(FluxToggleCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	return create_toggle_node(info, FLUX_CONTROL_RADIO);
}

XentNodeId flux_create_switch(FluxToggleCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	return create_toggle_node(info, FLUX_CONTROL_SWITCH);
}

XentNodeId flux_create_progress(FluxProgressCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_PROGRESS);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_value(info->ctx, node, info->value, 0.0f, info->max_value);

	FluxNodeData     *nd = flux_node_store_get(info->store, node);
	FluxProgressData *pd = nd ? ( FluxProgressData * ) calloc(1, sizeof(FluxProgressData)) : NULL;
	if (!nd || !pd) {
		free(pd);
		return node;
	}
	pd->value                  = info->value;
	pd->max_value              = info->max_value;
	pd->indeterminate          = false;

	nd->component_data         = pd;
	nd->destroy_component_data = free;

	xent_set_size(info->ctx, node, (XentSize) {NAN, 3.0f});
	return node;
}

XentNodeId flux_create_progress_ring(FluxProgressCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_PROGRESS_RING);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_value(info->ctx, node, info->value, 0.0f, info->max_value);

	FluxNodeData         *nd = flux_node_store_get(info->store, node);
	FluxProgressRingData *pd = nd ? ( FluxProgressRingData * ) calloc(1, sizeof(FluxProgressRingData)) : NULL;
	if (!nd || !pd) {
		free(pd);
		return node;
	}
	pd->value                  = info->value;
	pd->max_value              = info->max_value;
	pd->indeterminate          = (info->max_value <= 0.0f);

	nd->component_data         = pd;
	nd->destroy_component_data = free;

	xent_set_size(info->ctx, node, (XentSize) {32.0f, 32.0f});
	return node;
}

XentNodeId flux_create_card(FluxContainerCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_CARD);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_protocol(info->ctx, node, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, node, XENT_FLEX_COLUMN);
	xent_set_padding(info->ctx, node, (XentInsets) {16.0f, 16.0f, 16.0f, 16.0f});
	xent_set_gap(info->ctx, node, 8.0f);
	xent_set_wrap_content(info->ctx, node, false, true);
	return node;
}

XentNodeId flux_create_divider(FluxContainerCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_DIVIDER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_size(info->ctx, node, (XentSize) {NAN, 1.0f});
	return node;
}

static void hyperlink_on_click(void *ctx) {
	FluxHyperlinkData *hd = ( FluxHyperlinkData * ) ctx;
	if (hd) flux_hyperlink_open_url(hd->url);
	if (hd && hd->on_click) hd->on_click(hd->on_click_ctx);
}

XentNodeId flux_create_hyperlink(FluxHyperlinkCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_HYPERLINK);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_node_store_get(info->store, node);
	FluxHyperlinkData *hd = nd ? ( FluxHyperlinkData * ) calloc(1, sizeof(FluxHyperlinkData)) : NULL;
	if (!nd || !hd) {
		free(hd);
		return node;
	}
	hd->label                  = flux_str_dup(info->label);
	hd->url                    = flux_str_dup(info->url);
	hd->font_size              = 14.0f;
	hd->on_click               = info->on_click;
	hd->on_click_ctx           = info->userdata;

	nd->component_data         = hd;
	nd->destroy_component_data = hyperlink_data_destroy;
	nd->behavior.on_click      = hyperlink_on_click;
	nd->behavior.on_click_ctx  = hd;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);
	xent_set_focusable(info->ctx, node, true);

	xent_set_protocol(info->ctx, node, XENT_PROTOCOL_GRID);
	{
		XentGridSizeMode row_modes [] = {XENT_GRID_STAR};
		float            row_vals []  = {1.0f};
		xent_set_grid_rows(info->ctx, node, row_modes, row_vals, 1);

		XentGridSizeMode col_modes [] = {XENT_GRID_STAR, XENT_GRID_PIXEL, XENT_GRID_PIXEL};
		float            col_vals []  = {1.0f, 40.0f, 36.0f};
		xent_set_grid_columns(info->ctx, node, col_modes, col_vals, 3);
	}

	flux_leaf_default_metrics(&(FluxLeafMetrics) {
	  info->ctx, node, info->label, {11.0f, 5.0f, 11.0f, 6.0f},
         {0.0f, 32.0f},
         {NAN, 32.0f}
    });

	return node;
}

XentNodeId flux_create_image(FluxImageCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_IMAGE);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData  *nd = flux_node_store_get(info->store, node);
	FluxImageData *im = nd ? ( FluxImageData * ) calloc(1, sizeof(FluxImageData)) : NULL;
	if (!nd || !im) {
		free(im);
		return node;
	}
	im->source                 = flux_str_dup(info->source);
	im->stretch                = info->stretch;

	nd->component_data         = im;
	nd->destroy_component_data = image_data_destroy;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_IMAGE);
	return node;
}

void flux_image_set_source(FluxNodeStore *store, XentNodeId id, char const *source) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_IMAGE) return;
	flux_str_replace(&(( FluxImageData * ) nd->component_data)->source, source);
}

void flux_image_set_stretch(FluxNodeStore *store, XentNodeId id, FluxImageStretch stretch) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_IMAGE) return;
	(( FluxImageData * ) nd->component_data)->stretch = stretch;
}

XentNodeId flux_create_scroll(FluxContainerCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_SCROLL);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_node_store_get(info->store, node);
	FluxScrollData *sd = nd ? ( FluxScrollData * ) calloc(1, sizeof(FluxScrollData)) : NULL;
	if (!nd || !sd) {
		free(sd);
		return node;
	}
	sd->h_vis                  = FLUX_SCROLL_AUTO;
	sd->v_vis                  = FLUX_SCROLL_AUTO;

	nd->component_data         = sd;
	nd->destroy_component_data = free;

	xent_set_protocol(info->ctx, node, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, node, XENT_FLEX_COLUMN);
	return node;
}

static XentSize info_badge_default_size(FluxInfoBadgeMode mode, int32_t value) {
	if (mode == FLUX_BADGE_DOT) return (XentSize) {8.0f, 8.0f};
	if (mode == FLUX_BADGE_ICON || value < 10) return (XentSize) {16.0f, 16.0f};
	return value < 100 ? (XentSize) {24.0f, 16.0f} : (XentSize) {30.0f, 16.0f};
}

XentNodeId flux_create_info_badge(FluxInfoBadgeCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_INFO_BADGE);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_node_store_get(info->store, node);
	FluxInfoBadgeData *bd = nd ? ( FluxInfoBadgeData * ) calloc(1, sizeof(FluxInfoBadgeData)) : NULL;
	if (!nd || !bd) {
		free(bd);
		return node;
	}
	bd->mode                   = info->mode;
	bd->value                  = info->value;

	nd->component_data         = bd;
	nd->destroy_component_data = info_badge_data_destroy;

	xent_set_size(info->ctx, node, info_badge_default_size(info->mode, info->value));
	return node;
}
