#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "flux_menu_flyout_internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

static char *dup_cstr(char const *s) {
	if (!s) return NULL;
	size_t n = strlen(s);
	char  *c = ( char * ) malloc(n + 1);
	if (c) memcpy(c, s, n + 1);
	return c;
}

void invalidate(FluxMenuFlyout *m) {
	if (!m) return;
	HWND hwnd = flux_popup_get_hwnd(m->popup);
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

FluxMenuFlyout *root_of(FluxMenuFlyout *m) {
	while (m && m->parent) m = m->parent;
	return m;
}

void refresh_active_leaf(FluxMenuFlyout *root) {
	FluxMenuFlyout *leaf = root;
	while (leaf && leaf->open_submenu && flux_popup_is_visible(leaf->open_submenu->popup)) leaf = leaf->open_submenu;
	if (root) flux_window_set_active_menu(root->owner, leaf);
}

int first_enabled(FluxMenuFlyout const *m) {
	for (int i = 0; i < m->item_count; i++)
		if (m->items [i].type != FLUX_MENU_ITEM_SEPARATOR && m->items [i].enabled) return i;
	return -1;
}

int last_enabled(FluxMenuFlyout const *m) {
	for (int i = m->item_count - 1; i >= 0; i--)
		if (m->items [i].type != FLUX_MENU_ITEM_SEPARATOR && m->items [i].enabled) return i;
	return -1;
}

int find_next(FluxMenuFlyout const *m, int from, int dir, bool wrap) {
	if (m->item_count == 0) return -1;
	int idx = from;
	for (int attempt = 0; attempt < m->item_count; attempt++) {
		idx += dir;
		if (!wrap && (idx < 0 || idx >= m->item_count)) return from >= 0 && from < m->item_count ? from : -1;
		if (idx < 0) idx = m->item_count - 1;
		if (idx >= m->item_count) idx = 0;
		if (m->items [idx].type != FLUX_MENU_ITEM_SEPARATOR && m->items [idx].enabled) return idx;
	}
	return -1;
}

void _dismiss_internal(FluxMenuFlyout *m, bool fire_cb) {
	if (!m) return;
	close_submenu(m);
	flux_popup_dismiss(m->popup);

	m->hovered_index      = -1;
	m->pressed_index      = -1;
	m->keyboard_index     = -1;
	m->open_submenu_index = -1;

	if (m->parent && m->parent->open_submenu == m) {
		m->parent->open_submenu       = NULL;
		m->parent->open_submenu_index = -1;
		invalidate(m->parent);
	}

	FluxMenuFlyout *parent_before = m->parent;
	m->parent                     = NULL;
	m->parent_item_index          = -1;

	if (fire_cb && m->dismiss_cb) m->dismiss_cb(m->dismiss_ctx);

	if (parent_before) refresh_active_leaf(root_of(parent_before));
	else if (m->owner && flux_window_get_active_menu(m->owner) == m) flux_window_set_active_menu(m->owner, NULL);
}

void close_submenu(FluxMenuFlyout *m) {
	if (!m || !m->open_submenu) return;
	FluxMenuFlyout *child = m->open_submenu;
	m->open_submenu       = NULL;
	m->open_submenu_index = -1;
	_dismiss_internal(child, false);
	invalidate(m);
}

static bool menu_focus_open_submenu(FluxMenuFlyout *m, StoredItem *it, bool focus_first) {
	if (m->open_submenu != it->submenu || !flux_popup_is_visible(it->submenu->popup)) return false;
	if (!focus_first) return true;

	it->submenu->keyboard_index = first_enabled(it->submenu);
	it->submenu->hovered_index  = -1;
	invalidate(it->submenu);
	refresh_active_leaf(root_of(m));
	return true;
}

static void menu_inherit_child_context(FluxMenuFlyout *parent, FluxMenuFlyout *child, int idx) {
	child->parent            = parent;
	child->parent_item_index = idx;
	child->theme             = parent->theme;
	child->text              = parent->text;
	child->pad_top           = parent->pad_top > 0.0f ? parent->pad_top : FLUX_MENU_ITEM_PAD_TOP_NARROW;
	child->pad_bot           = parent->pad_bot > 0.0f ? parent->pad_bot : FLUX_MENU_ITEM_PAD_BOT_NARROW;
	child->scroll_y          = 0.0f;
}

static void menu_prepare_child_popup(FluxMenuFlyout *child) {
	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	( void ) theme;

	menu_resolve_theme(child, &theme, &is_dark);
	menu_measure(child);
	flux_popup_set_size(child->popup, child->total_w, child->total_h);
	flux_popup_enable_system_backdrop(child->popup, is_dark);
}

static FluxRect menu_submenu_anchor(FluxMenuFlyout *m, StoredItem const *it) {
	FluxDpiInfo dpi         = flux_window_dpi(m->owner);
	float       scale       = dpi.dpi_x / 96.0f;
	HWND        parent_hwnd = flux_popup_get_hwnd(m->popup);
	RECT        wr          = {0};
	GetWindowRect(parent_hwnd, &wr);

	float ir_x_dip = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_ITEM_MARGIN_H;
	float ir_y_dip
	  = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP + it->y + FLUX_MENU_ITEM_MARGIN_V - m->scroll_y;
	float ir_h_dip = it->h - 2.0f * FLUX_MENU_ITEM_MARGIN_V;
	float ir_w_dip = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER - 2.0f * FLUX_MENU_ITEM_MARGIN_H;

	return (FluxRect) {
	  .x = ( float ) wr.left + ir_x_dip * scale,
	  .y = ( float ) wr.top + ir_y_dip * scale,
	  .w = ir_w_dip * scale,
	  .h = ir_h_dip * scale,
	};
}

static void menu_open_child_popup(FluxMenuFlyout *m, FluxMenuFlyout *child, StoredItem const *it, bool focus_first) {
	FluxRect anchor           = menu_submenu_anchor(m, it);

	m->open_submenu           = child;
	m->open_submenu_index     = ( int ) (it - m->items);
	child->hovered_index      = -1;
	child->pressed_index      = -1;
	child->open_submenu_index = -1;
	child->keyboard_index     = focus_first ? first_enabled(child) : -1;

	flux_popup_show(child->popup, anchor, FLUX_PLACEMENT_RIGHT);
	invalidate(m);
	invalidate(child);
	refresh_active_leaf(root_of(m));
}

void open_submenu(FluxMenuFlyout *m, int idx, bool focus_first) {
	if (!m || idx < 0 || idx >= m->item_count) return;
	StoredItem *it = &m->items [idx];
	if (it->type != FLUX_MENU_ITEM_SUBMENU || !it->submenu || !it->enabled) return;
	if (menu_focus_open_submenu(m, it, focus_first)) return;

	close_submenu(m);

	FluxMenuFlyout *child = it->submenu;
	menu_inherit_child_context(m, child, idx);
	menu_measure(m);
	menu_prepare_child_popup(child);
	menu_open_child_popup(m, child, it, focus_first);
}

static void uncheck_radio_group(FluxMenuFlyout *m, char const *group, int except) {
	if (!group || !*group) return;
	for (int i = 0; i < m->item_count; i++) {
		if (i == except) continue;
		StoredItem *it = &m->items [i];
		if (it->type == FLUX_MENU_ITEM_RADIO && it->radio_group && strcmp(it->radio_group, group) == 0)
			it->checked = false;
	}
}

void activate_item(FluxMenuFlyout *m, int idx) {
	if (!m || idx < 0 || idx >= m->item_count) return;
	StoredItem *it = &m->items [idx];
	if (it->type == FLUX_MENU_ITEM_SEPARATOR || !it->enabled) return;

	if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu) {
		m->keyboard_index = idx;
		m->hovered_index  = -1;
		open_submenu(m, idx, true);
		return;
	}

	if (it->type == FLUX_MENU_ITEM_TOGGLE) { it->checked = !it->checked; }
	else if (it->type == FLUX_MENU_ITEM_RADIO && !it->checked) {
		uncheck_radio_group(m, it->radio_group, idx);
		it->checked = true;
	}

	if (it->on_click) it->on_click(it->on_click_ctx);

	flux_menu_flyout_dismiss_all(m);
}

