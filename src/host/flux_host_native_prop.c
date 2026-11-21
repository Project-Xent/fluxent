#include "controls/behavior/flux_list_internal.h"
#include "flux_host_internal.h"
#include "store/flux_node_data.h"

#include "controls/flux_module.h"
#include "controls/flux_module_props.h"
#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "controls/factory/flux_init_media.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/factory/flux_init_value.h"
#include "controls/factory/flux_init_picker.h"
#include "controls/behavior/flux_host_menu.h"
#include "controls/behavior/flux_slider.h"
#include "controls/behavior/flux_calendar.h"
#include "controls/behavior/flux_color_picker.h"
#include "controls/behavior/flux_tree_internal.h"
#include "controls/behavior/flux_colheader.h"
#include "controls/behavior/flux_cmdbar.h"
#include "host/flux_prop_apply.h"
#include "render/flux_resource_intern.h"
#include <controls/data/flux_list_view_data.h>
#include <controls/data/flux_pager_data.h>
#include <controls/data/flux_person_picture_data.h>
#include <controls/data/flux_radio_buttons_data.h>
#include <controls/data/flux_slider_data.h>
#include <controls/data/flux_title_bar_data.h>
#include <fluxent/flux_control_type.h>
#include <xtk/xtk_navmodel.h>
#include <xtk/xtk_treemodel.h>

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define HANDLER(name) static bool name(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkValue const *v)

#define SETTER(name, fn, value)    \
	HANDLER(name) {                \
		( void ) control;          \
		fn(R->store, node, value); \
		return true;               \
	}

#define CONFIG(name, apply)      \
	HANDLER(name) {              \
		( void ) v;              \
		apply(R, control, node); \
		return true;             \
	}

#define ON(id, kind, fn) {XTK_PROP_##id, XTK_VALUE_##kind, fn}
#define END              {XTK_PROP_ID_INVALID, 0, NULL}

static float stored_f32(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	PropEntry const *entry = flux_host_prop_findc(R, control, id);
	return entry && entry->value.kind == XTK_VALUE_F32 ? entry->value.u.f32 : 0.0f;
}

static double stored_f64(FluxHostState const *R, XtkCtrl control, XtkPropertyId id, double fallback) {
	PropEntry const *entry = flux_host_prop_findc(R, control, id);
	return entry && entry->value.kind == XTK_VALUE_F64 ? entry->value.u.f64 : fallback;
}

static uint32_t stored_enum(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	PropEntry const *entry = flux_host_prop_findc(R, control, id);
	return entry && entry->value.kind == XTK_VALUE_ENUM ? entry->value.u.u32 : 0u;
}

static bool stored_bool(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	PropEntry const *entry = flux_host_prop_findc(R, control, id);
	return entry && entry->value.kind == XTK_VALUE_BOOL && entry->value.u.b;
}

static int32_t stored_i32(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	PropEntry const *entry = flux_host_prop_findc(R, control, id);
	return entry && entry->value.kind == XTK_VALUE_I32 ? entry->value.u.i32 : 0;
}

static char const *stored_string(FluxHostState const *R, XtkCtrl control, XtkPropertyId id) {
	PropEntry const *entry = flux_host_prop_findc(R, control, id);
	return entry && entry->value.kind == XTK_VALUE_STRING ? entry->value.u.string.data : NULL;
}

static int stored_selected(FluxHostState const *R, XtkCtrl control) {
	PropEntry const *selected = flux_host_prop_findc(R, control, XTK_PROP_SELECTED_INDEX);
	return selected && selected->value.kind == XTK_VALUE_I32 ? selected->value.u.i32 : -1;
}

static char const *str_or_empty(XtkValue const *v) { return v->u.string.data ? v->u.string.data : ""; }

static void       *component(FluxHostState *R, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(R->store, node);
	return nd ? nd->component_data : NULL;
}

HANDLER(prop_none) {
	( void ) R;
	( void ) control;
	( void ) node;
	( void ) v;
	return true;
}

HANDLER(prop_width) {
	( void ) control;
	return xent_setw(R->ctx, node, v->u.f32);
}

HANDLER(prop_height) {
	( void ) control;
	return xent_seth(R->ctx, node, v->u.f32);
}

HANDLER(prop_grow) {
	( void ) control;
	if (!xent_setgrow(R->ctx, node, v->u.f32)) return false;
	return xent_setbasis(R->ctx, node, v->u.f32 > 0.0f ? 0.0f : NAN);
}

HANDLER(prop_shrink) {
	( void ) control;
	return xent_setshrink(R->ctx, node, v->u.f32);
}

HANDLER(prop_align_self) {
	( void ) control;
	return xent_setself(R->ctx, node, ( XentFlexAlign ) v->u.u32);
}

HANDLER(prop_flow) {
	( void ) control;
	XentDirection dir = ( XentDirection ) v->u.u32;
	if (node == R->root && R->env.overlay != XENT_NODE_INVALID && !xent_setdir(R->ctx, R->env.overlay, dir))
		return false;
	return xent_setdir(R->ctx, node, dir);
}

HANDLER(prop_margin) {
	( void ) v;
	XentInsets insets = {
	  stored_f32(R, control, XTK_PROP_MARGIN_LEFT), stored_f32(R, control, XTK_PROP_MARGIN_TOP),
	  stored_f32(R, control, XTK_PROP_MARGIN_RIGHT), stored_f32(R, control, XTK_PROP_MARGIN_BOTTOM)};
	return xent_setm(R->ctx, node, insets);
}

HANDLER(prop_padding) {
	( void ) v;
	XentInsets insets = {
	  stored_f32(R, control, XTK_PROP_PADDING_LEFT), stored_f32(R, control, XTK_PROP_PADDING_TOP),
	  stored_f32(R, control, XTK_PROP_PADDING_RIGHT), stored_f32(R, control, XTK_PROP_PADDING_BOTTOM)};
	return xent_setp(R->ctx, node, insets);
}

HANDLER(prop_orientation) {
	( void ) control;
	if (!xent_setproto(R->ctx, node, XENT_PROTOCOL_FLEX)) return false;
	return xent_setflexdir(R->ctx, node, v->u.u32 == XTK_STACK_ROW ? XENT_FLEX_ROW : XENT_FLEX_COLUMN);
}

HANDLER(prop_align_items) {
	( void ) control;
	return xent_setitems(R->ctx, node, ( XentFlexAlign ) v->u.u32);
}

HANDLER(prop_justify) {
	( void ) control;
	return xent_setjustify(R->ctx, node, ( XentFlexJustify ) v->u.u32);
}

HANDLER(prop_gap) {
	( void ) control;
	return xent_setgap(R->ctx, node, v->u.f32);
}

HANDLER(prop_fill) {
	bool card   = flux_host_types_get(R, node) == XTK_CTRL_CARD;
	bool row    = stored_enum(R, control, XTK_PROP_ORIENTATION) == XTK_STACK_ROW;
	bool wrap_w = card ? false : !v->u.b;
	bool wrap_h = card ? !v->u.b : row || !v->u.b;
	return xent_setfit(R->ctx, node, wrap_w, wrap_h);
}

HANDLER(prop_fit_width) { return xent_setfit(R->ctx, node, v->u.b, stored_bool(R, control, XTK_PROP_FIT_HEIGHT)); }

HANDLER(prop_fit_height) { return xent_setfit(R->ctx, node, stored_bool(R, control, XTK_PROP_FIT_WIDTH), v->u.b); }

HANDLER(prop_wrap) {
	( void ) control;
	return xent_setflexwrap(R->ctx, node, ( XentFlexWrap ) v->u.u32);
}

SETTER(prop_tooltip, flux_prop_node_tooltip, str_or_empty(v))
SETTER(prop_drop, flux_prop_node_drop, v->u.u32)
SETTER(prop_dragtext, flux_prop_node_dragtext, str_or_empty(v))
SETTER(prop_dragfiles, flux_prop_node_dragfiles, str_or_empty(v))

SETTER(button_label, flux_prop_button_label, str_or_empty(v))
SETTER(button_icon, flux_prop_button_icon, str_or_empty(v))
SETTER(button_style, flux_prop_button_style, ( FluxButtonStyle ) v->u.u32)
SETTER(button_enabled, flux_prop_button_enabled, v->u.b)
SETTER(togglesplit_checked, flux_togglesplit_set_checked, v->u.b)

FluxPropHandler const flux_props_button [] = {
  ON(TEXT, STRING, button_label),
  ON(ICON, STRING, button_icon),
  ON(BUTTON_STYLE, ENUM, button_style),
  ON(ENABLED, BOOL, button_enabled),
  END,
};

FluxPropHandler const flux_props_togglesplit [] = {
  ON(TEXT, STRING, button_label),
  ON(ICON, STRING, button_icon),
  ON(ENABLED, BOOL, button_enabled),
  ON(CHECKED, BOOL, togglesplit_checked),
  END,
};

SETTER(repeat_label, flux_prop_repeat_label, str_or_empty(v))
SETTER(repeat_icon, flux_prop_repeat_icon, str_or_empty(v))
SETTER(repeat_style, flux_prop_repeat_style, ( FluxButtonStyle ) v->u.u32)
SETTER(repeat_enabled, flux_prop_repeat_enabled, v->u.b)

FluxPropHandler const flux_props_repeat [] = {
  ON(TEXT, STRING, repeat_label),
  ON(ICON, STRING, repeat_icon),
  ON(BUTTON_STYLE, ENUM, repeat_style),
  ON(ENABLED, BOOL, repeat_enabled),
  END,
};

