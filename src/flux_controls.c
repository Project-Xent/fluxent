/**
 * @file flux_controls.c
 * @brief Control property setters implemented via macros to reduce boilerplate.
 *
 * All setters follow the same pattern:
 *   1. Validate store
 *   2. Get FluxNodeData
 *   3. Cast component_data to the specific type
 *   4. Set the field
 *
 * X-macros expand each setter with minimal code duplication.
 */
#include <stdbool.h>
#include "fluxent/flux_controls.h"

/* ══════════════════════════════════════════════════════════════════════════
 * Setter macros — reduce 440 lines to ~200
 * ══════════════════════════════════════════════════════════════════════════ */

/* Simple field setter: flux_TYPE_set_FIELD(store, id, value) → data->FIELD = value */
#define FLUX_SETTER(prefix, DataType, field, ValueType)                               \
	void prefix##_set_##field(FluxNodeStore *store, XentNodeId id, ValueType value) { \
		if (!store) return;                                                           \
		FluxNodeData *nd = flux_node_store_get(store, id);                            \
		if (!nd || !nd->component_data) return;                                       \
		DataType *d = ( DataType * ) nd->component_data;                              \
		d->field    = value;                                                          \
	}

/* Enabled setter: also updates nd->state.enabled */
#define FLUX_SETTER_ENABLED(prefix, DataType)                                      \
	void prefix##_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) { \
		if (!store) return;                                                        \
		FluxNodeData *nd = flux_node_store_get(store, id);                         \
		if (!nd || !nd->component_data) return;                                    \
		DataType *d       = ( DataType * ) nd->component_data;                     \
		d->enabled        = enabled;                                               \
		nd->state.enabled = enabled ? 1 : 0;                                       \
	}

/* Two-field setter (e.g., range, offset) */
#define FLUX_SETTER_2(prefix, DataType, func, f1, f2, T1, T2)                     \
	void prefix##_set_##func(FluxNodeStore *store, XentNodeId id, T1 v1, T2 v2) { \
		if (!store) return;                                                       \
		FluxNodeData *nd = flux_node_store_get(store, id);                        \
		if (!nd || !nd->component_data) return;                                   \
		DataType *d = ( DataType * ) nd->component_data;                          \
		d->f1       = v1;                                                         \
		d->f2       = v2;                                                         \
	}

/* ══════════════════════════════════════════════════════════════════════════
 * Button
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_button, FluxButtonData, label, char const *)
FLUX_SETTER(flux_button, FluxButtonData, icon_name, char const *)
FLUX_SETTER(flux_button, FluxButtonData, style, FluxButtonStyle)
FLUX_SETTER_ENABLED(flux_button, FluxButtonData)

/* Alias for API compatibility */
void flux_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_button_set_icon_name(store, id, icon_name);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Checkbox
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_checkbox, FluxCheckboxData, label, char const *)
FLUX_SETTER(flux_checkbox, FluxCheckboxData, state, FluxCheckState)
FLUX_SETTER_ENABLED(flux_checkbox, FluxCheckboxData)

/* ══════════════════════════════════════════════════════════════════════════
 * Slider
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_slider, FluxSliderData, current_value, float)
FLUX_SETTER_2(flux_slider, FluxSliderData, range, min_value, max_value, float, float)
FLUX_SETTER_ENABLED(flux_slider, FluxSliderData)

/* Alias: flux_slider_set_value → flux_slider_set_current_value */
void flux_slider_set_value(FluxNodeStore *store, XentNodeId id, float value) {
	flux_slider_set_current_value(store, id, value);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Text
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_text, FluxTextData, content, char const *)
FLUX_SETTER(flux_text, FluxTextData, font_size, float)
FLUX_SETTER(flux_text, FluxTextData, text_color, FluxColor)
FLUX_SETTER(flux_text, FluxTextData, alignment, FluxTextAlign)
FLUX_SETTER(flux_text, FluxTextData, font_weight, FluxFontWeight)

/* Alias: flux_text_set_color → flux_text_set_text_color */
void flux_text_set_color(FluxNodeStore *store, XentNodeId id, FluxColor color) {
	flux_text_set_text_color(store, id, color);
}

/* Alias: flux_text_set_weight → flux_text_set_font_weight */
void flux_text_set_weight(FluxNodeStore *store, XentNodeId id, FluxFontWeight weight) {
	flux_text_set_font_weight(store, id, weight);
}

