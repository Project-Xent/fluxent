#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "flux_menu_internal.h"

#define FLUX_MENU_WHEEL_STEP 48.0f
#include "controls/flux_control_uia.h"
#include "render/flux_render_snapshot.h"
#include "popup/flux_popup_tree.h"
#include "a11y/flux_uia_ids.h"
#include "store/flux_node_data.h"
#include "controls/flux_module.h"
#include "controls/factory/flux_factory.h"
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

static bool menu_nodes_build(FluxMenu *m, FluxStore *store);
static void menu_rows_sync(FluxMenu *m);

static void menu_prepare_child_popup(FluxMenu *parent, FluxMenu *child, int idx) {
	if (!child->presenter && parent->store) menu_nodes_build(child, parent->store);
	menu_measure(child);
	flux_popup_setsize(child->popup, child->total_w, child->total_h);
	menu_rows_sync(child);
	menu_rows_layout(child);
	if (child->input && idx < parent->row_count) flux_popup_sethost(child->popup, child->input, parent->rows [idx]);
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
	child->open_submenu_index = -1;
	child->keyboard_index     = focus_first ? first_enabled(child) : -1;

	flux_popup_setrtl(child->popup, flux_popup_rtl(m->popup));
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
	menu_prepare_child_popup(m, child, idx);
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

static void menu_row_hover(void *ctx, bool hovered) {
	MenuRow const *r = ( MenuRow const * ) ctx;
	FluxMenu      *m = r->menu;
	if (!hovered) {
		if (m->hovered_index == r->index) menu_set_hovered(m, -1);
		return;
	}
	menu_set_hovered(m, r->index);
	StoredItem *it = &m->items [r->index];
	if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu && it->enabled) {
		open_submenu(m, r->index, false);
		return;
	}
	if (m->open_submenu_index >= 0 && m->open_submenu_index != r->index) close_submenu(m);
}

static void menu_row_click(void *ctx) {
	MenuRow const *r = ( MenuRow const * ) ctx;
	activate_item(r->menu, r->index);
}

static MenuRow const *menu_row_of(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	return nd && nd->component_type == FLUX_CONTROL_MENU_ROW ? ( MenuRow const * ) nd->component_data : NULL;
}

static int menu_uia_expand(FluxStore *store, XentNodeId node) {
	MenuRow const *r = menu_row_of(store, node);
	return r && r->menu->open_submenu_index == r->index && r->menu->open_submenu ? 1 : 0;
}

static void menu_uia_toggle(FluxStore *store, XentNodeId node) {
	MenuRow const *r = menu_row_of(store, node);
	if (!r) return;
	if (menu_uia_expand(store, node)) close_submenu(r->menu);
	else open_submenu(r->menu, r->index, true);
}

static FluxUiaHooks const menu_uia_hidden = {.role = FLUX_UIA_HIDDEN};
static FluxUiaHooks const menu_uia_item   = {.role = FLUX_UIA_MENUITEM, .patterns = FLUX_UIA_P_INVOKE};
static FluxUiaHooks const menu_uia_check  = {.role = FLUX_UIA_MENUITEM, .patterns = FLUX_UIA_P_TOGGLE};
static FluxUiaHooks const menu_uia_sep    = {.role = FLUX_UIA_SEPARATOR};
static FluxUiaHooks const menu_uia_sub    = {
  .role          = FLUX_UIA_MENUITEM,
  .patterns      = FLUX_UIA_P_EXPAND,
  .expand        = menu_uia_expand,
  .toggle_expand = menu_uia_toggle};

static FluxUiaHooks const *menu_row_uia(FluxMenuItemType type) {
	switch (type) {
	case FLUX_MENU_ITEM_SEPARATOR : return &menu_uia_sep;
	case FLUX_MENU_ITEM_TOGGLE    :
	case FLUX_MENU_ITEM_RADIO     : return &menu_uia_check;
	case FLUX_MENU_ITEM_SUBMENU   : return &menu_uia_sub;
	default                       : return &menu_uia_item;
	}
}

