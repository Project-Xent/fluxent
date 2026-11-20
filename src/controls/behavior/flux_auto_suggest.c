#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "controls/factory/flux_factory.h"
#include "store/flux_node_data.h"
#include "bridge/flux_internal.h"
#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_anim.h"
#include "render/flux_fluent.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/factory/flux_init_textbox.h"
#include "controls/factory/flux_init_container.h"
#include "controls/data/flux_scroll_data.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/flux_control_uia.h"
#include "popup/flux_popup_tree.h"
#include "render/flux_render_snapshot.h"
#include "fluxent/flux_app.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define ASB_ROW_H      40.0f
#define ASB_MAX_H      374.0f
#define ASB_GAP        2.0f
#define ASB_PAD_Y      2.0f
#define ASB_CORNER     8.0f
#define ASB_BORDER     1.0f
#define ASB_TEXT_PAD_L 16.0f
#define ASB_TEXT_PAD_R 12.0f

typedef struct FluxAsbRuntime FluxAsbRuntime;

typedef struct FluxAsbItem {
	FluxAsbRuntime *rt;
	int             index;
} FluxAsbItem;

struct FluxAsbRuntime {
	FluxStore   *store;
	XentCtx     *ctx;
	XentNodeId   root;
	XentNodeId   textbox;
	XentNodeId   button;
	FluxWindow  *window;
	FluxText    *text;
	FluxTheme   *theme;
	FluxPopup   *popup;

	char const **items;
	int          count;
	int          highlight;
	float        width, height;
	float        scroll_from, scroll_to;
	DWORD        scroll_anim_start;
	bool         open;
	DWORD        rows_anim_start;
	bool         focused;
	bool         ignore_text;

	char const  *user_text;

	bool         (*tb_on_key)(void *ctx, FluxKey key, bool down);
	void        *tb_on_key_ctx;

	void         (*on_text)(void *ctx, char const *text);
	void         (*on_query)(void *ctx, char const *text);
	void         (*on_chosen)(void *ctx, int index);
	void        *text_ctx;
	void        *query_ctx;
	void        *chosen_ctx;
	FluxInput   *input;
	XentNodeId   list_root;
	XentNodeId   list_scroll;
	XentNodeId  *rows;
	FluxAsbItem *row_data;
	int          row_count;
};

static FluxAsbRuntime *asb_runtime(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_AUTO_SUGGEST || !nd->component_data) return NULL;
	return ( FluxAsbRuntime * ) nd->component_data;
}

static void asb_notify_text(FluxAsbRuntime *rt, char const *text) {
	void  (*notify)(void *, char const *) = rt->on_text;
	void *notify_ctx                      = rt->text_ctx;
	if (!text) text = "";
	flux_input_route_event(
	  flux_store_host(rt->store), rt->root, XTK_EVENT_CHANGE_STRING,
	  (XtkEventPayload) {
		.kind     = XTK_EVENT_PAYLOAD_STRING,
		.u.string = {text, ( uint32_t ) strlen(text)},
	},
	  XTK_EVENT_CAUSE_USER
	);
	if (notify) notify(notify_ctx, text);
}

static void asb_notify_chosen(FluxAsbRuntime *rt, int index) {
	void  (*notify)(void *, int) = rt->on_chosen;
	void *notify_ctx             = rt->chosen_ctx;
	flux_input_route_event(
	  flux_store_host(rt->store), rt->root, XTK_EVENT_SELECT,
	  (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index}, XTK_EVENT_CAUSE_USER
	);
	if (notify) notify(notify_ctx, index);
}

static void asb_notify_query(FluxAsbRuntime *rt, char const *text, int index) {
	void  (*notify)(void *, char const *) = rt->on_query;
	void *notify_ctx                      = rt->query_ctx;
	if (!text) text = "";
	flux_input_route_event(
	  flux_store_host(rt->store), rt->root, XTK_EVENT_QUERY,
	  (XtkEventPayload) {
		.kind    = XTK_EVENT_PAYLOAD_QUERY,
		.u.query = {text, ( uint32_t ) strlen(text), index},
	},
	  XTK_EVENT_CAUSE_USER
	);
	if (notify) notify(notify_ctx, text);
}

