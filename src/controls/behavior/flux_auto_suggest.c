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
#include "render/flux_anim.h"
#include "render/flux_fluent.h"
#include "runtime/flux_anim_driver.h"
#include "runtime/flux_str.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_graphics.h"
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
	FluxNodeStore        *store;
	XentContext          *ctx;
	XentNodeId            root;
	XentNodeId            textbox;
	XentNodeId            button;
	FluxWindow           *window;
	FluxTextRenderer     *text;
	FluxThemeManager     *theme;
	FluxPopup            *popup;
	ID2D1SolidColorBrush *brush;

	char const          **items; /**< Owned suggestion copies. */
	int                   count;
	int                   highlight;  /**< Keyboard selection; -1 = the typed text shows. */
	int                   hover;      /**< Pointer-over row (backplate only), or -1. */
	int                   pressed;    /**< Item the press landed on, or -1. */
	float                 width, height, scroll_y;
	float                 scroll_from, scroll_to; /**< Keyboard scroll-into-view tween. */
	DWORD                 scroll_anim_start;      /**< 0 = idle. */
	bool                  open;
	DWORD                 rows_anim_start; /**< Item add-transition start tick (0 = idle). */
	bool                  focused;
	bool                  ignore_text; /**< Programmatic writes skip on_text. */

	char const           *user_text;  /**< Owned copy of the user-typed text. */

	/* The inner textbox's original key handler (caret movement etc.). */
	bool                (*tb_on_key)(void *ctx, unsigned int vk, bool down);
	void                 *tb_on_key_ctx;

	void (*on_text)(void *ctx, char const *text);
	void (*on_query)(void *ctx, char const *text);
	void (*on_chosen)(void *ctx, int index);
	void  *cb;
} FluxAsbRuntime;

static FluxAsbRuntime *asb_runtime(FluxNodeStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_AUTO_SUGGEST || !nd->component_data) return NULL;
	return ( FluxAsbRuntime * ) nd->component_data;
}

static char const *asb_box_text(FluxAsbRuntime const *rt) {
	FluxNodeData *nd = flux_node_store_get(rt->store, rt->textbox);
	if (!nd || !nd->component_data) return NULL;
	return (( FluxTextBoxData * ) nd->component_data)->content;
}

static void asb_write_box(FluxAsbRuntime *rt, char const *text) {
	rt->ignore_text = true;
	flux_textbox_set_content(rt->store, rt->textbox, text ? text : "");
	rt->ignore_text = false;
}

/* -------------------------------------------------------------------------
 * Popup paint (ComboBox-style flyout surface, ListView-style rows)
 * ---------------------------------------------------------------------- */

static float asb_viewport_h(FluxAsbRuntime const *rt) { return rt->height - 2.0f * (ASB_BORDER + ASB_PAD_Y); }

static void asb_repaint(FluxAsbRuntime *rt);