static void menu_set_hovered(FluxMenuFlyout *m, int idx) {
	if (idx == m->hovered_index) return;
	m->hovered_index  = idx;
	m->keyboard_index = -1;
	invalidate(m);
}

static void menu_mouse_move(FluxMenuFlyout *m, float x, float y) {
	int idx = menu_hit_test(m, x, y);
	menu_set_hovered(m, idx);
	if (idx < 0) return;

	StoredItem *it = &m->items [idx];
	if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu && it->enabled) {
		open_submenu(m, idx, false);
		return;
	}
	if (m->open_submenu_index >= 0 && m->open_submenu_index != idx) close_submenu(m);
}

static void menu_mouse_down(FluxMenuFlyout *m, float x, float y) {
	m->pressed_index = menu_hit_test(m, x, y);
	invalidate(m);
}

static void menu_mouse_up(FluxMenuFlyout *m, float x, float y) {
	int idx          = menu_hit_test(m, x, y);
	int pressed      = m->pressed_index;
	m->pressed_index = -1;
	invalidate(m);
	if (idx >= 0 && idx == pressed) activate_item(m, idx);
}

static void menu_mouse_leave(FluxMenuFlyout *m) {
	m->hovered_index = -1;
	m->pressed_index = -1;
	invalidate(m);
}

