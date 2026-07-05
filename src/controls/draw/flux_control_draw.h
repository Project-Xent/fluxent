#ifndef FLUX_CONTROL_DRAW_H
#define FLUX_CONTROL_DRAW_H

#include "fluxent/flux_engine.h"

/** @brief Draw a container surface (background fill + border). */
void flux_draw_container(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a text block (snapshot text in the control's foreground). */
void flux_draw_text(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a button (state-driven fill chrome + centered label). */
void flux_draw_button(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a toggle button (checked/unchecked fill chrome + label). */
void flux_draw_toggle(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a checkbox (box + check/indeterminate glyph + label). */
void flux_draw_checkbox(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a radio button (ring + selected dot + label). */
void flux_draw_radio(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a toggle switch (track + knob + label). */
void flux_draw_switch(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a slider (track + filled portion + thumb). */
void flux_draw_slider(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a scroll viewport (background + clipped content area). */
void flux_draw_scroll(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw scroll bar overlays (thumbs over the content, overlay pass). */
void flux_draw_scroll_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds);

/** @brief Draw a progress bar (track + determinate/indeterminate fill). */
void flux_draw_progress(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a card (layer fill + rounded border). */
void flux_draw_card(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a divider (1px rule). */
void flux_draw_divider(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a hyperlink (accent-colored label by state). */
void flux_draw_hyperlink(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a progress ring (determinate/indeterminate arc). */
void flux_draw_progress_ring(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw an info badge (dot, or numeric/icon pill). */
void flux_draw_info_badge(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a drop-down button (button chrome + trailing chevron). */
void flux_draw_dropdown_button(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/* SplitButton zone geometry, shared by the renderer and pointer-down routing:
 * WinUI SplitButtonSecondaryButtonSize=35 plus a 1px divider. */
#define FLUX_SPLIT_SECONDARY_W 35.0f
#define FLUX_SPLIT_DIVIDER_W   1.0f

/** @brief Draw a split button (primary action zone + divider + secondary chevron zone). */
void flux_draw_split_button(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a combo box closed box (selected text + chevron). */
void flux_draw_combo_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/* Expander metrics (Expander_themeresources.xaml), shared by the renderers and
 * the factory/layout: header MinHeight 48, header padding 16,0,0,0; chevron 32x32
 * box, margin 20,0,8,0, glyph 12px. The header content cell is inset on the right
 * by the full chevron zone (box + left + right margin) so labels never collide. */
#define FLUX_EXPANDER_MIN_HEIGHT   48.0f
#define FLUX_EXPANDER_HEADER_PAD   16.0f
#define FLUX_EXPANDER_CHEVRON_BOX  32.0f
#define FLUX_EXPANDER_CHEVRON_ML   20.0f
#define FLUX_EXPANDER_CHEVRON_MR   8.0f
#define FLUX_EXPANDER_CHEVRON_ZONE (FLUX_EXPANDER_CHEVRON_ML + FLUX_EXPANDER_CHEVRON_BOX + FLUX_EXPANDER_CHEVRON_MR)
#define FLUX_EXPANDER_CONTENT_PAD  16.0f

/** @brief Draw the expander toggle header (background + border + chevron box). */
void flux_draw_expander_header(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw the expander content panel (secondary fill + bottom-rounded 1,0,1,1 border). */
void flux_draw_expander_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw an info bar (severity icon + title + message + close button). */
void flux_draw_info_bar(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a content dialog scrim (full-window dimming behind the card). */
void flux_draw_content_dialog(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw the dialog content region (LayerFillColorAlt fill + bottom separator hairline). */
void flux_draw_dialog_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw an image (decoded bitmap with stretch mode), or a placeholder. */
void flux_draw_image(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/* MenuBar metrics (MenuBar_themeresources.xaml): bar height 40, item button
 * padding 10,4,10,4, item margin 4 (so item box is bar height - 8 = 32 tall and
 * adjacent items sit 8px apart), ControlCornerRadius 4, BodyText font 14. */
#define FLUX_MENU_BAR_HEIGHT      40.0f
#define FLUX_MENU_BAR_ITEM_PAD_H  10.0f
#define FLUX_MENU_BAR_ITEM_MARGIN 4.0f
#define FLUX_MENU_BAR_ITEM_H      (FLUX_MENU_BAR_HEIGHT - 2.0f * FLUX_MENU_BAR_ITEM_MARGIN)
#define FLUX_MENU_BAR_ITEM_FONT   14.0f

/** @brief Draw a menu bar item (subtle-fill background by state + centered title). */
void flux_draw_menu_bar_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/* NavigationView metrics (NavigationView_themeresources.xaml). Pane lengths
 * 320/48; item box 36 tall with a 40px icon column and 16px glyph; 4px side
 * margins; pane-toggle 40x36 (GlobalNavButton); selection indicator 3x16 r2. */
#define FLUX_NAV_PANE_EXPANDED  320.0f
#define FLUX_NAV_PANE_COMPACT   48.0f
#define FLUX_NAV_ICON_BOX       40.0f
#define FLUX_NAV_ICON_GLYPH     16.0f
#define FLUX_NAV_ITEM_HEIGHT    36.0f
#define FLUX_NAV_ITEM_GAP       4.0f
#define FLUX_NAV_ITEM_MARGIN_H  4.0f
#define FLUX_NAV_TOGGLE_W       40.0f
#define FLUX_NAV_TOGGLE_H       36.0f
#define FLUX_NAV_INDICATOR_W    3.0f
#define FLUX_NAV_INDICATOR_H    16.0f
#define FLUX_NAV_INDICATOR_R    1.5f
#define FLUX_NAV_LABEL_FONT     14.0f
#define FLUX_NAV_PANE_TOP_PAD   4.0f
#define FLUX_NAV_TOP_H          48.0f /* NavigationViewTopPaneHeight */
#define FLUX_NAV_TOP_ITEM_H     36.0f
#define FLUX_NAV_TOP_ITEM_PAD_H 12.0f
#define FLUX_NAV_TOP_ICON_GAP   8.0f
#define FLUX_NAV_TOP_IND_W      16.0f /* horizontal pill: 16x3, bottom-centered */
#define FLUX_NAV_TOP_IND_H      3.0f
#define FLUX_NAV_SEP_H          8.0f  /* 1px rule, margins 0,3,0,4 */
#define FLUX_NAV_SEP_TOP_W      8.0f  /* Top: vertical rule, margins 3,0,4,0 */
#define FLUX_NAV_SEP_TOP_LINE_H 24.0f
#define FLUX_NAV_HEADER_H       40.0f /* InnerHeaderGrid height */
#define FLUX_NAV_HEADER_PAD     16.0f /* NavigationViewItemInnerHeaderMargin */
#define FLUX_NAV_HEADER_FONT    12.0f

/** @brief Draw the NavigationView pane background + right-edge separator. */
void flux_draw_nav_view(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw the NavigationView selection indicator (pane overlay pass). */
void flux_draw_nav_view_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds);

/** @brief Draw a NavigationView item (subtle-fill chrome + icon glyph + optional label). */
void flux_draw_nav_view_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/* TabView metrics (TabView_themeresources.xaml). Strip = 8px top pad + 32px tab;
 * tab MinWidth 100 / MaxWidth 240, top corners 8 (OverlayCornerRadius) with 4px
 * curving-out bottom flares on the selected tab (TabViewItem::UpdateTabGeometry);
 * header padding 8,3,4,3 (right 8 without close button), font 12, icon 16 (10px
 * trailing margin); close button 32x24 glyph E711 @12 (4px leading margin);
 * add 32x24 glyph E710 @12 (container pad 3,0,0,3); scroll buttons 32x24
 * glyphs EDD9/EDDA @8 (containers 8,0,3,3 / 3,0,8,3), 50px per click. */
#define FLUX_TAB_STRIP_TOP_PAD  8.0f
#define FLUX_TAB_MIN_H          32.0f
#define FLUX_TAB_STRIP_H        (FLUX_TAB_STRIP_TOP_PAD + FLUX_TAB_MIN_H)
#define FLUX_TAB_MIN_W          100.0f
#define FLUX_TAB_MAX_W          240.0f
#define FLUX_TAB_CORNER         8.0f
#define FLUX_TAB_FLARE          4.0f
#define FLUX_TAB_PAD_L          8.0f
#define FLUX_TAB_PAD_R          4.0f
#define FLUX_TAB_PAD_R_NOCLOSE  8.0f
#define FLUX_TAB_ICON           16.0f
#define FLUX_TAB_ICON_GAP       10.0f
#define FLUX_TAB_FONT           12.0f
#define FLUX_TAB_CLOSE_W        32.0f
#define FLUX_TAB_CLOSE_H        24.0f
#define FLUX_TAB_CLOSE_MARGIN_L 4.0f
#define FLUX_TAB_CLOSE_FONT     12.0f
#define FLUX_TAB_ADD_W          32.0f
#define FLUX_TAB_ADD_H          24.0f
#define FLUX_TAB_ADD_PAD_L      3.0f
#define FLUX_TAB_ADD_PAD_B      3.0f
#define FLUX_TAB_SCROLL_W       32.0f
#define FLUX_TAB_SCROLL_H       24.0f
#define FLUX_TAB_SCROLL_FONT    8.0f
#define FLUX_TAB_SCROLL_PAD_OUT 8.0f
#define FLUX_TAB_SCROLL_PAD_IN  3.0f
#define FLUX_TAB_SCROLL_PAD_B   3.0f
#define FLUX_TAB_SCROLL_AMOUNT  50.0f
#define FLUX_TAB_SEP_MARGIN_V   8.0f
#define FLUX_TAB_COMPACT_W      (FLUX_TAB_PAD_L + FLUX_TAB_ICON + FLUX_TAB_PAD_R_NOCLOSE)

/** @brief Draw the TabView root (content fill + strip bottom border). */
void flux_draw_tab_view(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a tab strip item (tab chrome + icon + label + close, or the add button). */
void flux_draw_tab_view_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a BreadcrumbBar element (crumb label / ellipsis + separator chevron). */
void flux_draw_breadcrumb_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a SelectorBarItem (icon + text + accent underline pill). */
void flux_draw_selector_bar_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a RatingControl (outline + clipped fill star strip + caption). */
void flux_draw_rating(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a ListView-family cell (per-kind chrome: pill / fill / ring + checkbox). */
void flux_draw_list_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw the ItemsView ItemContainer chrome (accent border + inner ring + checkbox). */
void flux_draw_items_container(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw the ListBox surface (ChromeMediumLow fill). */
void flux_draw_list_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a TreeView row (indent + chevron + pill/checkbox + label). */
void flux_draw_tree_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw the FlipView surface (SystemListLow fill). */
void flux_draw_flip_view(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw FlipView nav buttons above the pages (overlay pass). */
void flux_draw_flip_view_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds);
void flux_draw_refresh(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state);
void flux_draw_refresh_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds);

/** @brief Draw a PipsPager (dots + nav carets). */
void flux_draw_pips_pager(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a text box (chrome + content + elevation border). */
void flux_draw_textbox(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw password box chrome and content. */
void flux_draw_password_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw number box chrome and content. */
void flux_draw_number_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw shared text box content. */
void flux_draw_textbox_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *text_area, FluxControlState const *state
);

/** @brief Draw the shared text box elevation border. */
void textbox_draw_elevation_border(FluxRenderContext const *rc, FluxRect const *bounds, float radius, bool focused);

/** @brief Draw the shared text box focus accent. */
void textbox_draw_focus_accent(FluxRenderContext const *rc, FluxRect const *bounds, float radius);

#endif