static bool asb_scroll_step(void *ctx, unsigned long now) {
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
	HWND h = rt->popup ? flux_popup_get_hwnd(rt->popup) : NULL;
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
 * ListView selection (fill + pill); hover is only the subtle backplate. */
static void asb_paint_row(
  FluxAsbRuntime *rt, FluxRenderContext const *rc, FluxRect const *view, int i, float slide, float row_alpha
) {
	FluxThemeColors const *t   = rc->theme;
	float                  y   = view->y + ( float ) i * ASB_ROW_H - rt->scroll_y + slide;
	FluxRect               row = {view->x + 4.0f, y + 2.0f, view->w - 8.0f, ASB_ROW_H - 4.0f};

	bool selected = i == rt->highlight;
	bool hot      = i == rt->hover;
	if (selected || hot) {
		FluxColor fill = (rt->pressed == i || (selected && hot)) ? t->subtle_fill_tertiary : t->subtle_fill_secondary;
		flux_fill_rounded_rect(rc, &row, 4.0f, asb_fade(fill, row_alpha));
	}
	if (selected) {
		float    pill_h = 16.0f;
		FluxRect pill   = {row.x, y + (ASB_ROW_H - pill_h) * 0.5f, 3.0f, pill_h};
		flux_fill_rounded_rect(rc, &pill, 1.5f, asb_fade(t->accent_default, row_alpha));
	}
	if (!rt->items [i] || !rt->text) return;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_FONT_SIZE_DEFAULT;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = asb_fade(t->text_primary, row_alpha);
	FluxRect tr    = {view->x + ASB_TEXT_PAD_L, y, view->w - ASB_TEXT_PAD_L - ASB_TEXT_PAD_R, ASB_ROW_H};
	if (tr.w > 0.0f) flux_text_draw(rt->text, FLUX_RT(rc), rt->items [i], &tr, &ts);
}

static void asb_paint(void *ctx, FluxPopup *popup) {
	FluxAsbRuntime     *rt  = ( FluxAsbRuntime * ) ctx;
	FluxGraphics       *gfx = flux_popup_get_graphics(popup);
	ID2D1DeviceContext *d2d = gfx ? flux_graphics_get_d2d_context(gfx) : NULL;
	if (!d2d) return;
	if (!rt->brush) {
		D2D1_COLOR_F          black = {0.0f, 0.0f, 0.0f, 1.0f};
		D2D1_BRUSH_PROPERTIES bp    = flux_default_brush_props();
		ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &bp, &rt->brush);
	}
	if (!rt->brush) return;

	FluxThemeColors const *t = rt->theme ? flux_theme_colors(rt->theme) : flux_theme_default_colors();

	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d   = d2d;
	rc.brush = rt->brush;
	rc.text  = rt->text;
	rc.theme = t;
	rc.dpi   = flux_window_dpi(rt->window);

	FluxRect  bounds = {0.0f, 0.0f, rt->width, rt->height};
	FluxColor bg     = flux_popup_acrylic_tint(popup, t->flyout_background);
	flux_fill_rounded_rect(&rc, &bounds, ASB_CORNER, bg);
	float    half  = ASB_BORDER * 0.5f;
	FluxRect inset = {bounds.x + half, bounds.y + half, bounds.w - ASB_BORDER, bounds.h - ASB_BORDER};
	flux_draw_rounded_rect(&rc, &inset, ASB_CORNER, t->flyout_border, ASB_BORDER);

	float       vp        = asb_viewport_h(rt);
	FluxRect    view      = {ASB_BORDER, ASB_BORDER + ASB_PAD_Y, rt->width - 2.0f * ASB_BORDER, vp};
	D2D1_RECT_F view_clip = flux_d2d_rect(&view);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(&rc), &view_clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

	int first = ( int ) (rt->scroll_y / ASB_ROW_H);
	int last  = ( int ) ((rt->scroll_y + vp) / ASB_ROW_H) + 1;
	if (first < 0) first = 0;
	if (last > rt->count - 1) last = rt->count - 1;

	/* Replaced suggestion sets play the entrance on their content while the
	 * popup frame stays put; the timings are the ASB_POPIN_* constants. */
	float row_alpha = 1.0f;
	float slide     = 0.0f;
	if (rt->rows_anim_start) {
		float ms  = ( float ) (GetTickCount() - rt->rows_anim_start);
		row_alpha = (ms - ASB_POPIN_FADE_IN) / ASB_POPIN_FADE_LEN; /* linear, 83 ms delay */
		if (row_alpha < 0.0f) row_alpha = 0.0f;
		if (row_alpha > 1.0f) row_alpha = 1.0f;

		float t = ms / ( float ) ASB_POPIN_MS;
		if (t < 1.0f) slide = (1.0f - flux_cubic_bezier(t, 0.1f, 0.9f, 0.2f, 1.0f)) * ASB_POPIN_OFFSET;
	}

	for (int i = first; i <= last; i++) asb_paint_row(rt, &rc, &view, i, slide, row_alpha);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(&rc));
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
	xent_get_layout_rect(rt->ctx, rt->root, &r);
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

	rt->open        = true;
	FluxRect anchor = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->root);
	anchor.h       += ASB_GAP;
	flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
}

