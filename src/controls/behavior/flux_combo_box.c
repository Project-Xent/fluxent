#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "controls/factory/flux_factory.h"
#include "controls/draw/flux_control_draw.h"
#include "render/flux_fluent.h"

#include "fluxent/fluxent.h"
#include "fluxent/flux_graphics.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_window.h"

#include <stdlib.h>
#include <string.h>
#include <windows.h>

/* WinUI ComboBox drop-down (ComboBox_themeresources.xaml): item pitch driven by
 * ComboBoxItemThemePadding 11,5,11,7 with 5,2 margins and corner 3; selected items
 * show a 3x16 accent pill; OverlayCornerRadius 8; MaxDropDownHeight 504. */
#define CB_ROW_H            36.0f /* ComboBoxItemThemePadding 11,5,11,7 + 2,2 margin. */
#define CB_ROW_H_TOUCH      48.0f /* ComboBoxItemThemeTouchPadding 11,11,11,13 + margin. */
#define CB_ITEM_MARGIN_X    5.0f
#define CB_ITEM_MARGIN_Y    2.0f
#define CB_ITEM_CORNER      3.0f
#define CB_ITEM_PAD_LEFT    11.0f
#define CB_CONTENT_MARGIN_Y 4.0f
#define CB_PILL_W           3.0f
#define CB_PILL_H           16.0f
#define CB_PILL_CORNER      1.5f
#define CB_MAX_HEIGHT       504.0f
#define CB_CORNER           8.0f
#define CB_BORDER           1.0f

/* The runtime embeds the model as its first member so component_data can point at
 * the model (for snapshots) while the owned popup/brush are freed via the model's
 * destructor. */
typedef struct FluxComboRuntime {
	FluxComboBoxData      model;
	FluxNodeStore        *store;
	XentNodeId            node;
	XentContext          *ctx;
	FluxWindow           *window;
	FluxTextRenderer     *text;
	FluxThemeManager     *theme;
	FluxPopup            *popup;
	ID2D1SolidColorBrush *brush;
	int                   highlight;
	int                   pressed_index; /**< Item the press landed on; -1 when none. */
	float                 width;
	float                 height;
	float                 scroll_y;  /**< Drop-down scroll offset when items exceed 504px. */
	float                 content_h; /**< Total item-block height. */
	float                 row_h;     /**< Per-item height; grows under touch input. */
} FluxComboRuntime;

static void combo_ensure_brush(FluxComboRuntime *rt, ID2D1DeviceContext *d2d) {
	if (rt->brush || !d2d) return;
	D2D1_COLOR_F          black = {0.0f, 0.0f, 0.0f, 1.0f};
	D2D1_BRUSH_PROPERTIES bp    = flux_default_brush_props();
	ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &bp, &rt->brush);
}

static void combo_resolve_theme(FluxComboRuntime *rt, FluxThemeColors const **theme, bool *is_dark) {
	if (rt->theme) {
		*theme          = flux_theme_colors(rt->theme);
		FluxThemeMode m = flux_theme_get_mode(rt->theme);
		*is_dark        = (m == FLUX_THEME_DARK) || (m == FLUX_THEME_SYSTEM && flux_theme_system_is_dark());
		return;
	}
	*theme   = flux_theme_default_colors();
	*is_dark = false;
}

static float combo_dropdown_height(int count, float row_h) {
	float h = 2.0f * CB_BORDER + 2.0f * CB_CONTENT_MARGIN_Y + ( float ) count * row_h;
	return flux_minf(h, CB_MAX_HEIGHT);
}

static float combo_viewport_h(FluxComboRuntime const *rt) {
	return flux_maxf(0.0f, rt->height - 2.0f * CB_BORDER - 2.0f * CB_CONTENT_MARGIN_Y);
}

static float combo_max_scroll(FluxComboRuntime const *rt) {
	return flux_maxf(0.0f, rt->content_h - combo_viewport_h(rt));
}

static void combo_clamp_scroll(FluxComboRuntime *rt) {
	rt->scroll_y = flux_clampf(rt->scroll_y, 0.0f, combo_max_scroll(rt));
}

static void combo_ensure_visible(FluxComboRuntime *rt, int i) {
	if (i < 0) return;
	float top = ( float ) i * rt->row_h;
	float vp  = combo_viewport_h(rt);
	if (top < rt->scroll_y) rt->scroll_y = top;
	else if (top + rt->row_h > rt->scroll_y + vp) rt->scroll_y = top + rt->row_h - vp;
	combo_clamp_scroll(rt);
}