static void menu_mouse_wheel(FluxMenuFlyout *m, float dy) {
	if (m->scroll_max <= 0.0f) return;

	float new_y = m->scroll_y - dy;
	if (new_y < 0.0f) new_y = 0.0f;
	if (new_y > m->scroll_max) new_y = m->scroll_max;
	if (new_y == m->scroll_y) return;

	m->scroll_y      = new_y;
	m->hovered_index = -1;
	if (m->open_submenu_index >= 0) close_submenu(m);
	invalidate(m);
}

static void menu_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent ev, float x, float y) {
	( void ) popup;
	FluxMenuFlyout *m = ( FluxMenuFlyout * ) ctx;
	if (!m) return;

	switch (ev) {
	case FLUX_POPUP_MOUSE_MOVE  : menu_mouse_move(m, x, y); return;
	case FLUX_POPUP_MOUSE_DOWN  : menu_mouse_down(m, x, y); return;
	case FLUX_POPUP_MOUSE_UP    : menu_mouse_up(m, x, y); return;
	case FLUX_POPUP_MOUSE_LEAVE : menu_mouse_leave(m); return;
	case FLUX_POPUP_MOUSE_WHEEL : menu_mouse_wheel(m, y); return;
	}
}

void menu_resolve_theme(FluxMenuFlyout const *m, FluxThemeColors const **out_theme, bool *out_is_dark) {
	if (m->theme.manager) {
		*out_theme         = flux_theme_colors(m->theme.manager);
		FluxThemeMode mode = flux_theme_get_mode(m->theme.manager);
		*out_is_dark       = (mode == FLUX_THEME_DARK) || (mode == FLUX_THEME_SYSTEM && flux_theme_system_is_dark());
	}
	else {
		*out_theme   = m->theme.colors;
		*out_is_dark = m->theme.is_dark;
	}
}