SETTER(link_label, flux_prop_hyperlink_label, str_or_empty(v))
SETTER(link_enabled, flux_prop_hyperlink_enabled, v->u.b)
SETTER(link_url, flux_prop_hyperlink_url, str_or_empty(v))

FluxPropHandler const flux_props_link [] = {
  ON(TEXT, STRING, link_label),
  ON(ENABLED, BOOL, link_enabled),
  ON(URL, STRING, link_url),
  END,
};

static void check_state(FluxHostState *R, XentNodeId node, FluxCheckState state) {
	FluxCheckboxData *cd = component(R, node);
	if (cd) flux_checkbox_setstate(cd, state);
}

SETTER(check_label, flux_prop_checkbox_label, str_or_empty(v))
SETTER(check_enabled, flux_prop_checkbox_enabled, v->u.b)

HANDLER(check_checked) {
	( void ) control;
	check_state(R, node, v->u.b ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED);
	return true;
}

HANDLER(check_three_state) {
	( void ) control;
	FluxCheckboxData *cd = component(R, node);
	if (cd) cd->is_three_state = v->u.b;
	return true;
}

HANDLER(check_indeterminate) {
	( void ) control;
	FluxCheckboxData *cd = component(R, node);
	if (!cd) return true;
	if (v->u.b) check_state(R, node, FLUX_CHECK_INDETERMINATE);
	else if (cd->state == FLUX_CHECK_INDETERMINATE) check_state(R, node, FLUX_CHECK_UNCHECKED);
	return true;
}

FluxPropHandler const flux_props_checkbox [] = {
  ON(TEXT, STRING, check_label),
  ON(ENABLED, BOOL, check_enabled),
  ON(CHECKED, BOOL, check_checked),
  ON(IS_THREE_STATE, BOOL, check_three_state),
  ON(INDETERMINATE, BOOL, check_indeterminate),
  END,
};

FluxPropHandler const flux_props_radio [] = {
  ON(TEXT, STRING, check_label),
  ON(ENABLED, BOOL, check_enabled),
  ON(CHECKED, BOOL, check_checked),
  END,
};

FluxPropHandler const flux_props_switch [] = {
  ON(ENABLED, BOOL, check_enabled),
  ON(CHECKED, BOOL, check_checked),
  END,
};

HANDLER(slider_orientation) {
	bool vertical = v->u.u32 != XTK_STACK_ROW;
	flux_slider_setvertical(R->store, node, vertical);
	if (!flux_host_prop_findc(R, control, XTK_PROP_WIDTH)) xent_setw(R->ctx, node, vertical ? 32.0f : NAN);
	if (!flux_host_prop_findc(R, control, XTK_PROP_HEIGHT)) xent_seth(R->ctx, node, vertical ? NAN : 32.0f);
	return true;
}

HANDLER(slider_minmax) {
	( void ) v;
	FluxSliderData *slider = component(R, node);
	if (!slider) return true;
	PropEntry const *min_entry = flux_host_prop_findc(R, control, XTK_PROP_MIN_F32);
	PropEntry const *max_entry = flux_host_prop_findc(R, control, XTK_PROP_MAX_F32);
	float min = min_entry && min_entry->value.kind == XTK_VALUE_F32 ? min_entry->value.u.f32 : slider->min_value;
	float max = max_entry && max_entry->value.kind == XTK_VALUE_F32 ? max_entry->value.u.f32 : slider->max_value;
	flux_prop_slider_range(R->store, node, min, max);
	return true;
}

SETTER(slider_enabled, flux_prop_slider_enabled, v->u.b)
SETTER(slider_value, flux_prop_slider_value, v->u.f32)
SETTER(slider_step, flux_prop_slider_step, v->u.f32)
SETTER(slider_tick_frequency, flux_prop_slider_tick_frequency, v->u.f32)
SETTER(slider_tick_placement, flux_prop_slider_tick_placement, ( FluxTickPlacement ) v->u.u32)
SETTER(slider_snaps_to, flux_prop_slider_snaps_to, ( FluxSliderSnapsTo ) v->u.u32)
SETTER(slider_small_change, flux_prop_slider_small_change, v->u.f32)
SETTER(slider_large_change, flux_prop_slider_large_change, v->u.f32)
SETTER(slider_is_range, flux_slider_setrange, v->u.b)

HANDLER(slider_range_start) {
	( void ) control;
	flux_slider_setthumb(R->store, node, 0, v->u.f32);
	return true;
}

HANDLER(slider_range_end) {
	( void ) control;
	flux_slider_setthumb(R->store, node, 1, v->u.f32);
	return true;
}

FluxPropHandler const flux_props_slider [] = {
  ON(ORIENTATION, ENUM, slider_orientation),
  ON(ENABLED, BOOL, slider_enabled),
  ON(VALUE_F32, F32, slider_value),
  ON(MIN_F32, F32, slider_minmax),
  ON(MAX_F32, F32, slider_minmax),
  ON(STEP_F32, F32, slider_step),
  ON(TICK_FREQUENCY, F32, slider_tick_frequency),
  ON(TICK_PLACEMENT, ENUM, slider_tick_placement),
  ON(SLIDER_SNAPS_TO, ENUM, slider_snaps_to),
  ON(SMALL_CHANGE, F32, slider_small_change),
  ON(LARGE_CHANGE, F32, slider_large_change),
  ON(IS_RANGE, BOOL, slider_is_range),
  ON(RANGE_START, F32, slider_range_start),
  ON(RANGE_END, F32, slider_range_end),
  END,
};

SETTER(textbox_content, flux_prop_textbox_content, str_or_empty(v))
SETTER(textbox_enabled, flux_prop_textbox_enabled, v->u.b)
SETTER(textbox_placeholder, flux_prop_textbox_placeholder, str_or_empty(v))
SETTER(textbox_readonly, flux_prop_textbox_readonly, v->u.b)
SETTER(textbox_multiline, flux_prop_textbox_multiline, v->u.b)

FluxPropHandler const flux_props_textbox [] = {
  ON(TEXT, STRING, textbox_content),
  ON(ENABLED, BOOL, textbox_enabled),
  ON(PLACEHOLDER, STRING, textbox_placeholder),
  ON(READ_ONLY, BOOL, textbox_readonly),
  ON(MULTILINE, BOOL, textbox_multiline),
  END,
};

SETTER(password_content, flux_prop_password_content, str_or_empty(v))
SETTER(password_enabled, flux_prop_password_enabled, v->u.b)
SETTER(password_placeholder, flux_prop_password_placeholder, str_or_empty(v))

FluxPropHandler const flux_props_password [] = {
  ON(TEXT, STRING, password_content),
  ON(ENABLED, BOOL, password_enabled),
  ON(PLACEHOLDER, STRING, password_placeholder),
  END,
};

SETTER(number_enabled, flux_prop_numberbox_enabled, v->u.b)
SETTER(number_value, flux_prop_numberbox_value, v->u.f64)
SETTER(number_step, flux_prop_numberbox_step, v->u.f64)
SETTER(number_hide_spin, flux_prop_numberbox_spin_enabled, !v->u.b)

HANDLER(number_max) {
	flux_prop_numberbox_range(R->store, node, stored_f64(R, control, XTK_PROP_MIN_F64, 0.0), v->u.f64);
	return true;
}

HANDLER(number_min) {
	flux_prop_numberbox_range(R->store, node, v->u.f64, stored_f64(R, control, XTK_PROP_MAX_F64, 100.0));
	return true;
}

FluxPropHandler const flux_props_number [] = {
  ON(ENABLED, BOOL, number_enabled),
  ON(VALUE_F64, F64, number_value),
  ON(MAX_F64, F64, number_max),
  ON(MIN_F64, F64, number_min),
  ON(STEP_F64, F64, number_step),
  ON(HIDE_SPIN, BOOL, number_hide_spin),
  END,
};

SETTER(text_content, flux_prop_text_content, str_or_empty(v))
SETTER(text_font_size, flux_prop_text_fontsize, v->u.f32)
SETTER(text_font_weight, flux_prop_text_weight, ( FluxFontWeight ) v->u.u32)
SETTER(text_align, flux_prop_text_alignment, ( FluxTextAlign ) v->u.u32)
SETTER(text_foreground, flux_prop_text_color, v->u.color)
SETTER(text_max_lines, flux_prop_text_maxlines, v->u.i32 > 0 ? ( uint32_t ) v->u.i32 : 0u)
SETTER(text_wrap, flux_prop_text_wrap, v->u.b)
SETTER(text_selectable, flux_prop_text_selectable, v->u.b)

HANDLER(text_trim) {
	( void ) control;
	if (v->u.u32 <= XENT_TEXT_TRIM_PATH_ELLIPSIS) flux_prop_text_trim(R->store, node, ( XentTextTrim ) v->u.u32);
	return true;
}

FluxPropHandler const flux_props_text [] = {
  ON(TEXT, STRING, text_content),
  ON(FONT_SIZE, F32, text_font_size),
  ON(FONT_WEIGHT, ENUM, text_font_weight),
  ON(TEXT_ALIGN, ENUM, text_align),
  ON(FOREGROUND, COLOR, text_foreground),
  ON(MAX_LINES, I32, text_max_lines),
  ON(TEXT_TRIM, ENUM, text_trim),
  ON(TEXT_WRAP, BOOL, text_wrap),
  ON(SELECTABLE, BOOL, text_selectable),
  END,
};

