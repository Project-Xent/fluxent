/**
 * @file fx_el_compound.c
 * @brief FX element builders for compound controls: combo box, number box,
 * dropdown/split button, menu bar, tab view, content dialog, nav view,
 * and stack-based containers (column, row, card, scroll, expander).
 */
#include "fx_internal.h"

#include <string.h>

FluxEl *fx_combo_box(FxUi *ui, char const *placeholder, FxComboDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_COMBO_BOX, placeholder);
	if (!el) return NULL;
	if (desc.count < 0) desc.count = 0;
	char const **items
	  = ( char const ** ) fx_arena_alloc(fx_ui_arena(ui), sizeof(char *) * ( size_t ) (desc.count ? desc.count : 1));
	if (!items) return NULL;
	for (int i = 0; i < desc.count; i++) items [i] = fx_strdup(ui, desc.items [i]);
	desc.items = items;
	el->combo  = desc;
	return el;
}

FluxEl *fx_number_box(FxUi *ui, FxNumberDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_NUMBER_BOX, NULL);
	if (!el) return NULL;
	if (desc.max == 0.0 && desc.min == 0.0) desc.max = 100.0;
	if (desc.step == 0.0) desc.step = 1.0;
	el->number = desc;
	return el;
}

FluxEl *fx_dropdown_button(FxUi *ui, char const *label, FxDropdownDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_DROPDOWN_BUTTON, label);
	if (!el) return NULL;
	desc.icon = fx_strdup(ui, desc.icon);
	if (desc.item_count < 0) desc.item_count = 0;
	FxMenuItemDesc *items = ( FxMenuItemDesc * ) fx_arena_alloc(
	  fx_ui_arena(ui), sizeof(FxMenuItemDesc) * ( size_t ) (desc.item_count ? desc.item_count : 1)
	);
	if (!items) return NULL;
	for (int i = 0; i < desc.item_count; i++) {
		items [i]       = desc.items [i];
		items [i].label = fx_strdup(ui, items [i].label);
		items [i].icon  = fx_strdup(ui, items [i].icon);
	}
	desc.items   = items;
	el->dropdown = desc;
	return el;
}

/* Copy a declarative menu-item array into the frame arena. */
FxMenuItemDesc const *fx_menu_items_dup(FxUi *ui, FxMenuItemDesc const *items, int count) {
	FxMenuItemDesc *copy
	  = ( FxMenuItemDesc * ) fx_arena_alloc(fx_ui_arena(ui), sizeof(FxMenuItemDesc) * ( size_t ) (count ? count : 1));
	if (!copy) return NULL;
	for (int i = 0; i < count; i++) {
		copy [i]       = items [i];
		copy [i].label = fx_strdup(ui, copy [i].label);
		copy [i].icon  = fx_strdup(ui, copy [i].icon);
	}
	return copy;
}

FluxEl *fx_split_button(FxUi *ui, char const *label, FxSplitDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_SPLIT_BUTTON, label);
	if (!el) return NULL;
	desc.icon = fx_strdup(ui, desc.icon);
	if (desc.item_count < 0) desc.item_count = 0;
	desc.items = fx_menu_items_dup(ui, desc.items, desc.item_count);
	if (desc.item_count > 0 && !desc.items) return NULL;
	el->split = desc;
	return el;
}

FluxEl *fx_menu_bar(FxUi *ui, FxMenuBarDesc desc) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_MENU_BAR, NULL);
	if (!el) return NULL;
	if (desc.menu_count < 0) desc.menu_count = 0;
	FxMenuDesc *menus
	  = ( FxMenuDesc * ) fx_arena_alloc(fx_ui_arena(ui), sizeof(FxMenuDesc) * ( size_t ) (desc.menu_count ? desc.menu_count : 1));
	if (!menus) return NULL;
	for (int i = 0; i < desc.menu_count; i++) {
		menus [i]       = desc.menus [i];
		menus [i].title = fx_strdup(ui, menus [i].title);
		if (menus [i].item_count < 0) menus [i].item_count = 0;
		menus [i].items = fx_menu_items_dup(ui, menus [i].items, menus [i].item_count);
	}
	desc.menus   = menus;
	el->menu_bar = desc;
	return el;
}

