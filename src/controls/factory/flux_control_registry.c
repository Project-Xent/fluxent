#include "fluxent/flux_control_registry.h"
#include "controls/draw/flux_control_draw.h"

#include <stdlib.h>

struct FluxControlRegistry {
	FluxControlRenderer renderers [FLUX_CONTROL_CUSTOM + 1];
};

FluxControlRegistry *flux_control_registry_create(void) {
	return ( FluxControlRegistry * ) calloc(1, sizeof(FluxControlRegistry));
}

void flux_control_registry_destroy(FluxControlRegistry *reg) { free(reg); }

void flux_control_registry_register(
  FluxControlRegistry *reg, FluxControlType type,
  void (*draw)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *),
  void (*draw_overlay)(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *)
) {
	if (!reg || ( uint32_t ) type > FLUX_CONTROL_CUSTOM) return;
	reg->renderers [type].draw         = draw;
	reg->renderers [type].draw_overlay = draw_overlay;
}

FluxControlRenderer const *flux_control_registry_get(FluxControlRegistry const *reg, FluxControlType type) {
	if (!reg || ( uint32_t ) type > FLUX_CONTROL_CUSTOM) return NULL;
	return &reg->renderers [type];
}

void flux_register_builtins(FluxControlRegistry *reg) {
	if (!reg) return;
	flux_control_registry_register(reg, FLUX_CONTROL_CONTAINER, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_TEXT, flux_draw_text, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_BUTTON, flux_draw_button, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_TOGGLE_BUTTON, flux_draw_toggle, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_CHECKBOX, flux_draw_checkbox, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_RADIO, flux_draw_radio, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_SWITCH, flux_draw_switch, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_SLIDER, flux_draw_slider, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_TEXT_INPUT, flux_draw_textbox, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_SCROLL, flux_draw_scroll, flux_draw_scroll_overlay);
	flux_control_registry_register(reg, FLUX_CONTROL_IMAGE, flux_draw_image, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_PROGRESS, flux_draw_progress, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_LIST, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_TAB, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_CARD, flux_draw_card, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_DIVIDER, flux_draw_divider, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_CANVAS, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_PASSWORD_BOX, flux_draw_password_box, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_NUMBER_BOX, flux_draw_number_box, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_HYPERLINK, flux_draw_hyperlink, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_REPEAT_BUTTON, flux_draw_button, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_PROGRESS_RING, flux_draw_progress_ring, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_INFO_BADGE, flux_draw_info_badge, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_DROPDOWN_BUTTON, flux_draw_dropdown_button, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_SPLIT_BUTTON, flux_draw_split_button, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_TOGGLE_SPLIT_BUTTON, flux_draw_split_button, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_RADIO_BUTTONS, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_RATING, flux_draw_rating, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_COMBO_BOX, flux_draw_combo_box, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_EXPANDER_HEADER, flux_draw_expander_header, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_EXPANDER_CONTENT, flux_draw_expander_content, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_INFO_BAR, flux_draw_info_bar, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_CONTENT_DIALOG, flux_draw_content_dialog, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_DIALOG_CONTENT, flux_draw_dialog_content, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_MENU_BAR_ITEM, flux_draw_menu_bar_item, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_NAV_VIEW, flux_draw_nav_view, flux_draw_nav_view_overlay);
	flux_control_registry_register(reg, FLUX_CONTROL_NAV_VIEW_ITEM, flux_draw_nav_view_item, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_TAB_VIEW, flux_draw_tab_view, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_TAB_VIEW_ITEM, flux_draw_tab_view_item, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_LIST_ITEM, flux_draw_list_item, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_LIST_BOX, flux_draw_list_box, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_GRID_VIEW, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_ITEMS_REPEATER, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_FLIP_VIEW, flux_draw_flip_view, flux_draw_flip_view_overlay);
	flux_control_registry_register(reg, FLUX_CONTROL_PIPS_PAGER, flux_draw_pips_pager, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_AUTO_SUGGEST, flux_draw_container, NULL);
	flux_control_registry_register(reg, FLUX_CONTROL_CUSTOM, flux_draw_container, NULL);
}