static void asb_notify_chosen_query(FluxAsbRuntime *rt, char const *text, int index) {
	XtkHost   *host                           = flux_store_host(rt->store);
	XentNodeId root                           = rt->root;
	void       (*chosen)(void *, int)         = rt->on_chosen;
	void      *chosen_ctx                     = rt->chosen_ctx;
	void       (*query)(void *, char const *) = rt->on_query;
	void      *query_ctx                      = rt->query_ctx;
	if (!text) text = "";
	flux_input_route_event(
	  host, root, XTK_EVENT_SELECT, (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index},
	  XTK_EVENT_CAUSE_USER
	);
	flux_input_route_event(
	  host, root, XTK_EVENT_QUERY,
	  (XtkEventPayload) {
		.kind    = XTK_EVENT_PAYLOAD_QUERY,
		.u.query = {text, ( uint32_t ) strlen(text), index},
	},
	  XTK_EVENT_CAUSE_USER
	);
	if (chosen) chosen(chosen_ctx, index);
	if (query) query(query_ctx, text);
}

static char const *asb_box_text(FluxAsbRuntime const *rt) {
	FluxNodeData *nd = flux_store_find(rt->store, rt->textbox);
	if (!nd || !nd->component_data) return NULL;
	return (( FluxTextBoxData * ) nd->component_data)->content;
}

static void asb_write_box(FluxAsbRuntime *rt, char const *text) {
	rt->ignore_text = true;
	flux_prop_textbox_content(rt->store, rt->textbox, text ? text : "");
	rt->ignore_text = false;
}

static float           asb_viewport_h(FluxAsbRuntime const *rt) { return rt->height - 2.0f * (ASB_BORDER + ASB_PAD_Y); }

static void            asb_repaint(FluxAsbRuntime *rt);

static FluxScrollData *asb_scroll(FluxAsbRuntime const *rt) {
	FluxNodeData *nd = rt->list_scroll ? flux_store_find(rt->store, rt->list_scroll) : NULL;
	return nd ? ( FluxScrollData * ) nd->component_data : NULL;
}

static float asb_scroll_y(FluxAsbRuntime const *rt) {
	FluxScrollData const *sd = asb_scroll(rt);
	return sd ? sd->scroll_y : 0.0f;
}

static void asb_scroll_set(FluxAsbRuntime *rt, float y) {
	FluxScrollData *sd = asb_scroll(rt);
	if (!sd) return;
	sd->scroll_y   = y;
	sd->reported_y = y;
}

static bool asb_scroll_step(void *ctx, unsigned long now) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (!rt->scroll_anim_start || !rt->open) {
		rt->scroll_anim_start = 0;
		return false;
	}
	float t = ( float ) (( DWORD ) now - rt->scroll_anim_start) / 250.0f;
	if (t >= 1.0f) {
		asb_scroll_set(rt, rt->scroll_to);
		rt->scroll_anim_start = 0;
	}
	else {
		float e = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
		asb_scroll_set(rt, rt->scroll_from + (rt->scroll_to - rt->scroll_from) * e);
	}
	asb_repaint(rt);
	return rt->scroll_anim_start != 0;
}

static void asb_ensure_visible(FluxAsbRuntime *rt, int i) {
	if (i < 0) return;
	float current = rt->scroll_anim_start ? rt->scroll_to : asb_scroll_y(rt);
	float top = ( float ) i * ASB_ROW_H, vp = asb_viewport_h(rt);
	float target = current;
	if (top < current) target = top;
	else if (top + ASB_ROW_H > current + vp) target = top + ASB_ROW_H - vp;
	if (target == current) return;

	rt->scroll_from       = asb_scroll_y(rt);
	rt->scroll_to         = target;
	rt->scroll_anim_start = flux_anim_now();
	flux_anim_register(rt, asb_scroll_step);
}