static int combo_item_at(FluxComboRuntime const *rt, float y) {
	float start = CB_BORDER + CB_CONTENT_MARGIN_Y;
	if (y < start || y > start + combo_viewport_h(rt)) return -1;
	int i = ( int ) ((y - start + rt->scroll_y) / rt->row_h);
	return (i >= 0 && i < rt->model.item_count) ? i : -1;
}

static void combo_draw_item(FluxRenderContext const *rc, FluxComboRuntime const *rt, FluxThemeColors const *t, int i) {
	float    row  = CB_BORDER + CB_CONTENT_MARGIN_Y + ( float ) i * rt->row_h - rt->scroll_y;
	FluxRect item = {
	  CB_BORDER + CB_ITEM_MARGIN_X, row + CB_ITEM_MARGIN_Y, rt->width - 2.0f * (CB_BORDER + CB_ITEM_MARGIN_X),
	  rt->row_h - 2.0f * CB_ITEM_MARGIN_Y};

	bool selected = i == rt->model.selected_index;
	bool hot      = i == rt->highlight;
	if (hot)
		flux_fill_rounded_rect(
		  rc, &item, CB_ITEM_CORNER, selected ? t->subtle_fill_tertiary : t->subtle_fill_secondary
		);
	else if (selected) flux_fill_rounded_rect(rc, &item, CB_ITEM_CORNER, t->subtle_fill_secondary);

	if (selected) {
		FluxRect pill = {item.x + 1.0f, item.y + (item.h - CB_PILL_H) * 0.5f, CB_PILL_W, CB_PILL_H};
		flux_fill_rounded_rect(rc, &pill, CB_PILL_CORNER, t->accent_default);
	}

	if (rt->model.items [i] && rc->text) {
		FluxTextStyle ts;
		memset(&ts, 0, sizeof(ts));
		ts.font_size   = FLUX_FONT_SIZE_DEFAULT;
		ts.font_weight = FLUX_FONT_REGULAR;
		ts.text_align  = FLUX_TEXT_LEFT;
		ts.vert_align  = FLUX_TEXT_VCENTER;
		ts.color       = t->text_primary;
		FluxRect tr    = {item.x + CB_ITEM_PAD_LEFT, item.y, item.w - CB_ITEM_PAD_LEFT - 4.0f, item.h};
		if (tr.w > 0.0f) flux_text_draw(rc->text, FLUX_RT(rc), rt->model.items [i], &tr, &ts);
	}
}

static void
combo_draw_scrollbar(FluxRenderContext const *rc, FluxComboRuntime const *rt, FluxThemeColors const *t, float vp) {
	float max_scroll = combo_max_scroll(rt);
	if (max_scroll <= 0.0f) return;

	float    track_top = CB_BORDER + CB_CONTENT_MARGIN_Y;
	float    thumb_h   = flux_maxf(20.0f, vp * vp / rt->content_h);
	float    thumb_y   = track_top + (rt->scroll_y / max_scroll) * (vp - thumb_h);
	FluxRect thumb     = {rt->width - CB_BORDER - 5.0f, thumb_y, 3.0f, thumb_h};
	flux_fill_rounded_rect(rc, &thumb, 1.5f, t->ctrl_strong_fill_default);
}

static void combo_paint(void *ctx, FluxPopup *popup) {
	FluxComboRuntime   *rt  = ( FluxComboRuntime * ) ctx;
	FluxGraphics       *gfx = flux_popup_get_graphics(popup);
	ID2D1DeviceContext *d2d = gfx ? flux_graphics_get_d2d_context(gfx) : NULL;
	if (!d2d) return;
	combo_ensure_brush(rt, d2d);
	if (!rt->brush) return;

	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	combo_resolve_theme(rt, &theme, &is_dark);

	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d           = d2d;
	rc.brush         = rt->brush;
	rc.text          = rt->text;
	rc.theme         = theme;
	rc.dpi           = flux_window_dpi(rt->window);
	rc.is_dark       = is_dark;

	FluxRect  bounds = {0.0f, 0.0f, rt->width, rt->height};
	FluxColor bg     = is_dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : flux_color_rgb(0xf9, 0xf9, 0xf9);
	FluxColor border = is_dark ? flux_color_rgba(0, 0, 0, 0x33) : flux_color_rgba(0, 0, 0, 0x0f);
	bg               = flux_popup_acrylic_tint(popup, bg);
	flux_fill_rounded_rect(&rc, &bounds, CB_CORNER, bg);

	float    half  = CB_BORDER * 0.5f;
	FluxRect inset = {bounds.x + half, bounds.y + half, bounds.w - half * 2.0f, bounds.h - half * 2.0f};
	flux_draw_rounded_rect(&rc, &inset, CB_CORNER, border, CB_BORDER);

	float       vp        = combo_viewport_h(rt);
	FluxRect    view      = {CB_BORDER, CB_BORDER + CB_CONTENT_MARGIN_Y, rt->width - 2.0f * CB_BORDER, vp};
	D2D1_RECT_F view_clip = flux_d2d_rect(&view);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(&rc), &view_clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	for (int i = 0; i < rt->model.item_count; i++) combo_draw_item(&rc, rt, theme, i);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(&rc));

	combo_draw_scrollbar(&rc, rt, theme, vp);
}

