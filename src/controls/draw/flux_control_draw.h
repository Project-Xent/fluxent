#ifndef FLUX_CONTROL_DRAW_H
#define FLUX_CONTROL_DRAW_H

#include "render/flux_engine.h"

#include <xent/xent_display.h>

/** @brief Record container chrome into a node-local display list. */
XentDl *flux_container_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record text block into a node-local display list. */
XentDl *flux_text_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record divider into a node-local display list. */
XentDl *flux_divider_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record card chrome into a node-local display list. */
XentDl *flux_card_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record hyperlink into a node-local display list. */
XentDl *flux_link_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record button chrome + label. */
XentDl *flux_button_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record toggle button (checked uses accent chrome). */
XentDl *flux_toggle_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record repeat button (reuses button recorder). */
XentDl *flux_repeat_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record checkbox into a node-local display list. */
XentDl *flux_checkbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record toggle switch into a node-local display list. */
XentDl *flux_switch_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record radio button into a node-local display list. */
XentDl *flux_radio_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record slider into a node-local display list. */
XentDl *flux_slider_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record progress bar into a node-local display list. */
XentDl *flux_progress_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record scroll viewport background (overlay stays on draw). */
XentDl *flux_scroll_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record teaching-tip owner stub (container/empty). */
XentDl *flux_teachtip_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record content-dialog scrim. */
XentDl *flux_dialog_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record FlipView surface fill (nav overlay stays on draw). */
XentDl *flux_flip_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record TabView root fill + strip seam. */
XentDl *flux_tabs_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record ListBox surface. */
XentDl *flux_listbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record drop-down button. */
XentDl *flux_dropdown_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record combo box closed box. */
XentDl *flux_combo_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record split button (flat stroke, no elevation). */
XentDl *flux_split_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record info bar. */
XentDl *flux_infobar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record menu bar item. */
XentDl *flux_menuitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record breadcrumb item. */
XentDl *flux_crumb_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record selector bar item. */
XentDl *flux_selectoritem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record pager control. */
XentDl *flux_pager_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record pips pager dots. */
XentDl *flux_pips_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record expander header. */
XentDl *flux_exphead_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record expander content panel. */
XentDl *flux_expbody_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record dialog content region. */
XentDl *flux_dialogbody_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record list/grid cell chrome. */
XentDl *flux_listitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record tree view row. */
XentDl *flux_treeitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record image placeholder (no resource id → fill only). */
XentDl *flux_image_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record text box frame + optional text. */
XentDl *flux_textbox_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record password box (textbox frame). */
XentDl *flux_password_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record number box (textbox frame). */
XentDl *flux_number_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);


/** @brief Record NavigationView pane/root layer (overlay stays on draw). */
XentDl *flux_nav_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record NavigationView item row. */
XentDl *flux_navitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record SplitView pane wrapper. */
XentDl *flux_splitpane_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record title bar band. */
XentDl *flux_titlebar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record rating control stars. */
XentDl *flux_rating_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record info badge dot/pill. */
XentDl *flux_badge_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record progress ring (track ellipse + accent arc). */
XentDl *flux_progressring_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record PersonPicture avatar circle. */
XentDl *flux_avatar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record pull-to-refresh MAIN background. */
XentDl *flux_refresh_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record pull-to-refresh overlay glyph (per-frame, not node-owned). */
XentDl *flux_refresh_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record ScrollView scrollbar overlay (per-frame, not node-owned). */
XentDl *flux_scroll_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record NavigationView selection-indicator overlay. */
XentDl *flux_nav_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);
/** @brief Record FlipView nav-chip overlay. */
XentDl *flux_flip_overlay_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

/* SplitButton zone geometry, shared by record helpers and pointer routing. */
#define FLUX_SPLIT_SECONDARY_W     35.0f
#define FLUX_SPLIT_DIVIDER_W       1.0f

/* Expander metrics (Expander_themeresources.xaml), shared by record helpers. */
#define FLUX_EXPANDER_MIN_HEIGHT   48.0f
#define FLUX_EXPANDER_HEADER_PAD   16.0f
#define FLUX_EXPANDER_CHEVRON_BOX  32.0f
#define FLUX_EXPANDER_CHEVRON_ML   20.0f
#define FLUX_EXPANDER_CHEVRON_MR   8.0f
#define FLUX_EXPANDER_CHEVRON_ZONE (FLUX_EXPANDER_CHEVRON_ML + FLUX_EXPANDER_CHEVRON_BOX + FLUX_EXPANDER_CHEVRON_MR)
#define FLUX_EXPANDER_CONTENT_PAD  16.0f

/* MenuBar metrics (MenuBar_themeresources.xaml). */
#define FLUX_MENU_BAR_HEIGHT       40.0f
#define FLUX_MENU_BAR_ITEM_PAD_H   10.0f
#define FLUX_MENU_BAR_ITEM_MARGIN  4.0f
#define FLUX_MENU_BAR_ITEM_H       (FLUX_MENU_BAR_HEIGHT - 2.0f * FLUX_MENU_BAR_ITEM_MARGIN)
#define FLUX_MENU_BAR_ITEM_FONT    14.0f

/* NavigationView metrics (NavigationView_themeresources.xaml). */
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

/* TabView metrics (TabView_themeresources.xaml). */
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

/** @brief Tab strip item (MAIN record → FILL_SHAPE TabSelectedFrame). */
XentDl *flux_tabitem_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

#endif
