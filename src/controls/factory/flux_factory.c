#include "controls/factory/flux_factory.h"
#include "controls/behavior/flux_hyperlink_action.h"
#include "controls/draw/flux_control_draw.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_engine.h"

#include <stdlib.h>
#include <string.h>

static float inline fclampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

XentNodeId flux_factory_create_node(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, XentControlType type) {
	XentNodeId node = xent_create_node(ctx);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_control_type(ctx, node, type);

	if (parent != XENT_NODE_INVALID) xent_append_child(ctx, parent, node);

	FluxNodeData *nd = flux_node_store_get_or_create(store, node);
	if (nd) nd->component_type = type;
	xent_set_userdata(ctx, node, nd);

	return node;
}

XentNodeId flux_create_button(FluxButtonCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_BUTTON, flux_draw_button, NULL);
	flux_node_store_register_renderer(info->store, XENT_CONTROL_TOGGLE_BUTTON, flux_draw_toggle, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, XENT_CONTROL_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_node_store_get(info->store, node);
	FluxButtonData *bd = nd ? ( FluxButtonData * ) calloc(1, sizeof(FluxButtonData)) : NULL;
	if (!nd || !bd) {
		free(bd);
		return node;
	}
	bd->label                  = info->label;
	bd->font_size              = 14.0f;
	bd->style                  = FLUX_BUTTON_STANDARD;
	bd->enabled                = true;
	bd->on_click               = info->on_click;
	bd->on_click_ctx           = info->userdata;

	nd->component_data         = bd;
	nd->destroy_component_data = free;
	nd->behavior.on_click      = info->on_click;
	nd->behavior.on_click_ctx  = info->userdata;

	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);

	return node;
}

XentNodeId
flux_create_text(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *content, float font_size) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(store, XENT_CONTROL_TEXT, flux_draw_text, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_TEXT);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_text(ctx, node, content ? content : "");
	if (font_size > 0.0f) xent_set_font_size(ctx, node, font_size);

	FluxNodeData *nd = flux_node_store_get(store, node);
	FluxTextData *td = nd ? ( FluxTextData * ) calloc(1, sizeof(FluxTextData)) : NULL;
	if (!nd || !td) {
		free(td);
		return node;
	}
	td->content                = content;
	td->font_size              = font_size > 0.0f ? font_size : 14.0f;
	td->font_weight            = FLUX_FONT_REGULAR;
	td->alignment              = FLUX_TEXT_LEFT;
	td->vertical_alignment     = FLUX_TEXT_TOP;
	td->wrap                   = true;

	nd->component_data         = td;
	nd->destroy_component_data = free;

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_TEXT);
	if (content) xent_set_semantic_label(ctx, node, content);

	return node;
}

typedef struct FluxSliderInputData {
	FluxSliderData base;
	XentContext   *ctx;
	XentNodeId     node;
} FluxSliderInputData;

static void slider_move_trampoline(void *ctx, float local_x, float local_y) {
	( void ) local_y;
	FluxSliderInputData *sid = ( FluxSliderInputData * ) ctx;
	FluxSliderData      *sd  = &sid->base;
	if (!sd->enabled) return;

	XentRect rect = {0};
	if (!xent_get_layout_rect(sid->ctx, sid->node, &rect)) return;

	float pad     = 10.0f;
	float track_w = rect.width - pad * 2.0f;
	if (track_w <= 0.0f) return;

	float pct   = fclampf((local_x - pad) / track_w, 0.0f, 1.0f);
	float value = sd->min_value + pct * (sd->max_value - sd->min_value);
	if (sd->step > 0.0f) {
		float half = sd->step * 0.5f;
		value      = (( int ) ((value + half) / sd->step)) * sd->step;
	}
	value             = fclampf(value, sd->min_value, sd->max_value);

	sd->current_value = value;
	if (sd->on_change) sd->on_change(sd->on_change_ctx, value);
}

XentNodeId flux_create_slider(FluxSliderCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_SLIDER, flux_draw_slider, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, XENT_CONTROL_SLIDER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_value(info->ctx, node, info->value, info->min, info->max);

	FluxNodeData        *nd  = flux_node_store_get(info->store, node);
	FluxSliderInputData *sid = nd ? ( FluxSliderInputData * ) calloc(1, sizeof(FluxSliderInputData)) : NULL;
	if (!nd || !sid) {
		free(sid);
		return node;
	}
	sid->base.min_value              = info->min;
	sid->base.max_value              = info->max;
	sid->base.current_value          = info->value;
	sid->base.step                   = 0.0f;
	sid->base.enabled                = true;
	sid->base.on_change              = info->on_change;
	sid->base.on_change_ctx          = info->userdata;
	sid->ctx                         = info->ctx;
	sid->node                        = node;

	nd->component_data               = &sid->base;
	nd->destroy_component_data       = free;
	nd->behavior.on_pointer_move     = slider_move_trampoline;
	nd->behavior.on_pointer_move_ctx = sid;

	return node;
}

