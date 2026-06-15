#include <stdbool.h>
#include "fluxent/flux_controls.h"
#include "controls/textbox/tb_internal.h"
#include "render/flux_fluent.h"
#include "runtime/flux_str.h"

#define FLUX_EXPECT_TYPE(nd, type) ((nd)->component_type == (type))
#define FLUX_EXPECT_TOGGLE(nd)                        \
	((nd)->component_type == FLUX_CONTROL_CHECKBOX    \
	  || (nd)->component_type == FLUX_CONTROL_RADIO   \
	  || (nd)->component_type == FLUX_CONTROL_SWITCH)
/* Button, DropDownButton and SplitButton all carry FluxButtonData. */
#define FLUX_EXPECT_BUTTON_FAMILY(nd)                         \
	((nd)->component_type == FLUX_CONTROL_BUTTON              \
	  || (nd)->component_type == FLUX_CONTROL_DROPDOWN_BUTTON \
	  || (nd)->component_type == FLUX_CONTROL_SPLIT_BUTTON)

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

/* String fields are owned by the control data: setters swap in a fresh copy. */
#define FLUX_SETTER_STR(prefix, DataType, field, expect)                                \
	void prefix##_set_##field(FluxNodeStore *store, XentNodeId id, char const *value) { \
		FluxNodeData *nd = flux_component_node(store, id);                              \
		if (!nd || !(expect)) return;                                                   \
		DataType *d = ( DataType * ) nd->component_data;                                \
		flux_str_replace(&d->field, value);                                             \
	}

/* Label-driven controls mirror the label into xent's text (intrinsic sizing
 * re-measures it) and the semantic label (UIA name). */
static void flux_label_sync(FluxNodeStore *store, XentNodeId id, char const *value) {
	XentContext *ctx = flux_node_store_context(store);
	xent_set_text(ctx, id, value ? value : "");
	xent_set_semantic_label(ctx, id, value ? value : "");
}

/* Enabled is a single source of truth: xent's semantic enabled. Rendering, focus,
 * input gating and UIA all read it; this setter is the only writer. */
#define FLUX_SETTER_ENABLED(prefix, expect)                                        \
	void prefix##_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) { \
		FluxNodeData *nd = flux_component_node(store, id);                         \
		if (!nd || !(expect)) return;                                              \
		xent_set_semantic_enabled(flux_node_store_context(store), id, enabled);    \
	}

#define FLUX_SETTER_2(prefix, DataType, func, f1, f2, T1, T2, expect)             \
	void prefix##_set_##func(FluxNodeStore *store, XentNodeId id, T1 v1, T2 v2) { \
		FluxNodeData *nd = flux_component_node(store, id);                        \
		if (!nd || !(expect)) return;                                             \
		DataType *d = ( DataType * ) nd->component_data;                          \
		d->f1       = v1;                                                         \
		d->f2       = v2;                                                         \
	}

void flux_button_set_label(FluxNodeStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_BUTTON_FAMILY(nd)) return;
	flux_str_replace(&(( FluxButtonData * ) nd->component_data)->label, label);
	flux_label_sync(store, id, label);
}

FLUX_SETTER_STR(flux_button, FluxButtonData, icon_name, FLUX_EXPECT_BUTTON_FAMILY(nd))
FLUX_SETTER(flux_button, FluxButtonData, style, FluxButtonStyle, FLUX_EXPECT_BUTTON_FAMILY(nd))
FLUX_SETTER_ENABLED(flux_button, FLUX_EXPECT_BUTTON_FAMILY(nd))

/* The node is text-measured from the label alone, but the draw places an icon
 * glyph + content gap before the label; reserve that in the left padding so
 * layout width matches what gets drawn. The right inset is the control's own
 * (plain buttons 11; dropdown buttons add the trailing chevron box + gap).
 * An icon-only plain button has no text to measure at all — its factory
 * fallback width of 0 reads as auto and inflates to the parent's free space —
 * so give it the WinUI icon-button footprint (callers may still override). */
static void flux_button_reserve_icon_pad(
  FluxNodeStore *store, XentNodeId id, FluxNodeData const *nd, char const *label, float font_size, char const *icon_name
) {
	XentContext *ctx      = flux_node_store_context(store);
	bool         dropdown = nd->component_type == FLUX_CONTROL_DROPDOWN_BUTTON;
	bool         split    = nd->component_type == FLUX_CONTROL_SPLIT_BUTTON;
	bool         labeled  = label && label [0];
	bool         iconed   = icon_name && icon_name [0];
	float        fs       = font_size > 0.0f ? font_size : 14.0f;
	float        reserve  = (iconed && labeled) ? fs * FLUX_BTN_ICON_SIZE_MUL + FLUX_BTN_CONTENT_GAP : 0.0f;
	/* Right inset is the control's trailing chrome that the label must clear:
	 * dropdown chevron box+gap (11+12+8), split divider+secondary zone (11+1+35),
	 * plain button just its own 11 padding. */
	float        pad_r    = dropdown ? 31.0f : split ? 47.0f : 11.0f;
	xent_set_padding(ctx, id, (XentInsets) {11.0f + reserve, 5.0f, pad_r, 6.0f});
	if (iconed && !labeled && !dropdown && !split) xent_set_size(ctx, id, (XentSize) {40.0f, 32.0f});
}