HANDLER(scroll_x) {
	( void ) control;
	flux_scroll_request(R->store, node, v->u.f32, NAN);
	return true;
}

HANDLER(scroll_y) {
	( void ) control;
	flux_scroll_request(R->store, node, NAN, v->u.f32);
	return true;
}

FluxPropHandler const flux_props_scroll [] = {
  ON(SCROLL_X, F32, scroll_x),
  ON(SCROLL_Y, F32, scroll_y),
  END,
};

SETTER(image_source, flux_prop_image_source, flux_resource_path(( XentResourceId ) v->u.resource))
SETTER(image_stretch, flux_prop_image_stretch, ( FluxImageStretch ) v->u.u32)

#define NINE(name, prop)                                                 \
	HANDLER(name) {                                                      \
		( void ) control;                                                \
		flux_prop_image_nine(R->store, node, XTK_PROP_##prop, v->u.f32); \
		return true;                                                     \
	}

NINE(image_nl, NINE_GRID_LEFT)
NINE(image_nt, NINE_GRID_TOP)
NINE(image_nr, NINE_GRID_RIGHT)
NINE(image_nb, NINE_GRID_BOTTOM)

FluxPropHandler const flux_props_image [] = {
  ON(IMAGE_SOURCE, RESOURCE, image_source),
  ON(IMAGE_STRETCH, ENUM, image_stretch),
  ON(NINE_GRID_LEFT, F32, image_nl),
  ON(NINE_GRID_TOP, F32, image_nt),
  ON(NINE_GRID_RIGHT, F32, image_nr),
  ON(NINE_GRID_BOTTOM, F32, image_nb),
  END,
};

static void progress_min_height(FluxHostState *R, XentNodeId node, FluxNodeData const *nd) {
	FluxProgressData const *bar    = ( FluxProgressData const * ) nd->component_data;
	float                   height = 3.0f + bar->padding.top + bar->padding.bottom + 2.0f * nd->visuals.border_width;
	xent_setminsize(R->ctx, node, (XentSize) {0.0f, height});
}

HANDLER(progress_padding) {
	( void ) v;
	FluxNodeData     *nd  = flux_store_find(R->store, node);
	FluxProgressData *bar = nd ? ( FluxProgressData * ) nd->component_data : NULL;
	if (!bar) return false;
	bar->padding = (XentInsets) {
	  fmaxf(0.0f, stored_f32(R, control, XTK_PROP_PADDING_LEFT)),
	  fmaxf(0.0f, stored_f32(R, control, XTK_PROP_PADDING_TOP)),
	  fmaxf(0.0f, stored_f32(R, control, XTK_PROP_PADDING_RIGHT)),
	  fmaxf(0.0f, stored_f32(R, control, XTK_PROP_PADDING_BOTTOM))};
	progress_min_height(R, node, nd);
	return false;
}

static void progress_range(FluxHostState *R, XtkCtrl control, XentNodeId node, float min, float max, bool ring) {
	PropEntry const *min_entry = flux_host_prop_findc(R, control, XTK_PROP_MIN_F32);
	PropEntry const *max_entry = flux_host_prop_findc(R, control, XTK_PROP_MAX_F32);
	if (min_entry && min_entry->value.kind == XTK_VALUE_F32) min = min_entry->value.u.f32;
	if (max_entry && max_entry->value.kind == XTK_VALUE_F32) max = max_entry->value.u.f32;
	if (ring) flux_prop_progressring_range(R->store, node, min, max);
	else flux_prop_progress_range(R->store, node, min, max);
}

HANDLER(progress_minmax) {
	( void ) v;
	FluxProgressData *bar = component(R, node);
	progress_range(R, control, node, bar ? bar->min_value : 0.0f, bar ? bar->max_value : 100.0f, false);
	return true;
}

HANDLER(progress_state) {
	( void ) control;
	FluxProgressData *bar = component(R, node);
	if (bar) bar->state = ( uint8_t ) v->u.u32;
	return true;
}

HANDLER(progress_border_width) {
	( void ) control;
	FluxNodeData *nd = flux_store_find(R->store, node);
	if (!nd || !nd->component_data) return true;
	nd->visuals.border_width = fmaxf(0.0f, v->u.f32);
	progress_min_height(R, node, nd);
	return true;
}

SETTER(progress_value, flux_prop_progress_value, v->u.f32)
SETTER(progress_indeterminate, flux_prop_progress_indeterminate, v->u.b)

FluxPropHandler const flux_props_progress [] = {
  ON(PADDING_LEFT, F32, progress_padding),
  ON(PADDING_TOP, F32, progress_padding),
  ON(PADDING_RIGHT, F32, progress_padding),
  ON(PADDING_BOTTOM, F32, progress_padding),
  ON(VALUE_F32, F32, progress_value),
  ON(MIN_F32, F32, progress_minmax),
  ON(MAX_F32, F32, progress_minmax),
  ON(INDETERMINATE, BOOL, progress_indeterminate),
  ON(PROGRESS_STATE, ENUM, progress_state),
  ON(PROGRESS_BORDER_WIDTH, F32, progress_border_width),
  END,
};

HANDLER(ring_minmax) {
	( void ) v;
	FluxProgressRingData *ring = component(R, node);
	progress_range(R, control, node, ring ? ring->min_value : 0.0f, ring ? ring->max_value : 100.0f, true);
	return true;
}

HANDLER(ring_active) {
	( void ) control;
	FluxProgressRingData *ring = component(R, node);
	if (ring) ring->inactive = !v->u.b;
	return true;
}

SETTER(ring_value, flux_prop_progressring_value, v->u.f32)
SETTER(ring_indeterminate, flux_prop_progressring_indeterminate, v->u.b)

FluxPropHandler const flux_props_progressring [] = {
  ON(VALUE_F32, F32, ring_value),
  ON(MIN_F32, F32, ring_minmax),
  ON(MAX_F32, F32, ring_minmax),
  ON(INDETERMINATE, BOOL, ring_indeterminate),
  ON(PROGRESS_ACTIVE, BOOL, ring_active),
  END,
};

HANDLER(list_count) {
	( void ) control;
	FluxListViewData *ld = component(R, node);
	flux_list_set_extent(
	  R->store, node, v->u.i32, ld ? ld->item_height : 0.0f, ld ? ld->item_width : 0.0f, ld ? ld->columns : 0
	);
	return true;
}

HANDLER(list_height) {
	( void ) control;
	FluxListViewData *ld = component(R, node);
	if (ld) flux_list_set_extent(R->store, node, ld->count, v->u.f32, ld->item_width, ld->columns);
	return true;
}

HANDLER(list_width) {
	( void ) control;
	FluxListViewData *ld = component(R, node);
	if (ld) flux_list_set_extent(R->store, node, ld->count, ld->item_height, v->u.f32, ld->columns);
	return true;
}

HANDLER(list_columns) {
	( void ) control;
	FluxListViewData *ld = component(R, node);
	if (ld) flux_list_set_extent(R->store, node, ld->count, ld->item_height, ld->item_width, v->u.i32);
	return true;
}

HANDLER(list_invoked) {
	( void ) control;
	FluxListViewData *ld = component(R, node);
	if (ld) ld->item_invoked_enabled = v->u.b;
	return true;
}

HANDLER(list_rectsel) {
	( void ) control;
	FluxListViewData *ld = component(R, node);
	if (ld) flux_list_band_enable(ld, v->u.b);
	return true;
}

HANDLER(list_groups) {
	( void ) control;
	return flux_list_groups(R->store, node, v->u.bytes.data, ( int ) (v->u.bytes.len / sizeof(int32_t)));
}

HANDLER(list_names) {
	( void ) control;
	return flux_list_names(R->store, node, v->u.bytes.data, v->u.bytes.len);
}

SETTER(list_selected, flux_list_set_selected, v->u.i32)
SETTER(list_first, flux_list_stage_first, v->u.i32)
SETTER(list_mode, flux_list_setmode, ( XtkListSelMode ) v->u.u32)

FluxPropHandler const flux_props_list [] = {
  ON(ITEM_COUNT, I32, list_count),
  ON(ITEM_HEIGHT, F32, list_height),
  ON(ITEM_WIDTH, F32, list_width),
  ON(SELECTED_INDEX, I32, list_selected),
  ON(REALIZED_FIRST, I32, list_first),
  ON(COLUMN_COUNT, I32, list_columns),
  ON(SELECTION_MODE, ENUM, list_mode),
  ON(GROUPS, BYTES, list_groups),
  ON(ITEMS, BYTES, list_names),
  ON(RECTANGLE_SELECTION, BOOL, list_rectsel),
  END,
};

FluxPropHandler const flux_props_itemsview [] = {
  ON(ITEM_COUNT, I32, list_count),
  ON(ITEM_HEIGHT, F32, list_height),
  ON(ITEM_WIDTH, F32, list_width),
  ON(SELECTED_INDEX, I32, list_selected),
  ON(REALIZED_FIRST, I32, list_first),
  ON(COLUMN_COUNT, I32, list_columns),
  ON(SELECTION_MODE, ENUM, list_mode),
  ON(ITEM_INVOKED_ENABLED, BOOL, list_invoked),
  END,
};

HANDLER(listitem_selected) {
	( void ) control;
	fluxlistitem_set_state(R->store, node, v->u.b, false);
	return true;
}

FluxPropHandler const flux_props_listitem [] = {
  ON(SELECTED, BOOL, listitem_selected),
  END,
};

typedef struct FluxWireItems {
	char const **items;
	int          count;
} FluxWireItems;

static bool wire_items_open(XtkValue const *v, FluxWireItems *out) {
	uint8_t const *data  = v->u.bytes.data;
	uint32_t       bytes = v->u.bytes.len;
	int            count = 0;
	for (uint32_t at = 0u; at < bytes;) {
		uint8_t const *end = memchr(data + at, 0, bytes - at);
		if (!end || count == INT_MAX) return false;
		at = ( uint32_t ) (end - data) + 1u;
		count++;
	}
	char const **items = count ? ( char const ** ) malloc(sizeof(*items) * ( size_t ) count) : NULL;
	if (count && !items) return false;
	uint32_t at = 0u;
	for (int i = 0; i < count; i++) {
		items [i]  = ( char const * ) data + at;
		at        += ( uint32_t ) strlen(items [i]) + 1u;
	}
	*out = (FluxWireItems) {.items = items, .count = count};
	return true;
}

#define WIRE_ITEMS(name, apply)                      \
	HANDLER(name) {                                  \
		FluxWireItems wire = {0};                    \
		if (!wire_items_open(v, &wire)) return true; \
		apply(R, control, node, &wire);              \
		free(wire.items);                            \
		return true;                                 \
	}

static void combo_items(FluxHostState *R, XtkCtrl control, XentNodeId node, FluxWireItems const *wire) {
	flux_prop_combo_items(R->store, node, wire->items, wire->count);
	flux_prop_combo_selected(R->store, node, stored_selected(R, control));
}

static void looping_items(FluxHostState *R, XtkCtrl control, XentNodeId node, FluxWireItems const *wire) {
	flux_looping_setitems(R->store, node, wire->items, wire->count);
	flux_looping_setselected(R->store, node, stored_selected(R, control));
}

static void suggest_items(FluxHostState *R, XtkCtrl control, XentNodeId node, FluxWireItems const *wire) {
	( void ) control;
	flux_suggest_set_suggestions(R->store, node, wire->items, wire->count);
}

static void breadcrumb_items(FluxHostState *R, XtkCtrl control, XentNodeId node, FluxWireItems const *wire) {
	( void ) control;
	flux_breadcrumb_set_items(R->store, node, wire->items, wire->count);
}

static void tabs_items(FluxHostState *R, XtkCtrl control, XentNodeId node, FluxWireItems const *wire) {
	( void ) control;
	if ((wire->count & 1) != 0 || wire->count / 2 > FLUX_TAB_VIEW_MAX_TABS) return;
	char const *icons [FLUX_TAB_VIEW_MAX_TABS];
	char const *labels [FLUX_TAB_VIEW_MAX_TABS];
	for (int i = 0; i < wire->count / 2; i++) {
		icons [i]  = wire->items [i * 2][0] ? wire->items [i * 2] : NULL;
		labels [i] = wire->items [i * 2 + 1];
	}
	flux_tabs_set_items(R->store, node, icons, labels, wire->count / 2);
}

static void selector_items(FluxHostState *R, XtkCtrl control, XentNodeId node, FluxWireItems const *wire) {
	if (wire->count % 3 != 0) return;
	int              count = wire->count / 3;
	XtkSelectorItem *items = count ? ( XtkSelectorItem * ) malloc(sizeof(*items) * ( size_t ) count) : NULL;
	if (count && !items) return;
	for (int i = 0; i < count; i++)
		items [i] = (XtkSelectorItem) {
		  .icon     = wire->items [i * 3],
		  .text     = wire->items [i * 3 + 1],
		  .disabled = wire->items [i * 3 + 2][0] == '1',
		};
	flux_selector_set_items(R->store, node, items, count, stored_selected(R, control));
	free(items);
}

WIRE_ITEMS(combo_items_prop, combo_items)
WIRE_ITEMS(looping_items_prop, looping_items)
WIRE_ITEMS(suggest_items_prop, suggest_items)
WIRE_ITEMS(breadcrumb_items_prop, breadcrumb_items)
WIRE_ITEMS(tabs_items_prop, tabs_items)
WIRE_ITEMS(selector_items_prop, selector_items)

SETTER(combo_text, flux_prop_combo_text, str_or_empty(v))
SETTER(combo_placeholder, flux_prop_combo_placeholder, str_or_empty(v))
SETTER(combo_editable, flux_prop_combo_editable, v->u.b)
SETTER(combo_selected, flux_prop_combo_selected, v->u.i32)

FluxPropHandler const flux_props_combo [] = {
  ON(TEXT, STRING, combo_text),
  ON(PLACEHOLDER, STRING, combo_placeholder),
  ON(IS_EDITABLE, BOOL, combo_editable),
  ON(ITEMS, BYTES, combo_items_prop),
  ON(SELECTED_INDEX, I32, combo_selected),
  END,
};

SETTER(looping_enabled, flux_looping_setenabled, v->u.b)
SETTER(looping_selected, flux_looping_setselected, v->u.i32)
SETTER(looping_loop, flux_looping_setloop, v->u.b)
SETTER(looping_height, flux_looping_setitemheight, v->u.f32)
SETTER(looping_width, flux_looping_setitemwidth, v->u.f32)

FluxPropHandler const flux_props_looping [] = {
  ON(ENABLED, BOOL, looping_enabled),
  ON(ITEMS, BYTES, looping_items_prop),
  ON(SELECTED_INDEX, I32, looping_selected),
  ON(SHOULD_LOOP, BOOL, looping_loop),
  ON(ITEM_HEIGHT, F32, looping_height),
  ON(ITEM_WIDTH, F32, looping_width),
  END,
};

SETTER(suggest_content, flux_suggest_set_content, str_or_empty(v))
SETTER(suggest_icon, flux_suggest_set_query_icon, str_or_empty(v))
SETTER(suggest_enabled, flux_suggest_set_enabled, v->u.b)
SETTER(suggest_placeholder, flux_suggest_set_placeholder, str_or_empty(v))

FluxPropHandler const flux_props_suggest [] = {
  ON(TEXT, STRING, suggest_content),
  ON(ICON, STRING, suggest_icon),
  ON(ENABLED, BOOL, suggest_enabled),
  ON(PLACEHOLDER, STRING, suggest_placeholder),
  ON(ITEMS, BYTES, suggest_items_prop),
  END,
};

SETTER(breadcrumb_disabled, flux_breadcrumb_set_disabled, !v->u.b)

FluxPropHandler const flux_props_breadcrumb [] = {
  ON(ENABLED, BOOL, breadcrumb_disabled),
  ON(ITEMS, BYTES, breadcrumb_items_prop),
  END,
};

SETTER(tabs_select, flux_tabs_select, v->u.i32)
SETTER(tabs_add_visible, flux_tabs_setaddvisible, v->u.b)

FluxPropHandler const flux_props_tabs [] = {
  ON(ITEMS, BYTES, tabs_items_prop),
  ON(SELECTED_INDEX, I32, tabs_select),
  ON(ADD_VISIBLE, BOOL, tabs_add_visible),
  END,
};

SETTER(selector_selected, flux_selector_set_selected, v->u.i32)

FluxPropHandler const flux_props_selector [] = {
  ON(ITEMS, BYTES, selector_items_prop),
  ON(SELECTED_INDEX, I32, selector_selected),
  END,
};

HANDLER(tree_items) {
	XtkTreeModelView model;
	if (!xtk_treemodel_open(v->u.bytes.data, v->u.bytes.len, &model) || !flux_tree_set_model(R->store, node, &model))
		return true;
	int flat = stored_selected(R, control);
	flux_tree_set_selected(R->store, node, flat < 0 ? -1 : flux_tree_flat_node(R->store, node, flat), true);
	return true;
}

HANDLER(tree_height) {
	( void ) control;
	FluxTreeViewData *td = component(R, node);
	if (td) td->row_height = v->u.f32 > 0.0f ? v->u.f32 : FLUX_TREE_ROW_H;
	return true;
}

HANDLER(tree_selected) {
	( void ) control;
	int selected = v->u.i32 < 0 ? -1 : flux_tree_flat_node(R->store, node, v->u.i32);
	flux_tree_set_selected(R->store, node, selected, true);
	return true;
}

SETTER(tree_mode, flux_tree_setmode, ( XtkTreeSelMode ) v->u.u32)

FluxPropHandler const flux_props_tree [] = {
  ON(ITEMS, BYTES, tree_items),
  ON(ITEM_HEIGHT, F32, tree_height),
  ON(SELECTED_INDEX, I32, tree_selected),
  ON(SELECTION_MODE, ENUM, tree_mode),
  END,
};

HANDLER(treeitem_text) {
	( void ) control;
	xent_settext(R->ctx, node, str_or_empty(v));
	return true;
}

HANDLER(treeitem_expanded) {
	( void ) control;
	xent_sem_setexpanded(R->ctx, node, v->u.b);
	return true;
}

FluxPropHandler const flux_props_treeitem [] = {
  ON(TEXT, STRING, treeitem_text),
  ON(EXPANDED, BOOL, treeitem_expanded),
  END,
};

SETTER(radiogroup_enabled, flux_radiogroup_set_enabled, v->u.b)
SETTER(radiogroup_selected, flux_radiogroup_set_selected, v->u.i32)

HANDLER(radiogroup_columns) {
	( void ) control;
	FluxRadioButtonsData *g = component(R, node);
	if (g) g->columns = v->u.i32 > 0 ? v->u.i32 : 1;
	return true;
}

FluxPropHandler const flux_props_radiogroup [] = {
  ON(ENABLED, BOOL, radiogroup_enabled),
  ON(SELECTED_INDEX, I32, radiogroup_selected),
  ON(COLUMN_COUNT, I32, radiogroup_columns),
  END,
};

static void badge_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_badge_configure(
	  R->store, node, ( FluxInfoBadgeMode ) stored_enum(R, control, XTK_PROP_BADGE_MODE),
	  stored_i32(R, control, XTK_PROP_BADGE_VALUE), stored_string(R, control, XTK_PROP_ICON)
	);
}

