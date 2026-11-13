#include "controls/factory/flux_control_registry.h"
#include "controls/draw/flux_control_draw.h"
#include <stdlib.h>
#include "controls/behavior/flux_calendar.h"
#include "controls/behavior/flux_color_picker.h"

struct FluxControlRegistry {
	FluxControlRenderer renderers [XTK_CTRL_TYPE_COUNT];
};

FluxControlRegistry *flux_control_registry_create(void) {
	return ( FluxControlRegistry * ) calloc(1, sizeof(FluxControlRegistry));
}

void flux_control_registry_destroy(FluxControlRegistry *reg) { free(reg); }

void flux_registry_set_record(FluxControlRegistry *reg, FluxControlType type, FluxControlRecordFn record) {
	if (!reg || ( uint32_t ) type >= XTK_CTRL_TYPE_COUNT) return;
	reg->renderers [type].record = record;
}

void flux_registry_set_overlay(FluxControlRegistry *reg, FluxControlType type, FluxControlRecordFn record_overlay) {
	if (!reg || ( uint32_t ) type >= XTK_CTRL_TYPE_COUNT) return;
	reg->renderers [type].record_overlay = record_overlay;
}

FluxControlRenderer const *flux_control_registry_get(FluxControlRegistry const *reg, FluxControlType type) {
	if (!reg || ( uint32_t ) type >= XTK_CTRL_TYPE_COUNT) return NULL;
	return &reg->renderers [type];
}

static void register_recorded(FluxControlRegistry *reg, FluxControlType type, FluxControlRecordFn record) {
	flux_registry_set_record(reg, type, record);
}

static void register_recorded_overlay(
  FluxControlRegistry *reg, FluxControlType type, FluxControlRecordFn record, FluxControlRecordFn record_overlay
) {
	flux_registry_set_record(reg, type, record);
	flux_registry_set_overlay(reg, type, record_overlay);
}

