#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"
#include "render/flux_icon.h"
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

static char const *ib_severity_name(FluxInfoBarSeverity severity) {
	switch (severity) {
	case FLUX_INFOBAR_SUCCESS : return "Info message, severity success";
	case FLUX_INFOBAR_WARNING : return "Info message, severity warning";
	case FLUX_INFOBAR_ERROR   : return "Info message, severity error";
	default                   : return "Info message, severity informational";
	}
}

static void ib_notify(FluxStore *store, XentNodeId id, FluxInfoBarData const *d, bool open) {
	char text [512];
	bool loud = d->severity == FLUX_INFOBAR_ERROR || d->severity == FLUX_INFOBAR_WARNING;
	int  proc = loud ? FLUX_UIA_PROC_IMPORTANT_ALL : FLUX_UIA_PROC_CURRENT_RECENT;
	snprintf(
	  text, sizeof(text), "%s %s %s", ib_severity_name(d->severity), d->title ? d->title : "",
	  d->message ? d->message : ""
	);
	if (open) flux_uia_notify(store, id, FLUX_UIA_NOTE_OTHER, proc, text, "InfoBarOpenedActivityId");
	else flux_uia_notify(store, id, FLUX_UIA_NOTE_OTHER, proc, "InfoBar dismissed", "InfoBarClosedActivityId");
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
	if (info->is_closable) xent_set_focusable(info->ctx, node, true);
	d->notify_open = true;
	return node;
}

void flux_infobar_frame(FluxStore *store, XentNodeId node) {
	FluxInfoBarData *d = info_bar_data(store, node);
	if (!d || !d->notify_open) return;
	d->notify_open = false;
	if (d->is_open) ib_notify(store, node, d, true);
}

void flux_prop_infobar_severity(FluxStore *store, XentNodeId id, FluxInfoBarSeverity severity) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d) return;
	d->severity = severity;
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

#define IB_UIA_PAD_LEFT     16.0f
#define IB_UIA_ICON_TOP     16.0f
#define IB_UIA_ICON_SIZE    16.0f
#define IB_UIA_ICON_GAP     14.0f
#define IB_UIA_MSG_GAP      12.0f
#define IB_UIA_CLOSE_SIZE   38.0f
#define IB_UIA_CLOSE_MARGIN 5.0f

int flux_infobar_uia_heads(FluxStore *store, XentNodeId node) {
	FluxInfoBarData *d = info_bar_data(store, node);
	if (!d) return 0;
	return d->is_closable ? 4 : 3;
}

int flux_infobar_uia_kind(FluxStore *store, XentNodeId node, int sub, unsigned *patterns) {
	( void ) store;
	( void ) node;
	*patterns = sub == FLUX_UIA_SUB_HEAD + 3 ? FLUX_UIA_P_INVOKE : FLUX_UIA_P_TEXT;
	return sub == FLUX_UIA_SUB_HEAD + 3 ? FLUX_UIA_BUTTON : FLUX_UIA_TEXT;
}

static float ib_uia_width(FluxStore *store, char const *text, uint16_t weight, float avail) {
	XentTextMetrics m = {0};
	if (!text || !text [0] || avail <= 0.0f) return 0.0f;
	XentTextMeasureReq req = {
	  .text              = text,
	  .font_size         = IB_TEXT_FONT,
	  .font_weight       = weight,
	  .width_constraint  = avail,
	  .line_break_policy = XENT_LINEBREAK_WORDWRAP,
	  .width_mode        = XENT_MEASURE_AT_MOST};
	if (!xent_text_measure(flux_store_ctx(store), &req, &m)) return 0.0f;
	return m.width < avail ? m.width : avail;
}

static XentRect ib_uia_text_rect(FluxStore *store, FluxInfoBarData const *d, XentRect r, bool message) {
	float close = d->is_closable ? r.x + r.w - IB_UIA_CLOSE_MARGIN - IB_UIA_CLOSE_SIZE : r.x + r.w;
	float x     = r.x + IB_UIA_PAD_LEFT + IB_UIA_ICON_SIZE + IB_UIA_ICON_GAP;
	float right = close - IB_UIA_MSG_GAP > x ? close - IB_UIA_MSG_GAP : x;
	float tw    = ib_uia_width(store, d->title, 600u, right - x);
	if (message) x += tw + (tw > 0.0f ? IB_UIA_MSG_GAP : 0.0f);
	float w = message ? ib_uia_width(store, d->message, 400u, right - x) : tw;
	return (XentRect) {x, r.y + IB_TEXT_TOP, w, w > 0.0f ? 20.0f : 0.0f};
}

