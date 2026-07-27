#include "flux_menu_internal.h"

#include <windows.h>

typedef bool (*MenuKeyHandler)(FluxMenu *m);

typedef struct MenuKeyBinding {
	FluxKey        key;
	MenuKeyHandler handler;
} MenuKeyBinding;

static bool menu_set_keyboard_index(FluxMenu *m, int idx) {
	if (idx < 0) return true;
	m->keyboard_index = idx;
	m->hovered_index  = -1;
	close_submenu(m);
	invalidate(m);
	return true;
}

static bool menu_handle_move_key(FluxMenu *m, int from, int dir) {
	int idx = find_next(m, from, dir, m->parent == NULL);
	return menu_set_keyboard_index(m, idx);
}

static bool menu_handle_down_key(FluxMenu *m) {
	return menu_handle_move_key(m, m->keyboard_index >= 0 ? m->keyboard_index : -1, +1);
}

static bool menu_handle_up_key(FluxMenu *m) {
	return menu_handle_move_key(m, m->keyboard_index >= 0 ? m->keyboard_index : m->item_count, -1);
}

static bool menu_handle_home_key(FluxMenu *m) { return menu_set_keyboard_index(m, first_enabled(m)); }

static bool menu_handle_end_key(FluxMenu *m) { return menu_set_keyboard_index(m, last_enabled(m)); }

static bool menu_handle_activate_key(FluxMenu *m) {
	if (m->keyboard_index >= 0) activate_item(m, m->keyboard_index);
	return true;
}

static bool menu_handle_escape_key(FluxMenu *m) {
	flux_menu_dismissall(m);
	return true;
}

static bool menu_handle_right_key(FluxMenu *m) {
	int idx = m->keyboard_index;
	if (idx < 0 || idx >= m->item_count) return true;

	StoredItem *it = &m->items [idx];
	if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu) open_submenu(m, idx, true);
	return true;
}

static bool menu_handle_left_key(FluxMenu *m) {
	if (!m->parent) return true;

	FluxMenu          *parent      = m->parent;
	int                pidx        = m->parent_item_index;
	FluxPopupDismissFn dismiss     = m->dismiss_cb;
	void              *dismiss_ctx = m->dismiss_ctx;
	_dismiss_internal(m, false);
	parent->keyboard_index = pidx;
	parent->hovered_index  = -1;
	parent->pressed_index  = -1;
	invalidate(parent);
	refresh_active_leaf(root_of(parent));
	if (dismiss) dismiss(dismiss_ctx);
	return true;
}

bool flux_menu_onkey(FluxMenu *m, FluxKey key) {
	if (!m || !flux_popup_visible(m->popup)) return false;

	static MenuKeyBinding const bindings [] = {
	  {FLUX_KEY_DOWN,   menu_handle_down_key    },
	  {FLUX_KEY_UP,     menu_handle_up_key      },
	  {FLUX_KEY_HOME,   menu_handle_home_key    },
	  {FLUX_KEY_END,    menu_handle_end_key     },
	  {FLUX_KEY_ENTER,  menu_handle_activate_key},
	  {FLUX_KEY_SPACE,  menu_handle_activate_key},
	  {FLUX_KEY_ESCAPE, menu_handle_escape_key  },
	  {FLUX_KEY_RIGHT,  menu_handle_right_key   },
	  {FLUX_KEY_LEFT,   menu_handle_left_key    },
	};

	for (size_t i = 0; i < sizeof(bindings) / sizeof(bindings [0]); i++)
		if (bindings [i].key == key) return bindings [i].handler(m);
	return false;
}
