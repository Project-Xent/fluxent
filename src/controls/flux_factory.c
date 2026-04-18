/**
 * @file flux_factory.c
 * @brief Control factory functions for creating Fluxent UI controls.
 */
#include "flux_factory.h"
#include "fluxent/fluxent.h"
#include "fluxent/flux_engine.h"
#include "fluxent/flux_window.h"

#include <shellapi.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* ── Renderer forward declarations ─────────────────────────────────── */
extern void
flux_draw_container(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_text(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_button(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_toggle(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_checkbox(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_radio(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_switch(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_slider(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_scroll(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void flux_draw_scroll_overlay(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *);
extern void
flux_draw_progress(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_card(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_divider(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_hyperlink(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void flux_draw_progress_ring(
  FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *
);
extern void
flux_draw_info_badge(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);

/* ═══════════════════════════════════════════════════════════════════════
   Internal helper
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId flux_factory_create_node(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, XentControlType type) {
	XentNodeId node = xent_create_node(ctx);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_control_type(ctx, node, type);

	if (parent != XENT_NODE_INVALID) xent_append_child(ctx, parent, node);

	flux_node_store_get_or_create(store, node);
	xent_set_userdata(ctx, node, flux_node_store_get(store, node));

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   Button
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId flux_create_button(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_BUTTON, flux_draw_button, NULL);
	flux_register_renderer(XENT_CONTROL_TOGGLE_BUTTON, flux_draw_toggle, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxButtonData *bd = ( FluxButtonData * ) calloc(1, sizeof(FluxButtonData));
				if (bd) {
					bd->label        = label;
					bd->font_size    = 14.0f;
					bd->style        = FLUX_BUTTON_STANDARD;
					bd->enabled      = true;
					bd->on_click     = on_click;
					bd->on_click_ctx = userdata;
				}
			nd->component_data        = bd;
			nd->behavior.on_click     = on_click;
			nd->behavior.on_click_ctx = userdata;
		}

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_BUTTON);
	if (label) xent_set_semantic_label(ctx, node, label);

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   Text
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId
flux_create_text(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *content, float font_size) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_TEXT, flux_draw_text, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_TEXT);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_text(ctx, node, content ? content : "");
	if (font_size > 0.0f) xent_set_font_size(ctx, node, font_size);

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxTextData *td = ( FluxTextData * ) calloc(1, sizeof(FluxTextData));
				if (td) {
					td->content            = content;
					td->font_size          = font_size > 0.0f ? font_size : 14.0f;
					td->font_weight        = FLUX_FONT_REGULAR;
					td->alignment          = FLUX_TEXT_LEFT;
					td->vertical_alignment = FLUX_TEXT_TOP;
					td->wrap               = true;
				}
			nd->component_data = td;
		}

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_TEXT);
	if (content) xent_set_semantic_label(ctx, node, content);

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   Slider
   ═══════════════════════════════════════════════════════════════════════ */

typedef struct FluxSliderInputData {
	FluxSliderData base;
	XentContext   *ctx;
	XentNodeId     node;
} FluxSliderInputData;

static void slider_move_trampoline(void *ctx, float local_x, float local_y) {
	FluxSliderInputData *sid = ( FluxSliderInputData * ) ctx;
	FluxSliderData      *sd  = &sid->base;
	if (!sd->enabled) return;

	XentRect rect = {0};
	if (!xent_get_layout_rect(sid->ctx, sid->node, &rect)) return;

	float pad     = 10.0f;
	float track_w = rect.width - pad * 2.0f;
	if (track_w <= 0.0f) return;

	float pct = (local_x - pad) / track_w;
	if (pct < 0.0f) pct = 0.0f;
	if (pct > 1.0f) pct = 1.0f;

	float value = sd->min_value + pct * (sd->max_value - sd->min_value);
		if (sd->step > 0.0f) {
			float half = sd->step * 0.5f;
			value      = (( int ) ((value + half) / sd->step)) * sd->step;
		}
	if (value < sd->min_value) value = sd->min_value;
	if (value > sd->max_value) value = sd->max_value;

	sd->current_value = value;
	if (sd->on_change) sd->on_change(sd->on_change_ctx, value);
}

XentNodeId flux_create_slider(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, float min, float max, float value,
  void (*on_change)(void *, float), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_SLIDER, flux_draw_slider, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_SLIDER);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_value(ctx, node, value, min, max);

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxSliderInputData *sid = ( FluxSliderInputData * ) calloc(1, sizeof(FluxSliderInputData));
				if (sid) {
					sid->base.min_value     = min;
					sid->base.max_value     = max;
					sid->base.current_value = value;
					sid->base.step          = 0.0f;
					sid->base.enabled       = true;
					sid->base.on_change     = on_change;
					sid->base.on_change_ctx = userdata;
					sid->ctx                = ctx;
					sid->node               = node;
				}
			nd->component_data               = &sid->base;
			nd->behavior.on_pointer_move     = slider_move_trampoline;
			nd->behavior.on_pointer_move_ctx = sid;
		}

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   Checkbox / Radio / Switch
   ═══════════════════════════════════════════════════════════════════════ */

static void checkbox_click_trampoline(void *ctx) {
	FluxCheckboxData *cd = ( FluxCheckboxData * ) ctx;
	if (!cd || !cd->enabled) return;
	cd->state = (cd->state == FLUX_CHECK_CHECKED) ? FLUX_CHECK_UNCHECKED : FLUX_CHECK_CHECKED;
	if (cd->on_change) cd->on_change(cd->on_change_ctx, cd->state);
}

XentNodeId flux_create_checkbox(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, bool checked,
  void (*on_change)(void *, FluxCheckState), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_CHECKBOX, flux_draw_checkbox, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_CHECKBOX);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_checked(ctx, node, checked ? 1 : 0);

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxCheckboxData *cd = ( FluxCheckboxData * ) calloc(1, sizeof(FluxCheckboxData));
				if (cd) {
					cd->label         = label;
					cd->state         = checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
					cd->enabled       = true;
					cd->on_change     = on_change;
					cd->on_change_ctx = userdata;
				}
			nd->component_data        = cd;
			nd->behavior.on_click     = checkbox_click_trampoline;
			nd->behavior.on_click_ctx = cd;
		}

	if (label) xent_set_semantic_label(ctx, node, label);

	return node;
}

XentNodeId flux_create_radio(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, bool checked,
  void (*on_change)(void *, FluxCheckState), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_RADIO, flux_draw_radio, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_RADIO);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_checked(ctx, node, checked ? 1 : 0);

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxCheckboxData *cd = ( FluxCheckboxData * ) calloc(1, sizeof(FluxCheckboxData));
				if (cd) {
					cd->label         = label;
					cd->state         = checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
					cd->enabled       = true;
					cd->on_change     = on_change;
					cd->on_change_ctx = userdata;
				}
			nd->component_data        = cd;
			nd->behavior.on_click     = checkbox_click_trampoline;
			nd->behavior.on_click_ctx = cd;
		}

	if (label) xent_set_semantic_label(ctx, node, label);

	return node;
}

XentNodeId flux_create_switch(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, bool on,
  void (*on_change)(void *, FluxCheckState), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_SWITCH, flux_draw_switch, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_SWITCH);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_checked(ctx, node, on ? 1 : 0);

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxCheckboxData *cd = ( FluxCheckboxData * ) calloc(1, sizeof(FluxCheckboxData));
				if (cd) {
					cd->label         = label;
					cd->state         = on ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
					cd->enabled       = true;
					cd->on_change     = on_change;
					cd->on_change_ctx = userdata;
				}
			nd->component_data        = cd;
			nd->behavior.on_click     = checkbox_click_trampoline;
			nd->behavior.on_click_ctx = cd;
		}

	if (label) xent_set_semantic_label(ctx, node, label);

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   Progress / ProgressRing
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId
flux_create_progress(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, float value, float max_value) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_PROGRESS, flux_draw_progress, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_PROGRESS);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_value(ctx, node, value, 0.0f, max_value);

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxProgressData *pd = ( FluxProgressData * ) calloc(1, sizeof(FluxProgressData));
				if (pd) {
					pd->value         = value;
					pd->max_value     = max_value;
					pd->indeterminate = false;
				}
			nd->component_data = pd;
		}

	return node;
}

XentNodeId
flux_create_progress_ring(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, float value, float max_value) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_PROGRESS_RING, flux_draw_progress_ring, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_PROGRESS_RING);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	xent_set_semantic_value(ctx, node, value, 0.0f, max_value);

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxProgressRingData *pd = ( FluxProgressRingData * ) calloc(1, sizeof(FluxProgressRingData));
				if (pd) {
					pd->value         = value;
					pd->max_value     = max_value;
					pd->indeterminate = (max_value <= 0.0f);
				}
			nd->component_data = pd;
		}

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   Card / Divider
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId flux_create_card(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_CARD, flux_draw_card, NULL);

	return flux_factory_create_node(ctx, store, parent, XENT_CONTROL_CARD);
}

XentNodeId flux_create_divider(XentContext *ctx, FluxNodeStore *store, XentNodeId parent) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_DIVIDER, flux_draw_divider, NULL);

	return flux_factory_create_node(ctx, store, parent, XENT_CONTROL_DIVIDER);
}

