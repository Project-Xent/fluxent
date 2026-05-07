#include <stdbool.h>
#include "fluxent/flux_controls.h"
#include "controls/textbox/tb_internal.h"

#define FLUX_EXPECT_TYPE(nd, type) ((nd)->component_type == (type))
#define FLUX_EXPECT_TOGGLE(nd)                        \
	((nd)->component_type == XENT_CONTROL_CHECKBOX    \
	  || (nd)->component_type == XENT_CONTROL_RADIO   \
	  || (nd)->component_type == XENT_CONTROL_SWITCH)

static FluxNodeData *flux_component_node(FluxNodeStore *store, XentNodeId id) {
	if (!store) return NULL;
	FluxNodeData *nd = flux_node_store_get(store, id);
	if (!nd || !nd->component_data) return NULL;
	return nd;
}

#define FLUX_SETTER(prefix, DataType, field, ValueType, expect)                       \
	void prefix##_set_##field(FluxNodeStore *store, XentNodeId id, ValueType value) { \
		FluxNodeData *nd = flux_component_node(store, id);                            \
		if (!nd || !(expect)) return;                                                 \
		DataType *d = ( DataType * ) nd->component_data;                              \
		d->field    = value;                                                          \
	}

#define FLUX_SETTER_ENABLED(prefix, DataType, expect)                              \
	void prefix##_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) { \
		FluxNodeData *nd = flux_component_node(store, id);                         \
		if (!nd || !(expect)) return;                                              \
		DataType *d       = ( DataType * ) nd->component_data;                     \
		d->enabled        = enabled;                                               \
		nd->state.enabled = enabled ? 1 : 0;                                       \
	}

#define FLUX_SETTER_2(prefix, DataType, func, f1, f2, T1, T2, expect)             \
	void prefix##_set_##func(FluxNodeStore *store, XentNodeId id, T1 v1, T2 v2) { \
		FluxNodeData *nd = flux_component_node(store, id);                        \
		if (!nd || !(expect)) return;                                             \
		DataType *d = ( DataType * ) nd->component_data;                          \
		d->f1       = v1;                                                         \
		d->f2       = v2;                                                         \
	}

FLUX_SETTER(flux_button, FluxButtonData, label, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_BUTTON))
FLUX_SETTER(flux_button, FluxButtonData, icon_name, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_BUTTON))
FLUX_SETTER(flux_button, FluxButtonData, style, FluxButtonStyle, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_BUTTON))
FLUX_SETTER_ENABLED(flux_button, FluxButtonData, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_BUTTON))

void flux_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_button_set_icon_name(store, id, icon_name);
}

FLUX_SETTER(flux_checkbox, FluxCheckboxData, label, char const *, FLUX_EXPECT_TOGGLE(nd))
FLUX_SETTER(flux_checkbox, FluxCheckboxData, state, FluxCheckState, FLUX_EXPECT_TOGGLE(nd))
FLUX_SETTER_ENABLED(flux_checkbox, FluxCheckboxData, FLUX_EXPECT_TOGGLE(nd))

FLUX_SETTER(flux_slider, FluxSliderData, current_value, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_SLIDER))
FLUX_SETTER_2(
  flux_slider, FluxSliderData, range, min_value, max_value, float, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_SLIDER)
)
FLUX_SETTER_ENABLED(flux_slider, FluxSliderData, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_SLIDER))

void flux_slider_set_value(FluxNodeStore *store, XentNodeId id, float value) {
	flux_slider_set_current_value(store, id, value);
}

FLUX_SETTER(flux_text, FluxTextData, content, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_TEXT))
FLUX_SETTER(flux_text, FluxTextData, font_size, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_TEXT))
FLUX_SETTER(flux_text, FluxTextData, text_color, FluxColor, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_TEXT))
FLUX_SETTER(flux_text, FluxTextData, alignment, FluxTextAlign, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_TEXT))
FLUX_SETTER(flux_text, FluxTextData, font_weight, FluxFontWeight, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_TEXT))

void flux_text_set_color(FluxNodeStore *store, XentNodeId id, FluxColor color) {
	flux_text_set_text_color(store, id, color);
}

