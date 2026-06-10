#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <stdlib.h>
#include <windows.h>

/* WinUI Expander (Expander.xaml). Three nodes mirror the XAML tree:
 *   root    FLUX_CONTROL_EXPANDER          — layout-only 2-row grid, owns runtime
 *   header  FLUX_CONTROL_EXPANDER_HEADER   — ToggleButton, hosts arbitrary content
 *   content FLUX_CONTROL_EXPANDER_CONTENT  — sliding content panel (caller fills)
 *
 * Toggling flips `expanded` immediately (header chevron/corner follow) and starts
 * a TranslateY slide on the content. A Down expander pairs ExpandDown with
 * CollapseUp (see Expander.cpp UpdateExpandState): expand slides the panel down
 * into place (-ContentHeight -> 0) over 333ms; collapse retracts it back up behind
 * the header (0 -> -ContentHeight) over 167ms before the panel is detached. The
 * slide uses the node render-transform (render_translate_y + render_clip_subtree). */
#define EXP_EXPAND_MS   333.0f
#define EXP_COLLAPSE_MS 167.0f
#define EXP_MAX_ANIM    8

static FluxExpanderData *g_exp_anim [EXP_MAX_ANIM];
static int               g_exp_anim_count;
static UINT_PTR          g_exp_timer;

static FluxExpanderData *expander_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd) return NULL;
	if (nd->component_type != FLUX_CONTROL_EXPANDER && nd->component_type != FLUX_CONTROL_EXPANDER_HEADER) return NULL;
	return ( FluxExpanderData * ) nd->component_data;
}