/* ═══════════════════════════════════════════════════════════════════════
   Hyperlink
   ═══════════════════════════════════════════════════════════════════════ */

static void hyperlink_on_click(void *ctx) {
	FluxHyperlinkData *hd = ( FluxHyperlinkData * ) ctx;
		if (hd && hd->url && hd->url [0]) {
			int      wlen = MultiByteToWideChar(CP_UTF8, 0, hd->url, -1, NULL, 0);
			wchar_t *wurl = ( wchar_t * ) _alloca(wlen * sizeof(wchar_t));
			MultiByteToWideChar(CP_UTF8, 0, hd->url, -1, wurl, wlen);
			ShellExecuteW(NULL, L"open", wurl, NULL, NULL, SW_SHOWNORMAL);
		}
	if (hd && hd->on_click) hd->on_click(hd->on_click_ctx);
}

XentNodeId flux_create_hyperlink(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, char const *url,
  void (*on_click)(void *), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_HYPERLINK, flux_draw_hyperlink, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_HYPERLINK);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxHyperlinkData *hd = ( FluxHyperlinkData * ) calloc(1, sizeof(FluxHyperlinkData));
				if (hd) {
					hd->label        = label;
					hd->url          = url;
					hd->font_size    = 14.0f;
					hd->enabled      = true;
					hd->on_click     = on_click;
					hd->on_click_ctx = userdata;
				}
			nd->component_data        = hd;
			nd->behavior.on_click     = hyperlink_on_click;
			nd->behavior.on_click_ctx = hd;
		}

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_BUTTON);
	if (label) xent_set_semantic_label(ctx, node, label);
	xent_set_focusable(ctx, node, true);

	/* Grid layout: WinUI NumberBox template */
	xent_set_protocol(ctx, node, XENT_PROTOCOL_GRID);
	{
		XentGridSizeMode row_modes [] = {XENT_GRID_STAR};
		float            row_vals []  = {1.0f};
		xent_set_grid_rows(ctx, node, row_modes, row_vals, 1);

		XentGridSizeMode col_modes [] = {XENT_GRID_STAR, XENT_GRID_PIXEL, XENT_GRID_PIXEL};
		float            col_vals []  = {1.0f, 40.0f, 36.0f};
		xent_set_grid_columns(ctx, node, col_modes, col_vals, 3);
	}

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   RepeatButton
   ═══════════════════════════════════════════════════════════════════════ */