static FluxColor asb_fade(FluxColor c, float f) {
	if (f >= 1.0f) return c;
	uint32_t a = ( uint32_t ) (( float ) (c.rgba & 0xffu) * f);
	c.rgba     = (c.rgba & 0xffffff00u) | a;
	return c;
}

static void asb_repaint(FluxAsbRuntime *rt) {
	HWND h = rt->popup ? flux_popup_hwnd(rt->popup) : NULL;
	if (h) InvalidateRect(h, NULL, FALSE);
}

#define ASB_POPIN_MS       367
#define ASB_POPIN_FADE_IN  83.0f
#define ASB_POPIN_FADE_LEN 83.0f
#define ASB_POPIN_OFFSET   50.0f

static bool asb_rows_anim_step(void *ctx, unsigned long now) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (!rt->rows_anim_start || !rt->open) {
		rt->rows_anim_start = 0;
		return false;
	}
	asb_repaint(rt);
	if (( DWORD ) now - rt->rows_anim_start >= ASB_POPIN_MS) {
		rt->rows_anim_start = 0;
		return false;
	}
	return true;
}

static void asb_popin_params(FluxAsbRuntime const *rt, float *row_alpha, float *slide) {
	*row_alpha = 1.0f;
	*slide     = 0.0f;
	if (!rt->rows_anim_start) return;
	float ms   = ( float ) (flux_anim_now() - rt->rows_anim_start);
	*row_alpha = (ms - ASB_POPIN_FADE_IN) / ASB_POPIN_FADE_LEN;
	if (*row_alpha < 0.0f) *row_alpha = 0.0f;
	if (*row_alpha > 1.0f) *row_alpha = 1.0f;
	float t = ms / ( float ) ASB_POPIN_MS;
	if (t < 1.0f) *slide = (1.0f - flux_cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f)) * ASB_POPIN_OFFSET;
}

XentDl *flux_suggestitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	FluxThemeColors const         *t    = flux_rec_theme(rc);
	FluxSuggestItemSnapshot const *s    = &snap->u.suggest_item;
	bool                           hot  = state && state->hovered;
	bool                           down = state && state->pressed;
	float                          y    = bounds->y + s->slide;
	FluxRect                       row  = {bounds->x + 4.0f, y + 2.0f, bounds->w - 8.0f, bounds->h - 4.0f};
	bool                           ok   = true;
	FluxColor fill = (down || (s->highlight && hot)) ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
	if (s->highlight || hot) ok = flux_rec_fill_rounded(b, &row, 4.0f, asb_fade(fill, s->alpha));
	if (ok && s->highlight) {
		FluxRect pill = {row.x, y + (bounds->h - 16.0f) * 0.5f, 3.0f, 16.0f};
		ok            = flux_rec_fill_rounded(b, &pill, 1.5f, asb_fade(t->accent_default, s->alpha));
	}
	FluxRect tr = {bounds->x + ASB_TEXT_PAD_L, y, bounds->w - ASB_TEXT_PAD_L - ASB_TEXT_PAD_R, bounds->h};
	if (ok && s->label && tr.w > 0.0f) {
		XentTextStyle style
		  = {.font_size = FLUX_FONT_SIZE_DEFAULT, .font_weight = FLUX_FONT_REGULAR, .h_align = 0u, .v_align = 1u};
		ok = flux_rec_text_ex(b, &tr, s->label, &style, asb_fade(t->text_primary, s->alpha));
	}
	return flux_rec_finish(b, ok);
}

static FluxAsbItem const *asb_item_of(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_type == FLUX_CONTROL_SUGGEST_ITEM ? ( FluxAsbItem const * ) nd->component_data : NULL;
}

void flux_snap_suggestitem(FluxSnapCtx const *ctx) {
	FluxAsbItem const    *it    = ( FluxAsbItem const * ) ctx->data;
	FluxAsbRuntime const *rt    = it ? it->rt : NULL;
	bool                  ok    = rt && it->index < rt->count;
	float                 alpha = 1.0f, slide = 0.0f;
	if (rt) asb_popin_params(rt, &alpha, &slide);
	ctx->snap->u.suggest_item = (FluxSuggestItemSnapshot) {
	  .label     = ok ? rt->items [it->index] : NULL,
	  .highlight = ok && it->index == rt->highlight,
	  .alpha     = alpha,
	  .slide     = slide};
}

