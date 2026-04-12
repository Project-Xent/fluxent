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
    nd->state.enabled = enabled ? 1 : 0;
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
    nd->state.enabled = enabled ? 1 : 0;
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
    nd->state.enabled = enabled ? 1 : 0;
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
    nd->state.enabled = enabled ? 1 : 0;
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

/* ---- PasswordBox ---- */

void flux_password_set_content(FluxNodeStore *store, XentNodeId id, const char *content) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxPasswordBoxData *d = (FluxPasswordBoxData *)nd->component_data;
    d->content = content;
}

void flux_password_set_placeholder(FluxNodeStore *store, XentNodeId id, const char *placeholder) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxPasswordBoxData *d = (FluxPasswordBoxData *)nd->component_data;
    d->placeholder = placeholder;
}

void flux_password_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxPasswordBoxData *d = (FluxPasswordBoxData *)nd->component_data;
    d->enabled = enabled;
    nd->state.enabled = enabled ? 1 : 0;
}

void flux_password_set_reveal(FluxNodeStore *store, XentNodeId id, bool show_plain) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxPasswordBoxData *d = (FluxPasswordBoxData *)nd->component_data;
    d->show_plain = show_plain;
}

/* ---- NumberBox ---- */

void flux_numberbox_set_value(FluxNodeStore *store, XentNodeId id, double value) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxNumberBoxData *d = (FluxNumberBoxData *)nd->component_data;
    d->current_value = value;
}

void flux_numberbox_set_range(FluxNodeStore *store, XentNodeId id, double min_val, double max_val) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxNumberBoxData *d = (FluxNumberBoxData *)nd->component_data;
    d->min_value = min_val;
    d->max_value = max_val;
}

void flux_numberbox_set_step(FluxNodeStore *store, XentNodeId id, double step) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxNumberBoxData *d = (FluxNumberBoxData *)nd->component_data;
    d->step = step;
}

void flux_numberbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxNumberBoxData *d = (FluxNumberBoxData *)nd->component_data;
    d->enabled = enabled;
    nd->state.enabled = enabled ? 1 : 0;
}

void flux_numberbox_set_spin_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxNumberBoxData *d = (FluxNumberBoxData *)nd->component_data;
    d->spin_enabled = enabled;
}

/* ---- HyperlinkButton ---- */

void flux_hyperlink_set_label(FluxNodeStore *store, XentNodeId id, const char *label) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxHyperlinkData *d = (FluxHyperlinkData *)nd->component_data;
    d->label = label;
}

void flux_hyperlink_set_url(FluxNodeStore *store, XentNodeId id, const char *url) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxHyperlinkData *d = (FluxHyperlinkData *)nd->component_data;
    d->url = url;
}

void flux_hyperlink_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxHyperlinkData *d = (FluxHyperlinkData *)nd->component_data;
    d->enabled = enabled;
    nd->state.enabled = enabled ? 1 : 0;
}

/* ---- RepeatButton ---- */

void flux_repeat_button_set_label(FluxNodeStore *store, XentNodeId id, const char *label) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxRepeatButtonData *d = (FluxRepeatButtonData *)nd->component_data;
    d->label = label;
}

void flux_repeat_button_set_icon(FluxNodeStore *store, XentNodeId id, const char *icon_name) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxRepeatButtonData *d = (FluxRepeatButtonData *)nd->component_data;
    d->icon_name = icon_name;
}

void flux_repeat_button_set_style(FluxNodeStore *store, XentNodeId id, FluxButtonStyle style) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxRepeatButtonData *d = (FluxRepeatButtonData *)nd->component_data;
    d->style = style;
}

void flux_repeat_button_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxRepeatButtonData *d = (FluxRepeatButtonData *)nd->component_data;
    d->enabled = enabled;
    nd->state.enabled = enabled ? 1 : 0;
}

void flux_repeat_button_set_timing(FluxNodeStore *store, XentNodeId id, uint32_t delay_ms, uint32_t interval_ms) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxRepeatButtonData *d = (FluxRepeatButtonData *)nd->component_data;
    d->repeat_delay_ms = delay_ms;
    d->repeat_interval_ms = interval_ms;
}

/* ---- ProgressRing ---- */

void flux_progress_ring_set_value(FluxNodeStore *store, XentNodeId id, float value) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxProgressRingData *d = (FluxProgressRingData *)nd->component_data;
    d->value = value;
}

void flux_progress_ring_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxProgressRingData *d = (FluxProgressRingData *)nd->component_data;
    d->max_value = max_val;
}

void flux_progress_ring_set_indeterminate(FluxNodeStore *store, XentNodeId id, bool indeterminate) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxProgressRingData *d = (FluxProgressRingData *)nd->component_data;
    d->indeterminate = indeterminate;
}

/* ---- InfoBadge ---- */

void flux_info_badge_set_mode(FluxNodeStore *store, XentNodeId id, FluxInfoBadgeMode mode) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxInfoBadgeData *d = (FluxInfoBadgeData *)nd->component_data;
    d->mode = mode;
}

void flux_info_badge_set_value(FluxNodeStore *store, XentNodeId id, int32_t value) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxInfoBadgeData *d = (FluxInfoBadgeData *)nd->component_data;
    d->value = value;
}

void flux_info_badge_set_icon(FluxNodeStore *store, XentNodeId id, const char *icon_name) {
    if (!store) return;
    FluxNodeData *nd = flux_node_store_get(store, id);
    if (!nd || !nd->component_data) return;
    FluxInfoBadgeData *d = (FluxInfoBadgeData *)nd->component_data;
    d->icon_name = icon_name;
}