void flux_text_set_weight(FluxNodeStore *store, XentNodeId id, FluxFontWeight weight) {
	flux_text_set_font_weight(store, id, weight);
}

static FluxTextBoxInputData *flux_text_input_data(FluxNodeStore *store, XentNodeId id, XentControlType type) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || nd->component_type != type) return NULL;
	return ( FluxTextBoxInputData * ) nd->component_data;
}

static void flux_text_input_set_enabled(FluxNodeStore *store, XentNodeId id, XentControlType type, bool enabled) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || nd->component_type != type) return;
	FluxTextBoxData *tb = ( FluxTextBoxData * ) nd->component_data;
	tb->enabled         = enabled;
	nd->state.enabled   = enabled ? 1 : 0;
}

void flux_textbox_set_content(FluxNodeStore *store, XentNodeId id, char const *content) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, XENT_CONTROL_TEXT_INPUT);
	if (tb) tb_replace_text(tb, content);
}

void flux_textbox_set_placeholder(FluxNodeStore *store, XentNodeId id, char const *placeholder) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, XENT_CONTROL_TEXT_INPUT);
	if (tb) tb->base.placeholder = placeholder;
}

void flux_textbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
	flux_text_input_set_enabled(store, id, XENT_CONTROL_TEXT_INPUT, enabled);
}

FLUX_SETTER(flux_progress, FluxProgressData, value, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_PROGRESS))
FLUX_SETTER(flux_progress, FluxProgressData, max_value, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_PROGRESS))
FLUX_SETTER(flux_progress, FluxProgressData, indeterminate, bool, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_PROGRESS))

void flux_progress_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
	flux_progress_set_max_value(store, id, max_val);
}

FLUX_SETTER_2(
  flux_scroll, FluxScrollData, offset, scroll_x, scroll_y, float, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_SCROLL)
)

static FluxNodeData *flux_node_for_visual_setter(FluxNodeStore *store, XentNodeId id) {
	if (!store) return NULL;
	FluxNodeData *nd = flux_node_store_get(store, id);
	return nd;
}

void flux_node_set_background(FluxNodeStore *store, XentNodeId id, FluxColor color) {
	FluxNodeData *nd = flux_node_for_visual_setter(store, id);
	if (!nd) return;
	nd->visuals.background = color;
}

void flux_node_set_border(FluxNodeStore *store, XentNodeId id, FluxColor color, float width) {
	FluxNodeData *nd = flux_node_for_visual_setter(store, id);
	if (!nd) return;
	nd->visuals.border_color = color;
	nd->visuals.border_width = width;
}

void flux_node_set_corner_radius(FluxNodeStore *store, XentNodeId id, float radius) {
	FluxNodeData *nd = flux_node_for_visual_setter(store, id);
	if (!nd) return;
	nd->visuals.corner_radius = radius;
}

void flux_node_set_opacity(FluxNodeStore *store, XentNodeId id, float opacity) {
	FluxNodeData *nd = flux_node_for_visual_setter(store, id);
	if (!nd) return;
	nd->visuals.opacity = opacity;
}

void flux_node_set_tooltip(FluxNodeStore *store, XentNodeId id, char const *text) {
	FluxNodeData *nd = flux_node_for_visual_setter(store, id);
	if (!nd) return;
	nd->tooltip_text = text;
}

void flux_password_set_content(FluxNodeStore *store, XentNodeId id, char const *content) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, XENT_CONTROL_PASSWORD_BOX);
	if (tb) tb_replace_text(tb, content);
}

void flux_password_set_placeholder(FluxNodeStore *store, XentNodeId id, char const *placeholder) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, XENT_CONTROL_PASSWORD_BOX);
	if (tb) tb->base.placeholder = placeholder;
}

void flux_password_set_show_plain(FluxNodeStore *store, XentNodeId id, bool show_plain) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, XENT_CONTROL_PASSWORD_BOX);
	if (tb) tb->password_show_plain = show_plain;
}

void flux_password_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
	flux_text_input_set_enabled(store, id, XENT_CONTROL_PASSWORD_BOX, enabled);
}