FluxMenuFlyout *flux_menu_flyout_create(FluxWindow *owner) {
	if (!owner) return NULL;

	FluxMenuFlyout *m = ( FluxMenuFlyout * ) calloc(1, sizeof(*m));
	if (!m) return NULL;

	m->owner = owner;
	m->popup = flux_popup_create(owner);
	if (!m->popup) {
		free(m);
		return NULL;
	}

	flux_popup_set_dismiss_on_outside(m->popup, true);
	flux_popup_set_paint_callback(m->popup, menu_paint, m);
	flux_popup_set_mouse_callback(m->popup, menu_mouse, m);

	m->hovered_index      = -1;
	m->pressed_index      = -1;
	m->keyboard_index     = -1;
	m->open_submenu_index = -1;
	m->parent_item_index  = -1;
	return m;
}

void flux_menu_flyout_destroy(FluxMenuFlyout *m) {
	if (!m) return;

	if (m->open_submenu) {
		m->open_submenu->parent            = NULL;
		m->open_submenu->parent_item_index = -1;
		m->open_submenu                    = NULL;
	}
	if (m->parent && m->parent->open_submenu == m) {
		m->parent->open_submenu       = NULL;
		m->parent->open_submenu_index = -1;
	}
	if (m->owner && flux_window_get_active_menu(m->owner) == m) flux_window_set_active_menu(m->owner, NULL);

	for (int i = 0; i < m->item_count; i++) {
		free(m->items [i].label);
		free(m->items [i].icon_glyph);
		free(m->items [i].accelerator_text);
		free(m->items [i].radio_group);
	}

	if (m->brush) {
		ID2D1SolidColorBrush_Release(m->brush);
		m->brush = NULL;
	}
	if (m->popup) flux_popup_destroy(m->popup);
	free(m);
}

void flux_menu_flyout_set_theme(FluxMenuFlyout *m, FluxThemeColors const *theme, bool is_dark) {
	if (!m) return;
	m->theme.colors  = theme;
	m->theme.is_dark = is_dark;
	m->theme.manager = NULL;
}

void flux_menu_flyout_set_theme_manager(FluxMenuFlyout *m, FluxThemeManager *tm) {
	if (!m) return;
	m->theme.manager = tm;
	if (tm) {
		m->theme.colors  = NULL;
		m->theme.is_dark = false;
	}
}

void flux_menu_flyout_set_text_renderer(FluxMenuFlyout *m, FluxTextRenderer *tr) {
	if (m) m->text = tr;
}

int flux_menu_flyout_add_item(FluxMenuFlyout *m, FluxMenuItemDef const *def) {
	if (!m || !def || m->item_count >= FLUX_MENU_MAX_ITEMS) return -1;

	int         idx = m->item_count++;
	StoredItem *it  = &m->items [idx];
	memset(it, 0, sizeof(*it));

	it->type             = def->type;
	it->label            = dup_cstr(def->label);
	it->icon_glyph       = dup_cstr(def->icon_glyph);
	it->accelerator_text = dup_cstr(def->accelerator_text);
	it->radio_group      = dup_cstr(def->radio_group);
	it->enabled          = def->enabled;
	it->checked          = def->checked;
	it->on_click         = def->on_click;
	it->on_click_ctx     = def->on_click_ctx;
	it->submenu          = def->submenu;
	return idx;
}

int flux_menu_flyout_add_separator(FluxMenuFlyout *m) {
	if (!m || m->item_count >= FLUX_MENU_MAX_ITEMS) return -1;
	int         idx = m->item_count++;
	StoredItem *it  = &m->items [idx];
	memset(it, 0, sizeof(*it));
	it->type    = FLUX_MENU_ITEM_SEPARATOR;
	it->enabled = false;
	return idx;
}

void flux_menu_flyout_clear(FluxMenuFlyout *m) {
	if (!m) return;
	close_submenu(m);
	for (int i = 0; i < m->item_count; i++) {
		free(m->items [i].label);
		free(m->items [i].icon_glyph);
		free(m->items [i].accelerator_text);
		free(m->items [i].radio_group);
		memset(&m->items [i], 0, sizeof(StoredItem));
	}
	m->item_count         = 0;
	m->hovered_index      = -1;
	m->pressed_index      = -1;
	m->keyboard_index     = -1;
	m->open_submenu_index = -1;
}

