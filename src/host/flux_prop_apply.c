#include <stdbool.h>
#include <math.h>
#include "host/flux_prop_apply.h"
#include "store/flux_node_data.h"
#include "controls/behavior/flux_text_select.h"
#include "controls/textbox/tb_internal.h"
#include "controls/factory/flux_factory.h"
#include "render/flux_fluent.h"
#include "runtime/flux_str.h"

#include <xent/xent.h>

#define FLUX_EXPECT_TYPE(nd, type) ((nd)->component_type == (type))
#define FLUX_EXPECT_TOGGLE(nd)                        \
	((nd)->component_type == FLUX_CONTROL_CHECKBOX    \
	  || (nd)->component_type == FLUX_CONTROL_RADIO   \
	  || (nd)->component_type == FLUX_CONTROL_SWITCH)
#define FLUX_EXPECT_BUTTON_FAMILY(nd)                              \
	((nd)->component_type == FLUX_CONTROL_BUTTON                   \
	  || (nd)->component_type == FLUX_CONTROL_DROPDOWN_BUTTON      \
	  || (nd)->component_type == FLUX_CONTROL_SPLIT_BUTTON         \
	  || (nd)->component_type == FLUX_CONTROL_TOGGLE_SPLIT_BUTTON)

static FluxNodeData *component_node(FluxStore *store, XentNodeId id) {
	if (!store) return NULL;
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || !nd->component_data) return NULL;
	return nd;
}

#define FLUX_PROP(ctrl, DataType, field, ValueType, expect)                             \
	void flux_prop_##ctrl##_##field(FluxStore *store, XentNodeId id, ValueType value) { \
		FluxNodeData *nd = component_node(store, id);                                   \
		if (!nd || !(expect)) return;                                                   \
		DataType *d = ( DataType * ) nd->component_data;                                \
		d->field    = value;                                                            \
	}

#define FLUX_PROP_STR(ctrl, DataType, field, expect)                                      \
	void flux_prop_##ctrl##_##field(FluxStore *store, XentNodeId id, char const *value) { \
		FluxNodeData *nd = component_node(store, id);                                     \
		if (!nd || !(expect)) return;                                                     \
		DataType *d = ( DataType * ) nd->component_data;                                  \
		flux_str_replace(&d->field, value);                                               \
	}

static void label_sync(FluxStore *store, XentNodeId id, char const *value) {
	XentCtx *ctx = flux_store_ctx(store);
	xent_settext(ctx, id, value ? value : "");
	xent_sem_setlabel(ctx, id, value ? value : "");
}

#define FLUX_PROP_ENABLED(ctrl, expect)                                              \
	void flux_prop_##ctrl##_enabled(FluxStore *store, XentNodeId id, bool enabled) { \
		FluxNodeData *nd = component_node(store, id);                                \
		if (!nd || !(expect)) return;                                                \
		xent_sem_setenabled(flux_store_ctx(store), id, enabled);                     \
	}

#define FLUX_PROP_2(ctrl, DataType, func, f1, f2, T1, T2, expect)                   \
	void flux_prop_##ctrl##_##func(FluxStore *store, XentNodeId id, T1 v1, T2 v2) { \
		FluxNodeData *nd = component_node(store, id);                               \
		if (!nd || !(expect)) return;                                               \
		DataType *d = ( DataType * ) nd->component_data;                            \
		d->f1       = v1;                                                           \
		d->f2       = v2;                                                           \
	}

static void button_reserve_icon_pad(
  FluxStore *store, XentNodeId id, FluxNodeData const *nd, char const *label, float font_size, char const *icon_name
) {
	XentCtx *ctx      = flux_store_ctx(store);
	bool     dropdown = nd->component_type == FLUX_CONTROL_DROPDOWN_BUTTON;
	bool     split
	  = nd->component_type == FLUX_CONTROL_SPLIT_BUTTON || nd->component_type == FLUX_CONTROL_TOGGLE_SPLIT_BUTTON;
	bool  labeled = label && label [0];
	bool  iconed  = icon_name && icon_name [0];
	float fs      = font_size > 0.0f ? font_size : 14.0f;
	float reserve = (iconed && labeled) ? fs * FLUX_BTN_ICON_SIZE_MUL + FLUX_BTN_CONTENT_GAP : 0.0f;
	float pad_r   = dropdown ? 31.0f : split ? 47.0f : 11.0f;
	xent_setp(ctx, id, (XentInsets) {11.0f + reserve, 5.0f, pad_r, 6.0f});
	if (iconed && !labeled && !dropdown && !split) xent_setsize(ctx, id, (XentSize) {40.0f, 32.0f});
}