void flux_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_button_set_icon_name(store, id, icon_name);
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_BUTTON_FAMILY(nd)) return;
	FluxButtonData *bd = ( FluxButtonData * ) nd->component_data;
	flux_button_reserve_icon_pad(store, id, nd, bd->label, bd->font_size, icon_name);
}

void flux_checkbox_set_label(FluxNodeStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TOGGLE(nd)) return;
	flux_str_replace(&(( FluxCheckboxData * ) nd->component_data)->label, label);
	flux_label_sync(store, id, label);
}

FLUX_SETTER(flux_checkbox, FluxCheckboxData, state, FluxCheckState, FLUX_EXPECT_TOGGLE(nd))
FLUX_SETTER_ENABLED(flux_checkbox, FLUX_EXPECT_TOGGLE(nd))

FLUX_SETTER(flux_slider, FluxSliderData, step_frequency, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_SETTER(flux_slider, FluxSliderData, tick_frequency, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_SETTER(flux_slider, FluxSliderData, small_change, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_SETTER(flux_slider, FluxSliderData, large_change, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_SETTER(flux_slider, FluxSliderData, snaps_to, FluxSliderSnapsTo, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_SETTER(flux_slider, FluxSliderData, tick_placement, FluxTickPlacement, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_SETTER_2(
  flux_slider, FluxSliderData, range, min_value, max_value, float, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER)
)
FLUX_SETTER_ENABLED(flux_slider, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))

void flux_slider_set_step(FluxNodeStore *store, XentNodeId id, float step) {
	flux_slider_set_step_frequency(store, id, step);
}

/* WinUI Slider::OnValueChanged mirrors Value into IntermediateValue unless the
 * change came from input — the thumb is drawn from intermediate while held. */
void flux_slider_set_value(FluxNodeStore *store, XentNodeId id, float value) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER)) return;
	FluxSliderData *sd = ( FluxSliderData * ) nd->component_data;
	sd->current_value  = value;
	if (!nd->state.pressed) sd->intermediate_value = value;
}

/* Text nodes mirror content into xent for measurement (the factory seeds it
 * with xent_set_text), so a content change must re-mirror it. */
void flux_text_set_content(FluxNodeStore *store, XentNodeId id, char const *content) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT)) return;
	flux_str_replace(&(( FluxTextData * ) nd->component_data)->content, content);
	xent_set_text(flux_node_store_context(store), id, content ? content : "");
}

/* Font size also feeds xent's text measurement, so re-mirror it. */
void flux_text_set_font_size(FluxNodeStore *store, XentNodeId id, float size) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT)) return;
	(( FluxTextData * ) nd->component_data)->font_size = size;
	xent_set_font_size(flux_node_store_context(store), id, size);
}

/* Font weight also feeds xent's text measurement, so re-mirror it. */
void flux_text_set_font_weight(FluxNodeStore *store, XentNodeId id, FluxFontWeight weight) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT)) return;
	(( FluxTextData * ) nd->component_data)->font_weight = weight;
	xent_set_font_weight(flux_node_store_context(store), id, flux_font_weight_numeric(weight));
}

FLUX_SETTER(flux_text, FluxTextData, text_color, FluxColor, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT))
FLUX_SETTER(flux_text, FluxTextData, alignment, FluxTextAlign, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT))

void flux_text_set_color(FluxNodeStore *store, XentNodeId id, FluxColor color) {
	flux_text_set_text_color(store, id, color);
}

void flux_text_set_weight(FluxNodeStore *store, XentNodeId id, FluxFontWeight weight) {
	flux_text_set_font_weight(store, id, weight);
}

static FluxTextBoxInputData *flux_text_input_data(FluxNodeStore *store, XentNodeId id, FluxControlType type) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || nd->component_type != type) return NULL;
	return ( FluxTextBoxInputData * ) nd->component_data;
}

static void flux_text_input_set_enabled(FluxNodeStore *store, XentNodeId id, FluxControlType type, bool enabled) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || nd->component_type != type) return;
	xent_set_semantic_enabled(flux_node_store_context(store), id, enabled);
	/* Text editing has key/IME/selection gates that fire on the focused node outside the
	 * pointer path, so the box keeps a local flag to also reject input if disabled while
	 * focused. Kept in sync with the canonical semantic enabled here. */
	(( FluxTextBoxData * ) nd->component_data)->enabled = enabled;
}

void flux_textbox_set_content(FluxNodeStore *store, XentNodeId id, char const *content) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, FLUX_CONTROL_TEXT_INPUT);
	if (tb) tb_replace_text(tb, content);
}