static void combo_repaint_popup(FluxComboRuntime *rt) {
	HWND h = flux_popup_get_hwnd(rt->popup);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void combo_repaint_owner(FluxComboRuntime *rt) {
	HWND h = flux_window_hwnd(rt->window);
	if (h) InvalidateRect(h, NULL, FALSE);
}

static void combo_close(FluxComboRuntime *rt) {
	rt->model.open = false;
	flux_popup_dismiss(rt->popup);
	combo_repaint_owner(rt);
}

static void combo_commit(FluxComboRuntime *rt, int index) {
	if (index < 0 || index >= rt->model.item_count) return;
	rt->model.selected_index = index;
	if (rt->model.on_select) rt->model.on_select(rt->model.on_select_ctx, index);
	combo_close(rt);
}

static void combo_open(FluxComboRuntime *rt) {
	XentRect r = {0};
	xent_get_layout_rect(rt->ctx, rt->node, &r);

	/* WinUI grows drop-down items for touch (ComboBoxItemThemeTouchPadding). The
	 * device that opened the box decides the item height for this session. */
	FluxNodeData *nd    = flux_node_store_get(rt->store, rt->node);
	bool          touch = nd && nd->state.pointer_type == 1;
	rt->row_h           = touch ? CB_ROW_H_TOUCH : CB_ROW_H;

	rt->width           = r.w > 0.0f ? r.w : 120.0f;
	rt->height          = combo_dropdown_height(rt->model.item_count, rt->row_h);
	rt->content_h       = ( float ) rt->model.item_count * rt->row_h;
	flux_popup_set_size(rt->popup, rt->width, rt->height);

	rt->model.open = true;
	rt->highlight  = rt->model.selected_index;
	rt->scroll_y   = 0.0f;
	combo_ensure_visible(rt, rt->highlight);
	FluxRect anchor = flux_binding_screen_anchor(rt->window, rt->ctx, rt->store, rt->node);
	flux_popup_show(rt->popup, anchor, FLUX_PLACEMENT_BOTTOM);
	combo_repaint_owner(rt);
}

/* WinUI ComboBox opens its drop-down on pointer release (OnPointerReleased), not press --
 * press only sets the pressed visual. Firing on click also gives press-cancel for free. */
static void combo_on_click(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt) return;
	if (rt->model.open) combo_close(rt);
	else combo_open(rt);
}

static void combo_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent event, float x, float y) {
	( void ) popup;
	( void ) x;
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt) return;
	/* WinUI selects on release: the press marks the item, the release on the same item
	 * commits (press-cancel if you drag off). Matches MenuFlyout item activation. */
	if (event == FLUX_POPUP_MOUSE_DOWN) {
		rt->pressed_index = combo_item_at(rt, y);
		return;
	}
	if (event == FLUX_POPUP_MOUSE_UP) {
		int idx = combo_item_at(rt, y);
		if (idx >= 0 && idx == rt->pressed_index) combo_commit(rt, idx);
		rt->pressed_index = -1;
		return;
	}
	if (event == FLUX_POPUP_MOUSE_WHEEL) {
		rt->scroll_y -= y;
		combo_clamp_scroll(rt);
		combo_repaint_popup(rt);
		return;
	}
	int hot = (event == FLUX_POPUP_MOUSE_LEAVE) ? -1 : combo_item_at(rt, y);
	if (hot != rt->highlight) {
		rt->highlight = hot;
		combo_repaint_popup(rt);
	}
}