CONFIG(badge_config_prop, badge_config)

FluxPropHandler const flux_props_badge [] = {
  ON(ICON, STRING, badge_config_prop),
  ON(BADGE_MODE, ENUM, badge_config_prop),
  ON(BADGE_VALUE, I32, badge_config_prop),
  END,
};

SETTER(expander_width, flux_prop_expander_width, v->u.f32)
SETTER(expander_header, flux_prop_expander_header, str_or_empty(v))
SETTER(expander_content_height, flux_prop_expander_content_height, v->u.f32)

HANDLER(expander_expanded) {
	( void ) control;
	flux_prop_expander_expanded(R->store, node, R->ctx, v->u.b);
	return true;
}

FluxPropHandler const flux_props_expander [] = {
  ON(WIDTH, F32, expander_width),
  ON(TEXT, STRING, expander_header),
  ON(EXPANDED, BOOL, expander_expanded),
  ON(CONTENT_HEIGHT, F32, expander_content_height),
  END,
};

SETTER(infobar_title, flux_prop_infobar_title, str_or_empty(v))
SETTER(infobar_message, flux_prop_infobar_message, str_or_empty(v))
SETTER(infobar_severity, flux_prop_infobar_severity, ( FluxInfoBarSeverity ) v->u.u32)
SETTER(infobar_closable, flux_prop_infobar_closable, v->u.b)