void flux_register_builtins(FluxControlRegistry *reg) {
	if (!reg) return;
	register_recorded(reg, FLUX_CONTROL_CONTAINER, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_TEXT, flux_text_record);
	register_recorded(reg, FLUX_CONTROL_CARD, flux_card_record);
	register_recorded(reg, FLUX_CONTROL_DIVIDER, flux_divider_record);
	register_recorded(reg, FLUX_CONTROL_HYPERLINK, flux_link_record);
	register_recorded(reg, FLUX_CONTROL_LIST, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_TAB, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_CANVAS, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_RADIO_BUTTONS, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_SELECTOR_BAR, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_GRID_VIEW, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_ITEMS_REPEATER, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_ITEMS_VIEW, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_AUTO_SUGGEST, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_TREE_VIEW, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_SPLIT_VIEW, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_SPLIT_VIEW_CONTENT, flux_container_record);
	register_recorded(reg, FLUX_CONTROL_BUTTON, flux_button_record);
	register_recorded(reg, FLUX_CONTROL_TOGGLE_BUTTON, flux_toggle_record);
	register_recorded(reg, FLUX_CONTROL_REPEAT_BUTTON, flux_repeat_record);
	register_recorded(reg, FLUX_CONTROL_CHECKBOX, flux_checkbox_record);
	register_recorded(reg, FLUX_CONTROL_RADIO, flux_radio_record);
	register_recorded(reg, FLUX_CONTROL_SWITCH, flux_switch_record);
	register_recorded(reg, FLUX_CONTROL_SLIDER, flux_slider_record);
	register_recorded(reg, FLUX_CONTROL_PROGRESS, flux_progress_record);
	register_recorded_overlay(reg, FLUX_CONTROL_SCROLL, flux_scroll_record, flux_scroll_overlay_record);
	register_recorded(reg, FLUX_CONTROL_SCROLL_BAR, flux_scroll_bar_record);
	register_recorded_overlay(reg, FLUX_CONTROL_LOOPING_SELECTOR, NULL, flux_looping_record);
	register_recorded(reg, FLUX_CONTROL_LOOPING_ITEM, flux_looping_item_record);
	register_recorded(reg, FLUX_CONTROL_DATE_PICKER, flux_picker_record);
	register_recorded(reg, FLUX_CONTROL_TIME_PICKER, flux_picker_record);
	register_recorded(reg, FLUX_CONTROL_PICKER_FLYOUT, flux_pickercard_record);
	register_recorded(reg, FLUX_CONTROL_IMAGE, flux_image_record);
	register_recorded(reg, FLUX_CONTROL_TEXT_INPUT, flux_textbox_record);
	register_recorded(reg, FLUX_CONTROL_PASSWORD_BOX, flux_password_record);
	register_recorded(reg, FLUX_CONTROL_NUMBER_BOX, flux_number_record);
	register_recorded(reg, FLUX_CONTROL_DROPDOWN_BUTTON, flux_dropdown_record);
	register_recorded(reg, FLUX_CONTROL_SPLIT_BUTTON, flux_split_record);
	register_recorded(reg, FLUX_CONTROL_TOGGLE_SPLIT_BUTTON, flux_split_record);
	register_recorded(reg, FLUX_CONTROL_COMBO_BOX, flux_combo_record);
	register_recorded(reg, FLUX_CONTROL_INFO_BAR, flux_infobar_record);
	register_recorded(reg, FLUX_CONTROL_CONTENT_DIALOG, flux_dialog_record);
	register_recorded(reg, FLUX_CONTROL_DIALOG_CONTENT, flux_dialogbody_record);
	register_recorded(reg, FLUX_CONTROL_TEACHING_TIP, flux_teachtip_record);
	register_recorded(reg, FLUX_CONTROL_MENU_BAR_ITEM, flux_menuitem_record);
	register_recorded_overlay(reg, FLUX_CONTROL_NAV_VIEW, flux_nav_record, flux_nav_overlay_record);
	register_recorded(reg, FLUX_CONTROL_NAV_VIEW_ITEM, flux_navitem_record);
	register_recorded(reg, FLUX_CONTROL_TAB_VIEW, flux_tabs_record);
	register_recorded(reg, FLUX_CONTROL_LIST_ITEM, flux_listitem_record);
	register_recorded(reg, FLUX_CONTROL_LIST_BOX, flux_listbox_record);
	register_recorded_overlay(reg, FLUX_CONTROL_FLIP_VIEW, flux_flip_record, flux_flip_overlay_record);
	register_recorded(reg, FLUX_CONTROL_PIPS_PAGER, flux_pips_record);
	register_recorded(reg, FLUX_CONTROL_TREE_ITEM, flux_treeitem_record);
	register_recorded(reg, FLUX_CONTROL_EXPANDER_HEADER, flux_exphead_record);
	register_recorded(reg, FLUX_CONTROL_EXPANDER_CONTENT, flux_expbody_record);
	register_recorded(reg, FLUX_CONTROL_BREADCRUMB_ITEM, flux_crumb_record);
	register_recorded(reg, FLUX_CONTROL_SELECTOR_BAR_ITEM, flux_selectoritem_record);
	register_recorded(reg, FLUX_CONTROL_PAGER, flux_pager_record);
	register_recorded(reg, FLUX_CONTROL_SPLIT_VIEW_PANE, flux_splitpane_record);
	register_recorded(reg, FLUX_CONTROL_TITLE_BAR, flux_titlebar_record);
	register_recorded(reg, FLUX_CONTROL_RATING, flux_rating_record);
	register_recorded(reg, FLUX_CONTROL_INFO_BADGE, flux_badge_record);
	register_recorded(reg, FLUX_CONTROL_PROGRESS_RING, flux_progressring_record);
	register_recorded(reg, FLUX_CONTROL_CALENDAR_VIEW, flux_calendar_record);
	register_recorded(reg, FLUX_CONTROL_COLOR_PICKER, flux_colorpicker_record);
	register_recorded(reg, FLUX_CONTROL_PERSON_PICTURE, flux_avatar_record);
	register_recorded(reg, FLUX_CONTROL_TAB_VIEW_ITEM, flux_tabitem_record);
	register_recorded_overlay(reg, FLUX_CONTROL_REFRESH, flux_refresh_record, flux_refresh_overlay_record);
}