static void asb_close(FluxAsbRuntime *rt) {
	if (!rt->open) return;
	rt->open      = false;
	rt->highlight = -1;
	flux_popup_dismiss(rt->popup);
}

static void asb_choose(FluxAsbRuntime *rt, int i) {
	asb_write_box(rt, rt->items [i]);
	char *text = flux_str_dup(asb_box_text(rt));
	asb_close(rt);
	asb_notify_chosen_query(rt, text, i);
	flux_str_free(text);
}

static void asb_row_click(void *ctx) {
	FluxAsbItem const *it = ( FluxAsbItem const * ) ctx;
	if (it->index < it->rt->count) asb_choose(it->rt, it->index);
}

static void asb_rows_clear(FluxAsbRuntime *rt) {
	for (int i = 0; i < rt->row_count; i++) xent_node_destroy(rt->ctx, rt->rows [i]);
	free(rt->rows);
	free(rt->row_data);
	rt->rows      = NULL;
	rt->row_data  = NULL;
	rt->row_count = 0;
}

static bool asb_row_make(FluxAsbRuntime *rt, int i) {
	XentNodeId    n  = flux_factory_create_node(rt->ctx, rt->store, rt->list_scroll, FLUX_CONTROL_SUGGEST_ITEM);
	FluxNodeData *nd = n ? flux_store_find(rt->store, n) : NULL;
	if (!nd) return false;
	rt->rows [i]              = n;
	rt->row_data [i]          = (FluxAsbItem) {rt, i};
	nd->component_data        = &rt->row_data [i];
	nd->behavior.on_click     = asb_row_click;
	nd->behavior.on_click_ctx = &rt->row_data [i];
	xent_sem_setlabel(rt->ctx, n, rt->items [i]);
	return true;
}

static void asb_rows_sync(FluxAsbRuntime *rt) {
	asb_rows_clear(rt);
	int n        = rt->list_scroll ? rt->count : 0;
	rt->rows     = n > 0 ? ( XentNodeId * ) calloc(( size_t ) n, sizeof(*rt->rows)) : NULL;
	rt->row_data = n > 0 ? ( FluxAsbItem * ) calloc(( size_t ) n, sizeof(*rt->row_data)) : NULL;
	if (n > 0 && (!rt->rows || !rt->row_data)) n = 0;
	for (int i = 0; i < n && asb_row_make(rt, i); i++) rt->row_count = i + 1;
}

static void asb_list_layout(FluxAsbRuntime *rt) {
	FluxThemeColors const *t    = rt->theme ? flux_theme_colors(rt->theme) : flux_theme_defaults();
	FluxNodeData          *root = flux_store_find(rt->store, rt->list_root);
	if (!root) return;
	root->visuals.background    = flux_popup_acrylictint(rt->popup, t->flyout_background);
	root->visuals.border_color  = t->flyout_border;
	root->visuals.border_width  = ASB_BORDER;
	root->visuals.corner_radius = ASB_CORNER;
	for (int i = 0; i < rt->row_count; i++) xent_setsize(rt->ctx, rt->rows [i], (XentSize) {NAN, ASB_ROW_H});
	xent_setpos(rt->ctx, rt->list_scroll, (XentPoint) {ASB_BORDER, ASB_BORDER + ASB_PAD_Y});
	xent_setsize(rt->ctx, rt->list_scroll, (XentSize) {rt->width - 2.0f * ASB_BORDER, asb_viewport_h(rt)});
	xent_setpos(rt->ctx, rt->list_root, (XentPoint) {0.0f, 0.0f});
	xent_setsize(rt->ctx, rt->list_root, (XentSize) {rt->width, rt->height});
	xent_layout(rt->ctx, rt->list_root, rt->width, rt->height);
}

