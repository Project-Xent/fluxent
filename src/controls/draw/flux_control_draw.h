#ifndef FLUX_CONTROL_DRAW_H
#define FLUX_CONTROL_DRAW_H

#include "fluxent/flux_engine.h"

/** @brief Draw a generic container control. */
void flux_draw_container(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a text control. */
void flux_draw_text(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a button control. */
void flux_draw_button(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a toggle button control. */
void flux_draw_toggle(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a checkbox control. */
void flux_draw_checkbox(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a radio button control. */
void flux_draw_radio(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a switch control. */
void flux_draw_switch(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a slider control. */
void flux_draw_slider(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a scroll view. */
void flux_draw_scroll(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a scroll view overlay. */
void flux_draw_scroll_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds);

/** @brief Draw a progress bar. */
void flux_draw_progress(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a card control. */
void flux_draw_card(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a divider control. */
void flux_draw_divider(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a hyperlink control. */
void flux_draw_hyperlink(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw a progress ring. */
void flux_draw_progress_ring(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
);

/** @brief Draw an info badge. */
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

/** @brief Draw a text box. */
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
