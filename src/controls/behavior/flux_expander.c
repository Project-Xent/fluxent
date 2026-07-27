#include "controls/behavior/flux_expander_internal.h"
#include "store/flux_node_data.h"
#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_anim_driver.h"

#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_container.h"
#include "host/flux_prop_apply.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_window.h"
#include <xtk/xtk_catalog.h>

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
#define EXP_MIN_WIDTH   96.0f
#define EXP_EXPAND_MS   333.0f
#define EXP_COLLAPSE_MS 167.0f

static FluxExpanderData *expander_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
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
	FluxNodeData *nd = flux_store_find(d->store, d->content);
	if (nd) nd->render_translate_y = dy;
}

static float expander_content_extent(FluxExpanderData const *d) {
	XentRect rect = {0};
	if (xent_layout_rect(d->ctx, d->content, &rect) && rect.h > 0.0f) return rect.h;
	if (isfinite(d->content_height) && d->content_height > 0.0f) return d->content_height;
	return FLUX_EXPANDER_MIN_HEIGHT;
}

static void expander_anim_remove(FluxExpanderData *d) { flux_anim_unregister(d); }

static void expander_anim_finish(FluxExpanderData *d) {
	d->anim_active = false;
	expander_set_translate(d, 0.0f);
	if (!d->anim_expanding) xent_node_remove(d->ctx, d->root, d->content);
}

static bool expander_step(void *ctx, unsigned long now) {
	FluxExpanderData *d        = ( FluxExpanderData * ) ctx;
	float             duration = d->anim_expanding ? EXP_EXPAND_MS : EXP_COLLAPSE_MS;
	float             elapsed  = ( float ) (( unsigned long ) now - d->anim_start);
	float             t        = expander_clamp01(elapsed / duration);
	float             eased    = 1.0f - powf(1.0f - t, 3.0f); /* approx KeySpline 0,0,0,1 / 1,1,0,1 */
	float             h        = expander_content_extent(d);
	/* Expand: -h -> 0 (slide down into place). Collapse (CollapseUp): 0 -> -h
	 * (retract up behind the header). Both end clipped/hidden above row top. */
	expander_set_translate(d, d->anim_expanding ? -(1.0f - eased) * h : -eased * h);
	expander_repaint(d);
	if (elapsed >= duration) {
		expander_anim_finish(d);
		return false;
	}
	return true;
}

static void expander_anim_start(FluxExpanderData *d, bool expanding) {
	if (expanding) xent_node_append(d->ctx, d->root, d->content);
	d->anim_active    = true;
	d->anim_expanding = expanding;
	d->anim_start     = GetTickCount();
	expander_set_translate(d, expanding ? -expander_content_extent(d) : 0.0f);
	expander_repaint(d);
	flux_anim_register(d, expander_step);
}

static void expander_report(FluxExpanderData *d, bool expanded) {
	XtkHost *host = flux_store_host(d->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_TOGGLE,
		  .target         = ( XtkCtrl ) d->root,
		  .source         = ( XtkCtrl ) d->root,
		  .current_target = ( XtkCtrl ) d->root,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		  .payload        = {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = expanded},
		};
		flux_host_route(host, &event);
	}
	if (d->on_toggle) d->on_toggle(d->on_toggle_ctx, expanded);
}

static void expander_apply_expanded(FluxExpanderData *d, bool expanded, bool report) {
	if (d->expanded == expanded) return;
	d->expanded = expanded;
	xent_sem_setexpanded(d->ctx, d->root, expanded);
	expander_anim_start(d, expanded);
	if (report) expander_report(d, expanded);
}

static void expander_toggle(void *ctx) {
	FluxExpanderData *d = ( FluxExpanderData * ) ctx;
	if (d) expander_apply_expanded(d, !d->expanded, true);
}

static void expander_destroy(void *component_data) {
	expander_anim_remove(( FluxExpanderData * ) component_data);
	free(component_data);
}

/* A vertical flex stack (header over content) rather than a Grid: we don't
 * implement ExpandDirection.Up, so the row-swap a Grid would buy us is unused,
 * and a flex column resolves its intrinsic width cleanly when the expander is
 * measured inside another flex container (a Grid star column cannot). */
