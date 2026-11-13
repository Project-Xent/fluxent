#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_popup.h"
#include "fluxent/flux_host.h"
#include <xtk/xtk_catalog.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#define IB_CLOSE_ZONE_W    43.0f
#define IB_MIN_HEIGHT      48.0f
#define IB_PAD_LEFT_TOTAL  46.0f
#define IB_PAD_RIGHT_CLOSE 55.0f
#define IB_TEXT_TOP        14.0f
#define IB_TEXT_FONT       14.0f

typedef struct FluxInfoBarBinding {
	FluxStore *store;
	XentNodeId node;
	XentCtx   *ctx;
	bool       pressed_close;
} FluxInfoBarBinding;

typedef struct FluxInfoBarRuntime {
	FluxInfoBarData    data;
	FluxInfoBarBinding binding;
} FluxInfoBarRuntime;

static FluxInfoBarData *info_bar_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_INFO_BAR) return NULL;
	return ( FluxInfoBarData * ) nd->component_data;
}

static bool info_bar_in_close_zone(FluxInfoBarBinding const *b, float local_x) {
	FluxInfoBarData *d = info_bar_data(b->store, b->node);
	if (!d || !d->is_closable) return false;
	XentRect r = {0};
	xent_layout_rect(b->ctx, b->node, &r);
	return local_x >= r.w - IB_CLOSE_ZONE_W;
}

static void info_bar_do_close(FluxInfoBarBinding const *b) {
	FluxInfoBarData *d = info_bar_data(b->store, b->node);
	if (!d) return;
	flux_prop_infobar_open(b->store, b->node, false);
	XtkHost *host = flux_store_host(b->store);
	if (host) {
		XtkEvent event = {
		  .id             = XTK_EVENT_CLOSE,
		  .target         = ( XtkCtrl ) b->node,
		  .source         = ( XtkCtrl ) b->node,
		  .current_target = ( XtkCtrl ) b->node,
		  .phase          = XTK_EVENT_PHASE_TARGET,
		  .cause          = XTK_EVENT_CAUSE_USER,
		};
		flux_host_route(host, &event);
	}
	if (d->on_close) d->on_close(d->on_close_ctx);
}

static void info_bar_destroy(void *component_data) {
	FluxInfoBarData *d = ( FluxInfoBarData * ) component_data;
	if (!d) return;
	flux_str_free(d->title);
	flux_str_free(d->message);
	free(d);
}

static void info_bar_on_pointer_down(void *ctx, float local_x, float local_y, int click_count) {
	( void ) local_y;
	( void ) click_count;
	FluxInfoBarBinding *b = ( FluxInfoBarBinding * ) ctx;
	if (b) b->pressed_close = info_bar_in_close_zone(b, local_x);
}

static void info_bar_on_click(void *ctx) {
	FluxInfoBarBinding *b = ( FluxInfoBarBinding * ) ctx;
	if (b && b->pressed_close) info_bar_do_close(b);
}

static bool info_bar_on_key(void *ctx, FluxKey key, bool down) {
	FluxInfoBarBinding *b = ( FluxInfoBarBinding * ) ctx;
	if (!down || !b || (key != FLUX_KEY_ENTER && key != FLUX_KEY_SPACE)) return false;
	info_bar_do_close(b);
	return true;
}

static void ib_set_layout_text(XentCtx *ctx, XentNodeId node, char const *title, char const *message) {
	char        buf [512];
	char const *t = title && title [0] ? title : "";
	char const *m = message && message [0] ? message : "";
	if (t [0] && m [0]) snprintf(buf, sizeof(buf), "%s  %s", t, m);
	else snprintf(buf, sizeof(buf), "%s%s", t, m);
	xent_settext(ctx, node, buf);
}