/* ══════════════════════════════════════════════════════════════════════════
 * TextBox
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_textbox, FluxTextBoxData, content, char const *)
FLUX_SETTER(flux_textbox, FluxTextBoxData, placeholder, char const *)
FLUX_SETTER_ENABLED(flux_textbox, FluxTextBoxData)

/* ══════════════════════════════════════════════════════════════════════════
 * Progress
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_progress, FluxProgressData, value, float)
FLUX_SETTER(flux_progress, FluxProgressData, max_value, float)
FLUX_SETTER(flux_progress, FluxProgressData, indeterminate, bool)

/* Alias: flux_progress_set_max → flux_progress_set_max_value */
void flux_progress_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
	flux_progress_set_max_value(store, id, max_val);
}

/* ══════════════════════════════════════════════════════════════════════════
 * Scroll
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER_2(flux_scroll, FluxScrollData, offset, scroll_x, scroll_y, float, float)

/* ══════════════════════════════════════════════════════════════════════════
 * Node visual properties (no component_data, use nd->visuals directly)
 * ══════════════════════════════════════════════════════════════════════════ */
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

void flux_node_set_tooltip(FluxNodeStore *store, XentNodeId id, char const *text) {
	if (!store) return;
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd) return;
	nd->tooltip_text = text;
}

/* ══════════════════════════════════════════════════════════════════════════
 * PasswordBox
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_password, FluxPasswordBoxData, content, char const *)
FLUX_SETTER(flux_password, FluxPasswordBoxData, placeholder, char const *)
FLUX_SETTER(flux_password, FluxPasswordBoxData, show_plain, bool)
FLUX_SETTER_ENABLED(flux_password, FluxPasswordBoxData)

/* Alias: flux_password_set_reveal → flux_password_set_show_plain */
void flux_password_set_reveal(FluxNodeStore *store, XentNodeId id, bool show_plain) {
	flux_password_set_show_plain(store, id, show_plain);
}

/* ══════════════════════════════════════════════════════════════════════════
 * NumberBox
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_numberbox, FluxNumberBoxData, current_value, double)
FLUX_SETTER_2(flux_numberbox, FluxNumberBoxData, range, min_value, max_value, double, double)
FLUX_SETTER(flux_numberbox, FluxNumberBoxData, step, double)
FLUX_SETTER(flux_numberbox, FluxNumberBoxData, spin_enabled, bool)
FLUX_SETTER_ENABLED(flux_numberbox, FluxNumberBoxData)

/* Alias: flux_numberbox_set_value → flux_numberbox_set_current_value */
void flux_numberbox_set_value(FluxNodeStore *store, XentNodeId id, double value) {
	flux_numberbox_set_current_value(store, id, value);
}

/* ══════════════════════════════════════════════════════════════════════════
 * HyperlinkButton
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_hyperlink, FluxHyperlinkData, label, char const *)
FLUX_SETTER(flux_hyperlink, FluxHyperlinkData, url, char const *)
FLUX_SETTER_ENABLED(flux_hyperlink, FluxHyperlinkData)

/* ══════════════════════════════════════════════════════════════════════════
 * RepeatButton
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_repeat_button, FluxRepeatButtonData, label, char const *)
FLUX_SETTER(flux_repeat_button, FluxRepeatButtonData, icon_name, char const *)
FLUX_SETTER(flux_repeat_button, FluxRepeatButtonData, style, FluxButtonStyle)
FLUX_SETTER_2(flux_repeat_button, FluxRepeatButtonData, timing, repeat_delay_ms, repeat_interval_ms, uint32_t, uint32_t)
FLUX_SETTER_ENABLED(flux_repeat_button, FluxRepeatButtonData)

/* Alias for API compatibility */
void flux_repeat_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_repeat_button_set_icon_name(store, id, icon_name);
}

/* ══════════════════════════════════════════════════════════════════════════
 * ProgressRing
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_progress_ring, FluxProgressRingData, value, float)
FLUX_SETTER(flux_progress_ring, FluxProgressRingData, max_value, float)
FLUX_SETTER(flux_progress_ring, FluxProgressRingData, indeterminate, bool)

/* Alias: flux_progress_ring_set_max → flux_progress_ring_set_max_value */
void flux_progress_ring_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
	flux_progress_ring_set_max_value(store, id, max_val);
}

/* ══════════════════════════════════════════════════════════════════════════
 * InfoBadge
 * ══════════════════════════════════════════════════════════════════════════ */
FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, mode, FluxInfoBadgeMode)
FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, value, int32_t)
FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, icon_name, char const *)

/* Alias for API compatibility */
void flux_info_badge_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_info_badge_set_icon_name(store, id, icon_name);
}