static void checkbox_click_trampoline(void *ctx) {
	FluxCheckboxData *cd = ( FluxCheckboxData * ) ctx;
	if (!cd || !cd->enabled) return;
	cd->state = (cd->state == FLUX_CHECK_CHECKED) ? FLUX_CHECK_UNCHECKED : FLUX_CHECK_CHECKED;
	if (cd->on_change) cd->on_change(cd->on_change_ctx, cd->state);
}

static XentNodeId create_toggle_node(FluxToggleCreateInfo const *info, XentControlType type) {
	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, type);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_checked(info->ctx, node, info->checked ? 1 : 0);

	FluxNodeData     *nd = flux_node_store_get(info->store, node);
	FluxCheckboxData *cd = nd ? ( FluxCheckboxData * ) calloc(1, sizeof(FluxCheckboxData)) : NULL;
	if (!nd || !cd) {
		free(cd);
		return node;
	}
	cd->label                  = info->label;
	cd->state                  = info->checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
	cd->enabled                = true;
	cd->on_change              = info->on_change;
	cd->on_change_ctx          = info->userdata;

	nd->component_data         = cd;
	nd->destroy_component_data = free;
	nd->behavior.on_click      = checkbox_click_trampoline;
	nd->behavior.on_click_ctx  = cd;

	if (info->label) xent_set_semantic_label(info->ctx, node, info->label);

	return node;
}

XentNodeId flux_create_checkbox(FluxToggleCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_CHECKBOX, flux_draw_checkbox, NULL);
	return create_toggle_node(info, XENT_CONTROL_CHECKBOX);
}

XentNodeId flux_create_radio(FluxToggleCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_RADIO, flux_draw_radio, NULL);
	return create_toggle_node(info, XENT_CONTROL_RADIO);
}

XentNodeId flux_create_switch(FluxToggleCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_SWITCH, flux_draw_switch, NULL);
	return create_toggle_node(info, XENT_CONTROL_SWITCH);
}

XentNodeId flux_create_progress(FluxProgressCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_PROGRESS, flux_draw_progress, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, XENT_CONTROL_PROGRESS);
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
	return node;
}

XentNodeId flux_create_progress_ring(FluxProgressCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_PROGRESS_RING, flux_draw_progress_ring, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, XENT_CONTROL_PROGRESS_RING);
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
	return node;
}

XentNodeId flux_create_card(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(store, XENT_CONTROL_CARD, flux_draw_card, NULL);
	return flux_factory_create_node(ctx, store, parent, XENT_CONTROL_CARD);
}

XentNodeId flux_create_divider(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(store, XENT_CONTROL_DIVIDER, flux_draw_divider, NULL);
	return flux_factory_create_node(ctx, store, parent, XENT_CONTROL_DIVIDER);
}

static void hyperlink_on_click(void *ctx) {
	FluxHyperlinkData *hd = ( FluxHyperlinkData * ) ctx;
	if (hd) flux_hyperlink_open_url(hd->url);
	if (hd && hd->on_click) hd->on_click(hd->on_click_ctx);
}

XentNodeId flux_create_hyperlink(FluxHyperlinkCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_HYPERLINK, flux_draw_hyperlink, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, XENT_CONTROL_HYPERLINK);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData      *nd = flux_node_store_get(info->store, node);
	FluxHyperlinkData *hd = nd ? ( FluxHyperlinkData * ) calloc(1, sizeof(FluxHyperlinkData)) : NULL;
	if (!nd || !hd) {
		free(hd);
		return node;
	}
	hd->label                  = info->label;
	hd->url                    = info->url;
	hd->font_size              = 14.0f;
	hd->enabled                = true;
	hd->on_click               = info->on_click;
	hd->on_click_ctx           = info->userdata;

	nd->component_data         = hd;
	nd->destroy_component_data = free;
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

	return node;
}

XentNodeId flux_create_info_badge(FluxInfoBadgeCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, XENT_CONTROL_INFO_BADGE, flux_draw_info_badge, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, XENT_CONTROL_INFO_BADGE);
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
	nd->destroy_component_data = free;
	return node;
}