static void expander_setup_root(FluxExpanderData *d) {
	xent_setproto(d->ctx, d->root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(d->ctx, d->root, XENT_FLEX_COLUMN);
	xent_setitems(d->ctx, d->root, XENT_FLEX_ALIGN_STRETCH);
	xent_setfit(d->ctx, d->root, false, true);
	xent_setminsize(d->ctx, d->root, (XentSize) {EXP_MIN_WIDTH, FLUX_EXPANDER_MIN_HEIGHT});
	xent_setw(d->ctx, d->root, d->width);
}

static void expander_setup_header(FluxExpanderData *d) {
	XentCtx *ctx = d->ctx;
	xent_setproto(ctx, d->header, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, d->header, XENT_FLEX_ROW);
	xent_setitems(ctx, d->header, XENT_FLEX_ALIGN_CENTER);
	xent_setsize(ctx, d->header, (XentSize) {NAN, FLUX_EXPANDER_MIN_HEIGHT});
	xent_setp(ctx, d->header, (XentInsets) {FLUX_EXPANDER_HEADER_PAD, 0.0f, FLUX_EXPANDER_CHEVRON_ZONE, 0.0f});
	xent_set_focusable(ctx, d->header, true);
}

static void expander_setup_content(FluxExpanderData *d) {
	XentCtx *ctx = d->ctx;
	xent_setproto(ctx, d->content, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ctx, d->content, XENT_FLEX_COLUMN);
	xent_setitems(ctx, d->content, XENT_FLEX_ALIGN_STRETCH);
	xent_setfit(ctx, d->content, false, true);
	xent_setsize(ctx, d->content, (XentSize) {NAN, d->content_height});
	xent_setminsize(ctx, d->content, (XentSize) {0.0f, FLUX_EXPANDER_MIN_HEIGHT});
	xent_setp(
	  ctx, d->content,
	  (XentInsets) {
		FLUX_EXPANDER_CONTENT_PAD, FLUX_EXPANDER_CONTENT_PAD, FLUX_EXPANDER_CONTENT_PAD, FLUX_EXPANDER_CONTENT_PAD}
	);
}

/* Allocate the shared runtime and wire it onto the root (owner) and header (borrower:
 * its toggle behavior and expanded-state snapshot read the same pointer). NULL on OOM. */
static FluxExpanderData *
expander_make_runtime(FluxExpanderDesc const *info, XentNodeId root, XentNodeId header, XentNodeId content) {
	FluxNodeData     *root_nd = flux_store_find(info->store, root);
	FluxNodeData     *hdr_nd  = flux_store_find(info->store, header);
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
	d->width                        = info->width > 0.0f ? info->width : NAN;
	d->content_height               = info->content_height > 0.0f ? info->content_height : NAN;
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

static void expander_finalize(FluxExpanderData *d, FluxExpanderDesc const *info) {
	FluxNodeData *content_nd = flux_store_find(info->store, d->content);
	if (content_nd) content_nd->render_clip_subtree = true;

	if (info->header) {
		/* Default body font size (BodyTextBlockStyle, 14). */
		FluxTextDesc ti = {info->ctx, info->store, d->header, info->header, 14.0f};
		d->header_text  = flux_text_init(&ti);
		xent_sem_setlabel(info->ctx, d->header, info->header);
	}
	if (!info->expanded) xent_node_remove(info->ctx, d->root, d->content);
	xent_sem_setexpanded(info->ctx, d->root, info->expanded);

	if (info->out_content) *info->out_content = d->content;
	if (info->out_header) *info->out_header = d->header;
}

XentNodeId flux_expander_init(FluxExpanderDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_EXPANDER);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	XentNodeId header  = flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_EXPANDER_HEADER);
	XentNodeId content = flux_factory_create_node(info->ctx, info->store, root, FLUX_CONTROL_EXPANDER_CONTENT);
	if (header == XENT_NODE_INVALID || content == XENT_NODE_INVALID) return root;

	FluxExpanderData *d = expander_make_runtime(info, root, header, content);
	if (!d) return root;

	expander_setup_root(d);
	expander_setup_header(d);
	expander_setup_content(d);
	expander_finalize(d, info);
	return root;
}

XentNodeId flux_expbody_node(FluxStore *store, XentNodeId id) {
	FluxExpanderData *d = expander_data(store, id);
	return d ? d->content : XENT_NODE_INVALID;
}

XentNodeId flux_exphead_node(FluxStore *store, XentNodeId id) {
	FluxExpanderData *d = expander_data(store, id);
	return d ? d->header : XENT_NODE_INVALID;
}

void flux_prop_expander_expanded(FluxStore *store, XentNodeId id, XentCtx *ctx, bool expanded) {
	( void ) ctx;
	FluxExpanderData *d = expander_data(store, id);
	if (d) expander_apply_expanded(d, expanded, false);
}

void flux_prop_expander_header(FluxStore *store, XentNodeId id, char const *header) {
	FluxExpanderData *d = expander_data(store, id);
	if (!d) return;
	if (d->header_text == XENT_NODE_INVALID) {
		FluxTextDesc desc = {d->ctx, d->store, d->header, header, 14.0f};
		d->header_text    = flux_text_init(&desc);
	}
	else flux_prop_text_content(store, d->header_text, header);
	xent_sem_setlabel(d->ctx, d->header, header);
}

void flux_prop_expander_width(FluxStore *store, XentNodeId id, float width) {
	FluxExpanderData *d = expander_data(store, id);
	if (!d) return;
	d->width = width > 0.0f ? width : NAN;
	xent_setw(d->ctx, d->root, d->width);
}

void flux_prop_expander_content_height(FluxStore *store, XentNodeId id, float height) {
	FluxExpanderData *d = expander_data(store, id);
	if (!d) return;
	d->content_height = height > 0.0f ? height : NAN;
	xent_seth(d->ctx, d->content, d->content_height);
}
