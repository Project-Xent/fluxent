#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_control_uia.h"
#include "controls/behavior/flux_cmdbar_internal.h"
#include "controls/factory/flux_control_build.h"
#include "controls/factory/flux_factory.h"
#include "bridge/flux_internal.h"
#include "render/flux_anim.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"
#include "store/flux_node_data.h"
#include "render/flux_render_snapshot.h"
#include "fluxent/flux_host.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_window.h"

#include <float.h>
#include <math.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

typedef struct PrimaryList {
	XentNodeId node [FLUX_CMDBAR_MAX_ROWS];
	float      width [FLUX_CMDBAR_MAX_ROWS];
	bool       sep [FLUX_CMDBAR_MAX_ROWS];
	int32_t    order [FLUX_CMDBAR_MAX_ROWS];
	bool       flag [FLUX_CMDBAR_MAX_ROWS];
	int        count;
	float      total;
} PrimaryList;

static bool is_item_type(FluxControlType t) {
	return t == FLUX_CONTROL_APPBARBUTTON || t == FLUX_CONTROL_APPBARTOGGLE || t == FLUX_CONTROL_APPBARSEP;
}

FluxAppBarItemData *flux_cmdbar_item(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && is_item_type(nd->component_type) ? ( FluxAppBarItemData * ) nd->component_data : NULL;
}

static FluxCmdBarData *bar_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_type == FLUX_CONTROL_COMMANDBAR ? ( FluxCmdBarData * ) nd->component_data : NULL;
}

static FluxCmdBarData *item_bar(FluxAppBarItemData const *d) {
	XentNodeId p = xent_node_parent(d->ctx, d->node);
	for (int i = 0; i < 3 && p != XENT_NODE_INVALID; i++, p = xent_node_parent(d->ctx, p))
		if (flux_node_type(d->store, p) == FLUX_CONTROL_COMMANDBAR) return bar_data(d->store, p);
	return NULL;
}

void flux_cmdbar_repaint(FluxCmdBarData const *b) {
	HWND hwnd = b->window ? flux_window_hwnd(b->window) : NULL;
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
	hwnd = b->popup ? flux_popup_hwnd(b->popup) : NULL;
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

static void route(FluxStore *store, XentNodeId node, XtkEventId id, XtkEventPayload payload) {
	XtkHost *host = flux_store_host(store);
	if (host) flux_input_route_event(host, node, id, payload, XTK_EVENT_CAUSE_USER);
}

bool flux_cmdbar_moved(FluxCmdBarData const *b, XentNodeId node) {
	for (int i = 0; i < b->nmoved; i++)
		if (b->moved [i] == node) return true;
	return false;
}

static FluxSize text_size(FluxText *text, char const *s, float size, float max_w, bool wrap) {
	if (!s || !s [0]) return (FluxSize) {0.0f, 0.0f};
	if (!text) return (FluxSize) {0.6f * size * ( float ) strlen(s), FLUX_ABB_LABEL_LINE};
	FluxTextStyle ts = {.font_size = size, .font_weight = FLUX_FONT_REGULAR, .word_wrap = wrap};
	return flux_text_measure(text, s, &ts, max_w);
}

static uint8_t label_view(uint8_t pos, bool compact) {
	if (pos == XTK_LABEL_RIGHT) return FLUX_APPBAR_VIEW_RIGHT;
	if (pos == XTK_LABEL_COLLAPSED) return FLUX_APPBAR_VIEW_COLLAPSED;
	return compact ? FLUX_APPBAR_VIEW_COMPACT : FLUX_APPBAR_VIEW_FULL;
}

static uint8_t item_view(FluxCmdBarData const *b, FluxAppBarItemData const *d) {
	bool compact = b && !b->open;
	if (d->kind == FLUX_APPBAR_SEPARATOR) return compact ? FLUX_APPBAR_VIEW_COMPACT : FLUX_APPBAR_VIEW_FULL;
	uint8_t pos = d->label_collapsed ? XTK_LABEL_COLLAPSED : b ? b->label_pos : XTK_LABEL_BOTTOM;
	return label_view(pos, compact);
}

static float right_width(FluxAppBarItemData const *d) {
	if (!isnan(d->user_w)) return d->user_w;
	float tw = ceilf(text_size(d->text, d->label, FLUX_ABB_LABEL_SIZE, FLT_MAX, false).w);
	return FLUX_ABB_ICON_RIGHT_LEFT + FLUX_ABB_ICON + FLUX_ABB_RIGHT_LABEL_LEFT + tw + FLUX_ABB_RIGHT_LABEL_RIGHT;
}

static void measure_full(FluxAppBarItemData *d, float w) {
	float lh   = ceilf(text_size(d->text, d->label, FLUX_ABB_LABEL_SIZE, w - 2.0f * FLUX_ABB_LABEL_PAD_H, true).h);
	d->label_h = lh > 0.0f ? lh : FLUX_ABB_LABEL_LINE;
	d->height  = fmaxf(
	  FLUX_APPBAR_MIN_H,
	  FLUX_ABB_ICON_TOP + FLUX_ABB_ICON + FLUX_ABB_ICON_BOTTOM + d->label_h + FLUX_ABB_LABEL_PAD_BOTTOM
	);
}

static void measure_button(FluxAppBarItemData *d) {
	float w    = isnan(d->user_w) ? FLUX_ABB_W : d->user_w;
	d->label_h = FLUX_ABB_LABEL_LINE;
	d->width   = d->view == FLUX_APPBAR_VIEW_RIGHT ? right_width(d) : w;
	if (d->view == FLUX_APPBAR_VIEW_FULL) measure_full(d, w);
	else d->height = d->view == FLUX_APPBAR_VIEW_COMPACT ? FLUX_APPBAR_MIN_H : FLUX_APPBAR_COMPACT_H;
}

static void item_measure(FluxAppBarItemData *d) {
	d->mdirty = false;
	if (d->kind != FLUX_APPBAR_SEPARATOR) {
		measure_button(d);
		return;
	}
	d->width  = FLUX_ABB_SEP_W + 2.0f * FLUX_ABB_SEP_PAD_H;
	d->height = d->view == FLUX_APPBAR_VIEW_FULL ? NAN : FLUX_APPBAR_COMPACT_H;
}

static bool item_update(FluxCmdBarData const *b, FluxAppBarItemData *d) {
	uint8_t view = item_view(b, d);
	if (view == d->view && !d->mdirty) return false;
	d->view = view;
	item_measure(d);
	return true;
}

static void item_refresh(FluxAppBarItemData *d) {
	d->mdirty         = true;
	FluxCmdBarData *b = item_bar(d);
	if (b) {
		b->dirty = true;
		flux_cmdbar_repaint(b);
		return;
	}
	( void ) item_update(NULL, d);
	xent_setsize(d->ctx, d->node, (XentSize) {d->width, isnan(d->height) ? FLUX_APPBAR_MIN_H : d->height});
}

static bool place(XentCtx *ctx, XentNodeId node, FluxRect r) {
	XentRect cur = {0}, parent = {0};
	xent_layout_rect(ctx, node, &cur);
	xent_layout_rect(ctx, xent_node_parent(ctx, node), &parent);
	bool same = fabsf(cur.x - parent.x - r.x) < 0.01f
	         && fabsf(cur.y - parent.y - r.y) < 0.01f
	         && fabsf(cur.w - r.w) < 0.01f
	         && fabsf(cur.h - r.h) < 0.01f;
	xent_setpos(ctx, node, (XentPoint) {r.x, r.y});
	xent_setsize(ctx, node, (XentSize) {r.w, r.h});
	return !same;
}

static bool in_overflow(FluxCmdBarData const *b, FluxAppBarItemData const *d) {
	return d->secondary || flux_cmdbar_moved(b, d->node);
}

static void list_primary(FluxCmdBarData const *b, PrimaryList *l) {
	memset(l, 0, sizeof(*l));
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID && l->count < FLUX_CMDBAR_MAX_ROWS;
	  c               = xent_node_next(b->ctx, c))
	{
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (!d || in_overflow(b, d)) continue;
		l->node [l->count]   = c;
		l->width [l->count]  = d->width;
		l->sep [l->count]    = d->kind == FLUX_APPBAR_SEPARATOR;
		l->order [l->count]  = d->order;
		l->total            += d->width;
		l->count++;
	}
}

