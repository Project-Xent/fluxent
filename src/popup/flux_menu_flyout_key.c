#include "flux_menu_internal.h"
#include "fluxent/flux_input.h"

#include <windows.h>

typedef bool (*MenuKeyHandler)(FluxMenu *m);

typedef struct MenuKeyBinding {
	FluxKey        key;
	MenuKeyHandler handler;
} MenuKeyBinding;

static bool menu_set_keyboard_index(FluxMenu *m, int idx) {
	if (idx < 0) return true;
	m->hovered_index = -1;
	close_submenu(m);
	menu_focus_row(m, idx, true);
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

void menu_cancel_activation(FluxMenu *m) {
	if (!m->keyboard_pressed) return;
	m->keyboard_pressed = false;
	invalidate(m);
}

static bool menu_key_row_accepts_activation(FluxMenu const *m) {
	int index = m->keyboard_index;
	if (index < 0 || index >= m->item_count) return false;
	StoredItem const *item = &m->items [index];
	if (item->type == FLUX_MENU_ITEM_SEPARATOR || !item->enabled) return false;
	if (!m->input) return true;
	return index < m->row_count && flux_input_focus(m->input) == m->rows [index].node;
}

static bool menu_pointer_holds_row(FluxMenu const *m) {
	int index = m->keyboard_index;
	return index >= 0 && index < m->row_count && flux_input_pressed(m->input) == m->rows [index].node;
}

static void menu_activation_down(FluxMenu *m) {
	if (!menu_key_row_accepts_activation(m)) return;
	int index = m->keyboard_index;
	if (m->items [index].type == FLUX_MENU_ITEM_SUBMENU) {
		if (!flux_key_held(FLUX_KEY_ALT)) open_submenu(m, index, true);
		return;
	}
	m->keyboard_pressed = true;
	invalidate(m);
}

static void menu_activation_up(FluxMenu *m) {
	bool pressed = m->keyboard_pressed;
	menu_cancel_activation(m);
	if (pressed && menu_key_row_accepts_activation(m) && !menu_pointer_holds_row(m))
		activate_item(m, m->keyboard_index);
}

bool menu_key_event(FluxMenu *m, FluxKey key, bool down) {
	if (!m || !flux_popup_visible(m->popup)) return false;
	if (key == FLUX_KEY_ENTER || key == FLUX_KEY_SPACE) {
		if (down) menu_activation_down(m);
		else menu_activation_up(m);
		return true;
	}
	if (!down) return false;
	menu_cancel_activation(m);
	return flux_menu_onkey(m, key);
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
	parent->hovered_index = -1;
	menu_focus_row(parent, pidx, true);
	refresh_active_leaf(root_of(parent));
	if (dismiss) dismiss(dismiss_ctx);
	return true;
}

static bool menu_handle_escape_key(FluxMenu *m) {
	if (m->parent) return menu_handle_left_key(m);
	flux_menu_dismissall(m);
	return true;
}

static FluxKey menu_logical_key(FluxMenu *m, FluxKey key) {
	if (!flux_popup_rtl(m->popup)) return key;
	if (key == FLUX_KEY_LEFT) return FLUX_KEY_RIGHT;
	if (key == FLUX_KEY_RIGHT) return FLUX_KEY_LEFT;
	return key;
}

bool flux_menu_onkey(FluxMenu *m, FluxKey key) {
	if (!m || !flux_popup_visible(m->popup)) return false;
	if (key == FLUX_KEY_TAB) return true;
	key                                     = menu_logical_key(m, key);

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
