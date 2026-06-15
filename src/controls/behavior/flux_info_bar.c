#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

/* Close hit zone: InfoBarCloseButtonSize (38) + InfoBarCloseButtonMargin (5). */
#define IB_CLOSE_ZONE_W 43.0f
#define IB_MIN_HEIGHT   48.0f
#define IB_PAD_LEFT_TOTAL 46.0f   /* 16 + 16 + 14: pad + icon + gap */
#define IB_PAD_RIGHT_CLOSE 55.0f  /* 12 + 5 + 38: gap + margin + close */
#define IB_TEXT_TOP     14.0f
#define IB_TEXT_FONT    14.0f

typedef struct FluxInfoBarBinding {
	FluxNodeStore *store;
	XentNodeId     node;
	XentContext   *ctx;
	bool           pressed_close; /**< The active press landed on the close button. */
} FluxInfoBarBinding;

static FluxInfoBarData *info_bar_data(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_INFO_BAR) return NULL;
	return ( FluxInfoBarData * ) nd->component_data;
}

static bool info_bar_in_close_zone(FluxInfoBarBinding const *b, float local_x) {
	FluxInfoBarData *d = info_bar_data(b->store, b->node);
	if (!d || !d->is_closable) return false;
	XentRect r = {0};
	xent_get_layout_rect(b->ctx, b->node, &r);
	return local_x >= r.w - IB_CLOSE_ZONE_W;
}

static void info_bar_do_close(FluxInfoBarBinding const *b) {
	FluxInfoBarData *d = info_bar_data(b->store, b->node);
	if (!d) return;
	d->is_open = false;
	if (d->on_close) d->on_close(d->on_close_ctx);
}

static void info_bar_destroy(void *component_data) {
	FluxInfoBarData *d = ( FluxInfoBarData * ) component_data;
	if (!d) return;
	flux_str_free(d->title);
	flux_str_free(d->message);
	free(d);
}

/* The close button is a Button: it closes on Click (release within the button), not on
 * press. Record whether the press hit the close zone, then act on release. */
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

/* The close button is the InfoBar's only tab stop: Space/Enter closes it (WinUI InfoBar
 * CloseButton). The focus rectangle is drawn around the close glyph by flux_draw_info_bar. */
static bool info_bar_on_key(void *ctx, unsigned int vk, bool down) {
	FluxInfoBarBinding *b = ( FluxInfoBarBinding * ) ctx;
	if (!down || !b || (vk != VK_RETURN && vk != VK_SPACE)) return false;
	info_bar_do_close(b);
	return true;
}

static void ib_set_layout_text(XentContext *ctx, XentNodeId node, char const *title, char const *message) {
	char buf [512];
	char const *t = title && title [0] ? title : "";
	char const *m = message && message [0] ? message : "";
	if (t [0] && m [0]) snprintf(buf, sizeof(buf), "%s  %s", t, m);
	else snprintf(buf, sizeof(buf), "%s%s", t, m);
	xent_set_text(ctx, node, buf);
}

XentNodeId flux_create_info_bar(FluxInfoBarCreateInfo const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_INFO_BAR);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData       *nd = flux_node_store_get(info->store, node);
	FluxInfoBarData    *d  = nd ? ( FluxInfoBarData * ) calloc(1, sizeof(FluxInfoBarData)) : NULL;
	FluxInfoBarBinding *b  = nd ? ( FluxInfoBarBinding * ) calloc(1, sizeof(*b)) : NULL;
	if (!nd || !d || !b) {
		free(d);
		free(b);
		return node;
	}
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

	xent_set_size(info->ctx, node, (XentSize) {NAN, NAN});
	xent_set_min_size(info->ctx, node, (XentSize) {0.0f, IB_MIN_HEIGHT});
	float pad_r = info->is_closable ? IB_PAD_RIGHT_CLOSE : 16.0f;
	xent_set_padding(info->ctx, node, (XentInsets) {IB_PAD_LEFT_TOTAL, IB_TEXT_TOP, pad_r, IB_TEXT_TOP});
	xent_set_font_size(info->ctx, node, IB_TEXT_FONT);
	xent_set_text_line_break_policy(info->ctx, node, XENT_LINE_BREAK_WORD_WRAP);
	ib_set_layout_text(info->ctx, node, info->title, info->message);
	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_CONTAINER);
	if (info->title) xent_set_semantic_label(info->ctx, node, info->title);
	/* Only a closable InfoBar is a tab stop -- its single interactive part is the close
	 * button. The focus rectangle renders around the close glyph (flux_draw_info_bar). */
	if (info->is_closable) xent_set_focusable(info->ctx, node, true);

	return node;
}

void flux_info_bar_set_severity(FluxNodeStore *store, XentNodeId id, FluxInfoBarSeverity severity) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (d) d->severity = severity;
}

void flux_info_bar_set_title(FluxNodeStore *store, XentNodeId id, char const *title) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d) return;
	flux_str_replace(&d->title, title);
	ib_set_layout_text(( XentContext * ) d->layout_ctx, ( XentNodeId ) d->layout_node, d->title, d->message);
}

void flux_info_bar_set_message(FluxNodeStore *store, XentNodeId id, char const *message) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d) return;
	flux_str_replace(&d->message, message);
	ib_set_layout_text(( XentContext * ) d->layout_ctx, ( XentNodeId ) d->layout_node, d->title, d->message);
}

void flux_info_bar_set_open(FluxNodeStore *store, XentNodeId id, bool open) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (d) d->is_open = open;
}