void flux_prop_button_label(FluxStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_BUTTON_FAMILY(nd)) return;
	FluxButtonData *bd = ( FluxButtonData * ) nd->component_data;
	flux_str_replace(&bd->label, label);
	label_sync(store, id, label);

	XentCtx *ctx = flux_store_ctx(store);
	xent_setlinebreak(ctx, id, XENT_LINEBREAK_NOWRAP);
	xent_setfontsize(ctx, id, bd->font_size > 0.0f ? bd->font_size : 14.0f);
	xent_setsize(ctx, id, (XentSize) {NAN, NAN});
	xent_setminsize(ctx, id, (XentSize) {NAN, 32.0f});
	button_reserve_icon_pad(store, id, nd, bd->label, bd->font_size, bd->icon_name);
}

FLUX_PROP_STR(button, FluxButtonData, icon_name, FLUX_EXPECT_BUTTON_FAMILY(nd))
FLUX_PROP(button, FluxButtonData, style, FluxButtonStyle, FLUX_EXPECT_BUTTON_FAMILY(nd))
FLUX_PROP_ENABLED(button, FLUX_EXPECT_BUTTON_FAMILY(nd))

void flux_prop_button_icon(FluxStore *store, XentNodeId id, char const *icon_name) {
	flux_prop_button_icon_name(store, id, icon_name);
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_BUTTON_FAMILY(nd)) return;
	FluxButtonData *bd = ( FluxButtonData * ) nd->component_data;
	button_reserve_icon_pad(store, id, nd, bd->label, bd->font_size, icon_name);
}

void flux_prop_checkbox_label(FluxStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TOGGLE(nd)) return;
	flux_str_replace(&(( FluxCheckboxData * ) nd->component_data)->label, label);
	label_sync(store, id, label);

	if (nd->component_type == FLUX_CONTROL_SWITCH) return;
	XentCtx *ctx = flux_store_ctx(store);
	xent_setsize(ctx, id, (XentSize) {NAN, NAN});
	flux_toggle_applymetrics(ctx, id, nd->component_type, label);
}

void flux_prop_checkbox_state(FluxStore *store, XentNodeId id, FluxCheckState state) {
	FluxNodeData *nd = component_node(store, id);
	if (nd && FLUX_EXPECT_TOGGLE(nd)) flux_checkbox_setstate(( FluxCheckboxData * ) nd->component_data, state);
}
FLUX_PROP_ENABLED(checkbox, FLUX_EXPECT_TOGGLE(nd))

FLUX_PROP(slider, FluxSliderData, step_frequency, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_PROP(slider, FluxSliderData, tick_frequency, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_PROP(slider, FluxSliderData, small_change, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_PROP(slider, FluxSliderData, large_change, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_PROP(slider, FluxSliderData, snaps_to, FluxSliderSnapsTo, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_PROP(slider, FluxSliderData, tick_placement, FluxTickPlacement, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))
FLUX_PROP_2(
  slider, FluxSliderData, range, min_value, max_value, float, float, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER)
)
FLUX_PROP_ENABLED(slider, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER))

void flux_prop_slider_step(FluxStore *store, XentNodeId id, float step) {
	flux_prop_slider_step_frequency(store, id, step);
}

void flux_prop_slider_value(FluxStore *store, XentNodeId id, float value) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SLIDER)) return;
	FluxSliderData *sd = ( FluxSliderData * ) nd->component_data;
	sd->current_value  = value;
	if (!nd->state.pressed) sd->intermediate_value = value;
}

void flux_prop_text_content(FluxStore *store, XentNodeId id, char const *content) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT)) return;
	flux_str_replace(&(( FluxTextData * ) nd->component_data)->content, content);
	xent_settext(flux_store_ctx(store), id, content ? content : "");
}

void flux_prop_text_fontsize(FluxStore *store, XentNodeId id, float size) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT)) return;
	(( FluxTextData * ) nd->component_data)->font_size = size;
	xent_setfontsize(flux_store_ctx(store), id, size);
}

void flux_prop_text_font_weight(FluxStore *store, XentNodeId id, FluxFontWeight weight) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT)) return;
	(( FluxTextData * ) nd->component_data)->font_weight = weight;
	xent_setfontweight(flux_store_ctx(store), id, flux_font_weightnum(weight));
}