FluxPropHandler const flux_props_infobar [] = {
  ON(TEXT, STRING, infobar_title),
  ON(MESSAGE, STRING, infobar_message),
  ON(SEVERITY, ENUM, infobar_severity),
  ON(CLOSABLE, BOOL, infobar_closable),
  END,
};

static void dialog_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	float width   = stored_f32(R, control, XTK_PROP_WIDTH);
	float content = stored_f32(R, control, XTK_PROP_CONTENT_HEIGHT);
	flux_dialog_configure(
	  R->store, node, stored_string(R, control, XTK_PROP_TEXT), stored_string(R, control, XTK_PROP_PRIMARY_TEXT),
	  stored_string(R, control, XTK_PROP_SECONDARY_TEXT), stored_string(R, control, XTK_PROP_CLOSE_TEXT),
	  width > 0.0f ? width : 400.0f, content > 0.0f ? content : 120.0f
	);
}

CONFIG(dialog_config_prop, dialog_config)
SETTER(dialog_open, flux_dialog_sync_host, v->u.b)

FluxPropHandler const flux_props_dialog [] = {
  ON(WIDTH, F32, dialog_config_prop),
  ON(TEXT, STRING, dialog_config_prop),
  ON(CONTENT_HEIGHT, F32, dialog_config_prop),
  ON(PRIMARY_TEXT, STRING, dialog_config_prop),
  ON(SECONDARY_TEXT, STRING, dialog_config_prop),
  ON(CLOSE_TEXT, STRING, dialog_config_prop),
  ON(OPEN, BOOL, dialog_open),
  END,
};

static void teachtip_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_teachtip_configure(
	  R->store, node, stored_string(R, control, XTK_PROP_ICON), stored_string(R, control, XTK_PROP_ACTION_TEXT),
	  stored_bool(R, control, XTK_PROP_ACTION_ACCENT), stored_string(R, control, XTK_PROP_CLOSE_TEXT),
	  stored_bool(R, control, XTK_PROP_LIGHT_DISMISS), ( int ) stored_enum(R, control, XTK_PROP_TAIL_VISIBILITY),
	  stored_f32(R, control, XTK_PROP_PLACEMENT_MARGIN)
	);
}

CONFIG(teachtip_config_prop, teachtip_config)

HANDLER(teachtip_text) {
	flux_teachtip_set_texts(R->store, node, str_or_empty(v), stored_string(R, control, XTK_PROP_SUBTITLE));
	return true;
}

HANDLER(teachtip_subtitle) {
	flux_teachtip_set_texts(R->store, node, stored_string(R, control, XTK_PROP_TEXT), str_or_empty(v));
	return true;
}

HANDLER(teachtip_open) {
	( void ) control;
	if (v->u.b) flux_teachtip_show(R->store, node);
	else flux_teachtip_hide(R->store, node);
	return true;
}

HANDLER(teachtip_untargeted) {
	( void ) control;
	flux_teachtip_set_target(R->store, node, v->u.b ? XENT_NODE_INVALID : xent_node_parent(R->ctx, node));
	return true;
}

SETTER(teachtip_placement, flux_teachtip_set_placement, ( FluxTeachingTipPlacement ) v->u.u32)

FluxPropHandler const flux_props_teachtip [] = {
  ON(TEXT, STRING, teachtip_text),
  ON(SUBTITLE, STRING, teachtip_subtitle),
  ON(ICON, STRING, teachtip_config_prop),
  ON(CLOSE_TEXT, STRING, teachtip_config_prop),
  ON(ACTION_TEXT, STRING, teachtip_config_prop),
  ON(TAIL_VISIBILITY, ENUM, teachtip_config_prop),
  ON(ACTION_ACCENT, BOOL, teachtip_config_prop),
  ON(LIGHT_DISMISS, BOOL, teachtip_config_prop),
  ON(PLACEMENT_MARGIN, F32, teachtip_config_prop),
  ON(OPEN, BOOL, teachtip_open),
  ON(PLACEMENT, ENUM, teachtip_placement),
  ON(UNTARGETED, BOOL, teachtip_untargeted),
  END,
};

SETTER(menufly_text, flux_host_menufly_set_text, str_or_empty(v))

FluxPropHandler const flux_props_menufly [] = {
  ON(TEXT, STRING, menufly_text),
  END,
};

SETTER(menuitem_text, flux_host_menuitem_set_text, str_or_empty(v))
SETTER(menuitem_icon, flux_host_menuitem_set_icon, str_or_empty(v))
SETTER(menuitem_enabled, flux_host_menuitem_set_enabled, v->u.b)

FluxPropHandler const flux_props_menuitem [] = {
  ON(TEXT, STRING, menuitem_text),
  ON(ICON, STRING, menuitem_icon),
  ON(ENABLED, BOOL, menuitem_enabled),
  END,
};

static int nav_append(FluxHostState *R, XentNodeId node, XtkNavModelItemView const *item) {
	switch (item->kind) {
	case XTK_NAV_ITEM      : return flux_nav_add_item(R->store, node, item->icon, item->label);
	case XTK_NAV_HEADER    : return flux_nav_add_header(R->store, node, item->label);
	case XTK_NAV_SEPARATOR : return flux_nav_add_separator(R->store, node);
	case XTK_NAV_FOOTER    : return flux_nav_addfooter(R->store, node, item->icon, item->label);
	case XTK_NAV_CHILD     : return flux_nav_addchild(R->store, node, item->parent, item->icon, item->label);
	default                : return -1;
	}
}

static bool nav_load(FluxHostState *R, XentNodeId node, XtkNavModelView const *model) {
	if (model->count > FLUX_NAV_VIEW_MAX_ITEMS || !flux_nav_reset_items(R->store, node)) return false;
	bool title_bar = (model->flags & XTK_NAV_MODEL_WINDOW_TITLE_BAR) != 0u;
	if (!flux_nav_set_chrome(R->store, node, title_bar, model->app_title)) return false;
	for (uint32_t i = 0; i < model->count; i++) {
		XtkNavModelItemView item;
		if (!xtk_navmodel_item(model, i, &item) || nav_append(R, node, &item) < 0) return false;
	}
	return true;
}

HANDLER(nav_model) {
	( void ) control;
	XtkNavModelView model;
	if (xtk_navmodel_open(v->u.bytes.data, v->u.bytes.len, &model)) ( void ) nav_load(R, node, &model);
	return true;
}

SETTER(nav_selected, flux_nav_set_selected, v->u.i32)

FluxPropHandler const flux_props_nav [] = {
  ON(SELECTED_INDEX, I32, nav_selected),
  ON(NAV_MODEL, BYTES, nav_model),
  END,
};

SETTER(flip_vertical, flux_flip_set_vertical, v->u.u32 != 0u)
SETTER(flip_select, flux_flip_select, v->u.i32)

FluxPropHandler const flux_props_flip [] = {
  ON(ORIENTATION, ENUM, flip_vertical),
  ON(SELECTED_INDEX, I32, flip_select),
  END,
};

SETTER(pips_vertical, flux_pips_set_vertical, v->u.u32 != 0u)
SETTER(pips_count, flux_pips_set_count, v->u.i32)
SETTER(pips_selected, flux_pips_set_selected, v->u.i32)
SETTER(pips_max_visible, flux_pips_set_max_visible, v->u.i32)
SETTER(pips_nav_visibility, flux_pips_set_nav_visibility, ( int ) v->u.u32)