static void flag(PrimaryList *l, int i, float *pw) {
	if (i < 0 || i >= l->count || l->flag [i]) return;
	l->flag [i]  = true;
	*pw         -= l->width [i];
}

static bool list_sep(PrimaryList const *l, int i) { return i >= 0 && i < l->count && l->sep [i]; }

static int  nonsep(PrimaryList const *l, int i, int dir) {
	for (; i >= 0 && i < l->count; i += dir)
		if (!l->sep [i]) return i;
	return -1;
}

static void flag_run(PrimaryList *l, int k, int ns, int dir, float *pw) {
	if (ns < 0) {
		for (; k >= 0 && k < l->count; k += dir) flag(l, k, pw);
		return;
	}
	for (; (ns - k) * dir > 1; k += dir) flag(l, k, pw);
}

static void seps_backward(PrimaryList *l, int i, float *pw) {
	if (list_sep(l, i - 1)) flag_run(l, i - 1, nonsep(l, i - 1, -1), -1, pw);
}

static void seps_forward(PrimaryList *l, int i, float *pw) {
	if (!list_sep(l, i + 1)) return;
	int  ns   = nonsep(l, i + 1, 1);
	bool left = i > 0 && list_sep(l, i - 1);
	flag_run(l, i + 1, ns, 1, pw);
	if (i == 0 || left) flag(l, i + 1, pw);
	if (left && ns < 0) flag(l, i - 1, pw);
}

static int next_order(PrimaryList const *l, int prev) {
	int first = 0;
	for (int i = 0; i < l->count; i++)
		if (l->order [i] > 0 && l->order [i] > prev && (!first || l->order [i] < first)) first = l->order [i];
	return first;
}

static bool move_order_set(PrimaryList *l, int order, float avail, float *pw) {
	bool fits = false;
	for (int i = 0; i < l->count; i++) {
		if (l->order [i] != order) continue;
		flag(l, i, pw);
		if (i > 0) seps_backward(l, i, pw);
		if (i < l->count - 1) seps_forward(l, i, pw);
		fits |= *pw < avail;
	}
	return fits;
}