FLUX_PROP(text, FluxTextData, text_color, FluxColor, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT))
FLUX_PROP(text, FluxTextData, alignment, FluxTextAlign, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT))

void flux_prop_text_color(FluxStore *store, XentNodeId id, FluxColor color) {
	flux_prop_text_text_color(store, id, color);
}

void flux_prop_text_weight(FluxStore *store, XentNodeId id, FluxFontWeight weight) {
	flux_prop_text_font_weight(store, id, weight);
}

static FluxTextData *text_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = component_node(store, id);
	return nd && FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_TEXT) ? ( FluxTextData * ) nd->component_data : NULL;
}

void flux_prop_text_maxlines(FluxStore *store, XentNodeId id, uint32_t max_lines) {
	FluxTextData *td = text_data(store, id);
	if (!td) return;
	td->max_lines = ( uint8_t ) (max_lines > 255u ? 255u : max_lines);
	xent_setmaxlines(flux_store_ctx(store), id, td->max_lines);
}

void flux_prop_text_trim(FluxStore *store, XentNodeId id, XentTextTrim trim) {
	FluxTextData *td = text_data(store, id);
	if (!td) return;
	td->trim = trim;
	xent_settexttrim(flux_store_ctx(store), id, trim);
}

void flux_prop_text_wrap(FluxStore *store, XentNodeId id, bool wrap) {
	FluxTextData *td = text_data(store, id);
	if (!td) return;
	td->wrap = wrap;
	xent_setlinebreak(flux_store_ctx(store), id, wrap ? XENT_LINEBREAK_WORDWRAP : XENT_LINEBREAK_NOWRAP);
}

void flux_prop_text_selectable(FluxStore *store, XentNodeId id, bool selectable) {
	FluxTextData *td = text_data(store, id);
	if (!td) return;
	td->selectable = selectable;
	if (!selectable) td->selection_start = td->selection_end = 0u;
	flux_text_select_wire(component_node(store, id), td, selectable);
	xent_set_focusable(flux_store_ctx(store), id, selectable);
}

static FluxTextBoxInputData *text_input_data(FluxStore *store, XentNodeId id, FluxControlType type) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || nd->component_type != type) return NULL;
	return ( FluxTextBoxInputData * ) nd->component_data;
}

static void prop_text_input_enabled(FluxStore *store, XentNodeId id, FluxControlType type, bool enabled) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || nd->component_type != type) return;
	xent_sem_setenabled(flux_store_ctx(store), id, enabled);
	(( FluxTextBoxData * ) nd->component_data)->enabled = enabled;
}

void flux_prop_textbox_content(FluxStore *store, XentNodeId id, char const *content) {
	FluxTextBoxInputData *tb = text_input_data(store, id, FLUX_CONTROL_TEXT_INPUT);
	if (tb) tb_replace_text(tb, content);
}

void flux_prop_textbox_placeholder(FluxStore *store, XentNodeId id, char const *placeholder) {
	FluxTextBoxInputData *tb = text_input_data(store, id, FLUX_CONTROL_TEXT_INPUT);
	if (tb) flux_str_replace(&tb->base.placeholder, placeholder);
}

void flux_prop_textbox_enabled(FluxStore *store, XentNodeId id, bool enabled) {
	prop_text_input_enabled(store, id, FLUX_CONTROL_TEXT_INPUT, enabled);
}

void flux_prop_textbox_multiline(FluxStore *store, XentNodeId id, bool multiline) {
	FluxTextBoxInputData *tb = text_input_data(store, id, FLUX_CONTROL_TEXT_INPUT);
	if (!tb || tb->base.multiline == multiline) return;
	tb->base.multiline       = multiline;
	tb->base.scroll_offset_x = 0.0f;
	tb->base.scroll_offset_y = 0.0f;
}

void flux_prop_textbox_readonly(FluxStore *store, XentNodeId id, bool readonly) {
	FluxTextBoxInputData *tb = text_input_data(store, id, FLUX_CONTROL_TEXT_INPUT);
	if (tb) tb->base.readonly = readonly;
}

FLUX_PROP(progress, FluxProgressData, indeterminate, bool, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS))

