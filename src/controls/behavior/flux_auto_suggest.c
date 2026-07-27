/**
 * @file flux_auto_suggest.c
 * @brief AutoSuggestBox: an app-hosted TextBox + query button + owner-drawn
 * suggestions popup, following AutoSuggestBox_Partial.cpp:
 *
 * - Suggestions arrive from the model (Elm round trip); the list opens when
 *   the box is focused with a non-empty list and closes on empty list,
 *   focus loss, Escape, Tab, or submission.
 * - Down/Up walk the list with the WinUI edge rules (past either end
 *   restores the user-typed text at highlight -1) and preview the
 *   highlighted suggestion into the box (UpdateTextOnSelect).
 * - Enter submits (query = box text; the app knows the previewed index from
 *   on_chosen); the query button submits ignoring the highlight; a
 *   suggestion click submits that item.
 *
 * The popup is the WinUI 3 SuggestionsContainer: acrylic flyout surface,
 * OverlayCornerRadius 8, SurfaceStrokeColorFlyout border, Padding 0,2,
 * MaxHeight 374, and default ListViewItem rows (40px, text at 16/12,
 * 4,2-inset backplate; the keyboard highlight is a real selection with
 * the accent pill, hover is the backplate only).
 */
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
#include "fluxent/flux_popup.h"
#include "fluxent/flux_window.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define ASB_ROW_H      40.0f  /* ListViewItemMinHeight */
#define ASB_MAX_H      374.0f /* AutoSuggestListMaxHeight */
#define ASB_GAP        2.0f   /* AutoSuggestListMargin 0,2 */
#define ASB_PAD_Y      2.0f   /* SuggestionsContainer Padding 0,2,0,2 */
#define ASB_CORNER     8.0f
#define ASB_BORDER     1.0f
/* Rows are default ListViewItems: Padding 16,0,12,0 over the 4,2,4,2
 * backplate, 3x16 pill on the leading edge. */
#define ASB_TEXT_PAD_L 16.0f
#define ASB_TEXT_PAD_R 12.0f

typedef struct FluxAsbRuntime {
	FluxStore   *store;
	XentCtx     *ctx;
	XentNodeId   root;
	XentNodeId   textbox;
	XentNodeId   button;
	FluxWindow  *window;
	FluxText    *text;
	FluxTheme   *theme;
	FluxPopup   *popup;

	char const **items;                  /**< Owned suggestion copies. */
	int          count;
	int          highlight;              /**< Keyboard selection; -1 = the typed text shows. */
	int          hover;                  /**< Pointer-over row (backplate only), or -1. */
	int          pressed;                /**< Item the press landed on, or -1. */
	float        width, height, scroll_y;
	float        scroll_from, scroll_to; /**< Keyboard scroll-into-view tween. */
	DWORD        scroll_anim_start;      /**< 0 = idle. */
	bool         open;
	DWORD        rows_anim_start;        /**< Item add-transition start tick (0 = idle). */
	bool         focused;
	bool         ignore_text;            /**< Programmatic writes skip on_text. */

	char const  *user_text;              /**< Owned copy of the user-typed text. */

	/* The inner textbox's original key handler (caret movement etc.). */
	bool         (*tb_on_key)(void *ctx, FluxKey key, bool down);
	void        *tb_on_key_ctx;

	void         (*on_text)(void *ctx, char const *text);
	void         (*on_query)(void *ctx, char const *text);
	void         (*on_chosen)(void *ctx, int index);
	void        *text_ctx;
	void        *query_ctx;
	void        *chosen_ctx;
} FluxAsbRuntime;

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

/* -------------------------------------------------------------------------
 * Popup paint (ComboBox-style flyout surface, ListView-style rows)
 * ---------------------------------------------------------------------- */

static float asb_viewport_h(FluxAsbRuntime const *rt) { return rt->height - 2.0f * (ASB_BORDER + ASB_PAD_Y); }

static void  asb_repaint(FluxAsbRuntime *rt);