typedef struct FluxRepeatButtonInputData {
	FluxRepeatButtonData base;
	XentNodeId           node;
	UINT_PTR             timer_id;
	FluxWindow          *window;
	XentContext         *ctx;
	bool                 pointer_inside; /* true while pointer is within button bounds */
} FluxRepeatButtonInputData;

static FluxRepeatButtonInputData *s_active_repeat = NULL;

static void CALLBACK              repeat_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed) {
	( void ) hwnd;
	( void ) msg;
	( void ) elapsed;
		if (s_active_repeat && s_active_repeat->timer_id == id) {
			if (s_active_repeat->base.on_click) s_active_repeat->base.on_click(s_active_repeat->base.on_click_ctx);

			uint32_t interval = s_active_repeat->base.repeat_interval_ms;
			if (interval < 10) interval = 50;
			KillTimer(NULL, id);
			s_active_repeat->timer_id = SetTimer(NULL, 0, interval, repeat_timer_proc);
		}
}

static void repeat_start_timer(FluxRepeatButtonInputData *rb, uint32_t delay_ms) {
	if (!rb || rb->timer_id) return; /* already running */
	s_active_repeat = rb;
	rb->timer_id    = SetTimer(NULL, 0, delay_ms, repeat_timer_proc);
}

static void repeat_pause_timer(FluxRepeatButtonInputData *rb) {
	if (!rb) return;
		if (rb->timer_id) {
			KillTimer(NULL, rb->timer_id);
			rb->timer_id = 0;
		}
	/* Keep s_active_repeat so we can resume on re-enter */
}

static void repeat_on_pointer_down(void *ctx, float x, float y, int click_count) {
	( void ) x;
	( void ) y;
	( void ) click_count;
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (!rb) return;

	if (rb->base.on_click) rb->base.on_click(rb->base.on_click_ctx);

	rb->pointer_inside = true;
	uint32_t delay     = rb->base.repeat_delay_ms;
	if (delay < 10) delay = 400;
	repeat_start_timer(rb, delay);
}