static bool move_by_order(PrimaryList *l, float avail, float *pw) {
	bool any = false;
	for (int prev = 0, order; (order = next_order(l, prev)) > 0; prev = order) {
		any = true;
		if (move_order_set(l, order, avail, pw)) break;
	}
	for (int i = l->count - 1; any && *pw > avail && i >= 0; i--)
		if (l->order [i] == 0) flag(l, i, pw);
	return any;
}

static void move_from_end(PrimaryList *l, float avail, float *pw) {
	int start = l->count - 1;
	for (int i = start; i > 0; i--) {
		*pw -= l->width [i];
		if (*pw < avail) break;
		start--;
	}
	for (int i = start; i < l->count; i++) l->flag [i] = true;
}

static bool commit_moves(FluxCmdBarData *b, PrimaryList const *l) {
	float min_w = -1.0f;
	bool  any   = false;
	for (int i = 0; i < l->count && b->nmoved < FLUX_CMDBAR_MAX_ROWS; i++) {
		if (!l->flag [i]) continue;
		b->moved [b->nmoved++] = l->node [i];
		any                    = true;
		if (!l->sep [i]) min_w = min_w < 0.0f ? l->width [i] : fminf(min_w, l->width [i]);
	}
	if (any) b->restore_min = min_w;
	return any;
}

static bool move_primary(FluxCmdBarData *b, PrimaryList *l, float avail) {
	float pw = l->total;
	if (!move_by_order(l, avail, &pw)) move_from_end(l, avail, &pw);
	return commit_moves(b, l);
}

static int restore_count(FluxCmdBarData const *b) {
	int top = 0, count = 0;
	for (int i = 0; i < b->nmoved; i++) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, b->moved [i]);
		if (d && d->order > top) top = d->order;
	}
	for (int i = 0; top > 0 && i < b->nmoved; i++) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, b->moved [i]);
		if (d && d->order == top) count++;
	}
	return top > 0 ? count : 1;
}

static float content_natural(FluxCmdBarData const *b) {
	XentRect host = {0}, child = {0};
	float    right = 0.0f;
	xent_layout_rect(b->ctx, b->content, &host);
	for (XentNodeId c = xent_node_first(b->ctx, b->content); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c))
		if (xent_layout_rect(b->ctx, c, &child)) right = fmaxf(right, child.x + child.w - host.x);
	return FLUX_CMDBAR_PAD_LEFT + right;
}

static bool sync_overflow(FluxCmdBarData *b, float width) {
	if (!b->dynamic) {
		bool had  = b->nmoved > 0;
		b->nmoved = 0;
		return had;
	}
	PrimaryList l;
	list_primary(b, &l);
	float more    = b->more_shown ? FLUX_CMDBAR_MORE_W : FLUX_CMDBAR_MORE_MIN_W;
	float content = content_natural(b);
	float desired = content + l.total + more;
	bool  changed = false;
	if (desired > width && l.count > 0) changed = move_primary(b, &l, width - content - more);
	else if (width > b->last_avail
	         && b->nmoved > 0
	         && b->restore_min >= 0.0f
	         && width - desired > b->restore_min * ( float ) restore_count(b))
	{
		b->nmoved = 0;
		changed   = true;
	}
	b->last_avail = width;
	return changed;
}

static bool has_label_at(FluxCmdBarData const *b, uint8_t view) {
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (!d || in_overflow(b, d) || d->kind == FLUX_APPBAR_SEPARATOR || !d->label) continue;
		bool bottom = d->view == FLUX_APPBAR_VIEW_FULL || d->view == FLUX_APPBAR_VIEW_COMPACT;
		if (view == FLUX_APPBAR_VIEW_RIGHT ? d->view == FLUX_APPBAR_VIEW_RIGHT : bottom) return true;
	}
	return false;
}

static bool has_overflow_items(FluxCmdBarData const *b) {
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (d && in_overflow(b, d) && (d->secondary || d->kind != FLUX_APPBAR_SEPARATOR)) return true;
	}
	return false;
}

static bool more_visible(FluxCmdBarData const *b) {
	if (b->more_vis != XTK_OVERFLOW_BUTTON_AUTO) return b->more_vis == XTK_OVERFLOW_BUTTON_VISIBLE;
	return has_overflow_items(b)
	    || b->mode != XTK_APPBAR_COMPACT
	    || b->compact_h != b->content_h
	    || has_label_at(b, FLUX_APPBAR_VIEW_FULL);
}

static bool sync_items(FluxCmdBarData *b) {
	bool changed = false;
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (d) changed |= item_update(b, d);
	}
	return changed;
}

static float content_height(FluxCmdBarData const *b) {
	XentRect r = {0};
	xent_layout_rect(b->ctx, b->content, &r);
	float h = fmaxf(FLUX_APPBAR_COMPACT_H, r.h);
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (d && !in_overflow(b, d) && !isnan(d->height)) h = fmaxf(h, d->height);
	}
	return h;
}

