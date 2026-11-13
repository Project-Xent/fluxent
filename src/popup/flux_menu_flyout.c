#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "flux_menu_internal.h"
#include "runtime/flux_str.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

static FluxMenu *live_menus;
static uint64_t  next_lifetime = 1;

static void      menu_live_add(FluxMenu *m) {
	m->lifetime = next_lifetime++;
	if (next_lifetime == 0) next_lifetime = 1;
	m->live_next = live_menus;
	live_menus   = m;
}

static void menu_live_remove(FluxMenu *m) {
	FluxMenu **link = &live_menus;
	while (*link && *link != m) link = &(*link)->live_next;
	if (*link) *link = m->live_next;
}

static bool menu_is_live(FluxMenu *m, uint64_t lifetime) {
	for (FluxMenu *it = live_menus; it; it = it->live_next)
		if (it == m) return it->lifetime == lifetime;
	return false;
}

void invalidate(FluxMenu *m) {
	if (!m) return;
	HWND hwnd = flux_popup_hwnd(m->popup);
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

FluxMenu *root_of(FluxMenu *m) {
	while (m && m->parent) m = m->parent;
	return m;
}

void refresh_active_leaf(FluxMenu *root) {
	FluxMenu *leaf = root;
	while (leaf && leaf->open_submenu && flux_popup_visible(leaf->open_submenu->popup)) leaf = leaf->open_submenu;
	if (root) flux_window_setmenu(root->owner, leaf);
}

int first_enabled(FluxMenu const *m) {
	for (int i = 0; i < m->item_count; i++)
		if (m->items [i].type != FLUX_MENU_ITEM_SEPARATOR && m->items [i].enabled) return i;
	return -1;
}

int last_enabled(FluxMenu const *m) {
	for (int i = m->item_count - 1; i >= 0; i--)
		if (m->items [i].type != FLUX_MENU_ITEM_SEPARATOR && m->items [i].enabled) return i;
	return -1;
}

int find_next(FluxMenu const *m, int from, int dir, bool wrap) {
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

void _dismiss_internal(FluxMenu *m, bool fire_cb) {
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

	FluxMenu *parent_before = m->parent;
	if (parent_before) refresh_active_leaf(root_of(parent_before));
	else if (m->owner && flux_window_menu(m->owner) == m) flux_window_setmenu(m->owner, NULL);
	m->parent            = NULL;
	m->parent_item_index = -1;

	if (fire_cb && m->dismiss_cb) m->dismiss_cb(m->dismiss_ctx);
}

void close_submenu(FluxMenu *m) {
	if (!m || !m->open_submenu) return;
	FluxMenu *child       = m->open_submenu;
	m->open_submenu       = NULL;
	m->open_submenu_index = -1;
	_dismiss_internal(child, false);
	invalidate(m);
}

static bool menu_focus_open_submenu(FluxMenu *m, StoredItem *it, bool focus_first) {
	if (m->open_submenu != it->submenu || !flux_popup_visible(it->submenu->popup)) return false;
	if (!focus_first) return true;

	it->submenu->keyboard_index = first_enabled(it->submenu);
	it->submenu->hovered_index  = -1;
	invalidate(it->submenu);
	refresh_active_leaf(root_of(m));
	return true;
}

static void menu_inherit_child_context(FluxMenu *parent, FluxMenu *child, int idx) {
	child->parent            = parent;
	child->parent_item_index = idx;
	child->theme             = parent->theme;
	child->text              = parent->text;
	child->pad_top           = parent->pad_top > 0.0f ? parent->pad_top : FLUX_MENU_ITEM_PAD_TOP_NARROW;
	child->pad_bot           = parent->pad_bot > 0.0f ? parent->pad_bot : FLUX_MENU_ITEM_PAD_BOT_NARROW;
	child->scroll_y          = 0.0f;
}

static void menu_prepare_child_popup(FluxMenu *child) {
	menu_measure(child);
	flux_popup_setsize(child->popup, child->total_w, child->total_h);
}

static FluxRect menu_submenu_anchor(FluxMenu *m, StoredItem const *it) {
	FluxDpi dpi         = flux_window_dpi(m->owner);
	float   scale       = dpi.dpi_x / FLUX_DPI_BASE;
	HWND    parent_hwnd = flux_popup_hwnd(m->popup);
	RECT    wr          = {0};
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

static void menu_open_child_popup(FluxMenu *m, FluxMenu *child, StoredItem const *it, bool focus_first) {
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

void open_submenu(FluxMenu *m, int idx, bool focus_first) {
	if (!m || idx < 0 || idx >= m->item_count) return;
	StoredItem *it = &m->items [idx];
	if (it->type != FLUX_MENU_ITEM_SUBMENU || !it->submenu || !it->enabled) return;
	if (menu_focus_open_submenu(m, it, focus_first)) return;

	close_submenu(m);

	FluxMenu *child = it->submenu;
	menu_inherit_child_context(m, child, idx);
	menu_measure(m);
	menu_prepare_child_popup(child);
	menu_open_child_popup(m, child, it, focus_first);
}

static void uncheck_radio_group(FluxMenu *m, char const *group, int except) {
	if (!group || !*group) return;
	for (int i = 0; i < m->item_count; i++) {
		if (i == except) continue;
		StoredItem *it = &m->items [i];
		if (it->type == FLUX_MENU_ITEM_RADIO && it->radio_group && strcmp(it->radio_group, group) == 0)
			it->checked = false;
	}
}

void activate_item(FluxMenu *m, int idx) {
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

	uint64_t lifetime         = m->lifetime;
	void     (*click)(void *) = it->on_click;
	void    *click_ctx        = it->on_click_ctx;
	if (click) click(click_ctx);
	if (menu_is_live(m, lifetime)) flux_menu_dismissall(m);
}

static void menu_set_hovered(FluxMenu *m, int idx) {
	if (idx == m->hovered_index) return;
	m->hovered_index  = idx;
	m->keyboard_index = -1;
	invalidate(m);
}

static void menu_mouse_move(FluxMenu *m, float x, float y) {
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

static void menu_mouse_down(FluxMenu *m, float x, float y) {
	m->pressed_index = menu_hit_test(m, x, y);
	invalidate(m);
}

static void menu_mouse_up(FluxMenu *m, float x, float y) {
	int idx          = menu_hit_test(m, x, y);
	int pressed      = m->pressed_index;
	m->pressed_index = -1;
	invalidate(m);
	if (idx >= 0 && idx == pressed) activate_item(m, idx);
}

static void menu_mouse_leave(FluxMenu *m) {
	m->hovered_index = -1;
	m->pressed_index = -1;
	invalidate(m);
}

static void menu_mouse_wheel(FluxMenu *m, float dy) {
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
	FluxMenu *m = ( FluxMenu * ) ctx;
	if (!m) return;

	switch (ev) {
	case FLUX_POPUP_MOUSE_MOVE  : menu_mouse_move(m, x, y); return;
	case FLUX_POPUP_MOUSE_DOWN  : menu_mouse_down(m, x, y); return;
	case FLUX_POPUP_MOUSE_UP    : menu_mouse_up(m, x, y); return;
	case FLUX_POPUP_MOUSE_LEAVE : menu_mouse_leave(m); return;
	case FLUX_POPUP_MOUSE_WHEEL : menu_mouse_wheel(m, y); return;
	}
}

void menu_resolve_theme(FluxMenu const *m, FluxThemeColors const **out_theme, bool *out_is_dark) {
	if (m->theme.manager) {
		*out_theme         = flux_theme_colors(m->theme.manager);
		FluxThemeMode mode = flux_theme_mode(m->theme.manager);
		*out_is_dark       = (mode == FLUX_THEME_DARK) || (mode == FLUX_THEME_SYSTEM && flux_theme_systemdark());
	}
	else {
		*out_theme   = m->theme.colors;
		*out_is_dark = m->theme.is_dark;
	}
}

FluxMenu *flux_menu_create(FluxWindow *owner) {
	if (!owner) return NULL;

	FluxMenu *m = ( FluxMenu * ) calloc(1, sizeof(*m));
	if (!m) return NULL;

	m->owner = owner;
	m->popup = flux_popup_create(owner);
	if (!m->popup) {
		free(m);
		return NULL;
	}

	flux_popup_setdismissoutside(m->popup, true);
	flux_popup_onpaint(m->popup, menu_paint, m);
	flux_popup_onmouse(m->popup, menu_mouse, m);

	m->hovered_index      = -1;
	m->pressed_index      = -1;
	m->keyboard_index     = -1;
	m->open_submenu_index = -1;
	m->parent_item_index  = -1;
	menu_live_add(m);
	return m;
}

void flux_menu_destroy(FluxMenu *m) {
	if (!m) return;
	menu_live_remove(m);
	if (m->owner) {
		FluxMenu *active = flux_window_menu(m->owner);
		if (active && root_of(active) == m) flux_window_setmenu(m->owner, NULL);
	}

	if (m->open_submenu) {
		m->open_submenu->parent            = NULL;
		m->open_submenu->parent_item_index = -1;
		m->open_submenu                    = NULL;
	}
	if (m->parent && m->parent->open_submenu == m) {
		m->parent->open_submenu       = NULL;
		m->parent->open_submenu_index = -1;
	}
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

void flux_menu_settheme(FluxMenu *m, FluxThemeColors const *theme, bool is_dark) {
	if (!m) return;
	m->theme.colors  = theme;
	m->theme.is_dark = is_dark;
	m->theme.manager = NULL;
}

void flux_menu_setthememanager(FluxMenu *m, FluxTheme *tm) {
	if (!m) return;
	m->theme.manager = tm;
	if (tm) {
		m->theme.colors  = NULL;
		m->theme.is_dark = false;
	}
}

void flux_menu_settext(FluxMenu *m, FluxText *tr) {
	if (m) m->text = tr;
}

void flux_menu_setminh(FluxMenu *m, float height) {
	if (m) m->min_item_height = height > 0.0f ? height : 0.0f;
}

void flux_menu_setanim(FluxMenu *m, FluxPopupAnimStyle style) {
	if (m && m->popup) flux_popup_setanim(m->popup, style);
}

int flux_menu_additem(FluxMenu *m, FluxMenuItemDef const *def) {
	if (!m || !def || m->item_count >= FLUX_MENU_MAX_ITEMS) return -1;

	int         idx = m->item_count++;
	StoredItem *it  = &m->items [idx];
	memset(it, 0, sizeof(*it));

	it->type             = def->type;
	it->label            = flux_str_dup(def->label);
	it->icon_glyph       = flux_str_dup(def->icon_glyph);
	it->accelerator_text = flux_str_dup(def->accelerator_text);
	it->radio_group      = flux_str_dup(def->radio_group);
	it->enabled          = def->enabled;
	it->checked          = def->checked;
	it->on_click         = def->on_click;
	it->on_click_ctx     = def->on_click_ctx;
	it->submenu          = def->submenu;
	return idx;
}

int flux_menu_addsep(FluxMenu *m) {
	if (!m || m->item_count >= FLUX_MENU_MAX_ITEMS) return -1;
	int         idx = m->item_count++;
	StoredItem *it  = &m->items [idx];
	memset(it, 0, sizeof(*it));
	it->type    = FLUX_MENU_ITEM_SEPARATOR;
	it->enabled = false;
	return idx;
}

void flux_menu_clear(FluxMenu *m) {
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

int  flux_menu_nitem(FluxMenu const *m) { return m ? m->item_count : 0; }

void flux_menu_show(FluxMenu *m, FluxRect anchor, FluxPlacement placement) {
	flux_menu_showinput(m, anchor, placement, FLUX_MENU_INPUT_MOUSE);
}

void flux_menu_showinput(FluxMenu *m, FluxRect anchor, FluxPlacement placement, FluxMenuInputKind input_kind) {
	if (!m) return;

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
	flux_popup_setsize(m->popup, m->total_w, m->total_h);

	m->hovered_index  = -1;
	m->keyboard_index = (input_kind == FLUX_MENU_INPUT_KEYBOARD) ? first_enabled(m) : -1;
	m->pressed_index  = -1;
	close_submenu(m);

	flux_popup_show(m->popup, anchor, placement);
	refresh_active_leaf(root_of(m));
}

void flux_menu_dismiss(FluxMenu *m) { _dismiss_internal(m, true); }

void flux_menu_dismissall(FluxMenu *m) {
	FluxMenu *root = root_of(m);
	if (!root) return;

	while (root->open_submenu) close_submenu(root);

	FluxPopupDismissFn cb    = root->dismiss_cb;
	void              *cx    = root->dismiss_ctx;
	FluxWindow        *owner = root->owner;

	flux_popup_dismiss(root->popup);
	root->hovered_index      = -1;
	root->pressed_index      = -1;
	root->keyboard_index     = -1;
	root->open_submenu_index = -1;
	root->open_submenu       = NULL;
	root->parent             = NULL;
	root->parent_item_index  = -1;

	if (owner) flux_window_setmenu(owner, NULL);
	if (cb) cb(cx);
}

bool flux_menu_visible(FluxMenu const *m) { return m ? flux_popup_visible(m->popup) : false; }

void flux_menu_ondismiss(FluxMenu *m, FluxPopupDismissFn cb, void *ctx) {
	if (!m) return;
	m->dismiss_cb  = cb;
	m->dismiss_ctx = ctx;
}

FluxMenu *flux_menu_submenu(FluxMenu *m) { return m ? m->open_submenu : NULL; }

FluxMenu *flux_menu_parent(FluxMenu *m) { return m ? m->parent : NULL; }

int       flux_menu_parentidx(FluxMenu *m) { return m ? m->parent_item_index : -1; }

FluxMenu *flux_menu_active(FluxWindow *owner) { return flux_window_menu(owner); }