static XentNodeId menu_part(FluxMenu *m, XentNodeId parent, FluxControlType type) {
	XentNodeId    n  = flux_factory_create_node(m->ctx, m->store, parent, type);
	FluxNodeData *nd = n ? flux_store_find(m->store, n) : NULL;
	if (!nd) return XENT_NODE_INVALID;
	xent_setproto(m->ctx, n, XENT_PROTOCOL_ABSOLUTE);
	if (type == FLUX_CONTROL_CONTAINER) nd->uia = &menu_uia_hidden;
	return n;
}

static bool menu_nodes_build(FluxMenu *m, FluxStore *store) {
	m->store           = store;
	m->ctx             = flux_store_ctx(store);
	m->input           = flux_store_input(store);
	m->presenter       = menu_part(m, XENT_NODE_INVALID, FLUX_CONTROL_MENU_PRESENTER);
	m->shift           = m->presenter ? menu_part(m, m->presenter, FLUX_CONTROL_CONTAINER) : XENT_NODE_INVALID;
	m->body            = m->shift ? menu_part(m, m->shift, FLUX_CONTROL_CONTAINER) : XENT_NODE_INVALID;
	FluxNodeData *root = m->presenter ? flux_store_find(store, m->presenter) : NULL;
	FluxNodeData *body = m->body ? flux_store_find(store, m->body) : NULL;
	if (!root || !body) return false;
	root->component_data      = m;
	root->focus_passive       = true;
	body->render_clip_subtree = true;
	return flux_popup_setcontent(m->popup, m->ctx, store, m->presenter);
}

static void menu_rows_clear(FluxMenu *m) {
	for (int i = 0; i < m->row_count; i++) xent_node_destroy(m->ctx, m->rows [i]);
	m->row_count = 0;
}

static bool menu_row_make(FluxMenu *m, int i) {
	XentNodeId    n  = flux_factory_create_node(m->ctx, m->store, m->body, FLUX_CONTROL_MENU_ROW);
	FluxNodeData *nd = n ? flux_store_find(m->store, n) : NULL;
	if (!nd) return false;
	StoredItem const *it = &m->items [i];
	m->rows [i]          = n;
	m->row_data [i]      = (MenuRow) {m, i};
	nd->component_data   = &m->row_data [i];
	nd->uia              = menu_row_uia(it->type);
	if (it->type == FLUX_MENU_ITEM_SEPARATOR) return true;
	nd->behavior.on_click             = menu_row_click;
	nd->behavior.on_click_ctx         = &m->row_data [i];
	nd->behavior.on_hover_changed     = menu_row_hover;
	nd->behavior.on_hover_changed_ctx = &m->row_data [i];
	xent_sem_setlabel(m->ctx, n, it->label);
	xent_sem_setenabled(m->ctx, n, it->enabled);
	xent_sem_setchecked(m->ctx, n, it->checked ? 1u : 0u);
	return true;
}

static void menu_rows_sync(FluxMenu *m) {
	if (!m->body) return;
	menu_rows_clear(m);
	for (int i = 0; i < m->item_count && menu_row_make(m, i); i++) m->row_count = i + 1;
}

void menu_rows_layout(FluxMenu *m) {
	if (!m->presenter) return;
	float cw = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER;
	float ch
	  = m->total_h - 2.0f * FLUX_MENU_PRESENTER_BORDER - FLUX_MENU_PRESENTER_PAD_TOP - FLUX_MENU_PRESENTER_PAD_BOTTOM;
	for (int i = 0; i < m->row_count; i++) {
		xent_setpos(m->ctx, m->rows [i], (XentPoint) {0.0f, m->items [i].y - m->scroll_y});
		xent_setsize(m->ctx, m->rows [i], (XentSize) {cw, m->items [i].h});
	}
	xent_setpos(
	  m->ctx, m->body,
	  (XentPoint) {FLUX_MENU_PRESENTER_BORDER, FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP}
	);
	xent_setsize(m->ctx, m->body, (XentSize) {cw, ch > 0.0f ? ch : 0.0f});
	xent_setpos(m->ctx, m->shift, (XentPoint) {0.0f, 0.0f});
	xent_setsize(m->ctx, m->shift, (XentSize) {m->total_w, m->total_h});
	xent_setpos(m->ctx, m->presenter, (XentPoint) {0.0f, 0.0f});
	xent_setsize(m->ctx, m->presenter, (XentSize) {m->total_w, m->total_h});
	xent_layout(m->ctx, m->presenter, m->total_w, m->total_h);
}