FluxEl *fx_tab_view(FxUi *ui, FxTabViewDesc desc, FluxEl *children []) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_TAB_VIEW, NULL);
	if (!el) return NULL;
	if (desc.tab_count < 0) desc.tab_count = 0;
	FxTabDesc *tabs
	  = ( FxTabDesc * ) fx_arena_alloc(fx_ui_arena(ui), sizeof(FxTabDesc) * ( size_t ) (desc.tab_count ? desc.tab_count : 1));
	if (!tabs) return NULL;
	for (int i = 0; i < desc.tab_count; i++) {
		tabs [i]       = desc.tabs [i];
		tabs [i].icon  = fx_strdup(ui, tabs [i].icon);
		tabs [i].label = fx_strdup(ui, tabs [i].label);
	}
	desc.tabs    = tabs;
	el->tab_view = desc;
	fx_el_adopt(ui, el, children);
	return el;
}

FluxEl *fx_content_dialog(FxUi *ui, char const *title, FxDialogDesc desc, FluxEl *children []) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_CONTENT_DIALOG, title);
	if (!el) return NULL;
	desc.primary   = fx_strdup(ui, desc.primary);
	desc.secondary = fx_strdup(ui, desc.secondary);
	desc.close     = fx_strdup(ui, desc.close);
	if (desc.width <= 0.0f) desc.width = 400.0f;
	if (desc.content_height <= 0.0f) desc.content_height = 120.0f;
	el->dialog = desc;
	fx_el_adopt(ui, el, children);
	return el;
}

static FluxEl *fx_stack(FxUi *ui, FluxControlType type, bool row, FxStackDesc desc, FluxEl *children []) {
	FluxEl *el = fx_el_new(ui, type, NULL);
	if (!el) return NULL;
	el->stack_row = row;
	el->stack     = desc;
	fx_el_adopt(ui, el, children);
	return el;
}

FluxEl *fx_column(FxUi *ui, FxStackDesc desc, FluxEl *children []) {
	return fx_stack(ui, FLUX_CONTROL_CONTAINER, false, desc, children);
}

FluxEl *fx_row(FxUi *ui, FxStackDesc desc, FluxEl *children []) {
	return fx_stack(ui, FLUX_CONTROL_CONTAINER, true, desc, children);
}

/* A zero desc means "default WinUI card chrome", not a zero-padded card. */
FluxEl *fx_card(FxUi *ui, FxStackDesc desc, FluxEl *children []) {
	XentInsets const none = {0};
	if (memcmp(&desc.padding, &none, sizeof(none)) == 0) desc.padding = (XentInsets) {16.0f, 16.0f, 16.0f, 16.0f};
	if (desc.gap == 0.0f) desc.gap = 8.0f;
	return fx_stack(ui, FLUX_CONTROL_CARD, false, desc, children);
}

FluxEl *fx_scroll(FxUi *ui, FluxEl *children []) {
	return fx_stack(ui, FLUX_CONTROL_SCROLL, false, (FxStackDesc) {.fill = true, .align = XENT_FLEX_ALIGN_STRETCH}, children);
}

FluxEl *fx_expander(FxUi *ui, char const *header, FxExpanderDesc desc, FluxEl *children []) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_EXPANDER, header);
	if (!el) return NULL;
	if (desc.width <= 0.0f) desc.width = 600.0f;
	if (desc.content_height <= 0.0f) desc.content_height = 120.0f;
	el->expander = desc;
	fx_el_adopt(ui, el, children);
	return el;
}

FluxEl *fx_nav_view(FxUi *ui, FxNavDesc desc, FluxEl *children []) {
	FluxEl *el = fx_el_new(ui, FLUX_CONTROL_NAV_VIEW, NULL);
	if (!el) return NULL;
	if (desc.item_count < 0) desc.item_count = 0;
	FxNavItemDesc *items = ( FxNavItemDesc * ) fx_arena_alloc(
	  fx_ui_arena(ui), sizeof(FxNavItemDesc) * ( size_t ) (desc.item_count ? desc.item_count : 1)
	);
	if (!items) return NULL;
	for (int i = 0; i < desc.item_count; i++) {
		items [i]       = desc.items [i];
		items [i].icon  = fx_strdup(ui, items [i].icon);
		items [i].label = fx_strdup(ui, items [i].label);
	}
	desc.items = items;
	el->nav    = desc;
	fx_el_adopt(ui, el, children);
	return el;
}