static bool asb_list_build(FluxAsbRuntime *rt) {
	rt->list_root      = flux_factory_create_node(rt->ctx, rt->store, XENT_NODE_INVALID, FLUX_CONTROL_CONTAINER);
	FluxNodeData *root = rt->list_root ? flux_store_find(rt->store, rt->list_root) : NULL;
	if (!root) return false;
	root->focus_passive = true;
	xent_setproto(rt->ctx, rt->list_root, XENT_PROTOCOL_ABSOLUTE);
	rt->list_scroll
	  = flux_scroll_init(&(FluxContainerDesc) {.ctx = rt->ctx, .store = rt->store, .parent = rt->list_root});
	return rt->list_scroll
	    && flux_popup_setcontent(rt->popup, rt->ctx, rt->store, rt->list_root)
	    && (!rt->input || flux_popup_sethost(rt->popup, rt->input, rt->root));
}

static void asb_open_if_ready(FluxAsbRuntime *rt) {
	char const *text = asb_box_text(rt);
	if (!rt->focused || rt->count <= 0 || !text || !text [0]) {
		asb_close(rt);
		return;
	}

	XentRect r = {0};
	xent_layout_rect(rt->ctx, rt->root, &r);
	rt->width  = r.w > 0.0f ? r.w : 240.0f;
	float body = ( float ) rt->count * ASB_ROW_H + 2.0f * (ASB_BORDER + ASB_PAD_Y);
	rt->height = body < ASB_MAX_H ? body : ASB_MAX_H;
	flux_popup_setsize(rt->popup, rt->width, rt->height);
	if (rt->row_count != rt->count) asb_rows_sync(rt);
	asb_list_layout(rt);

	asb_scroll_set(rt, 0.0f);
	rt->scroll_anim_start = 0;
	if (rt->open) {
		flux_popup_reposition(rt->popup);
		rt->rows_anim_start = flux_anim_now();
		flux_anim_register(rt, asb_rows_anim_step);
		asb_repaint(rt);
		return;
	}

	rt->open         = true;
	FluxRect anchor  = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->root);
	anchor.h        += ASB_GAP;
	flux_popup_setrtl(rt->popup, xent_resolved_dir(rt->ctx, rt->root) == XENT_DIRECTION_RTL);
	flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
}

static void asb_submit(FluxAsbRuntime *rt) {
	char const *text  = asb_box_text(rt);
	int         index = rt->highlight;
	if (!text) text = "";
	asb_close(rt);
	asb_notify_query(rt, text, index);
}

static void asb_move_highlight(FluxAsbRuntime *rt, bool forward) {
	int next = rt->highlight;
	if (forward) next = rt->highlight >= rt->count - 1 ? -1 : rt->highlight + 1;
	else next = rt->highlight == -1 ? rt->count - 1 : rt->highlight - 1;

	rt->highlight = next;
	if (next == -1) asb_write_box(rt, rt->user_text);
	else {
		asb_write_box(rt, rt->items [next]);
		asb_ensure_visible(rt, next);
	}
	asb_repaint(rt);
	if (next >= 0) asb_notify_chosen(rt, next);
}

static bool asb_key_open(FluxAsbRuntime *rt, FluxKey key) {
	switch (key) {
	case FLUX_KEY_DOWN  : asb_move_highlight(rt, true); return true;
	case FLUX_KEY_UP    : asb_move_highlight(rt, false); return true;
	case FLUX_KEY_ENTER : asb_submit(rt); return true;
	case FLUX_KEY_ESCAPE :
		asb_write_box(rt, rt->user_text);
		asb_close(rt);
		return true;
	case FLUX_KEY_TAB :
		asb_write_box(rt, rt->user_text);
		asb_close(rt);
		return false;
	default : return false;
	}
}

static bool asb_on_key(void *ctx, FluxKey key, bool down) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (down && key == FLUX_KEY_ENTER && !rt->open) {
		asb_submit(rt);
		return true;
	}
	if (down && rt->open && rt->count > 0 && asb_key_open(rt, key)) return true;
	return rt->tb_on_key ? rt->tb_on_key(rt->tb_on_key_ctx, key, down) : false;
}

static void asb_text_changed(void *ctx, char const *text) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (rt->ignore_text) return;
	flux_str_replace(&rt->user_text, text);
	rt->highlight = -1;
	asb_notify_text(rt, text);
}