static void sync_heights(FluxCmdBarData *b) {
	b->content_h = content_height(b);
	if (!b->open) b->compact_h = has_label_at(b, FLUX_APPBAR_VIEW_RIGHT) ? b->content_h : FLUX_APPBAR_COMPACT_H;
	b->closed_h  = b->mode == XTK_APPBAR_COMPACT ? b->compact_h
	             : b->mode == XTK_APPBAR_MINIMAL ? FLUX_APPBAR_MINIMAL_H
	                                             : 0.0f;
	b->visible_h = b->closed_h + (b->content_h - b->closed_h) * b->reveal;
	b->shift     = b->open_up ? (b->closed_h - b->content_h) * b->reveal : 0.0f;
}

static bool reachable(FluxCmdBarData const *b) { return b->open || b->animating || b->mode == XTK_APPBAR_COMPACT; }

static bool place_item(FluxCmdBarData const *b, XentNodeId c, float *x) {
	FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
	if (!d) return false;
	d->hidden = in_overflow(b, d);
	xent_set_focusable(b->ctx, c, !d->hidden && d->enabled && d->kind != FLUX_APPBAR_SEPARATOR && reachable(b));
	if (d->hidden) return place(b->ctx, c, (FluxRect) {0});
	float h   = isnan(d->height) ? b->content_h : d->height;
	bool  ch  = place(b->ctx, c, (FluxRect) {*x, 0.0f, d->width, h});
	*x       += d->width;
	return ch;
}

static void set_opacity(FluxCmdBarData const *b, float opacity) {
	FluxNodeData *content = flux_store_find(b->store, b->content);
	FluxNodeData *primary = flux_store_find(b->store, b->primary);
	if (content) content->render_opacity = opacity;
	if (primary) primary->render_opacity = opacity;
}

static float primary_width(FluxCmdBarData const *b) {
	float w = 0.0f;
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (d && !in_overflow(b, d)) w += d->width;
	}
	return w;
}

static bool place_parts(FluxCmdBarData *b, float width) {
	float more = b->more_shown ? FLUX_CMDBAR_MORE_W : FLUX_CMDBAR_MORE_MIN_W;
	float pw   = primary_width(b);
	float px   = width - more - pw;
	float mh   = b->open || b->animating       ? b->content_h
	           : b->mode == XTK_APPBAR_MINIMAL ? FLUX_APPBAR_MINIMAL_H
	                                           : FLUX_APPBAR_COMPACT_H;
	bool  ch   = false;
	xent_seth(b->ctx, b->node, b->closed_h);
	ch |= place(b->ctx, b->root, (FluxRect) {0.0f, b->shift, width, b->visible_h});
	( void ) flux_node_lift(b->store, b->node, b->open || b->animating);
	ch |= place(b->ctx, b->primary, (FluxRect) {px, 0.0f, pw, b->content_h});
	ch |= place(
	  b->ctx, b->more, (FluxRect) {width - FLUX_CMDBAR_MORE_W, 0.0f, b->more_shown ? FLUX_CMDBAR_MORE_W : 0.0f, mh}
	);
	xent_set_focusable(b->ctx, b->more, b->more_shown && b->enabled && (b->open || b->mode != XTK_APPBAR_HIDDEN));
	set_opacity(b, b->mode == XTK_APPBAR_MINIMAL && !b->open ? 0.0f : 1.0f);
	xent_setpos(b->ctx, b->content, (XentPoint) {FLUX_CMDBAR_PAD_LEFT, 0.0f});
	if (b->dynamic) xent_setw(b->ctx, b->content, NAN);
	else xent_setw(b->ctx, b->content, fmaxf(0.0f, px - FLUX_CMDBAR_PAD_LEFT));
	float x = 0.0f;
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c))
		ch |= place_item(b, c, &x);
	return ch;
}

static bool cmdbar_sync(FluxCmdBarData *b) {
	XentRect r = {0};
	if (!xent_layout_rect(b->ctx, b->node, &r)) return false;
	bool changed = sync_items(b);
	b->width     = r.w;
	sync_heights(b);
	b->more_shown  = more_visible(b);
	changed       |= sync_overflow(b, r.w);
	sync_heights(b);
	b->more_shown  = more_visible(b);
	changed       |= place_parts(b, r.w);
	changed       |= b->dirty;
	b->dirty       = false;
	return changed;
}

uint32_t flux_cmdbar_frame(XentCtx *ctx, FluxStore *store, XentNodeId node, FluxNodeData *nd) {
	( void ) ctx;
	( void ) store;
	( void ) node;
	FluxCmdBarData *b = ( FluxCmdBarData * ) nd->component_data;
	if (b && cmdbar_sync(b)) flux_cmdbar_repaint(b);
	return 0u;
}

static bool anim_step(void *ctx, unsigned long now) {
	FluxCmdBarData *b   = ( FluxCmdBarData * ) ctx;
	float           dur = ( float ) (b->anim_open ? FLUX_CMDBAR_OPEN_MS : FLUX_CMDBAR_CLOSE_MS);
	float           t   = fminf(1.0f, ( float ) (now - b->anim_start) / dur);
	float           e   = flux_cubic_bezier(t, 0.0f, 0.0f, 0.0f, 1.0f);
	b->reveal           = b->anim_open ? e : 1.0f - e;
	flux_cmdbar_repaint(b);
	if (t < 1.0f) return true;
	b->animating = false;
	if (!b->open && b->popup) flux_popup_dismiss(b->popup);
	return false;
}