static bool  asb_scroll_step(void *ctx, unsigned long now) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (!rt->scroll_anim_start || !rt->open) {
		rt->scroll_anim_start = 0;
		return false;
	}
	float t = ( float ) (( DWORD ) now - rt->scroll_anim_start) / 250.0f;
	if (t >= 1.0f) {
		rt->scroll_y          = rt->scroll_to;
		rt->scroll_anim_start = 0;
	}
	else {
		float e      = 1.0f - (1.0f - t) * (1.0f - t) * (1.0f - t);
		rt->scroll_y = rt->scroll_from + (rt->scroll_to - rt->scroll_from) * e;
	}
	asb_repaint(rt);
	return rt->scroll_anim_start != 0;
}

/* Keyboard scroll-into-view animates (AutoSuggestBox_Partial scrolls its
 * ScrollViewer via ChangeViewWithOptionalAnimation, not a jump). */
static void asb_ensure_visible(FluxAsbRuntime *rt, int i) {
	if (i < 0) return;
	float current = rt->scroll_anim_start ? rt->scroll_to : rt->scroll_y;
	float top = ( float ) i * ASB_ROW_H, vp = asb_viewport_h(rt);
	float target = current;
	if (top < current) target = top;
	else if (top + ASB_ROW_H > current + vp) target = top + ASB_ROW_H - vp;
	if (target == current) return;

	rt->scroll_from       = rt->scroll_y;
	rt->scroll_to         = target;
	rt->scroll_anim_start = GetTickCount();
	flux_anim_register(rt, asb_scroll_step);
}

static int asb_item_at(FluxAsbRuntime const *rt, float y) {
	int i = ( int ) ((y - ASB_BORDER - ASB_PAD_Y + rt->scroll_y) / ASB_ROW_H);
	return i >= 0 && i < rt->count ? i : -1;
}

/* Scale a color's alpha for the row entrance fade. */
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

/* PVL SHOWPOPUP (Animations storyboard 18, dumped from the OS theme):
 * translate runs 367 ms on cubic-bezier(0.1, 0.9, 0.2, 1); opacity holds
 * for 83 ms then fades 0 -> 1 linearly over 83 ms. */
#define ASB_POPIN_MS       367
#define ASB_POPIN_FADE_IN  83.0f
#define ASB_POPIN_FADE_LEN 83.0f
#define ASB_POPIN_OFFSET   50.0f /* FlyoutBase g_entranceThemeOffset */

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

/* One suggestion row. WinUI semantics: the keyboard highlight is a real
 * ListView selection (fill + pill); hover is only the subtle backplate.
 * `slide` is applied by the caller via PUSH_TRANSFORM. */
static bool asb_rec_row(
  XentDlBuilder *b, FluxAsbRuntime const *rt, FluxThemeColors const *t, FluxRect const *view, int i, float row_alpha
) {
	float    y        = view->y + ( float ) i * ASB_ROW_H - rt->scroll_y;
	FluxRect row      = {view->x + 4.0f, y + 2.0f, view->w - 8.0f, ASB_ROW_H - 4.0f};
	bool     ok       = true;

	bool     selected = i == rt->highlight;
	bool     hot      = i == rt->hover;
	if (selected || hot) {
		FluxColor fill = (rt->pressed == i || (selected && hot)) ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
		ok             = flux_rec_fill_rounded(b, &row, 4.0f, asb_fade(fill, row_alpha));
	}
	if (ok && selected) {
		float    pill_h = 16.0f;
		FluxRect pill   = {row.x, y + (ASB_ROW_H - pill_h) * 0.5f, 3.0f, pill_h};
		ok              = flux_rec_fill_rounded(b, &pill, 1.5f, asb_fade(t->accent_default, row_alpha));
	}
	if (!ok || !rt->items [i]) return ok;
	FluxRect tr = {view->x + ASB_TEXT_PAD_L, y, view->w - ASB_TEXT_PAD_L - ASB_TEXT_PAD_R, ASB_ROW_H};
	if (tr.w <= 0.0f) return ok;
	XentTextStyle style
	  = {.font_size = FLUX_FONT_SIZE_DEFAULT, .font_weight = FLUX_FONT_REGULAR, .h_align = 0u, .v_align = 1u};
	return flux_rec_text_ex(b, &tr, rt->items [i], &style, asb_fade(t->text_primary, row_alpha));
}

