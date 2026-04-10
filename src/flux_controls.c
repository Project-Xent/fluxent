#include <stdbool.h>
#include "fluxent/flux_controls.h"

/* ---- Button ---- */

void flux_button_set_label(FluxNodeStore *store, XentNodeId id, const char *label) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxButtonData *d = (FluxButtonData *)nd->component_data;
    d->label = label;
}

void flux_button_set_icon(FluxNodeStore *store, XentNodeId id, const char *icon_name) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxButtonData *d = (FluxButtonData *)nd->component_data;
    d->icon_name = icon_name;
}

void flux_button_set_style(FluxNodeStore *store, XentNodeId id, FluxButtonStyle style) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxButtonData *d = (FluxButtonData *)nd->component_data;
    d->style = style;
}

void flux_button_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxButtonData *d = (FluxButtonData *)nd->component_data;
    d->enabled = enabled;
}

/* ---- Checkbox ---- */

void flux_checkbox_set_label(FluxNodeStore *store, XentNodeId id, const char *label) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxCheckboxData *d = (FluxCheckboxData *)nd->component_data;
    d->label = label;
}

void flux_checkbox_set_state(FluxNodeStore *store, XentNodeId id, FluxCheckState state) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxCheckboxData *d = (FluxCheckboxData *)nd->component_data;
    d->state = state;
}

void flux_checkbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxCheckboxData *d = (FluxCheckboxData *)nd->component_data;
    d->enabled = enabled;
}

/* ---- Slider ---- */

void flux_slider_set_value(FluxNodeStore *store, XentNodeId id, float value) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxSliderData *d = (FluxSliderData *)nd->component_data;
    d->current_value = value;
}

void flux_slider_set_range(FluxNodeStore *store, XentNodeId id, float min_val, float max_val) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxSliderData *d = (FluxSliderData *)nd->component_data;
    d->min_value = min_val;
    d->max_value = max_val;
}

void flux_slider_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxSliderData *d = (FluxSliderData *)nd->component_data;
    d->enabled = enabled;
}

/* ---- Text ---- */

void flux_text_set_content(FluxNodeStore *store, XentNodeId id, const char *content) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextData *d = (FluxTextData *)nd->component_data;
    d->content = content;
}

void flux_text_set_font_size(FluxNodeStore *store, XentNodeId id, float size) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextData *d = (FluxTextData *)nd->component_data;
    d->font_size = size;
}

void flux_text_set_color(FluxNodeStore *store, XentNodeId id, FluxColor color) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextData *d = (FluxTextData *)nd->component_data;
    d->text_color = color;
}

void flux_text_set_alignment(FluxNodeStore *store, XentNodeId id, FluxTextAlign align) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextData *d = (FluxTextData *)nd->component_data;
    d->alignment = align;
}

void flux_text_set_weight(FluxNodeStore *store, XentNodeId id, FluxFontWeight weight) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextData *d = (FluxTextData *)nd->component_data;
    d->font_weight = weight;
}

/* ---- TextBox ---- */

void flux_textbox_set_content(FluxNodeStore *store, XentNodeId id, const char *content) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextBoxData *d = (FluxTextBoxData *)nd->component_data;
    d->content = content;
}

void flux_textbox_set_placeholder(FluxNodeStore *store, XentNodeId id, const char *placeholder) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextBoxData *d = (FluxTextBoxData *)nd->component_data;
    d->placeholder = placeholder;
}

void flux_textbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxTextBoxData *d = (FluxTextBoxData *)nd->component_data;
    d->enabled = enabled;
}

/* ---- Progress ---- */

void flux_progress_set_value(FluxNodeStore *store, XentNodeId id, float value) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxProgressData *d = (FluxProgressData *)nd->component_data;
    d->value = value;
}

void flux_progress_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxProgressData *d = (FluxProgressData *)nd->component_data;
    d->max_value = max_val;
}

void flux_progress_set_indeterminate(FluxNodeStore *store, XentNodeId id, bool indeterminate) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxProgressData *d = (FluxProgressData *)nd->component_data;
    d->indeterminate = indeterminate;
}

/* ---- Scroll ---- */

void flux_scroll_set_offset(FluxNodeStore *store, XentNodeId id, float x, float y) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxScrollData *d = (FluxScrollData *)nd->component_data;
    d->scroll_x = x;
    d->scroll_y = y;
}

/* ---- Common visual properties (any node) ---- */

void flux_node_set_background(FluxNodeStore *store, XentNodeId id, FluxColor color) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd) return;
    nd->visuals.background = color;
}

void flux_node_set_border(FluxNodeStore *store, XentNodeId id, FluxColor color, float width) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd) return;
    nd->visuals.border_color = color;
    nd->visuals.border_width = width;
}

void flux_node_set_corner_radius(FluxNodeStore *store, XentNodeId id, float radius) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd) return;
    nd->visuals.corner_radius = radius;
}

void flux_node_set_opacity(FluxNodeStore *store, XentNodeId id, float opacity) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd) return;
    nd->visuals.opacity = opacity;
}