int  flux_menu_flyout_item_count(FluxMenuFlyout const *m) { return m ? m->item_count : 0; }

void flux_menu_flyout_show(FluxMenuFlyout *m, FluxRect anchor, FluxPlacement placement) {
	flux_menu_flyout_show_for_input(m, anchor, placement, FLUX_MENU_INPUT_MOUSE);
}

void flux_menu_flyout_show_for_input(
  FluxMenuFlyout *m, FluxRect anchor, FluxPlacement placement, FluxMenuInputKind input_kind
) {
	if (!m) return;

	/* WinUI PaddingSizeStates: touch → DefaultPadding (11,8,11,9), others → NarrowPadding (11,4,11,5). */
	if (input_kind == FLUX_MENU_INPUT_TOUCH) {
		m->pad_top = FLUX_MENU_ITEM_PAD_TOP_TOUCH;
		m->pad_bot = FLUX_MENU_ITEM_PAD_BOT_TOUCH;
	}
	else {
		m->pad_top = FLUX_MENU_ITEM_PAD_TOP_NARROW;
		m->pad_bot = FLUX_MENU_ITEM_PAD_BOT_NARROW;
	}

	m->scroll_y = 0.0f;
	menu_measure(m);
	flux_popup_set_size(m->popup, m->total_w, m->total_h);

	FluxThemeColors const *theme   = NULL;
	bool                   is_dark = false;
	menu_resolve_theme(m, &theme, &is_dark);

	/* DWM transient-acrylic on Windows 11; resolve_colors() overlays a WinUI tint on top.
	 * Falls back to opaque fill (#2C2C2C/#F9F9F9) on older Windows. */
	flux_popup_enable_system_backdrop(m->popup, is_dark);

	m->hovered_index  = -1;
	m->keyboard_index = -1;
	m->pressed_index  = -1;
	close_submenu(m);

	flux_popup_show(m->popup, anchor, placement);
	refresh_active_leaf(root_of(m));
}

void flux_menu_flyout_dismiss(FluxMenuFlyout *m) { _dismiss_internal(m, true); }

void flux_menu_flyout_dismiss_all(FluxMenuFlyout *m) {
	FluxMenuFlyout *root = root_of(m);
	if (!root) return;

	while (root->open_submenu) close_submenu(root);

	FluxPopupDismissCallback cb = root->dismiss_cb;
	void                    *cx = root->dismiss_ctx;

	flux_popup_dismiss(root->popup);
	root->hovered_index      = -1;
	root->pressed_index      = -1;
	root->keyboard_index     = -1;
	root->open_submenu_index = -1;
	root->open_submenu       = NULL;
	root->parent             = NULL;
	root->parent_item_index  = -1;

	if (cb) cb(cx);
	flux_window_set_active_menu(root->owner, NULL);
}

bool flux_menu_flyout_is_visible(FluxMenuFlyout const *m) { return m ? flux_popup_is_visible(m->popup) : false; }

void flux_menu_flyout_set_dismiss_callback(FluxMenuFlyout *m, FluxPopupDismissCallback cb, void *ctx) {
	if (!m) return;
	m->dismiss_cb  = cb;
	m->dismiss_ctx = ctx;
}

FluxMenuFlyout *flux_menu_flyout_get_open_submenu(FluxMenuFlyout *m) { return m ? m->open_submenu : NULL; }

FluxMenuFlyout *flux_menu_flyout_get_parent(FluxMenuFlyout *m) { return m ? m->parent : NULL; }

int             flux_menu_flyout_get_parent_item_index(FluxMenuFlyout *m) { return m ? m->parent_item_index : -1; }

FluxMenuFlyout *flux_menu_flyout_get_active(FluxWindow *owner) { return flux_window_get_active_menu(owner); }