static void asb_popin_params(FluxAsbRuntime const *rt, float *row_alpha, float *slide) {
	*row_alpha = 1.0f;
	*slide     = 0.0f;
	if (!rt->rows_anim_start) return;
	float ms   = ( float ) (GetTickCount() - rt->rows_anim_start);
	*row_alpha = (ms - ASB_POPIN_FADE_IN) / ASB_POPIN_FADE_LEN;
	if (*row_alpha < 0.0f) *row_alpha = 0.0f;
	if (*row_alpha > 1.0f) *row_alpha = 1.0f;
	float t = ms / ( float ) ASB_POPIN_MS;
	if (t < 1.0f) *slide = (1.0f - flux_cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f)) * ASB_POPIN_OFFSET;
}

static XentDl *asb_record_popup(FluxAsbRuntime const *rt, FluxThemeColors const *t, FluxColor bg) {
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return NULL;
	FluxRect bounds = {0.0f, 0.0f, rt->width, rt->height};
	bool     ok     = flux_rec_fill_rounded(b, &bounds, ASB_CORNER, bg);
	float    half   = ASB_BORDER * 0.5f;
	FluxRect inset  = {bounds.x + half, bounds.y + half, bounds.w - ASB_BORDER, bounds.h - ASB_BORDER};
	if (ok) ok = flux_rec_stroke_rounded(b, &inset, ASB_CORNER, t->flyout_border, ASB_BORDER);

	float    vp      = asb_viewport_h(rt);
	FluxRect view    = {ASB_BORDER, ASB_BORDER + ASB_PAD_Y, rt->width - 2.0f * ASB_BORDER, vp};
	bool     clipped = false;
	if (ok) {
		ok      = xent_dl_pushclip_rect(b, flux_rec_xrect(&view));
		clipped = ok;
	}

	float row_alpha = 1.0f, slide = 0.0f;
	asb_popin_params(rt, &row_alpha, &slide);
	bool slid = false;
	if (ok && slide != 0.0f) {
		ok   = xent_dl_pushtransform(b, xent_affine_translate(0.0f, slide));
		slid = ok;
	}

	int first = ( int ) (rt->scroll_y / ASB_ROW_H);
	int last  = ( int ) ((rt->scroll_y + vp) / ASB_ROW_H) + 1;
	if (first < 0) first = 0;
	if (last > rt->count - 1) last = rt->count - 1;
	for (int i = first; ok && i <= last; i++) ok = asb_rec_row(b, rt, t, &view, i, row_alpha);

	if (slid) ok = xent_dl_poptransform(b) && ok;
	if (clipped) ok = xent_dl_popclip(b) && ok;
	return flux_rec_finish(b, ok);
}

static XentDl *asb_record(void *ctx, FluxPopup const *popup) {
	FluxAsbRuntime        *rt = ( FluxAsbRuntime * ) ctx;
	FluxThemeColors const *t  = rt->theme ? flux_theme_colors(rt->theme) : flux_theme_defaults();
	FluxColor              bg = flux_popup_acrylic_tint(popup, t->flyout_background);
	return asb_record_popup(rt, t, bg);
}

/* -------------------------------------------------------------------------
 * Open / close / submit
 * ---------------------------------------------------------------------- */

static void asb_close(FluxAsbRuntime *rt) {
	if (!rt->open) return;
	rt->open      = false;
	rt->highlight = -1;
	rt->hover     = -1;
	flux_popup_dismiss(rt->popup);
}

/* Called on every suggestion-set change. WinUI keeps the open popup and
 * updates it in place (UpdateSuggestionListVisibility only toggles
 * IsSuggestionListOpen at the empty/non-empty edges) — the show animation
 * plays only on the closed→open transition, never per keystroke. */
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
	flux_popup_set_size(rt->popup, rt->width, rt->height);

	rt->scroll_y          = 0.0f;
	rt->scroll_anim_start = 0;
	if (rt->open) {
		/* Already showing: re-anchor for the new height and fade the new
		 * rows in — no re-show, no replayed flyout animation. */
		flux_popup_update_position(rt->popup);
		rt->rows_anim_start = GetTickCount();
		flux_anim_register(rt, asb_rows_anim_step);
		asb_repaint(rt);
		return;
	}

	rt->open         = true;
	FluxRect anchor  = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->root);
	anchor.h        += ASB_GAP;
	flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
}

