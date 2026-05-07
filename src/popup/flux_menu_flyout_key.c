#include "flux_menu_flyout_internal.h"

#include <windows.h>

typedef bool (*MenuKeyHandler)(FluxMenuFlyout *m);

typedef struct MenuKeyBinding {
	unsigned int   vk;
	MenuKeyHandler handler;
} MenuKeyBinding;

static bool menu_set_keyboard_index(FluxMenuFlyout *m, int idx) {
	if (idx < 0) return true;
	m->keyboard_index = idx;
	m->hovered_index  = -1;
	close_submenu(m);
	invalidate(m);
	return true;
}

static bool menu_handle_move_key(FluxMenuFlyout *m, int from, int dir) {
	int idx = find_next(m, from, dir, m->parent == NULL);
	return menu_set_keyboard_index(m, idx);
}

static bool menu_handle_down_key(FluxMenuFlyout *m) {
	return menu_handle_move_key(m, m->keyboard_index >= 0 ? m->keyboard_index : -1, +1);
}

static bool menu_handle_up_key(FluxMenuFlyout *m) {
	return menu_handle_move_key(m, m->keyboard_index >= 0 ? m->keyboard_index : m->item_count, -1);
}

static bool menu_handle_home_key(FluxMenuFlyout *m) { return menu_set_keyboard_index(m, first_enabled(m)); }

static bool menu_handle_end_key(FluxMenuFlyout *m) { return menu_set_keyboard_index(m, last_enabled(m)); }

static bool menu_handle_activate_key(FluxMenuFlyout *m) {
	if (m->keyboard_index >= 0) activate_item(m, m->keyboard_index);
	return true;
}

static bool menu_handle_escape_key(FluxMenuFlyout *m) {
	flux_menu_flyout_dismiss_all(m);
	return true;
}

static bool menu_handle_right_key(FluxMenuFlyout *m) {
	int idx = m->keyboard_index;
	if (idx < 0 || idx >= m->item_count) return true;

	StoredItem *it = &m->items [idx];
	if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu) open_submenu(m, idx, true);
	return true;
}

static bool menu_handle_left_key(FluxMenuFlyout *m) {
	if (!m->parent) return true;

	FluxMenuFlyout *parent = m->parent;
	int             pidx   = m->parent_item_index;
	_dismiss_internal(m, true);
	parent->keyboard_index = pidx;
	parent->hovered_index  = -1;
	parent->pressed_index  = -1;
	invalidate(parent);
	refresh_active_leaf(root_of(parent));
	return true;
}

bool flux_menu_flyout_on_key(FluxMenuFlyout *m, unsigned int vk) {
	if (!m || !flux_popup_is_visible(m->popup)) return false;

	static MenuKeyBinding const bindings [] = {
	  {VK_DOWN,   menu_handle_down_key    },
	  {VK_UP,     menu_handle_up_key      },
	  {VK_HOME,   menu_handle_home_key    },
	  {VK_END,    menu_handle_end_key     },
	  {VK_RETURN, menu_handle_activate_key},
	  {VK_SPACE,  menu_handle_activate_key},
	  {VK_ESCAPE, menu_handle_escape_key  },
	  {VK_RIGHT,  menu_handle_right_key   },
	  {VK_LEFT,   menu_handle_left_key    },
	};

	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings [0]); i++)
		if (bindings [i].vk == vk) return bindings [i].handler(m);
	return false;
}