static void start_anim(FluxCmdBarData *b) {
	b->anim_open  = b->open;
	b->anim_start = flux_anim_now();
	if (b->animating) return;
	b->animating = true;
	flux_anim_register(b, anim_step);
}

static bool space_below(FluxCmdBarData const *b) {
	RECT     client = {0};
	XentRect r      = {0};
	HWND     hwnd   = b->window ? flux_window_hwnd(b->window) : NULL;
	if (!hwnd || !GetClientRect(hwnd, &client)) return true;
	UINT  dpi   = GetDpiForWindow(hwnd);
	float scale = dpi ? ( float ) dpi / 96.0f : 1.0f;
	xent_layout_rect(b->ctx, b->node, &r);
	return roundf(r.y + b->content_h) <= roundf(( float ) client.bottom / scale + 1.0f);
}

void flux_cmdbar_open(FluxCmdBarData *b, bool open, bool user, bool keyboard) {
	if (b->open == open) return;
	b->open      = open;
	b->keyboard  = keyboard;
	b->focus_row = -1;
	flux_uia_note(b->store, b->node, -1, open ? FLUX_UIA_EVT_MENU_OPENED : FLUX_UIA_EVT_MENU_CLOSED);
	( void ) cmdbar_sync(b);
	if (open) {
		b->open_up = !space_below(b);
		( void ) cmdbar_sync(b);
		flux_cmdbar_ovf_show(b);
		if (keyboard) b->focus_row = flux_cmdbar_ovf_edge(b, false);
	}
	start_anim(b);
	flux_cmdbar_repaint(b);
	if (user) route(b->store, b->node, XTK_EVENT_OPEN, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = open});
}

void flux_cmdbar_execute(FluxAppBarItemData *d) {
	if (!d->enabled || d->kind == FLUX_APPBAR_SEPARATOR) return;
	FluxCmdBarData *b = item_bar(d);
	if (d->kind == FLUX_APPBAR_TOGGLE) {
		d->checked = !d->checked;
		xent_sem_setchecked(d->ctx, d->node, d->checked);
		route(
		  d->store, d->node, XTK_EVENT_CHANGE_BOOL,
		  (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_BOOL, .u.b = d->checked}
		);
	}
	else route(d->store, d->node, XTK_EVENT_INVOKE, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_NONE});
	if (b && b->open) flux_cmdbar_open(b, false, true, false);
}

static void item_click(void *ctx) { flux_cmdbar_execute(( FluxAppBarItemData * ) ctx); }

static int  bar_stops(FluxCmdBarData const *b, XentNodeId *stops) {
	int n = 0;
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID && n < FLUX_CMDBAR_MAX_ROWS;
	  c               = xent_node_next(b->ctx, c))
		if (xent_get_focusable(b->ctx, c)) stops [n++] = c;
	if (xent_get_focusable(b->ctx, b->more)) stops [n++] = b->more;
	return n;
}

static XentNodeId bar_stop(FluxCmdBarData const *b, XentNodeId from, int dir) {
	XentNodeId stops [FLUX_CMDBAR_MAX_ROWS + 1];
	int        n  = bar_stops(b, stops);
	int        at = 0;
	while (at < n && stops [at] != from) at++;
	int to = at + dir;
	return at < n && to >= 0 && to < n ? stops [to] : XENT_NODE_INVALID;
}

static bool bar_key(FluxCmdBarData *b, XentNodeId from, FluxKey key) {
	if (key == FLUX_KEY_ESCAPE && b->open && !b->sticky) {
		flux_cmdbar_open(b, false, true, false);
		return true;
	}
	if (key != FLUX_KEY_LEFT && key != FLUX_KEY_RIGHT) return false;
	XentNodeId to = bar_stop(b, from, key == FLUX_KEY_LEFT ? -1 : 1);
	if (to != XENT_NODE_INVALID && b->input) flux_input_setfocus(b->input, to, false);
	return true;
}

static bool item_key(void *ctx, FluxKey key, bool down) {
	FluxAppBarItemData *d = ( FluxAppBarItemData * ) ctx;
	FluxCmdBarData     *b = down ? item_bar(d) : NULL;
	return b && bar_key(b, d->node, key);
}

static void more_down(void *ctx, float x, float y, int clicks) {
	( void ) x;
	( void ) y;
	( void ) clicks;
	(( FluxCmdBarData * ) ctx)->more_pointer = true;
}

static void more_click(void *ctx) {
	FluxCmdBarData *b        = ( FluxCmdBarData * ) ctx;
	bool            keyboard = !b->more_pointer;
	b->more_pointer          = false;
	flux_cmdbar_open(b, !b->open, true, keyboard);
}