void flux_prop_progress_value(FluxStore *store, XentNodeId id, float value) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS)) return;
	FluxProgressData *data = ( FluxProgressData * ) nd->component_data;
	data->value            = value;
	xent_sem_setvalue(flux_store_ctx(store), id, value, data->min_value, data->max_value);
}

void flux_prop_progress_range(FluxStore *store, XentNodeId id, float min_val, float max_val) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS)) return;
	FluxProgressData *data = ( FluxProgressData * ) nd->component_data;
	data->min_value        = min_val;
	data->max_value        = max_val;
	xent_sem_setvalue(flux_store_ctx(store), id, data->value, min_val, max_val);
}

void flux_prop_scroll_offset(FluxStore *store, XentNodeId id, float x, float y) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_SCROLL)) return;
	FluxScrollData *d       = ( FluxScrollData * ) nd->component_data;
	bool            changed = d->scroll_x != x || d->scroll_y != y;
	d->scroll_x             = x;
	d->scroll_y             = y;
	if (changed) flux_scroll_offset_changed(d);
}

static FluxNodeData *node_for_visual_setter(FluxStore *store, XentNodeId id) {
	if (!store) return NULL;
	FluxNodeData *nd = flux_store_find(store, id);
	return nd;
}

void flux_prop_node_background(FluxStore *store, XentNodeId id, FluxColor color) {
	FluxNodeData *nd = node_for_visual_setter(store, id);
	if (!nd) return;
	nd->visuals.background = color;
}

void flux_prop_node_border(FluxStore *store, XentNodeId id, FluxColor color, float width) {
	FluxNodeData *nd = node_for_visual_setter(store, id);
	if (!nd) return;
	nd->visuals.border_color = color;
	nd->visuals.border_width = width;
}

void flux_prop_radius(FluxStore *store, XentNodeId id, float radius) {
	FluxNodeData *nd = node_for_visual_setter(store, id);
	if (!nd) return;
	nd->visuals.corner_radius = radius;
}

void flux_prop_node_tooltip(FluxStore *store, XentNodeId id, char const *text) {
	FluxNodeData *nd = node_for_visual_setter(store, id);
	if (!nd) return;
	flux_str_replace(&nd->tooltip_text, text);
}

void flux_prop_password_content(FluxStore *store, XentNodeId id, char const *content) {
	FluxTextBoxInputData *tb = text_input_data(store, id, FLUX_CONTROL_PASSWORD_BOX);
	if (tb) tb_replace_text(tb, content);
}

void flux_prop_password_placeholder(FluxStore *store, XentNodeId id, char const *placeholder) {
	FluxTextBoxInputData *tb = text_input_data(store, id, FLUX_CONTROL_PASSWORD_BOX);
	if (tb) flux_str_replace(&tb->base.placeholder, placeholder);
}

void flux_prop_password_enabled(FluxStore *store, XentNodeId id, bool enabled) {
	prop_text_input_enabled(store, id, FLUX_CONTROL_PASSWORD_BOX, enabled);
}

static FluxTextBoxInputData *numberbox_data(FluxStore *store, XentNodeId id) {
	FluxTextBoxInputData *tb = text_input_data(store, id, FLUX_CONTROL_NUMBER_BOX);
	if (!tb || !tb->nb) return NULL;
	return tb;
}

void flux_prop_numberbox_current_value(FluxStore *store, XentNodeId id, double value) {
	FluxTextBoxInputData *tb = numberbox_data(store, id);
	if (tb) nb_project_value(tb, value);
}

void flux_prop_numberbox_range(FluxStore *store, XentNodeId id, double min_val, double max_val) {
	FluxTextBoxInputData *tb = numberbox_data(store, id);
	if (!tb) return;
	tb->nb->minimum = min_val;
	tb->nb->maximum = max_val;
	nb_coerce_value(tb);
	nb_update_text_to_value(tb);
	nb_sync_semantics(tb);
}

void flux_prop_numberbox_step(FluxStore *store, XentNodeId id, double step) {
	FluxTextBoxInputData *tb = numberbox_data(store, id);
	if (!tb) return;
	tb->nb->small_change = step;
	tb->nb->large_change = step * 10.0;
}

void flux_prop_numberbox_spin_enabled(FluxStore *store, XentNodeId id, bool enabled) {
	FluxTextBoxInputData *tb = numberbox_data(store, id);
	if (!tb) return;
	tb->nb->spin_placement = enabled ? FLUX_NB_SPIN_INLINE : FLUX_NB_SPIN_HIDDEN;
	xent_sem_setexpanded(tb->ctx, tb->node, enabled);
}