static void asb_query_click(void *ctx) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	rt->highlight      = -1;
	asb_write_box(rt, rt->user_text);
	asb_submit(rt);
}

static void asb_focus(void *ctx) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	rt->focused        = true;
	asb_open_if_ready(rt);
}

static void asb_blur(void *ctx) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	rt->focused        = false;
	asb_close(rt);
}

static void asb_dismissed(void *ctx) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	rt->open           = false;
	rt->highlight      = -1;
}

static void asb_destroy(void *component_data) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) component_data;
	if (!rt) return;
	flux_anim_unregister(rt);
	for (int i = 0; i < rt->count; i++) flux_str_free(rt->items [i]);
	free(( void * ) rt->items);
	flux_str_free(rt->user_text);
	if (rt->popup) flux_popup_destroy(rt->popup);
	asb_rows_clear(rt);
	if (rt->list_root) xent_node_destroy(rt->ctx, rt->list_root);
	free(rt);
}

static void asb_make_query_button(FluxAsbRuntime *rt, char const *icon) {
	if (!icon || !icon [0]) return;
	rt->button = flux_button_init(&(FluxButtonDesc) {rt->ctx, rt->store, rt->root, NULL, asb_query_click, rt});
	if (rt->button == XENT_NODE_INVALID) return;

	flux_prop_button_icon(rt->store, rt->button, icon);
	flux_prop_button_style(rt->store, rt->button, 1);
	xent_setsize(rt->ctx, rt->button, (XentSize) {32.0f, 28.0f});
	xent_setself(rt->ctx, rt->button, XENT_FLEX_ALIGN_CENTER);
	xent_setm(rt->ctx, rt->button, (XentInsets) {2.0f, 0.0f, 0.0f, 0.0f});
	FluxNodeData   *bnd = flux_store_find(rt->store, rt->button);
	FluxButtonData *bd  = bnd ? ( FluxButtonData * ) bnd->component_data : NULL;
	if (bd) bd->font_size = 12.0f;
}

XentNodeId flux_suggest_init(FluxAutoSuggestDesc const *info) {
	if (!info || !info->ctx || !info->store || !info->app) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_AUTO_SUGGEST);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_store_find(info->store, root);
	FluxAsbRuntime *rt = nd ? ( FluxAsbRuntime * ) calloc(1, sizeof(*rt)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_setproto(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(info->ctx, root, XENT_FLEX_ROW);
	xent_setitems(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);
	xent_setsize(info->ctx, root, (XentSize) {NAN, 32.0f});

	rt->store      = info->store;
	rt->ctx        = info->ctx;
	rt->root       = root;
	rt->window     = info->window;
	rt->text       = info->text;
	rt->theme      = info->theme;
	rt->highlight  = -1;
	rt->input      = flux_app_input(info->app);
	rt->on_text    = info->on_text;
	rt->on_query   = info->on_query;
	rt->on_chosen  = info->on_chosen;
	rt->text_ctx   = info->text_ctx;
	rt->query_ctx  = info->query_ctx;
	rt->chosen_ctx = info->chosen_ctx;

	rt->textbox
	  = flux_app_create_textbox(&(FluxAppTextBoxDesc) {info->app, root, info->placeholder, asb_text_changed, rt});
	if (rt->textbox == XENT_NODE_INVALID) {
		free(rt);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	xent_setgrow(info->ctx, rt->textbox, 1.0f);
	xent_setbasis(info->ctx, rt->textbox, 0.0f);

	FluxNodeData *tnd = flux_store_find(info->store, rt->textbox);
	if (tnd) {
		rt->tb_on_key              = tnd->behavior.on_key;
		rt->tb_on_key_ctx          = tnd->behavior.on_key_ctx;
		tnd->behavior.on_key       = asb_on_key;
		tnd->behavior.on_key_ctx   = rt;
		tnd->behavior.on_focus     = asb_focus;
		tnd->behavior.on_focus_ctx = rt;
		tnd->behavior.on_blur      = asb_blur;
		tnd->behavior.on_blur_ctx  = rt;
	}

	asb_make_query_button(rt, info->query_icon);

	rt->popup = info->window ? flux_popup_create(info->window) : NULL;
	if (rt->popup) {
		flux_popup_setdismissoutside(rt->popup, true);
		flux_popup_setanim(rt->popup, FLUX_POPUP_ANIM_FLYOUT);
		flux_popup_ondismiss(rt->popup, asb_dismissed, rt);
	}
	if (rt->popup && !asb_list_build(rt)) {
		flux_popup_destroy(rt->popup);
		rt->popup = NULL;
	}

	nd->component_data         = rt;
	nd->destroy_component_data = asb_destroy;
	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);
	return root;
}

static void asb_items_free(char const **items, int count) {
	for (int i = 0; i < count; i++) flux_str_free(items [i]);
	free(( void * ) items);
}

static char const **asb_items_dup(char const *const *items, int count) {
	if (!items || count <= 0) return NULL;
	char const **copy = ( char const ** ) calloc(( size_t ) count, sizeof(*copy));
	if (!copy) return NULL;
	for (int i = 0; i < count; i++) {
		copy [i] = flux_str_dup(items [i]);
		if (!copy [i]) {
			asb_items_free(copy, i);
			return NULL;
		}
	}
	return copy;
}

void flux_suggest_set_suggestions(FluxStore *store, XentNodeId asb, char const *const *items, int count) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (!rt) return;

	int          normalized_count = items && count > 0 ? count : 0;
	char const **copy             = asb_items_dup(items, normalized_count);
	if (normalized_count && !copy) return;

	asb_items_free(rt->items, rt->count);
	rt->items = copy;
	rt->count = normalized_count;
	if (rt->highlight >= rt->count) rt->highlight = -1;
	asb_rows_sync(rt);

	asb_open_if_ready(rt);
	if (rt->open) asb_repaint(rt);
}