FluxPropHandler const flux_props_pips [] = {
  ON(ORIENTATION, ENUM, pips_vertical),
  ON(ITEM_COUNT, I32, pips_count),
  ON(SELECTED_INDEX, I32, pips_selected),
  ON(MAX_VISIBLE, I32, pips_max_visible),
  ON(NAV_VISIBILITY, ENUM, pips_nav_visibility),
  END,
};

static void rating_behavior(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_rating_setbehavior(
	  R->store, node, stored_f64(R, control, XTK_PROP_PLACEHOLDER_F64, -1.0),
	  stored_i32(R, control, XTK_PROP_INITIAL_VALUE), stored_bool(R, control, XTK_PROP_CLEAR_ENABLED)
	);
}

CONFIG(rating_behavior_prop, rating_behavior)
SETTER(rating_caption, flux_rating_set_caption, str_or_empty(v))
SETTER(rating_value, flux_rating_set_value, v->u.f64)
SETTER(rating_max, flux_rating_set_max, ( int ) v->u.f64)
SETTER(rating_readonly, flux_rating_setreadonly, v->u.b)

FluxPropHandler const flux_props_rating [] = {
  ON(TEXT, STRING, rating_caption),
  ON(VALUE_F64, F64, rating_value),
  ON(MAX_F64, F64, rating_max),
  ON(READ_ONLY, BOOL, rating_readonly),
  ON(CLEAR_ENABLED, BOOL, rating_behavior_prop),
  ON(PLACEHOLDER_F64, F64, rating_behavior_prop),
  ON(INITIAL_VALUE, I32, rating_behavior_prop),
  END,
};

SETTER(refresh_direction, flux_refresh_set_direction, ( FluxPullDirection ) v->u.u32)
SETTER(refresh_visualizer, flux_refresh_set_visualizer_size, v->u.f32)
SETTER(refresh_threshold, flux_refresh_set_threshold_ratio, v->u.f32)
SETTER(refresh_refreshing, flux_refresh_set_refreshing, v->u.b)

FluxPropHandler const flux_props_refresh [] = {
  ON(PULL_DIRECTION, ENUM, refresh_direction),
  ON(VISUALIZER_SIZE, F32, refresh_visualizer),
  ON(THRESHOLD_RATIO, F32, refresh_threshold),
  ON(REFRESHING, BOOL, refresh_refreshing),
  END,
};

HANDLER(avatar_diameter) {
	( void ) control;
	FluxPersonPictureData *d = component(R, node);
	if (!d) return false;
	d->diameter = v->u.f32;
	xent_setsize(R->ctx, node, (XentSize) {d->diameter, d->diameter});
	return true;
}

static void avatar_badge(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_avatar_set_badge(
	  R->store, node, stored_string(R, control, XTK_PROP_BADGE_GLYPH), stored_i32(R, control, XTK_PROP_BADGE_VALUE)
	);
}

CONFIG(avatar_badge_prop, avatar_badge)
SETTER(avatar_name, flux_avatar_setname, str_or_empty(v))
SETTER(avatar_image, flux_avatar_set_image, flux_resource_path(( XentResourceId ) v->u.resource))
SETTER(avatar_initials, flux_avatar_set_initials, str_or_empty(v))
SETTER(avatar_group, flux_avatar_set_group, v->u.b)

FluxPropHandler const flux_props_avatar [] = {
  ON(WIDTH, F32, avatar_diameter),
  ON(HEIGHT, F32, avatar_diameter),
  ON(TEXT, STRING, avatar_name),
  ON(IMAGE_SOURCE, RESOURCE, avatar_image),
  ON(INITIALS, STRING, avatar_initials),
  ON(BADGE_GLYPH, STRING, avatar_badge_prop),
  ON(BADGE_VALUE, I32, avatar_badge_prop),
  ON(IS_GROUP, BOOL, avatar_group),
  END,
};

static void pager_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_pager_configure(
	  R->store, node, stored_i32(R, control, XTK_PROP_ITEM_COUNT), stored_i32(R, control, XTK_PROP_SELECTED_INDEX),
	  ( int ) stored_enum(R, control, XTK_PROP_DISPLAY_MODE)
	);
}

static void pager_buttons(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_pager_setbuttons(
	  R->store, node, ( int ) stored_enum(R, control, XTK_PROP_FIRST_BUTTON),
	  ( int ) stored_enum(R, control, XTK_PROP_PREVIOUS_BUTTON), ( int ) stored_enum(R, control, XTK_PROP_NEXT_BUTTON),
	  ( int ) stored_enum(R, control, XTK_PROP_LAST_BUTTON)
	);
}

static void pager_text(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_pager_settext(
	  R->store, node, stored_string(R, control, XTK_PROP_PREFIX), stored_string(R, control, XTK_PROP_SUFFIX)
	);
}

HANDLER(pager_count) {
	( void ) control;
	FluxPagerData *pd = component(R, node);
	if (pd) flux_pager_configure(R->store, node, v->u.i32, pd->selected, pd->display_mode);
	return true;
}

CONFIG(pager_config_prop, pager_config)
CONFIG(pager_buttons_prop, pager_buttons)
CONFIG(pager_text_prop, pager_text)
SETTER(pager_selected, flux_pager_set_selected, v->u.i32)

FluxPropHandler const flux_props_pager [] = {
  ON(ITEM_COUNT, I32, pager_count),
  ON(SELECTED_INDEX, I32, pager_selected),
  ON(DISPLAY_MODE, ENUM, pager_config_prop),
  ON(FIRST_BUTTON, ENUM, pager_buttons_prop),
  ON(PREVIOUS_BUTTON, ENUM, pager_buttons_prop),
  ON(NEXT_BUTTON, ENUM, pager_buttons_prop),
  ON(LAST_BUTTON, ENUM, pager_buttons_prop),
  ON(PREFIX, STRING, pager_text_prop),
  ON(SUFFIX, STRING, pager_text_prop),
  END,
};

static void splitview_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_splitview_configure(
	  R->store, node, ( int ) stored_enum(R, control, XTK_PROP_DISPLAY_MODE),
	  ( int ) stored_enum(R, control, XTK_PROP_PANE_PLACEMENT), stored_f32(R, control, XTK_PROP_OPEN_PANE_LENGTH),
	  stored_f32(R, control, XTK_PROP_COMPACT_PANE_LENGTH)
	);
}

CONFIG(splitview_config_prop, splitview_config)
SETTER(splitview_open, flux_splitview_setopen, v->u.b)

FluxPropHandler const flux_props_splitview [] = {
  ON(OPEN, BOOL, splitview_open),
  ON(DISPLAY_MODE, ENUM, splitview_config_prop),
  ON(PANE_PLACEMENT, ENUM, splitview_config_prop),
  ON(OPEN_PANE_LENGTH, F32, splitview_config_prop),
  ON(COMPACT_PANE_LENGTH, F32, splitview_config_prop),
  END,
};

static void titlebar_back(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_titlebar_set_back(
	  R->store, node, stored_bool(R, control, XTK_PROP_SHOW_BACK), stored_bool(R, control, XTK_PROP_BACK_DISABLED)
	);
}

HANDLER(titlebar_title) {
	( void ) control;
	FluxTitleBarData *d = component(R, node);
	flux_titlebar_set_title(R->store, node, str_or_empty(v), d ? d->subtitle : NULL);
	return true;
}

HANDLER(titlebar_subtitle) {
	( void ) control;
	FluxTitleBarData *d = component(R, node);
	flux_titlebar_set_title(R->store, node, d ? d->title : NULL, str_or_empty(v));
	return true;
}

CONFIG(titlebar_back_prop, titlebar_back)
SETTER(titlebar_icon, flux_titlebar_set_icon, str_or_empty(v))
SETTER(titlebar_toggle, flux_titlebar_settoggle, v->u.b)
SETTER(titlebar_integrate, flux_titlebar_set_window_integration, v->u.b)

FluxPropHandler const flux_props_titlebar [] = {
  ON(TEXT, STRING, titlebar_title),
  ON(SUBTITLE, STRING, titlebar_subtitle),
  ON(ICON, STRING, titlebar_icon),
  ON(CHECKED, BOOL, titlebar_toggle),
  ON(SHOW_PANE_TOGGLE, BOOL, titlebar_toggle),
  ON(SHOW_BACK, BOOL, titlebar_back_prop),
  ON(BACK_DISABLED, BOOL, titlebar_back_prop),
  ON(INTEGRATE_WINDOW, BOOL, titlebar_integrate),
  END,
};

HANDLER(scrollbar_orientation) {
	( void ) control;
	flux_scroll_bar_setorientation(R->ctx, R->store, node, v->u.u32);
	return true;
}

SETTER(scrollbar_enabled, flux_scroll_bar_setenabled, v->u.b)
SETTER(scrollbar_value, flux_scroll_bar_setvalue, v->u.f64)
SETTER(scrollbar_max, flux_scroll_bar_setmaximum, v->u.f64)
SETTER(scrollbar_min, flux_scroll_bar_setminimum, v->u.f64)
SETTER(scrollbar_small, flux_scroll_bar_setsmallchange, v->u.f32)
SETTER(scrollbar_large, flux_scroll_bar_setlargechange, v->u.f32)
SETTER(scrollbar_viewport, flux_scroll_bar_setviewport, v->u.f64)
SETTER(scrollbar_indicator, flux_scroll_bar_setindicator, v->u.u32)