static bool more_key(void *ctx, FluxKey key, bool down) {
	FluxCmdBarData *b = ( FluxCmdBarData * ) ctx;
	if (!down) return false;
	if (b->open && flux_cmdbar_ovf_key(b, key)) return true;
	if (!b->open && (key == FLUX_KEY_UP || key == FLUX_KEY_DOWN)) {
		flux_cmdbar_open(b, true, true, true);
		b->focus_row = flux_cmdbar_ovf_edge(b, key == FLUX_KEY_UP);
		return true;
	}
	return bar_key(b, b->more, key);
}

static void more_blur(void *ctx) {
	FluxCmdBarData *b = ( FluxCmdBarData * ) ctx;
	b->focus_row      = -1;
	b->more_pointer   = false;
	flux_cmdbar_repaint(b);
}

static void appbar_destroy(void *data) {
	FluxAppBarItemData *d = ( FluxAppBarItemData * ) data;
	if (!d) return;
	flux_str_free(d->label);
	flux_str_free(d->icon);
	flux_str_free(d->accel);
	free(d);
}

static void cmdbar_destroy(void *data) {
	FluxCmdBarData *b = ( FluxCmdBarData * ) data;
	if (!b) return;
	flux_anim_unregister(b);
	flux_cmdbar_ovf_destroy(b);
	free(b);
}

static FluxControlType const kind_type []
  = {FLUX_CONTROL_APPBARBUTTON, FLUX_CONTROL_APPBARTOGGLE, FLUX_CONTROL_APPBARSEP};

XentNodeId flux_appbar_init(XentCtx *ctx, FluxStore *store, FluxText *text, FluxAppBarKind kind) {
	XentNodeId          node = flux_factory_create_node(ctx, store, XENT_NODE_INVALID, kind_type [kind]);
	FluxNodeData       *nd   = node == XENT_NODE_INVALID ? NULL : flux_store_find(store, node);
	FluxAppBarItemData *d    = nd ? ( FluxAppBarItemData * ) calloc(1, sizeof(*d)) : NULL;
	if (!d) return node == XENT_NODE_INVALID ? node : flux_factory_fail_node(ctx, store, node);
	*d = (FluxAppBarItemData) {
	  .ctx     = ctx,
	  .store   = store,
	  .text    = text,
	  .node    = node,
	  .kind    = ( uint8_t ) kind,
	  .enabled = true,
	  .user_w  = NAN,
	  .view    = 0xffu};
	nd->component_data         = d;
	nd->destroy_component_data = appbar_destroy;
	if (kind != FLUX_APPBAR_SEPARATOR) {
		nd->behavior
		  = (FluxNodeBehavior) {.on_click = item_click, .on_click_ctx = d, .on_key = item_key, .on_key_ctx = d};
		xent_set_focusable(ctx, node, true);
	}
	xent_sem_setrole(ctx, node, kind == FLUX_APPBAR_SEPARATOR ? XENT_SEM_ROLE_CONTAINER : XENT_SEM_ROLE_BUTTON);
	xent_setshrink(ctx, node, 0.0f);
	item_refresh(d);
	return node;
}

static XentNodeId make_part(XentCtx *ctx, FluxStore *store, XentNodeId parent, XentProtocol proto) {
	XentNodeId part = flux_factory_create_node(ctx, store, parent, FLUX_CONTROL_CONTAINER);
	if (part != XENT_NODE_INVALID) xent_setproto(ctx, part, proto);
	return part;
}

static bool make_parts(FluxCmdBarData *b) {
	b->root    = make_part(b->ctx, b->store, b->node, XENT_PROTOCOL_ABSOLUTE);
	b->content = make_part(b->ctx, b->store, b->root, XENT_PROTOCOL_FLEX);
	b->primary = make_part(b->ctx, b->store, b->root, XENT_PROTOCOL_ABSOLUTE);
	b->more    = flux_factory_create_node(b->ctx, b->store, b->root, FLUX_CONTROL_CMDMORE);
	if (b->root == XENT_NODE_INVALID
	    || b->content == XENT_NODE_INVALID
	    || b->primary == XENT_NODE_INVALID
	    || b->more == XENT_NODE_INVALID)
		return false;
	xent_setflexdir(b->ctx, b->content, XENT_FLEX_ROW);
	xent_setitems(b->ctx, b->content, XENT_FLEX_ALIGN_CENTER);
	FluxNodeData *root = flux_store_find(b->store, b->root);
	FluxNodeData *more = flux_store_find(b->store, b->more);
	if (!root || !more) return false;
	root->clips_children = true;
	more->component_data = b;
	more->behavior       = (FluxNodeBehavior) {
	  .on_click            = more_click,
	  .on_click_ctx        = b,
	  .on_pointer_down     = more_down,
	  .on_pointer_down_ctx = b,
	  .on_key              = more_key,
	  .on_key_ctx          = b,
	  .on_blur             = more_blur,
	  .on_blur_ctx         = b,
	};
	xent_sem_setrole(b->ctx, b->more, XENT_SEM_ROLE_BUTTON);
	return true;
}