void flux_textbox_set_placeholder(FluxNodeStore *store, XentNodeId id, char const *placeholder) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, FLUX_CONTROL_TEXT_INPUT);
	if (tb) flux_str_replace(&tb->base.placeholder, placeholder);
}

void flux_textbox_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
	flux_text_input_set_enabled(store, id, FLUX_CONTROL_TEXT_INPUT, enabled);
}

FLUX_SETTER(flux_progress, FluxProgressData, value, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS))
FLUX_SETTER(flux_progress, FluxProgressData, max_value, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS))
FLUX_SETTER(flux_progress, FluxProgressData, indeterminate, bool, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS))

void flux_progress_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
	flux_progress_set_max_value(store, id, max_val);
}

FLUX_SETTER_2(
  flux_scroll, FluxScrollData, offset, scroll_x, scroll_y, float, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SCROLL)
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
	flux_str_replace(&nd->tooltip_text, text);
}

void flux_password_set_content(FluxNodeStore *store, XentNodeId id, char const *content) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, FLUX_CONTROL_PASSWORD_BOX);
	if (tb) tb_replace_text(tb, content);
}

void flux_password_set_placeholder(FluxNodeStore *store, XentNodeId id, char const *placeholder) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, FLUX_CONTROL_PASSWORD_BOX);
	if (tb) flux_str_replace(&tb->base.placeholder, placeholder);
}

void flux_password_set_show_plain(FluxNodeStore *store, XentNodeId id, bool show_plain) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, FLUX_CONTROL_PASSWORD_BOX);
	if (tb) tb->password_show_plain = show_plain;
}

void flux_password_set_enabled(FluxNodeStore *store, XentNodeId id, bool enabled) {
	flux_text_input_set_enabled(store, id, FLUX_CONTROL_PASSWORD_BOX, enabled);
}

void flux_password_set_reveal(FluxNodeStore *store, XentNodeId id, bool show_plain) {
	flux_password_set_show_plain(store, id, show_plain);
}

static FluxTextBoxInputData *flux_numberbox_data(FluxNodeStore *store, XentNodeId id) {
	FluxTextBoxInputData *tb = flux_text_input_data(store, id, FLUX_CONTROL_NUMBER_BOX);
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
	flux_text_input_set_enabled(store, id, FLUX_CONTROL_NUMBER_BOX, enabled);
}

void flux_numberbox_set_value(FluxNodeStore *store, XentNodeId id, double value) {
	flux_numberbox_set_current_value(store, id, value);
}

void flux_hyperlink_set_label(FluxNodeStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_HYPERLINK)) return;
	flux_str_replace(&(( FluxHyperlinkData * ) nd->component_data)->label, label);
	flux_label_sync(store, id, label);
}

FLUX_SETTER_STR(flux_hyperlink, FluxHyperlinkData, url, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_HYPERLINK))
FLUX_SETTER_ENABLED(flux_hyperlink, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_HYPERLINK))

void flux_repeat_button_set_label(FluxNodeStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON)) return;
	flux_str_replace(&(( FluxRepeatButtonData * ) nd->component_data)->label, label);
	flux_label_sync(store, id, label);
}

FLUX_SETTER_STR(flux_repeat_button, FluxRepeatButtonData, icon_name, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON))
FLUX_SETTER(
  flux_repeat_button, FluxRepeatButtonData, style, FluxButtonStyle, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON)
)
FLUX_SETTER_2(
  flux_repeat_button, FluxRepeatButtonData, timing, repeat_delay_ms, repeat_interval_ms, uint32_t, uint32_t,
  FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON)
)
FLUX_SETTER_ENABLED(flux_repeat_button, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON))

void flux_repeat_button_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_repeat_button_set_icon_name(store, id, icon_name);
	FluxNodeData *nd = flux_component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON)) return;
	FluxRepeatButtonData *bd = ( FluxRepeatButtonData * ) nd->component_data;
	flux_button_reserve_icon_pad(store, id, nd, bd->label, bd->font_size, icon_name);
}

FLUX_SETTER(flux_progress_ring, FluxProgressRingData, value, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS_RING))
FLUX_SETTER(
  flux_progress_ring, FluxProgressRingData, max_value, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS_RING)
)
FLUX_SETTER(
  flux_progress_ring, FluxProgressRingData, indeterminate, bool, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS_RING)
)

void flux_progress_ring_set_max(FluxNodeStore *store, XentNodeId id, float max_val) {
	flux_progress_ring_set_max_value(store, id, max_val);
}

FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, mode, FluxInfoBadgeMode, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_INFO_BADGE))
FLUX_SETTER(flux_info_badge, FluxInfoBadgeData, value, int32_t, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_INFO_BADGE))
FLUX_SETTER_STR(flux_info_badge, FluxInfoBadgeData, icon_name, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_INFO_BADGE))

void flux_info_badge_set_icon(FluxNodeStore *store, XentNodeId id, char const *icon_name) {
	flux_info_badge_set_icon_name(store, id, icon_name);
}