FluxPropHandler const flux_props_scrollbar [] = {
  ON(ORIENTATION, ENUM, scrollbar_orientation),
  ON(ENABLED, BOOL, scrollbar_enabled),
  ON(VALUE_F64, F64, scrollbar_value),
  ON(MAX_F64, F64, scrollbar_max),
  ON(MIN_F64, F64, scrollbar_min),
  ON(SMALL_CHANGE, F32, scrollbar_small),
  ON(LARGE_CHANGE, F32, scrollbar_large),
  ON(VIEWPORT_SIZE, F64, scrollbar_viewport),
  ON(INDICATOR_MODE, ENUM, scrollbar_indicator),
  END,
};

SETTER(picker_enabled, flux_picker_setenabled, v->u.b)
SETTER(picker_header, flux_picker_setheader, str_or_empty(v))
SETTER(datepicker_date, flux_datepicker_setdate, v->u.i32)
SETTER(datepicker_min_year, flux_datepicker_setminyear, v->u.i32)
SETTER(datepicker_max_year, flux_datepicker_setmaxyear, v->u.i32)
SETTER(timepicker_time, flux_timepicker_settime, v->u.i32)
SETTER(timepicker_clock, flux_timepicker_setclock, ( int ) v->u.u32)
SETTER(timepicker_step, flux_timepicker_setstep, v->u.i32)

FluxPropHandler const flux_props_datepicker [] = {
  ON(ENABLED, BOOL, picker_enabled),
  ON(HEADER, STRING, picker_header),
  ON(DATE, I32, datepicker_date),
  ON(MIN_YEAR, I32, datepicker_min_year),
  ON(MAX_YEAR, I32, datepicker_max_year),
  END,
};

FluxPropHandler const flux_props_timepicker [] = {
  ON(ENABLED, BOOL, picker_enabled),
  ON(HEADER, STRING, picker_header),
  ON(TIME, I32, timepicker_time),
  ON(CLOCK, ENUM, timepicker_clock),
  ON(MINUTE_INCREMENT, I32, timepicker_step),
  END,
};

#define FORWARD(name, fn, id)               \
	HANDLER(name) {                         \
		( void ) control;                   \
		( void ) fn(R->store, node, id, v); \
		return true;                        \
	}

FORWARD(calendar_mode, flux_calendar_set, XTK_PROP_SELECTION_MODE)
FORWARD(calendar_date, flux_calendar_set, XTK_PROP_DATE)
FORWARD(calendar_min_date, flux_calendar_set, XTK_PROP_MIN_DATE)
FORWARD(calendar_max_date, flux_calendar_set, XTK_PROP_MAX_DATE)
FORWARD(calendar_first_day, flux_calendar_set, XTK_PROP_FIRST_DAY)

FluxPropHandler const flux_props_calendar [] = {
  ON(SELECTION_MODE, ENUM, calendar_mode),
  ON(DATE, I32, calendar_date),
  ON(MIN_DATE, I32, calendar_min_date),
  ON(MAX_DATE, I32, calendar_max_date),
  ON(FIRST_DAY, I32, calendar_first_day),
  END,
};

FORWARD(colorpicker_color, flux_colorpicker_set, XTK_PROP_COLOR)
FORWARD(colorpicker_alpha, flux_colorpicker_set, XTK_PROP_ALPHA_ENABLED)
FORWARD(colorpicker_more, flux_colorpicker_set, XTK_PROP_MORE_BUTTON_VISIBLE)
FORWARD(colorpicker_spectrum, flux_colorpicker_set, XTK_PROP_SPECTRUM)

FluxPropHandler const flux_props_colorpicker [] = {
  ON(COLOR, COLOR, colorpicker_color),
  ON(ALPHA_ENABLED, BOOL, colorpicker_alpha),
  ON(MORE_BUTTON_VISIBLE, BOOL, colorpicker_more),
  ON(SPECTRUM, ENUM, colorpicker_spectrum),
  END,
};

SETTER(colitem_text, flux_colitem_settext, str_or_empty(v))
SETTER(colitem_width, flux_colitem_setwidth, v->u.f32)
SETTER(colitem_min, flux_colitem_setmin, v->u.f32)
SETTER(colitem_max, flux_colitem_setmax, v->u.f32)
SETTER(colitem_align, flux_colitem_setalign, v->u.u32)
SETTER(colitem_sort, flux_colitem_setsort, v->u.u32)
SETTER(colitem_sortable, flux_colitem_setsortable, v->u.b)
SETTER(colitem_resizable, flux_colitem_setresizable, v->u.b)

FluxPropHandler const flux_props_colitem [] = {
  ON(TEXT, STRING, colitem_text),
  ON(WIDTH, F32, colitem_width),
  ON(MIN_WIDTH, F32, colitem_min),
  ON(MAX_WIDTH, F32, colitem_max),
  ON(TEXT_ALIGN, ENUM, colitem_align),
  ON(SORT_DIRECTION, ENUM, colitem_sort),
  ON(ENABLED, BOOL, colitem_sortable),
  ON(RESIZABLE, BOOL, colitem_resizable),
  END,
};

SETTER(cmdbar_open, flux_cmdbar_setopen, v->u.b)
SETTER(cmdbar_sticky, flux_cmdbar_setsticky, v->u.b)
SETTER(cmdbar_mode, flux_cmdbar_setmode, v->u.u32)
SETTER(cmdbar_labels, flux_cmdbar_setlabels, v->u.u32)
SETTER(cmdbar_more, flux_cmdbar_setmore, v->u.u32)
SETTER(cmdbar_dynamic, flux_cmdbar_setdynamic, v->u.b)
SETTER(cmdbar_enabled, flux_cmdbar_setenabled, v->u.b)

FluxPropHandler const flux_props_commandbar [] = {
  ON(OPEN, BOOL, cmdbar_open),
  ON(IS_STICKY, BOOL, cmdbar_sticky),
  ON(CLOSED_DISPLAY_MODE, ENUM, cmdbar_mode),
  ON(DEFAULT_LABEL_POSITION, ENUM, cmdbar_labels),
  ON(OVERFLOW_BUTTON_VISIBILITY, ENUM, cmdbar_more),
  ON(DYNAMIC_OVERFLOW, BOOL, cmdbar_dynamic),
  ON(ENABLED, BOOL, cmdbar_enabled),
  END,
};

SETTER(appbar_text, flux_appbar_settext, str_or_empty(v))
SETTER(appbar_icon, flux_appbar_seticon, str_or_empty(v))
SETTER(appbar_accel, flux_appbar_setaccel, str_or_empty(v))
SETTER(appbar_enabled, flux_appbar_setenabled, v->u.b)
SETTER(appbar_checked, flux_appbar_setchecked, v->u.b)
SETTER(appbar_secondary, flux_appbar_setsecondary, v->u.b)
SETTER(appbar_order, flux_appbar_setorder, v->u.i32)
SETTER(appbar_labelpos, flux_appbar_setlabelpos, v->u.u32)
SETTER(appbar_width, flux_appbar_setwidth, v->u.f32)

FluxPropHandler const flux_props_appbar [] = {
  ON(TEXT, STRING, appbar_text),
  ON(ICON, STRING, appbar_icon),
  ON(ACCELERATOR_TEXT, STRING, appbar_accel),
  ON(ENABLED, BOOL, appbar_enabled),
  ON(CHECKED, BOOL, appbar_checked),
  ON(SECONDARY, BOOL, appbar_secondary),
  ON(DYNAMIC_OVERFLOW_ORDER, I32, appbar_order),
  ON(LABEL_POSITION, ENUM, appbar_labelpos),
  ON(WIDTH, F32, appbar_width),
  END,
};