XentNodeId flux_cmdbar_init(XentCtx *ctx, FluxStore *store, FluxControlEnv const *env) {
	XentNodeId      node = flux_factory_create_node(ctx, store, XENT_NODE_INVALID, FLUX_CONTROL_COMMANDBAR);
	FluxNodeData   *nd   = node == XENT_NODE_INVALID ? NULL : flux_store_find(store, node);
	FluxCmdBarData *b    = nd ? ( FluxCmdBarData * ) calloc(1, sizeof(*b)) : NULL;
	if (!b) return node == XENT_NODE_INVALID ? node : flux_factory_fail_node(ctx, store, node);
	*b = (FluxCmdBarData) {
	  .ctx         = ctx,
	  .store       = store,
	  .node        = node,
	  .window      = env->window,
	  .text        = env->text,
	  .theme       = env->theme,
	  .input       = env->input,
	  .enabled     = true,
	  .restore_min = -1.0f,
	  .compact_h   = FLUX_APPBAR_COMPACT_H,
	  .content_h   = FLUX_APPBAR_COMPACT_H,
	  .focus_row   = -1};
	nd->component_data         = b;
	nd->destroy_component_data = cmdbar_destroy;
	xent_setproto(ctx, node, XENT_PROTOCOL_ABSOLUTE);
	xent_seth(ctx, node, FLUX_APPBAR_COMPACT_H);
	xent_sem_setrole(ctx, node, XENT_SEM_ROLE_CONTAINER);
	if (!make_parts(b) || !flux_cmdbar_ovf_create(b)) return flux_factory_fail_node(ctx, store, node);
	return node;
}

void flux_cmdbar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxCmdBarData *b = bar_data(store, node);
	if (!b) return;
	b->ctx  = ctx;
	b->node = node;
}

void flux_appbar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	d->ctx  = ctx;
	d->node = node;
}

XentNodeId flux_cmdbar_slot(FluxStore *store, XentNodeId bar, XentNodeId child) {
	FluxCmdBarData *b = bar_data(store, bar);
	if (!b) return XENT_NODE_INVALID;
	b->dirty = true;
	return is_item_type(flux_node_type(store, child)) ? b->primary : b->content;
}

XentNodeId flux_cmdbar_content(FluxStore *store, XentNodeId bar) {
	FluxCmdBarData *b = bar_data(store, bar);
	return b ? b->content : XENT_NODE_INVALID;
}

static void bar_changed(FluxCmdBarData *b) {
	b->dirty = true;
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData *d = flux_cmdbar_item(b->store, c);
		if (d) d->mdirty = true;
	}
	flux_cmdbar_repaint(b);
}

void flux_cmdbar_setopen(FluxStore *store, XentNodeId node, bool open) {
	FluxCmdBarData *b = bar_data(store, node);
	if (b) flux_cmdbar_open(b, open, false, false);
}

void flux_cmdbar_setsticky(FluxStore *store, XentNodeId node, bool sticky) {
	FluxCmdBarData *b = bar_data(store, node);
	if (!b) return;
	b->sticky = sticky;
	if (b->popup) flux_popup_setdismissoutside(b->popup, !sticky);
}

void flux_cmdbar_setmode(FluxStore *store, XentNodeId node, uint32_t mode) {
	FluxCmdBarData *b = bar_data(store, node);
	if (!b) return;
	b->mode = ( uint8_t ) mode;
	bar_changed(b);
}

void flux_cmdbar_setlabels(FluxStore *store, XentNodeId node, uint32_t position) {
	FluxCmdBarData *b = bar_data(store, node);
	if (!b) return;
	b->label_pos = ( uint8_t ) position;
	bar_changed(b);
}

void flux_cmdbar_setmore(FluxStore *store, XentNodeId node, uint32_t visibility) {
	FluxCmdBarData *b = bar_data(store, node);
	if (!b) return;
	b->more_vis = ( uint8_t ) visibility;
	bar_changed(b);
}

void flux_cmdbar_setdynamic(FluxStore *store, XentNodeId node, bool dynamic) {
	FluxCmdBarData *b = bar_data(store, node);
	if (!b) return;
	b->dynamic = dynamic;
	bar_changed(b);
}

void flux_cmdbar_setenabled(FluxStore *store, XentNodeId node, bool enabled) {
	FluxCmdBarData *b = bar_data(store, node);
	if (!b) return;
	b->enabled = enabled;
	xent_sem_setenabled(b->ctx, node, enabled);
	bar_changed(b);
}

static void item_setstr(FluxStore *store, XentNodeId node, size_t offset, char const *text) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	flux_str_replace(( char const ** ) (( char * ) d + offset), text && text [0] ? text : NULL);
	if (offset == offsetof(FluxAppBarItemData, label)) xent_sem_setlabel(d->ctx, node, text ? text : "");
	item_refresh(d);
}

void flux_appbar_settext(FluxStore *store, XentNodeId node, char const *text) {
	item_setstr(store, node, offsetof(FluxAppBarItemData, label), text);
}

void flux_appbar_seticon(FluxStore *store, XentNodeId node, char const *icon) {
	item_setstr(store, node, offsetof(FluxAppBarItemData, icon), icon);
}

void flux_appbar_setaccel(FluxStore *store, XentNodeId node, char const *accel) {
	item_setstr(store, node, offsetof(FluxAppBarItemData, accel), accel);
}

void flux_appbar_setenabled(FluxStore *store, XentNodeId node, bool enabled) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	d->enabled = enabled;
	xent_sem_setenabled(d->ctx, node, enabled);
	item_refresh(d);
}

