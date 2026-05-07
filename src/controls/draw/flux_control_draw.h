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