static void asb_submit(FluxAsbRuntime *rt) {
	char const *text  = asb_box_text(rt);
	int         index = rt->highlight;
	if (!text) text = "";
	asb_close(rt);
	asb_notify_query(rt, text, index);
}

/* Down/Up with the WinUI edge rules: walking past either end restores the
 * user-typed text at highlight -1 (OnKeyDown, AutoSuggestBox_Partial). */
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
		return false; /* let focus move (WinUI: not handled) */
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

/* User edits: capture the typed text, reset the preview, notify the app —
 * the model recomputes suggestions and the next set_suggestions applies
 * the open/close rule. */
static void asb_text_changed(void *ctx, char const *text) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (rt->ignore_text) return;
	flux_str_replace(&rt->user_text, text);
	rt->highlight = -1;
	asb_notify_text(rt, text);
}

static void asb_query_click(void *ctx) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	rt->highlight      = -1; /* the button ignores any highlight (ProgrammaticSubmitQuery) */
	asb_write_box(rt, rt->user_text);
	asb_submit(rt);
}

static void asb_focus(void *ctx) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	rt->focused        = true;
	asb_open_if_ready(rt); /* GotFocus with existing text reopens */
}

static void asb_blur(void *ctx) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	rt->focused        = false;
	asb_close(rt);
}

static void asb_mouse_set_hover(FluxAsbRuntime *rt, int i) {
	if (i == rt->hover) return;
	rt->hover = i; /* hover shows the backplate only — no pill, no preview */
	asb_repaint(rt);
}

/* Release commits the pressed row (WinUI click-on-release semantics). */
static void asb_mouse_up(FluxAsbRuntime *rt, int i) {
	int pressed = rt->pressed;
	rt->pressed = -1;
	if (i < 0 || i != pressed) return;
	asb_write_box(rt, rt->items [i]);
	char *text = flux_str_dup(asb_box_text(rt));
	asb_close(rt);
	asb_notify_chosen_query(rt, text, i);
	flux_str_free(text);
}

static void asb_mouse_wheel(FluxAsbRuntime *rt, float dy) {
	float max_scroll = ( float ) rt->count * ASB_ROW_H - asb_viewport_h(rt);
	if (max_scroll <= 0.0f) return;
	rt->scroll_anim_start  = 0;  /* wheel overrides the keyboard tween */
	rt->scroll_y          -= dy; /* wheel delta in DIPs */
	if (rt->scroll_y < 0.0f) rt->scroll_y = 0.0f;
	if (rt->scroll_y > max_scroll) rt->scroll_y = max_scroll;
	asb_repaint(rt);
}

static void asb_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y) {
	( void ) x;
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	int             i  = asb_item_at(rt, y);
	switch (event) {
	case FLUX_POPUP_MOUSE_MOVE : asb_mouse_set_hover(rt, i); break;
	case FLUX_POPUP_MOUSE_DOWN : rt->pressed = i; break;
	case FLUX_POPUP_MOUSE_UP   : asb_mouse_up(rt, i); break;
	case FLUX_POPUP_MOUSE_LEAVE :
		rt->pressed = -1;
		asb_mouse_set_hover(rt, -1);
		break;
	case FLUX_POPUP_MOUSE_WHEEL : asb_mouse_wheel(rt, y); break;
	default                     : break;
	}
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
	free(rt);
}

/* -------------------------------------------------------------------------
 * Create + setters
 * ---------------------------------------------------------------------- */

/* WinUI QueryButton: 32×28 inside the box's trailing edge, 12px glyph, subtle
 * chrome. The fluxent textbox has no trailing slot yet, so the button sits
 * flush against it (2px lead margin per the template); visually it reads as
 * the in-box button. */
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
	if (bd) bd->font_size = 12.0f; /* AutoSuggestBoxIconFontSize */
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
	rt->hover      = -1;
	rt->pressed    = -1;
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

	/* Steal the textbox's key handler; ours filters the suggestion-list
	 * keys and forwards everything else (caret movement, editing). */
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
		flux_popup_onrecord(rt->popup, asb_record, rt, rt->text);
		flux_popup_onmouse(rt->popup, asb_mouse, rt);
		flux_popup_ondismiss(rt->popup, asb_dismissed, rt);
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
	rt->hover = -1; /* the rows under the pointer changed identity */

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