static void repeat_on_pointer_move(void *ctx, float local_x, float local_y) {
	FluxRepeatButtonInputData *rb = ( FluxRepeatButtonInputData * ) ctx;
	if (!rb || !rb->ctx) return;

	/* Get button bounds to check if pointer is inside */
	XentRect rect = {0};
	xent_get_layout_rect(rb->ctx, rb->node, &rect);
	bool inside = (local_x >= 0.0f && local_x < rect.width && local_y >= 0.0f && local_y < rect.height);

		if (inside && !rb->pointer_inside) {
			/* Re-entered button bounds while pressed — resume repeat */
			rb->pointer_inside = true;
				if (s_active_repeat == rb && !rb->timer_id) {
					uint32_t interval = rb->base.repeat_interval_ms;
					if (interval < 10) interval = 50;
					repeat_start_timer(rb, interval);
				}
		}
		else if (!inside && rb->pointer_inside) {
			/* Left button bounds while pressed — pause repeat */
			rb->pointer_inside = false;
			repeat_pause_timer(rb);
		}
}

static void repeat_stop_timer(FluxRepeatButtonInputData *rb) {
	if (!rb) return;
		if (rb->timer_id) {
			KillTimer(NULL, rb->timer_id);
			rb->timer_id = 0;
		}
	rb->pointer_inside = false;
	if (s_active_repeat == rb) s_active_repeat = NULL;
}

static void repeat_on_blur(void *ctx) { repeat_stop_timer(( FluxRepeatButtonInputData * ) ctx); }

static void repeat_on_pointer_up(void *ctx) { repeat_stop_timer(( FluxRepeatButtonInputData * ) ctx); }

XentNodeId  flux_create_repeat_button(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_REPEAT_BUTTON, flux_draw_button, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_REPEAT_BUTTON);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxRepeatButtonInputData *rb
			  = ( FluxRepeatButtonInputData * ) calloc(1, sizeof(FluxRepeatButtonInputData));
				if (rb) {
					rb->base.label              = label;
					rb->base.font_size          = 14.0f;
					rb->base.style              = FLUX_BUTTON_STANDARD;
					rb->base.enabled            = true;
					rb->base.repeat_delay_ms    = 400;
					rb->base.repeat_interval_ms = 50;
					rb->base.on_click           = on_click;
					rb->base.on_click_ctx       = userdata;
					rb->node                    = node;
					rb->ctx                     = ctx;
				}
			nd->component_data               = &rb->base;
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
		}

	xent_set_semantic_role(ctx, node, XENT_SEMANTIC_BUTTON);
	if (label) xent_set_semantic_label(ctx, node, label);
	xent_set_focusable(ctx, node, true);

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   InfoBadge
   ═══════════════════════════════════════════════════════════════════════ */

XentNodeId flux_create_info_badge(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, FluxInfoBadgeMode mode, int32_t value
) {
	if (!ctx || !store) return XENT_NODE_INVALID;
	flux_register_renderer(XENT_CONTROL_INFO_BADGE, flux_draw_info_badge, NULL);

	XentNodeId node = flux_factory_create_node(ctx, store, parent, XENT_CONTROL_INFO_BADGE);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData *nd = flux_node_store_get(store, node);
		if (nd) {
			FluxInfoBadgeData *bd = ( FluxInfoBadgeData * ) calloc(1, sizeof(FluxInfoBadgeData));
				if (bd) {
					bd->mode  = mode;
					bd->value = value;
				}
			nd->component_data = bd;
		}

	return node;
}

/* ═══════════════════════════════════════════════════════════════════════
   Flyout / MenuFlyout binding helpers
   ═══════════════════════════════════════════════════════════════════════ */

typedef struct FluxFlyoutBinding {
	FluxFlyout    *flyout;
	FluxPlacement  placement;
	FluxNodeStore *store;
	XentNodeId     node;
	FluxWindow    *window;
	XentContext   *ctx;
} FluxFlyoutBinding;