bool flux_infobar_uia_rect(FluxStore *store, XentNodeId node, int sub, XentRect *rect) {
	FluxInfoBarData *d = info_bar_data(store, node);
	XentRect         r = *rect;
	if (!d) return false;
	switch (sub - FLUX_UIA_SUB_HEAD) {
	case 0 :
		*rect = (XentRect) {r.x + IB_UIA_PAD_LEFT, r.y + IB_UIA_ICON_TOP, IB_UIA_ICON_SIZE, IB_UIA_ICON_SIZE};
		break;
	case 1 : *rect = ib_uia_text_rect(store, d, r, false); break;
	case 2 : *rect = ib_uia_text_rect(store, d, r, true); break;
	default :
		*rect = (XentRect) {
		  r.x + r.w - IB_UIA_CLOSE_MARGIN - IB_UIA_CLOSE_SIZE, r.y + IB_UIA_CLOSE_MARGIN, IB_UIA_CLOSE_SIZE,
		  IB_UIA_CLOSE_SIZE};
		break;
	}
	return true;
}

static char const *ib_uia_glyph(FluxInfoBarSeverity severity) {
	static char const *const glyph [4] = {"ï¿", "ï¾", "ï¼", "ï½"};
	return ( unsigned ) severity < 4u ? glyph [severity] : glyph [0];
}

static char const *ib_uia_text_content(FluxInfoBarData const *d, int part) {
	if (part == 0) return ib_uia_glyph(d->severity);
	return part == 1 ? d->title : d->message;
}

bool flux_infobar_uia_text(FluxStore *store, XentNodeId node, int sub, FluxUiaTextView *out) {
	FluxInfoBarData *d    = info_bar_data(store, node);
	int              part = sub - FLUX_UIA_SUB_HEAD;
	XentRect         r    = {0};
	if (!d || part < 0 || part > 2) return false;
	xent_layout_rect(flux_store_ctx(store), node, &r);
	XentRect box
	  = part ? ib_uia_text_rect(store, d, r, part == 2) : (XentRect) {0, 0, IB_UIA_ICON_SIZE, IB_UIA_ICON_SIZE};
	char const *text = ib_uia_text_content(d, part);
	*out             = (FluxUiaTextView) {
	  .text = text ? text : "", .layout_w = box.w + 1.0f, .clip = {0.0f, 0.0f, box.w + 1.0f, box.h},
                    .readonly = true
    };
	out->style.font_family = part == 0 ? flux_icon_font_family() : NULL;
	out->style.font_size   = part == 0 ? IB_UIA_ICON_SIZE : IB_TEXT_FONT;
	out->style.font_weight = part == 1 ? FLUX_FONT_SEMI_BOLD : FLUX_FONT_REGULAR;
	out->style.word_wrap   = part != 0;
	return true;
}

void flux_infobar_uia_name(FluxStore *store, XentNodeId node, int sub, char *out, size_t size) {
	FluxInfoBarData *d = info_bar_data(store, node);
	if (!d) return;
	switch (sub - FLUX_UIA_SUB_HEAD) {
	case 0  : snprintf(out, size, "%s", ib_severity_name(d->severity)); break;
	case 1  : snprintf(out, size, "%s", d->title ? d->title : ""); break;
	case 2  : snprintf(out, size, "%s", d->message ? d->message : ""); break;
	default : snprintf(out, size, "Close"); break;
	}
}

bool flux_infobar_uia_focused(FluxStore *store, XentNodeId node, int sub) {
	FluxInfoBarData *d = info_bar_data(store, node);
	return d && d->is_closable && sub == FLUX_UIA_SUB_HEAD + 3;
}

bool flux_infobar_uia_invoke(FluxStore *store, XentNodeId node, int sub) {
	XentCtx         *ctx = flux_store_ctx(store);
	FluxInfoBarData *d   = info_bar_data(store, node);
	if (!d || sub != FLUX_UIA_SUB_HEAD + 3) return false;
	flux_uia_note(store, node, sub, FLUX_UIA_EVT_INVOKED);
	info_bar_do_close(&(FluxInfoBarBinding) {.store = store, .node = node, .ctx = ctx});
	return true;
}

bool flux_infobar_uia_concealed(FluxStore *store, XentNodeId node) {
	FluxInfoBarData *d = info_bar_data(store, node);
	return d && !d->is_open;
}

void flux_prop_infobar_open(FluxStore *store, XentNodeId id, bool open) {
	FluxInfoBarData *d = info_bar_data(store, id);
	if (!d || d->is_open == open) return;
	d->is_open = open;
	ib_notify(store, id, d, open);
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