void flux_password_set_reveal(FluxNodeStore *store, XentNodeId id, bool show_plain) {
	flux_password_set_show_plain(store, id, show_plain);
}

static FluxTextBoxInputData *flux_numberbox_data(FluxNodeStore *store, XentNodeId id) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, XENT_CONTROL_NUMBER_BOX);
	if (!tb || !tb->nb) return NULL;
	return tb;
}

void flux_numberbox_set_current_value(FluxNodeStore *store, XentNodeId id, double value) {
	FluxTextBoxInputData *tb = flux_numberbox_data(store, id);
	if (tb) nb_set_value(tb, value);
}

void flux_numberbox_set_range(FluxNodeStore *store, XentNodeId id, double min_val, double max_val) {
	FluxTextBoxInputData *tb = flux_numberbox_data(store, id);
	if (!tb) return;
	tb->nb->minimum = min_val;
	tb->nb->maximum = max_val;
	nb_coerce_value(tb);
	nb_update_text_to_value(tb);
	nb_sync_semantics(tb);
}

void flux_numberbox_set_step(FluxNodeStore *store, XentNodeId id, double step) {
	FluxTextBoxInputData *tb = flux_numberbox_data(store, id);
	if (!tb) return;
	tb->nb->small_change = step;
	tb->nb->large_change = step * 10.0;
}

void flux_numberbox_set_spin_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
	FluxTextBoxInputData *tb = flux_numberbox_data(store, id);
	if (!tb) return;
	tb->nb->spin_placement = enabled ? FLUX_NB_SPIN_INLINE : FLUX_NB_SPIN_HIDDEN;
	xent_set_semantic_expanded(tb->ctx, tb->node, enabled);
}

void flux_numberbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
	flux_text_input_set_enabled(store, id, XENT_CONTROL_NUMBER_BOX, enabled);
}

void flux_numberbox_set_value(FluxNodeStore *store, XentNodeId id, double value) {
	flux_numberbox_set_current_value(store, id, value);
}

FLUX_SETTER(flux_hyperlink, FluxHyperlinkData, label, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_HYPERLINK))
FLUX_SETTER(flux_hyperlink, FluxHyperlinkData, url, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_HYPERLINK))
FLUX_SETTER_ENABLED(flux_hyperlink, FluxHyperlinkData, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_HYPERLINK))

FLUX_SETTER(
  flux_repeat_button, FluxRepeatButtonData, label, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_REPEAT_BUTTON)
)
FLUX_SETTER(
  flux_repeat_button, FluxRepeatButtonData, icon_name, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_REPEAT_BUTTON)
)
FLUX_SETTER(
  flux_repeat_button, FluxRepeatButtonData, style, FluxButtonStyle, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_REPEAT_BUTTON)
)
FLUX_SETTER_2(
  flux_repeat_button, FluxRepeatButtonData, timing, repeat_delay_ms, repeat_interval_ms, uint32_t, uint32_t,
  FLUX_EXPECT_TYPE(nd, XENT_CONTROL_REPEAT_BUTTON)
)
FLUX_SETTER_ENABLED(flux_repeat_button, FluxRepeatButtonData, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_REPEAT_BUTTON))

void flux_repeat_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_repeat_button_set_icon_name(store, id, icon_name);
}

FLUX_SETTER(flux_progress_ring, FluxProgressRingData, value, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_PROGRESS_RING))
FLUX_SETTER(
  flux_progress_ring, FluxProgressRingData, max_value, float, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_PROGRESS_RING)
)
FLUX_SETTER(
  flux_progress_ring, FluxProgressRingData, indeterminate, bool, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_PROGRESS_RING)
)

void flux_progress_ring_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
	flux_progress_ring_set_max_value(store, id, max_val);
}

FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, mode, FluxInfoBadgeMode, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_INFO_BADGE))
FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, value, int32_t, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_INFO_BADGE))
FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, icon_name, char const *, FLUX_EXPECT_TYPE(nd, XENT_CONTROL_INFO_BADGE))

void flux_info_badge_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_info_badge_set_icon_name(store, id, icon_name);
}