static void combo_step_selection(FluxComboRuntime *rt, int delta) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	int next = rt->model.selected_index + delta;
	if (next < 0) next = 0;
	if (next >= n) next = n - 1;
	if (next == rt->model.selected_index) return;
	rt->model.selected_index = next;
	if (rt->model.on_select) rt->model.on_select(rt->model.on_select_ctx, next);
	combo_repaint_owner(rt);
}

static void combo_step_highlight(FluxComboRuntime *rt, int delta) {
	int n = rt->model.item_count;
	if (n <= 0) return;
	int next = (rt->highlight < 0 ? rt->model.selected_index : rt->highlight) + delta;
	if (next < 0) next = 0;
	if (next >= n) next = n - 1;
	rt->highlight = next;
	combo_ensure_visible(rt, next);
	combo_repaint_popup(rt);
}

static bool combo_key_collapsed(FluxComboRuntime *rt, unsigned int vk) {
	switch (vk) {
	case VK_DOWN   : combo_step_selection(rt, +1); return true;
	case VK_UP     : combo_step_selection(rt, -1); return true;
	case VK_RETURN :
	case VK_SPACE  : combo_open(rt); return true;
	default        : return false;
	}
}

static bool combo_key_open(FluxComboRuntime *rt, unsigned int vk) {
	switch (vk) {
	case VK_DOWN   : combo_step_highlight(rt, +1); return true;
	case VK_UP     : combo_step_highlight(rt, -1); return true;
	case VK_RETURN : combo_commit(rt, rt->highlight); return true;
	case VK_ESCAPE : combo_close(rt); return true;
	default        : return false;
	}
}

static bool combo_on_key(void *ctx, unsigned int vk, bool down) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt || !down) return false;
	return rt->model.open ? combo_key_open(rt, vk) : combo_key_collapsed(rt, vk);
}

static void combo_destroy(void *component_data) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) component_data;
	if (!rt) return;
	if (rt->brush) ID2D1SolidColorBrush_Release(rt->brush);
	if (rt->popup) flux_popup_destroy(rt->popup);
	free(rt);
}

static void combo_dismissed(void *ctx) {
	FluxComboRuntime *rt = ( FluxComboRuntime * ) ctx;
	if (!rt) return;
	rt->model.open = false;
	combo_repaint_owner(rt);
}

XentNodeId flux_create_combo_box(FluxComboBoxCreateInfo const *info) {
	if (!info || !info->ctx || !info->store || !info->window) return XENT_NODE_INVALID;
	flux_node_store_register_renderer(info->store, FLUX_CONTROL_COMBO_BOX, flux_draw_combo_box, NULL);

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_COMBO_BOX);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData     *nd = flux_node_store_get(info->store, node);
	FluxComboRuntime *rt = nd ? ( FluxComboRuntime * ) calloc(1, sizeof(*rt)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return node;
	}
	rt->model.items          = info->items;
	rt->model.item_count     = info->item_count;
	rt->model.selected_index = info->selected_index;
	rt->model.placeholder    = info->placeholder;
	rt->model.on_select      = info->on_select;
	rt->model.on_select_ctx  = info->userdata;
	rt->store                = info->store;
	rt->node                 = node;
	rt->ctx                  = info->ctx;
	rt->window               = info->window;
	rt->text                 = info->text;
	rt->theme                = info->theme;
	rt->highlight            = -1;
	rt->pressed_index        = -1;

	rt->popup                = flux_popup_create(info->window);
	if (rt->popup) {
		flux_popup_set_dismiss_on_outside(rt->popup, true);
		flux_popup_set_anim_style(rt->popup, FLUX_POPUP_ANIM_FLYOUT);
		flux_popup_set_paint_callback(rt->popup, combo_paint, rt);
		flux_popup_set_mouse_callback(rt->popup, combo_mouse, rt);
		flux_popup_set_dismiss_callback(rt->popup, combo_dismissed, rt);
	}

	nd->component_data         = &rt->model;
	nd->destroy_component_data = combo_destroy;
	nd->behavior.on_click      = combo_on_click;
	nd->behavior.on_click_ctx  = rt;
	nd->behavior.on_key        = combo_on_key;
	nd->behavior.on_key_ctx    = rt;

	xent_set_focusable(info->ctx, node, true);
	xent_set_semantic_role(info->ctx, node, XENT_SEMANTIC_BUTTON);
	return node;
}