void flux_prop_numberbox_enabled(FluxStore *store, XentNodeId id, bool enabled) {
	prop_text_input_enabled(store, id, FLUX_CONTROL_NUMBER_BOX, enabled);
}

void flux_prop_numberbox_value(FluxStore *store, XentNodeId id, double value) {
	flux_prop_numberbox_current_value(store, id, value);
}

void flux_prop_hyperlink_label(FluxStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_HYPERLINK)) return;
	FluxHyperlinkData *link = ( FluxHyperlinkData * ) nd->component_data;
	flux_str_replace(&link->label, label);
	label_sync(store, id, label);
	XentCtx *ctx = flux_store_ctx(store);
	xent_setlinebreak(ctx, id, XENT_LINEBREAK_NOWRAP);
	xent_setfontsize(ctx, id, link->font_size > 0.0f ? link->font_size : 14.0f);
	xent_setp(ctx, id, (XentInsets) {11.0f, 5.0f, 11.0f, 6.0f});
	xent_setsize(ctx, id, (XentSize) {NAN, NAN});
	xent_setminsize(ctx, id, (XentSize) {NAN, 32.0f});
}

FLUX_PROP_STR(hyperlink, FluxHyperlinkData, url, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_HYPERLINK))
FLUX_PROP_ENABLED(hyperlink, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_HYPERLINK))

void flux_prop_repeat_label(FluxStore *store, XentNodeId id, char const *label) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON)) return;
	FluxRepeatButtonData *bd = ( FluxRepeatButtonData * ) nd->component_data;
	flux_str_replace(&bd->label, label);
	label_sync(store, id, label);

	XentCtx *ctx = flux_store_ctx(store);
	xent_setlinebreak(ctx, id, XENT_LINEBREAK_NOWRAP);
	xent_setfontsize(ctx, id, bd->font_size > 0.0f ? bd->font_size : 14.0f);
	xent_setsize(ctx, id, (XentSize) {NAN, NAN});
	xent_setminsize(ctx, id, (XentSize) {NAN, 32.0f});
	button_reserve_icon_pad(store, id, nd, bd->label, bd->font_size, bd->icon_name);
}

FLUX_PROP_STR(repeat, FluxRepeatButtonData, icon_name, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON))
FLUX_PROP(repeat, FluxRepeatButtonData, style, FluxButtonStyle, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON))
FLUX_PROP_2(
  repeat, FluxRepeatButtonData, timing, repeat_delay_ms, repeat_interval_ms, uint32_t, uint32_t,
  FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON)
)
FLUX_PROP_ENABLED(repeat, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON))

void flux_prop_repeat_icon(FluxStore *store, XentNodeId id, char const *icon_name) {
	flux_prop_repeat_icon_name(store, id, icon_name);
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_REPEAT_BUTTON)) return;
	FluxRepeatButtonData *bd = ( FluxRepeatButtonData * ) nd->component_data;
	button_reserve_icon_pad(store, id, nd, bd->label, bd->font_size, icon_name);
}

FLUX_PROP(progressring, FluxProgressRingData, indeterminate, bool, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS_RING))

void flux_prop_progressring_value(FluxStore *store, XentNodeId id, float value) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS_RING)) return;
	FluxProgressRingData *data = ( FluxProgressRingData * ) nd->component_data;
	data->value                = value;
	xent_sem_setvalue(flux_store_ctx(store), id, value, data->min_value, data->max_value);
}

void flux_prop_progressring_range(FluxStore *store, XentNodeId id, float min_val, float max_val) {
	FluxNodeData *nd = component_node(store, id);
	if (!nd || !FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_PROGRESS_RING)) return;
	FluxProgressRingData *data = ( FluxProgressRingData * ) nd->component_data;
	data->min_value            = min_val;
	data->max_value            = max_val;
	xent_sem_setvalue(flux_store_ctx(store), id, data->value, min_val, max_val);
}

FLUX_PROP(badge, FluxInfoBadgeData, mode, FluxInfoBadgeMode, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_INFO_BADGE))
FLUX_PROP(badge, FluxInfoBadgeData, value, int32_t, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_INFO_BADGE))
FLUX_PROP_STR(badge, FluxInfoBadgeData, icon_name, FLUX_EXPECT_TYPE(nd, FLUX_CONTROL_INFO_BADGE))