XentNodeId flux_infobar_init(FluxInfoBarDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_INFO_BAR);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData       *nd = flux_store_find(info->store, node);
	FluxInfoBarRuntime *rt = nd ? ( FluxInfoBarRuntime * ) calloc(1, sizeof(*rt)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return node;
	}
	FluxInfoBarData    *d            = &rt->data;
	FluxInfoBarBinding *b            = &rt->binding;
	d->severity                      = info->severity;
	d->title                         = flux_str_dup(info->title);
	d->message                       = flux_str_dup(info->message);
	d->is_open                       = true;
	d->is_closable                   = info->is_closable;
	d->on_close                      = info->on_close;
	d->on_close_ctx                  = info->userdata;
	d->layout_ctx                    = info->ctx;
	d->layout_node                   = node;

	b->store                         = info->store;
	b->node                          = node;
	b->ctx                           = info->ctx;

	nd->component_data               = d;
	nd->destroy_component_data       = info_bar_destroy;
	nd->behavior.on_pointer_down     = info_bar_on_pointer_down;
	nd->behavior.on_pointer_down_ctx = b;
	nd->behavior.on_click            = info_bar_on_click;
	nd->behavior.on_click_ctx        = b;
	nd->behavior.on_key              = info_bar_on_key;
	nd->behavior.on_key_ctx          = b;

	xent_setsize(info->ctx, node, (XentSize) {NAN, NAN});
	xent_setminsize(info->ctx, node, (XentSize) {0.0f, IB_MIN_HEIGHT});
	float pad_r = info->is_closable ? IB_PAD_RIGHT_CLOSE : 16.0f;
	xent_setp(info->ctx, node, (XentInsets) {IB_PAD_LEFT_TOTAL, IB_TEXT_TOP, pad_r, IB_TEXT_TOP});
	xent_setfontsize(info->ctx, node, IB_TEXT_FONT);
	xent_setlinebreak(info->ctx, node, XENT_LINEBREAK_WORDWRAP);
	ib_set_layout_text(info->ctx, node, info->title, info->message);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_CONTAINER);
	if (info->title) xent_sem_setlabel(info->ctx, node, info->title);
	if (info->is_closable) xent_set_focusable(info->ctx, node, true);

	return node;
}

void flux_prop_infobar_severity(FluxStore *store, XentNodeId id, FluxInfoBarSeverity severity) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (d) d->severity = severity;
}

void flux_prop_infobar_title(FluxStore *store, XentNodeId id, char const *title) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d) return;
	flux_str_replace(&d->title, title);
	ib_set_layout_text(( XentCtx * ) d->layout_ctx, d->layout_node, d->title, d->message);
}

void flux_prop_infobar_message(FluxStore *store, XentNodeId id, char const *message) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d) return;
	flux_str_replace(&d->message, message);
	ib_set_layout_text(( XentCtx * ) d->layout_ctx, d->layout_node, d->title, d->message);
}

void flux_prop_infobar_open(FluxStore *store, XentNodeId id, bool open) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d || d->is_open == open) return;
	d->is_open   = open;
	XentCtx *ctx = ( XentCtx * ) d->layout_ctx;
	if (!open) {
		xent_setsize(ctx, id, (XentSize) {0.0f, 0.0f});
		xent_setminsize(ctx, id, (XentSize) {0.0f, 0.0f});
		xent_setp(ctx, id, (XentInsets) {0});
		xent_settext(ctx, id, "");
		xent_set_focusable(ctx, id, false);
		return;
	}
	xent_setsize(ctx, id, (XentSize) {NAN, NAN});
	xent_setminsize(ctx, id, (XentSize) {0.0f, IB_MIN_HEIGHT});
	xent_setp(
	  ctx, id, (XentInsets) {IB_PAD_LEFT_TOTAL, IB_TEXT_TOP, d->is_closable ? IB_PAD_RIGHT_CLOSE : 16.0f, IB_TEXT_TOP}
	);
	ib_set_layout_text(ctx, id, d->title, d->message);
	xent_set_focusable(ctx, id, d->is_closable);
}

void flux_prop_infobar_closable(FluxStore *store, XentNodeId id, bool closable) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d || d->is_closable == closable) return;
	d->is_closable = closable;
	xent_setp(
	  d->layout_ctx, d->layout_node,
	  (XentInsets) {IB_PAD_LEFT_TOTAL, IB_TEXT_TOP, closable ? IB_PAD_RIGHT_CLOSE : 16.0f, IB_TEXT_TOP}
	);
	xent_set_focusable(d->layout_ctx, d->layout_node, closable);
}