static void flyout_on_pointer_down(void *ctx, float local_x, float local_y, int click_count) {
	( void ) click_count;
	( void ) local_x;
	( void ) local_y;
	FluxFlyoutBinding *b = ( FluxFlyoutBinding * ) ctx;
	if (!b || !b->flyout) return;

		if (flux_flyout_is_visible(b->flyout)) {
			flux_flyout_dismiss(b->flyout);
			return;
		}

	FluxRect anchor = {0.0f, 0.0f, 0.0f, 0.0f};

		if (b->window && b->ctx && b->node != XENT_NODE_INVALID) {
			HWND     hwnd  = flux_window_hwnd(b->window);
			UINT     dpi   = GetDpiForWindow(hwnd);
			float    scale = (dpi > 0) ? ( float ) dpi / 96.0f : 1.0f;

			XentRect lr    = {0};
			xent_get_layout_rect(b->ctx, b->node, &lr);

			POINT pt = {( LONG ) (lr.x * scale), ( LONG ) (lr.y * scale)};
			ClientToScreen(hwnd, &pt);

			anchor.x = ( float ) pt.x;
			anchor.y = ( float ) pt.y;
			anchor.w = lr.width * scale;
			anchor.h = lr.height * scale;
		}
		else {
			POINT cursor;
			GetCursorPos(&cursor);
			anchor.x = ( float ) cursor.x;
			anchor.y = ( float ) cursor.y;
		}

	flux_flyout_show(b->flyout, anchor, b->placement);
}

void flux_node_set_flyout(FluxNodeStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement) {
	if (!store || !flyout) return;
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd) return;

	FluxFlyoutBinding *b = ( FluxFlyoutBinding * ) calloc(1, sizeof(*b));
	if (!b) return;
	b->flyout                        = flyout;
	b->placement                     = placement;
	b->store                         = store;
	b->node                          = id;
	b->window                        = NULL;
	b->ctx                           = NULL;

	nd->behavior.on_pointer_down     = flyout_on_pointer_down;
	nd->behavior.on_pointer_down_ctx = b;
}

void flux_node_set_flyout_ex(
  FluxNodeStore *store, XentNodeId id, FluxFlyout *flyout, FluxPlacement placement, XentContext *xctx,
  FluxWindow *window
) {
	flux_node_set_flyout(store, id, flyout, placement);
	FluxNodeData *nd = flux_node_store_get(store, id);
		if (nd && nd->behavior.on_pointer_down_ctx) {
			FluxFlyoutBinding *b = ( FluxFlyoutBinding * ) nd->behavior.on_pointer_down_ctx;
			b->ctx               = xctx;
			b->window            = window;
		}
}

typedef struct FluxContextMenuBinding {
	FluxMenuFlyout *menu;
	FluxNodeStore  *store;
	XentNodeId      node;
	FluxWindow     *window;
	XentContext    *ctx;
} FluxContextMenuBinding;

static void context_menu_on_context_binding(void *ctx, float local_x, float local_y) {
	( void ) local_x;
	( void ) local_y;
	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) ctx;
	if (!b || !b->menu) return;

	FluxRect anchor = {0.0f, 0.0f, 0.0f, 0.0f};

		if (b->window && b->ctx && b->node != XENT_NODE_INVALID) {
			HWND     hwnd  = flux_window_hwnd(b->window);
			UINT     dpi   = GetDpiForWindow(hwnd);
			float    scale = (dpi > 0) ? ( float ) dpi / 96.0f : 1.0f;

			XentRect lr    = {0};
			xent_get_layout_rect(b->ctx, b->node, &lr);

			POINT pt = {( LONG ) (lr.x * scale), ( LONG ) (lr.y * scale)};
			ClientToScreen(hwnd, &pt);

			anchor.x = ( float ) pt.x;
			anchor.y = ( float ) pt.y;
			anchor.w = lr.width * scale;
			anchor.h = lr.height * scale;
		}
		else {
			POINT cursor;
			GetCursorPos(&cursor);
			anchor.x = ( float ) cursor.x;
			anchor.y = ( float ) cursor.y;
		}

	flux_menu_flyout_show(b->menu, anchor, FLUX_PLACEMENT_BOTTOM);
}

void flux_node_set_context_flyout(FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu) {
	if (!store || !menu) return;
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd) return;

	FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) calloc(1, sizeof(*b));
	if (!b) return;
	b->menu                          = menu;
	b->store                         = store;
	b->node                          = id;
	b->window                        = NULL;
	b->ctx                           = NULL;

	nd->behavior.on_context_menu     = context_menu_on_context_binding;
	nd->behavior.on_context_menu_ctx = b;
}

void flux_node_set_context_flyout_ex(
  FluxNodeStore *store, XentNodeId id, FluxMenuFlyout *menu, XentContext *xctx, FluxWindow *window
) {
	flux_node_set_context_flyout(store, id, menu);
	FluxNodeData *nd = flux_node_store_get(store, id);
		if (nd && nd->behavior.on_context_menu_ctx) {
			FluxContextMenuBinding *b = ( FluxContextMenuBinding * ) nd->behavior.on_context_menu_ctx;
			b->ctx                    = xctx;
			b->window                 = window;
		}
}