void flux_appbar_setchecked(FluxStore *store, XentNodeId node, bool checked) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	d->checked = checked;
	xent_sem_setchecked(d->ctx, node, checked);
	item_refresh(d);
}

void flux_appbar_setsecondary(FluxStore *store, XentNodeId node, bool secondary) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	d->secondary = secondary;
	item_refresh(d);
}

void flux_appbar_setorder(FluxStore *store, XentNodeId node, int32_t order) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	d->order = order;
	item_refresh(d);
}

void flux_appbar_setlabelpos(FluxStore *store, XentNodeId node, uint32_t position) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	d->label_collapsed = position == XTK_ELEMENT_LABEL_COLLAPSED;
	item_refresh(d);
}

void flux_appbar_setwidth(FluxStore *store, XentNodeId node, float width) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	if (!d) return;
	d->user_w = width;
	item_refresh(d);
}

static FluxAppBarSnapshot snap_item(FluxAppBarItemData const *d) {
	return (FluxAppBarSnapshot) {
	  .label   = d->label,
	  .icon    = d->icon,
	  .kind    = d->kind,
	  .view    = d->view,
	  .checked = d->checked,
	  .label_h = d->label_h,
	};
}

void flux_snap_appbar(FluxSnapCtx const *ctx) {
	ctx->snap->u.app_bar = snap_item(( FluxAppBarItemData const * ) ctx->data);
}

void flux_snap_cmdbar(FluxSnapCtx const *ctx) {
	FluxCmdBarData const *b = ( FluxCmdBarData const * ) ctx->data;
	ctx->snap->u.cmd_bar    = (FluxCmdBarSnapshot) {
	  .open      = b->open,
	  .bg_y      = b->shift,
	  .bg_h      = b->visible_h,
	  .more_open = b->open || b->animating,
	  .minimal   = b->mode == XTK_APPBAR_MINIMAL,
	};
}

void flux_snap_cmdmore(FluxSnapCtx const *ctx) {
	FluxCmdBarData const *b = ( FluxCmdBarData const * ) ctx->data;
	ctx->snap->u.cmd_bar    = (FluxCmdBarSnapshot) {.open = b->open, .more_open = b->open || b->animating};
}

static FluxCmdBarData *cmdbar_of(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (nd && nd->component_type == FLUX_CONTROL_CMDMORE) return ( FluxCmdBarData * ) nd->behavior.on_click_ctx;
	return bar_data(store, node);
}

static int bar_primary_set(FluxCmdBarData const *b, XentNodeId node, int *pos) {
	int n = 0;
	for (XentNodeId c = xent_node_first(b->ctx, b->primary); c != XENT_NODE_INVALID; c = xent_node_next(b->ctx, c)) {
		FluxAppBarItemData const *d = flux_cmdbar_item(b->store, c);
		if (!d || in_overflow(b, d) || d->kind == FLUX_APPBAR_SEPARATOR) continue;
		n++;
		if (c == node) *pos = n;
	}
	return n;
}

bool flux_appbar_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level) {
	( void ) level;
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	FluxCmdBarData     *b = d ? item_bar(d) : NULL;
	if (!b || in_overflow(b, d)) return false;
	*size = bar_primary_set(b, node, pos) + (b->more_shown ? 1 : 0);
	return true;
}

bool flux_appbar_uia_concealed(FluxStore *store, XentNodeId node) {
	FluxAppBarItemData *d = flux_cmdbar_item(store, node);
	FluxCmdBarData     *b = d ? item_bar(d) : NULL;
	return b && in_overflow(b, d);
}

bool flux_cmdmore_uia_setinfo(FluxStore *store, XentNodeId node, int *pos, int *size, int *level) {
	( void ) level;
	FluxCmdBarData *b = cmdbar_of(store, node);
	if (!b || !b->more_shown) return false;
	*pos = *size = bar_primary_set(b, XENT_NODE_INVALID, pos) + 1;
	return true;
}

int flux_cmdbar_uia_expand(FluxStore *store, XentNodeId node) {
	FluxCmdBarData *b = cmdbar_of(store, node);
	return b && b->open ? 1 : 0;
}

void flux_cmdbar_uia_toggle(FluxStore *store, XentNodeId node) {
	FluxCmdBarData *b = cmdbar_of(store, node);
	if (b) flux_cmdbar_open(b, !b->open, true, true);
}

unsigned flux_cmdbar_uia_live(FluxStore *store, XentNodeId node) {
	FluxCmdBarData *b = cmdbar_of(store, node);
	return b && b->open ? ~0u : ~( unsigned ) FLUX_UIA_P_WINDOW;
}

void flux_cmdmore_uia_name(FluxStore *store, XentNodeId node, char *out, size_t size) {
	FluxCmdBarData *b     = cmdbar_of(store, node);
	char const     *owner = b ? flux_uia_apptext(store, b->node, FLUX_A11Y_NAME) : NULL;
	bool            open  = b && b->open;
	if (owner && owner [0]) snprintf(out, size, open ? "Less app bar for %s" : "More options for %s", owner);
	else snprintf(out, size, "%s", open ? "Less app bar" : "More options");
}