void flux_snap_menupresenter(FluxSnapCtx const *ctx) {
	FluxMenu               *m     = ( FluxMenu * ) ctx->data;
	FluxPopupMenuTransition tr    = flux_popup_menutransition(m->popup, m->total_h);
	FluxNodeData           *shift = flux_store_find(m->store, m->shift);
	FluxNodeData           *body  = flux_store_find(m->store, m->body);
	if (shift) shift->render_translate_y = tr.clip_translate_y;
	if (body) body->render_translate_y = tr.content_translate_y - tr.clip_translate_y;
	if (body) body->clips_children = tr.clip_translate_y == 0.0f && body->render_translate_y == 0.0f;
	ctx->snap->u.menu_part = (FluxMenuPartSnapshot) {m, -1};
}

void flux_snap_menurow(FluxSnapCtx const *ctx) {
	MenuRow const *r       = ( MenuRow const * ) ctx->data;
	ctx->snap->u.menu_part = (FluxMenuPartSnapshot) {r ? r->menu : NULL, r ? r->index : -1};
}

bool flux_menu_wheel(FluxStore *store, XentNodeId node, float delta, float *remaining) {
	( void ) remaining;
	FluxNodeData *nd = flux_store_find(store, node);
	FluxMenu     *m  = nd ? ( FluxMenu * ) nd->component_data : NULL;
	if (!m || m->scroll_max <= 0.0f) return false;
	float next = m->scroll_y - delta * FLUX_MENU_WHEEL_STEP;
	next       = next < 0.0f ? 0.0f : (next > m->scroll_max ? m->scroll_max : next);
	if (next == m->scroll_y) return true;
	m->scroll_y      = next;
	m->hovered_index = -1;
	if (m->open_submenu_index >= 0) close_submenu(m);
	menu_rows_layout(m);
	invalidate(m);
	return true;
}

void flux_menu_bind(FluxMenu *m, FluxStore *store, XentNodeId host) {
	if (!m || !store) return;
	if (!m->presenter && !menu_nodes_build(m, store)) return;
	m->host = host;
	if (m->input) flux_popup_sethost(m->popup, m->input, host);
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

	m->hovered_index      = -1;
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

	if (m->popup) flux_popup_destroy(m->popup);
	if (m->presenter) xent_node_destroy(m->ctx, m->presenter);
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
	menu_rows_sync(m);
	menu_rows_layout(m);

	m->hovered_index  = -1;
	m->keyboard_index = (input_kind == FLUX_MENU_INPUT_KEYBOARD) ? first_enabled(m) : -1;
	close_submenu(m);

	flux_popup_show(m->popup, anchor, placement);
	refresh_active_leaf(root_of(m));
}

void flux_menu_dismiss(FluxMenu *m) { _dismiss_internal(m, true); }

void flux_menu_setrtl(FluxMenu *m, bool rtl) {
	if (m) flux_popup_setrtl(m->popup, rtl);
}

void flux_menu_dismissall(FluxMenu *m) {
	FluxMenu *root = root_of(m);
	if (!root) return;

	while (root->open_submenu) close_submenu(root);

	FluxPopupDismissFn cb    = root->dismiss_cb;
	void              *cx    = root->dismiss_ctx;
	FluxWindow        *owner = root->owner;

	flux_popup_dismiss(root->popup);
	root->hovered_index      = -1;
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
