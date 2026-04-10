#ifndef FLUX_CONTROLS_H
#define FLUX_CONTROLS_H

#include "flux_component_data.h"
#include "flux_node_store.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ---- Button ---- */
void flux_button_set_label(FluxNodeStore *store, XentNodeId id, const char *label);
void flux_button_set_icon(FluxNodeStore *store, XentNodeId id, const char *icon_name);
void flux_button_set_style(FluxNodeStore *store, XentNodeId id, FluxButtonStyle style);
void flux_button_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/* ---- Checkbox ---- */
void flux_checkbox_set_label(FluxNodeStore *store, XentNodeId id, const char *label);
void flux_checkbox_set_state(FluxNodeStore *store, XentNodeId id, FluxCheckState state);
void flux_checkbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/* ---- Slider ---- */
void flux_slider_set_value(FluxNodeStore *store, XentNodeId id, float value);
void flux_slider_set_range(FluxNodeStore *store, XentNodeId id, float min_val, float max_val);
void flux_slider_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/* ---- Text ---- */
void flux_text_set_content(FluxNodeStore *store, XentNodeId id, const char *content);
void flux_text_set_font_size(FluxNodeStore *store, XentNodeId id, float size);
void flux_text_set_color(FluxNodeStore *store, XentNodeId id, FluxColor color);
void flux_text_set_alignment(FluxNodeStore *store, XentNodeId id, FluxTextAlign align);
void flux_text_set_weight(FluxNodeStore *store, XentNodeId id, FluxFontWeight weight);

/* ---- TextBox ---- */
void flux_textbox_set_content(FluxNodeStore *store, XentNodeId id, const char *content);
void flux_textbox_set_placeholder(FluxNodeStore *store, XentNodeId id, const char *placeholder);
void flux_textbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled);

/* ---- Progress ---- */
void flux_progress_set_value(FluxNodeStore *store, XentNodeId id, float value);
void flux_progress_set_max(FluxNodeStore *store, XentNodeId id, float max_val);
void flux_progress_set_indeterminate(FluxNodeStore *store, XentNodeId id, bool indeterminate);

/* ---- Scroll ---- */
void flux_scroll_set_offset(FluxNodeStore *store, XentNodeId id, float x, float y);

/* ---- Common visual properties (any node) ---- */
void flux_node_set_background(FluxNodeStore *store, XentNodeId id, FluxColor color);
void flux_node_set_border(FluxNodeStore *store, XentNodeId id, FluxColor color, float width);
void flux_node_set_corner_radius(FluxNodeStore *store, XentNodeId id, float radius);
void flux_node_set_opacity(FluxNodeStore *store, XentNodeId id, float opacity);

#ifdef __cplusplus
}
#endif

#endif