static void asb_submit(FluxAsbRuntime *rt) {
	char const *text = asb_box_text(rt);
	if (rt->on_query) rt->on_query(rt->cb, text ? text : "");
	asb_close(rt);
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
		if (rt->on_chosen) rt->on_chosen(rt->cb, next);
	}
	asb_repaint(rt);
}

static bool asb_key_open(FluxAsbRuntime *rt, unsigned int vk) {
	switch (vk) {
	case VK_DOWN   : asb_move_highlight(rt, true); return true;
	case VK_UP     : asb_move_highlight(rt, false); return true;
	case VK_RETURN : asb_submit(rt); return true;
	case VK_ESCAPE :
		asb_write_box(rt, rt->user_text);
		asb_close(rt);
		return true;
	case VK_TAB :
		asb_write_box(rt, rt->user_text);
		asb_close(rt);
		return false; /* let focus move (WinUI: not handled) */
	default : return false;
	}
}

static bool asb_on_key(void *ctx, unsigned int vk, bool down) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (down && vk == VK_RETURN && !rt->open) {
		asb_submit(rt);
		return true;
	}
	if (down && rt->open && rt->count > 0 && asb_key_open(rt, vk)) return true;
	return rt->tb_on_key ? rt->tb_on_key(rt->tb_on_key_ctx, vk, down) : false;
}

/* User edits: capture the typed text, reset the preview, notify the app —
 * the model recomputes suggestions and the next set_suggestions applies
 * the open/close rule. */
static void asb_text_changed(void *ctx, char const *text) {
	FluxAsbRuntime *rt = ( FluxAsbRuntime * ) ctx;
	if (rt->ignore_text) return;
	flux_str_replace(&rt->user_text, text);
	rt->highlight = -1;
	if (rt->on_text) rt->on_text(rt->cb, text);
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
	if (rt->on_chosen) rt->on_chosen(rt->cb, i);
	asb_submit(rt);
}