void flux_suggest_set_content(FluxStore *store, XentNodeId asb, char const *text) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (!rt) return;
	char const *current = asb_box_text(rt);
	if (current && text && strcmp(current, text) == 0) return;
	flux_str_replace(&rt->user_text, text);
	asb_write_box(rt, text);
}

void flux_suggest_set_placeholder(FluxStore *store, XentNodeId asb, char const *placeholder) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (rt) flux_prop_textbox_placeholder(store, rt->textbox, placeholder);
}

void flux_suggest_set_enabled(FluxStore *store, XentNodeId asb, bool enabled) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (!rt) return;
	flux_prop_textbox_enabled(store, rt->textbox, enabled);
	if (rt->button != XENT_NODE_INVALID) flux_prop_button_enabled(store, rt->button, enabled);
	if (!enabled) asb_close(rt);
}

void flux_suggest_set_query_icon(FluxStore *store, XentNodeId asb, char const *icon) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (!rt) return;
	if (!icon || !icon [0]) {
		if (rt->button != XENT_NODE_INVALID) flux_tree_drop(store, rt->button);
		rt->button = XENT_NODE_INVALID;
		return;
	}
	if (rt->button == XENT_NODE_INVALID) asb_make_query_button(rt, icon);
	else flux_prop_button_icon(store, rt->button, icon);
}

bool flux_suggestitem_uia_selected(FluxStore *store, XentNodeId node) {
	FluxAsbItem const *it = asb_item_of(store, node);
	return it && it->index == it->rt->highlight;
}

bool flux_suggestitem_uia_select(FluxStore *store, XentNodeId node, int op) {
	FluxAsbItem const *it = asb_item_of(store, node);
	if (!it || op == FLUX_UIA_SEL_REMOVE) return it && it->index != it->rt->highlight;
	FluxAsbRuntime *rt = it->rt;
	if (it->index == rt->highlight || it->index >= rt->count) return true;
	rt->highlight = it->index;
	asb_write_box(rt, rt->items [it->index]);
	asb_ensure_visible(rt, it->index);
	asb_repaint(rt);
	asb_notify_chosen(rt, it->index);
	return true;
}