static void expander_repaint(FluxExpanderData *d) {
	HWND h = flux_window_hwnd(d->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static float expander_clamp01(float v) { return v < 0.0f ? 0.0f : v > 1.0f ? 1.0f : v; }

static void  expander_set_translate(FluxExpanderData *d, float dy) {
	FluxNodeData *nd = flux_node_store_get(d->store, d->content);
	if (nd) nd->render_translate_y = dy;
}

/* xent flex containers fill their parent's available main-axis space rather than
 * wrapping to content, so the control height is pinned explicitly: header only
 * when collapsed, header + content_height while expanded or animating (the
 * content panel grows into the reserved space, then slides within it). */
static void expander_pin_height(FluxExpanderData *d) {
	bool  open = d->expanded || d->anim_active;
	float h    = open ? FLUX_EXPANDER_MIN_HEIGHT + d->content_height : FLUX_EXPANDER_MIN_HEIGHT;
	if (d->width <= 0.0f) return;
	xent_set_min_size(d->ctx, d->root, (XentSize) {d->width, h});
	xent_set_max_size(d->ctx, d->root, (XentSize) {d->width, h});
}

static void expander_anim_remove(FluxExpanderData *d) {
	for (int i = 0; i < g_exp_anim_count; i++) {
		if (g_exp_anim [i] != d) continue;
		g_exp_anim [i] = g_exp_anim [--g_exp_anim_count];
		break;
	}
	if (g_exp_anim_count == 0 && g_exp_timer) {
		KillTimer(NULL, g_exp_timer);
		g_exp_timer = 0;
	}
}

static void expander_anim_finish(FluxExpanderData *d) {
	d->anim_active = false;
	expander_set_translate(d, 0.0f);
	if (!d->anim_expanding) xent_remove_child(d->ctx, d->root, d->content);
	expander_pin_height(d);
	expander_anim_remove(d);
}

static void CALLBACK expander_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD now) {
	( void ) hwnd;
	( void ) msg;
	( void ) id;
	( void ) now;
	for (int i = g_exp_anim_count - 1; i >= 0; i--) {
		FluxExpanderData *d        = g_exp_anim [i];
		float             duration = d->anim_expanding ? EXP_EXPAND_MS : EXP_COLLAPSE_MS;
		float             elapsed  = ( float ) (GetTickCount() - d->anim_start);
		float             t        = expander_clamp01(elapsed / duration);
		float             eased    = 1.0f - powf(1.0f - t, 3.0f); /* approx KeySpline 0,0,0,1 / 1,1,0,1 */
		float             h        = d->content_height;
		/* Expand: -h -> 0 (slide down into place). Collapse (CollapseUp): 0 -> -h
		 * (retract up behind the header). Both end clipped/hidden above row top. */
		expander_set_translate(d, d->anim_expanding ? -(1.0f - eased) * h : -eased * h);
		expander_repaint(d);
		if (elapsed >= duration) expander_anim_finish(d);
	}
}

static void expander_anim_start(FluxExpanderData *d, bool expanding) {
	if (expanding) xent_append_child(d->ctx, d->root, d->content);
	d->anim_active    = true;
	d->anim_expanding = expanding;
	d->anim_start     = GetTickCount();
	expander_pin_height(d); /* reserve full height up-front; content slides within it */
	expander_set_translate(d, expanding ? -d->content_height : 0.0f);
	expander_repaint(d);
	for (int i = 0; i < g_exp_anim_count; i++)
		if (g_exp_anim [i] == d) return;
	if (g_exp_anim_count < EXP_MAX_ANIM) g_exp_anim [g_exp_anim_count++] = d;
	if (!g_exp_timer) g_exp_timer = SetTimer(NULL, 0, 16, expander_timer_proc);
}

static void expander_apply_expanded(FluxExpanderData *d, bool expanded) {
	if (d->expanded == expanded) return;
	d->expanded = expanded;
	xent_set_semantic_expanded(d->ctx, d->root, expanded);
	expander_anim_start(d, expanded);
	if (d->on_toggle) d->on_toggle(d->on_toggle_ctx, expanded);
}

static void expander_toggle(void *ctx) {
	FluxExpanderData *d = ( FluxExpanderData * ) ctx;
	if (d) expander_apply_expanded(d, !d->expanded);
}

static void expander_destroy(void *component_data) {
	expander_anim_remove(( FluxExpanderData * ) component_data);
	free(component_data);
}

/* A vertical flex stack (header over content) rather than a Grid: we don't
 * implement ExpandDirection.Up, so the row-swap a Grid would buy us is unused,
 * and a flex column resolves its intrinsic width cleanly when the expander is
 * measured inside another flex container (a Grid star column cannot). */
static void expander_setup_root(XentContext *ctx, XentNodeId root) {
	xent_set_protocol(ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, root, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(ctx, root, XENT_FLEX_ALIGN_STRETCH);
}

/* Header and content get explicit sizes (xent flex would otherwise split the
 * root's main-axis height between them); width comes from the same control width. */
static void expander_setup_header(FluxExpanderData *d) {
	XentContext *ctx = d->ctx;
	xent_set_protocol(ctx, d->header, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, d->header, XENT_FLEX_ROW);
	xent_set_flex_align_items(ctx, d->header, XENT_FLEX_ALIGN_CENTER);
	xent_set_size(ctx, d->header, (XentSize) {d->width, FLUX_EXPANDER_MIN_HEIGHT});
	xent_set_padding(ctx, d->header, (XentInsets) {FLUX_EXPANDER_HEADER_PAD, 0.0f, FLUX_EXPANDER_CHEVRON_ZONE, 0.0f});
	xent_set_focusable(ctx, d->header, true);
}

static void expander_setup_content(FluxExpanderData *d) {
	XentContext *ctx = d->ctx;
	xent_set_size(ctx, d->content, (XentSize) {d->width, d->content_height});
	xent_set_padding(
	  ctx, d->content,
	  (XentInsets) {
		FLUX_EXPANDER_CONTENT_PAD, FLUX_EXPANDER_CONTENT_PAD, FLUX_EXPANDER_CONTENT_PAD, FLUX_EXPANDER_CONTENT_PAD}
	);
}

/* Allocate the shared runtime and wire it onto the root (owner) and header (borrower:
 * its toggle behavior and expanded-state snapshot read the same pointer). NULL on OOM. */
static FluxExpanderData *
expander_make_runtime(FluxExpanderCreateInfo const *info, XentNodeId root, XentNodeId header, XentNodeId content) {
	FluxNodeData     *root_nd = flux_node_store_get(info->store, root);
	FluxNodeData     *hdr_nd  = flux_node_store_get(info->store, header);
	FluxExpanderData *d       = root_nd ? ( FluxExpanderData * ) calloc(1, sizeof(*d)) : NULL;
	if (!root_nd || !hdr_nd || !d) {
		free(d);
		return NULL;
	}
	d->root                         = root;
	d->header                       = header;
	d->content                      = content;
	d->ctx                          = info->ctx;
	d->store                        = info->store;
	d->window                       = info->window;
	d->width                        = info->width;
	d->content_height               = info->content_height;
	d->expanded                     = info->expanded;
	d->on_toggle                    = info->on_toggle;
	d->on_toggle_ctx                = info->userdata;

	root_nd->component_data         = d;
	root_nd->destroy_component_data = expander_destroy;
	hdr_nd->component_data          = d;
	hdr_nd->behavior.on_click       = expander_toggle;
	hdr_nd->behavior.on_click_ctx   = d;
	return d;
}

static void expander_finalize(FluxExpanderData *d, FluxExpanderCreateInfo const *info) {
	FluxNodeData *content_nd = flux_node_store_get(info->store, d->content);
	if (content_nd) content_nd->render_clip_subtree = true;

	if (info->header) {
		/* Default body font size (BodyTextBlockStyle, 14). */
		FluxTextCreateInfo ti = {info->ctx, info->store, d->header, info->header, 14.0f};
		flux_create_text(&ti);
		xent_set_semantic_label(info->ctx, d->header, info->header);
	}
	if (!info->expanded) xent_remove_child(info->ctx, d->root, d->content);
	expander_pin_height(d);
	xent_set_semantic_expanded(info->ctx, d->root, info->expanded);

	if (info->out_content) *info->out_content = d->content;
	if (info->out_header) *info->out_header = d->header;
}

XentNodeId flux_create_expander(FluxExpanderCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, FLUX_CONTROL_EXPANDER_HEADER, flux_draw_expander_header, NULL);
	flux_node_store_register_renderer(info->store, FLUX_CONTROL_EXPANDER_CONTENT, flux_draw_expander_content, NULL);
	flux_node_store_register_renderer(info->store, FLUX_CONTROL_CONTAINER, flux_draw_container, NULL);
	flux_node_store_register_renderer(info->store, FLUX_CONTROL_TEXT, flux_draw_text, NULL);

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_EXPANDER);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	XentNodeId header  = flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_EXPANDER_HEADER);
	XentNodeId content = flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_EXPANDER_CONTENT);
	if (header == XENT_NODE_INVALID || content == XENT_NODE_INVALID) return root;

	FluxExpanderData *d = expander_make_runtime(info, root, header, content);
	if (!d) return root;

	expander_setup_root(info->ctx, root);
	expander_setup_header(d);
	expander_setup_content(d);
	expander_finalize(d, info);
	return root;
}

XentNodeId flux_expander_content_node(FluxNodeStore *store, XentNodeId id) {
	FluxExpanderData *d = expander_data(store, id);
	return d ? d->content : XENT_NODE_INVALID;
}

XentNodeId flux_expander_header_node(FluxNodeStore *store, XentNodeId id) {
	FluxExpanderData *d = expander_data(store, id);
	return d ? d->header : XENT_NODE_INVALID;
}

void flux_expander_set_expanded(FluxNodeStore *store, XentNodeId id, XentContext *ctx, bool expanded) {
	( void ) ctx;
	FluxExpanderData *d = expander_data(store, id);
	if (d) expander_apply_expanded(d, expanded);
}