static FluxPropHandler const core_props [] = {
  ON(WIDTH, F32, prop_width),
  ON(HEIGHT, F32, prop_height),
  ON(GROW, F32, prop_grow),
  ON(SHRINK, F32, prop_shrink),
  ON(ALIGN_SELF, ENUM, prop_align_self),
  ON(FLOW_DIRECTION, ENUM, prop_flow),
  ON(MARGIN_LEFT, F32, prop_margin),
  ON(MARGIN_TOP, F32, prop_margin),
  ON(MARGIN_RIGHT, F32, prop_margin),
  ON(MARGIN_BOTTOM, F32, prop_margin),
  ON(PADDING_LEFT, F32, prop_padding),
  ON(PADDING_TOP, F32, prop_padding),
  ON(PADDING_RIGHT, F32, prop_padding),
  ON(PADDING_BOTTOM, F32, prop_padding),
  ON(ORIENTATION, ENUM, prop_orientation),
  ON(ALIGN_ITEMS, ENUM, prop_align_items),
  ON(JUSTIFY_CONTENT, ENUM, prop_justify),
  ON(GAP, F32, prop_gap),
  ON(FILL, BOOL, prop_fill),
  ON(FIT_WIDTH, BOOL, prop_fit_width),
  ON(FIT_HEIGHT, BOOL, prop_fit_height),
  ON(WRAP, ENUM, prop_wrap),
  ON(TOOLTIP, STRING, prop_tooltip),
  ON(DROP_OPERATION, ENUM, prop_drop),
  ON(DRAG_TEXT, STRING, prop_dragtext),
  ON(DRAG_FILES, STRING, prop_dragfiles),
  ON(TEXT, STRING, prop_none),
  ON(ICON, STRING, prop_none),
  ON(ENABLED, BOOL, prop_none),
  ON(BUTTON_STYLE, ENUM, prop_none),
  ON(CHECKED, BOOL, prop_none),
  ON(MIN_F32, F32, prop_none),
  ON(MAX_F32, F32, prop_none),
  ON(VALUE_F32, F32, prop_none),
  ON(VALUE_F64, F64, prop_none),
  ON(MIN_F64, F64, prop_none),
  ON(MAX_F64, F64, prop_none),
  ON(STEP_F32, F32, prop_none),
  ON(TICK_FREQUENCY, F32, prop_none),
  ON(TICK_PLACEMENT, ENUM, prop_none),
  ON(SLIDER_SNAPS_TO, ENUM, prop_none),
  ON(SMALL_CHANGE, F32, prop_none),
  ON(LARGE_CHANGE, F32, prop_none),
  ON(VIEWPORT_SIZE, F64, prop_none),
  ON(INDICATOR_MODE, ENUM, prop_none),
  ON(STEP_F64, F64, prop_none),
  ON(INDETERMINATE, BOOL, prop_none),
  ON(FONT_SIZE, F32, prop_none),
  ON(FONT_WEIGHT, ENUM, prop_none),
  ON(TEXT_ALIGN, ENUM, prop_none),
  ON(FOREGROUND, COLOR, prop_none),
  ON(ITEM_COUNT, I32, prop_none),
  ON(ITEMS, BYTES, prop_none),
  ON(SHOULD_LOOP, BOOL, prop_none),
  ON(ITEM_HEIGHT, F32, prop_none),
  ON(ITEM_WIDTH, F32, prop_none),
  ON(COLUMN_COUNT, I32, prop_none),
  ON(REALIZED_FIRST, I32, prop_none),
  ON(SCROLL_X, F32, prop_none),
  ON(SCROLL_Y, F32, prop_none),
  ON(SELECTED_INDEX, I32, prop_none),
  ON(SELECTED, BOOL, prop_none),
  ON(OPEN, BOOL, prop_none),
  ON(EXPANDED, BOOL, prop_none),
  ON(CONTENT_HEIGHT, F32, prop_none),
  ON(MESSAGE, STRING, prop_none),
  ON(SEVERITY, ENUM, prop_none),
  ON(CLOSABLE, BOOL, prop_none),
  ON(PULL_DIRECTION, ENUM, prop_none),
  ON(VISUALIZER_SIZE, F32, prop_none),
  ON(THRESHOLD_RATIO, F32, prop_none),
  ON(REFRESHING, BOOL, prop_none),
  ON(SELECTION_MODE, ENUM, prop_none),
  ON(ITEM_INVOKED_ENABLED, BOOL, prop_none),
  ON(MAX_VISIBLE, I32, prop_none),
  ON(NAV_VISIBILITY, ENUM, prop_none),
  ON(PLACEHOLDER, STRING, prop_none),
  ON(IMAGE_SOURCE, RESOURCE, prop_none),
  ON(IMAGE_STRETCH, ENUM, prop_none),
  ON(NAV_MODEL, BYTES, prop_none),
  ON(SUBTITLE, STRING, prop_none),
  ON(PRIMARY_TEXT, STRING, prop_none),
  ON(SECONDARY_TEXT, STRING, prop_none),
  ON(CLOSE_TEXT, STRING, prop_none),
  ON(ACTION_TEXT, STRING, prop_none),
  ON(PREFIX, STRING, prop_none),
  ON(SUFFIX, STRING, prop_none),
  ON(INITIALS, STRING, prop_none),
  ON(BADGE_GLYPH, STRING, prop_none),
  ON(BADGE_MODE, ENUM, prop_none),
  ON(BADGE_VALUE, I32, prop_none),
  ON(DISPLAY_MODE, ENUM, prop_none),
  ON(PLACEMENT, ENUM, prop_none),
  ON(TAIL_VISIBILITY, ENUM, prop_none),
  ON(FIRST_BUTTON, ENUM, prop_none),
  ON(PREVIOUS_BUTTON, ENUM, prop_none),
  ON(NEXT_BUTTON, ENUM, prop_none),
  ON(LAST_BUTTON, ENUM, prop_none),
  ON(PANE_PLACEMENT, ENUM, prop_none),
  ON(ACTION_ACCENT, BOOL, prop_none),
  ON(UNTARGETED, BOOL, prop_none),
  ON(LIGHT_DISMISS, BOOL, prop_none),
  ON(IS_GROUP, BOOL, prop_none),
  ON(READ_ONLY, BOOL, prop_none),
  ON(MULTILINE, BOOL, prop_none),
  ON(MAX_LINES, I32, prop_none),
  ON(TEXT_TRIM, ENUM, prop_none),
  ON(TEXT_WRAP, BOOL, prop_none),
  ON(SELECTABLE, BOOL, prop_none),
  ON(CLEAR_ENABLED, BOOL, prop_none),
  ON(BACK_DISABLED, BOOL, prop_none),
  ON(INTEGRATE_WINDOW, BOOL, prop_none),
  ON(HIDE_SPIN, BOOL, prop_none),
  ON(SHOW_BACK, BOOL, prop_none),
  ON(SHOW_PANE_TOGGLE, BOOL, prop_none),
  ON(OPEN_PANE_LENGTH, F32, prop_none),
  ON(COMPACT_PANE_LENGTH, F32, prop_none),
  ON(PLACEMENT_MARGIN, F32, prop_none),
  ON(PLACEHOLDER_F64, F64, prop_none),
  ON(INITIAL_VALUE, I32, prop_none),
  ON(URL, STRING, prop_none),
  ON(ADD_VISIBLE, BOOL, prop_none),
  ON(HEADER, STRING, prop_none),
  ON(DATE, I32, prop_none),
  ON(TIME, I32, prop_none),
  ON(MIN_YEAR, I32, prop_none),
  ON(MAX_YEAR, I32, prop_none),
  ON(MIN_DATE, I32, prop_none),
  ON(MAX_DATE, I32, prop_none),
  ON(FIRST_DAY, I32, prop_none),
  ON(CLOCK, ENUM, prop_none),
  ON(MINUTE_INCREMENT, I32, prop_none),
  ON(COLOR, COLOR, prop_none),
  ON(ALPHA_ENABLED, BOOL, prop_none),
  ON(MORE_BUTTON_VISIBLE, BOOL, prop_none),
  ON(SPECTRUM, ENUM, prop_none),
  ON(IS_THREE_STATE, BOOL, prop_none),
  ON(PROGRESS_STATE, ENUM, prop_none),
  ON(PROGRESS_ACTIVE, BOOL, prop_none),
  ON(PROGRESS_BORDER_WIDTH, F32, prop_none),
  ON(IS_EDITABLE, BOOL, prop_none),
  ON(IS_RANGE, BOOL, prop_none),
  ON(RANGE_START, F32, prop_none),
  ON(RANGE_END, F32, prop_none),
  ON(MIN_WIDTH, F32, prop_none),
  ON(MAX_WIDTH, F32, prop_none),
  ON(RESIZABLE, BOOL, prop_none),
  ON(SORT_DIRECTION, ENUM, prop_none),
  ON(IS_STICKY, BOOL, prop_none),
  ON(CLOSED_DISPLAY_MODE, ENUM, prop_none),
  ON(DEFAULT_LABEL_POSITION, ENUM, prop_none),
  ON(OVERFLOW_BUTTON_VISIBILITY, ENUM, prop_none),
  ON(DYNAMIC_OVERFLOW, BOOL, prop_none),
  ON(SECONDARY, BOOL, prop_none),
  ON(DYNAMIC_OVERFLOW_ORDER, I32, prop_none),
  ON(LABEL_POSITION, ENUM, prop_none),
  ON(ACCELERATOR_TEXT, STRING, prop_none),
  ON(GROUPS, BYTES, prop_none),
  ON(RECTANGLE_SELECTION, BOOL, prop_none),
  ON(NINE_GRID_LEFT, F32, prop_none),
  ON(NINE_GRID_TOP, F32, prop_none),
  ON(NINE_GRID_RIGHT, F32, prop_none),
  ON(NINE_GRID_BOTTOM, F32, prop_none),
  END,
};

static FluxPropHandler const *core_prop(XtkPropPatch const *patch) {
	for (FluxPropHandler const *p = core_props; p->fn; p++)
		if (p->id == patch->id && p->kind == patch->value.kind) return p;
	return NULL;
}

bool flux_host_can_project(XtkCtrlId type, XtkPropPatch const *patch) {
	if (!core_prop(patch)) return false;
	if (patch->id == XTK_PROP_MIN_F32)
		return type == XTK_CTRL_SLIDER || type == XTK_CTRL_PROGRESS || type == XTK_CTRL_PROGRESSRING;
	return true;
}

bool flux_host_apply_native(FluxHostState *R, XtkCtrl control, XtkPropPatch const *patch) {
	XentNodeId             node   = flux_host_as_node(control);
	XtkCtrlId              type   = flux_host_types_get(R, node);
	FluxPropHandler const *module = flux_module_prop(( FluxControlType ) type, patch->id, patch->value.kind);
	if (module && module->fn(R, control, node, &patch->value)) return true;
	FluxPropHandler const *core = core_prop(patch);
	return core && core->fn(R, control, node, &patch->value);
}