static void asb_mouse_wheel(FluxAsbRuntime *rt, float dy) {
	float max_scroll = ( float ) rt->count * ASB_ROW_H - asb_viewport_h(rt);
	if (max_scroll <= 0.0f) return;
	rt->scroll_anim_start = 0;  /* wheel overrides the keyboard tween */
	rt->scroll_y         -= dy; /* wheel delta in DIPs */
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
	default : break;
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
	if (rt->brush) ID2D1SolidColorBrush_Release(rt->brush);
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
static void asb_make_query_button(FluxAsbRuntime *rt, FluxAutoSuggestCreateInfo const *info, XentNodeId root) {
	if (!info->query_icon || !info->query_icon [0]) return;
	rt->button = flux_create_button(&(FluxButtonCreateInfo) {info->ctx, info->store, root, NULL, asb_query_click, rt});
	if (rt->button == XENT_NODE_INVALID) return;

	flux_button_set_icon(info->store, rt->button, info->query_icon);
	flux_button_set_style(info->store, rt->button, 1 /* subtle */);
	xent_set_size(info->ctx, rt->button, (XentSize) {32.0f, 28.0f});
	xent_set_flex_align_self(info->ctx, rt->button, XENT_FLEX_ALIGN_CENTER);
	xent_set_margin(info->ctx, rt->button, (XentInsets) {2.0f, 0.0f, 0.0f, 0.0f});
	FluxNodeData   *bnd = flux_node_store_get(info->store, rt->button);
	FluxButtonData *bd  = bnd ? ( FluxButtonData * ) bnd->component_data : NULL;
	if (bd) bd->font_size = 12.0f; /* AutoSuggestBoxIconFontSize */
}

XentNodeId flux_create_auto_suggest(FluxAutoSuggestCreateInfo const *info) {
	if (!info || !info->ctx || !info->store || !info->app) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_AUTO_SUGGEST);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData   *nd = flux_node_store_get(info->store, root);
	FluxAsbRuntime *rt = nd ? ( FluxAsbRuntime * ) calloc(1, sizeof(*rt)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	xent_set_protocol(info->ctx, root, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(info->ctx, root, XENT_FLEX_ROW);
	xent_set_flex_align_items(info->ctx, root, XENT_FLEX_ALIGN_STRETCH);
	xent_set_size(info->ctx, root, (XentSize) {NAN, 32.0f});

	rt->store     = info->store;
	rt->ctx       = info->ctx;
	rt->root      = root;
	rt->window    = info->window;
	rt->text      = info->text;
	rt->theme     = info->theme;
	rt->highlight = -1;
	rt->hover     = -1;
	rt->pressed   = -1;
	rt->on_text   = info->on_text;
	rt->on_query  = info->on_query;
	rt->on_chosen = info->on_chosen;
	rt->cb        = info->userdata;

	rt->textbox = flux_app_create_textbox(&(FluxAppTextBoxCreateInfo) {
	  info->app, root, info->placeholder, asb_text_changed, rt});
	if (rt->textbox == XENT_NODE_INVALID) {
		free(rt);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}
	xent_set_flex_grow(info->ctx, rt->textbox, 1.0f);
	xent_set_flex_basis(info->ctx, rt->textbox, 0.0f);

	/* Steal the textbox's key handler; ours filters the suggestion-list
	 * keys and forwards everything else (caret movement, editing). */
	FluxNodeData *tnd = flux_node_store_get(info->store, rt->textbox);
	if (tnd) {
		rt->tb_on_key             = tnd->behavior.on_key;
		rt->tb_on_key_ctx         = tnd->behavior.on_key_ctx;
		tnd->behavior.on_key      = asb_on_key;
		tnd->behavior.on_key_ctx  = rt;
		tnd->behavior.on_focus     = asb_focus;
		tnd->behavior.on_focus_ctx = rt;
		tnd->behavior.on_blur      = asb_blur;
		tnd->behavior.on_blur_ctx  = rt;
	}

	asb_make_query_button(rt, info, root);

	rt->popup = info->window ? flux_popup_create(info->window) : NULL;
	if (rt->popup) {
		flux_popup_set_dismiss_on_outside(rt->popup, true);
		flux_popup_set_anim_style(rt->popup, FLUX_POPUP_ANIM_FLYOUT);
		flux_popup_set_paint_callback(rt->popup, asb_paint, rt);
		flux_popup_set_mouse_callback(rt->popup, asb_mouse, rt);
		flux_popup_set_dismiss_callback(rt->popup, asb_dismissed, rt);
	}

	nd->component_data         = rt;
	nd->destroy_component_data = asb_destroy;
	xent_set_semantic_role(info->ctx, root, XENT_SEMANTIC_CONTAINER);
	return root;
}

void flux_auto_suggest_set_suggestions(FluxNodeStore *store, XentNodeId asb, char const *const *items, int count) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (!rt) return;

	char const **copy = NULL;
	if (items && count > 0) {
		copy = ( char const ** ) calloc(( size_t ) count, sizeof(*copy));
		if (!copy) return;
		for (int i = 0; i < count; i++) copy [i] = flux_str_dup(items [i]);
	}
	for (int i = 0; i < rt->count; i++) flux_str_free(rt->items [i]);
	free(( void * ) rt->items);
	rt->items = copy;
	rt->count = copy ? count : 0;
	if (rt->highlight >= rt->count) rt->highlight = -1;
	rt->hover = -1; /* the rows under the pointer changed identity */

	asb_open_if_ready(rt);
	if (rt->open) asb_repaint(rt);
}

void flux_auto_suggest_set_content(FluxNodeStore *store, XentNodeId asb, char const *text) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (!rt) return;
	char const *current = asb_box_text(rt);
	if (current && text && strcmp(current, text) == 0) return;
	flux_str_replace(&rt->user_text, text);
	asb_write_box(rt, text);
}

void flux_auto_suggest_set_enabled(FluxNodeStore *store, XentNodeId asb, bool enabled) {
	FluxAsbRuntime *rt = asb_runtime(store, asb);
	if (!rt) return;
	flux_textbox_set_enabled(store, rt->textbox, enabled);
	if (rt->button != XENT_NODE_INVALID) flux_button_set_enabled(store, rt->button, enabled);
	if (!enabled) asb_close(rt);
}
