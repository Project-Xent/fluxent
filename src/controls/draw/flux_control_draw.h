#ifndef FLUX_CONTROL_DRAW_H
#define FLUX_CONTROL_DRAW_H

#include "render/flux_engine.h"

#include <xent/xent_display.h>

XentDl *flux_container_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_text_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_divider_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_card_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_link_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_button_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_toggle_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_repeat_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_checkbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_switch_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_radio_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_slider_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_progress_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_scroll_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_scroll_bar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

XentDl *flux_looping_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

XentDl *flux_looping_item_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_picker_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_pickercard_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_teachtip_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_dialog_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_flip_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_tabs_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_listbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_dropdown_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_combo_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_split_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_infobar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_menuitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_crumb_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_selectoritem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_pager_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_pips_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_exphead_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_expbody_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_dialogbody_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_listitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_treeitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_image_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_textbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_password_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_number_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

XentDl *flux_nav_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_navitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_splitpane_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_titlebar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_rating_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_badge_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_progressring_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_avatar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_refresh_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_refresh_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_scroll_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_nav_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
XentDl *flux_flip_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

#define FLUX_SPLIT_SECONDARY_W     35.0f
#define FLUX_SPLIT_DIVIDER_W       1.0f

#define FLUX_EXPANDER_MIN_HEIGHT   48.0f
#define FLUX_EXPANDER_HEADER_PAD   16.0f
#define FLUX_EXPANDER_CHEVRON_BOX  32.0f
#define FLUX_EXPANDER_CHEVRON_ML   20.0f
#define FLUX_EXPANDER_CHEVRON_MR   8.0f
#define FLUX_EXPANDER_CHEVRON_ZONE (FLUX_EXPANDER_CHEVRON_ML + FLUX_EXPANDER_CHEVRON_BOX + FLUX_EXPANDER_CHEVRON_MR)
#define FLUX_EXPANDER_CONTENT_PAD  16.0f

#define FLUX_MENU_BAR_HEIGHT       40.0f
#define FLUX_MENU_BAR_ITEM_PAD_H   10.0f
#define FLUX_MENU_BAR_ITEM_MARGIN  4.0f
#define FLUX_MENU_BAR_ITEM_H       (FLUX_MENU_BAR_HEIGHT - 2.0f * FLUX_MENU_BAR_ITEM_MARGIN)
#define FLUX_MENU_BAR_ITEM_FONT    14.0f

#define FLUX_NAV_PANE_EXPANDED     320.0f
#define FLUX_NAV_PANE_COMPACT      48.0f
#define FLUX_NAV_ICON_BOX          40.0f
#define FLUX_NAV_ICON_GLYPH        16.0f
#define FLUX_NAV_ITEM_HEIGHT       36.0f
#define FLUX_NAV_ITEM_GAP          4.0f
#define FLUX_NAV_ITEM_MARGIN_H     4.0f
#define FLUX_NAV_TOGGLE_W          40.0f
#define FLUX_NAV_TOGGLE_H          36.0f
#define FLUX_NAV_INDICATOR_W       3.0f
#define FLUX_NAV_INDICATOR_H       16.0f
#define FLUX_NAV_INDICATOR_R       1.5f
#define FLUX_NAV_LABEL_FONT        14.0f
#define FLUX_NAV_PANE_TOP_PAD      4.0f
#define FLUX_NAV_TOP_H             48.0f
#define FLUX_NAV_TOP_ITEM_H        36.0f
#define FLUX_NAV_TOP_ITEM_PAD_H    12.0f
#define FLUX_NAV_TOP_ICON_GAP      8.0f
#define FLUX_NAV_TOP_IND_W         16.0f
#define FLUX_NAV_TOP_IND_H         3.0f
#define FLUX_NAV_SEP_H             8.0f
#define FLUX_NAV_SEP_TOP_W         8.0f
#define FLUX_NAV_SEP_TOP_LINE_H    24.0f
#define FLUX_NAV_HEADER_H          40.0f
#define FLUX_NAV_HEADER_PAD        16.0f
#define FLUX_NAV_HEADER_FONT       12.0f

#define FLUX_TAB_STRIP_TOP_PAD     8.0f
#define FLUX_TAB_MIN_H             32.0f
#define FLUX_TAB_STRIP_H           (FLUX_TAB_STRIP_TOP_PAD + FLUX_TAB_MIN_H)
#define FLUX_TAB_MIN_W             100.0f
#define FLUX_TAB_MAX_W             240.0f
#define FLUX_TAB_CORNER            8.0f
#define FLUX_TAB_FLARE             4.0f
#define FLUX_TAB_PAD_L             8.0f
#define FLUX_TAB_PAD_R             4.0f
#define FLUX_TAB_PAD_R_NOCLOSE     8.0f
#define FLUX_TAB_ICON              16.0f
#define FLUX_TAB_ICON_GAP          10.0f
#define FLUX_TAB_FONT              12.0f
#define FLUX_TAB_CLOSE_W           32.0f
#define FLUX_TAB_CLOSE_H           24.0f
#define FLUX_TAB_CLOSE_MARGIN_L    4.0f
#define FLUX_TAB_CLOSE_FONT        12.0f
#define FLUX_TAB_ADD_W             32.0f
#define FLUX_TAB_ADD_H             24.0f
#define FLUX_TAB_ADD_PAD_L         3.0f
#define FLUX_TAB_ADD_PAD_B         3.0f
#define FLUX_TAB_SCROLL_W          32.0f
#define FLUX_TAB_SCROLL_H          24.0f
#define FLUX_TAB_SCROLL_FONT       8.0f
#define FLUX_TAB_SCROLL_PAD_OUT    8.0f
#define FLUX_TAB_SCROLL_PAD_IN     3.0f
#define FLUX_TAB_SCROLL_PAD_B      3.0f
#define FLUX_TAB_SCROLL_AMOUNT     50.0f
#define FLUX_TAB_SEP_MARGIN_V      8.0f
#define FLUX_TAB_COMPACT_W         (FLUX_TAB_PAD_L + FLUX_TAB_ICON + FLUX_TAB_PAD_R_NOCLOSE)

XentDl *flux_tabitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

#endif
