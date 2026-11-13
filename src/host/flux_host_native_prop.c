#include "flux_host_internal.h"
#include "store/flux_node_data.h"

#include "controls/factory/flux_init_basic.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "controls/factory/flux_init_media.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/factory/flux_init_popup.h"
#include "controls/factory/flux_init_value.h"
#include "controls/behavior/flux_host_menu.h"
#include "controls/behavior/flux_slider.h"
#include "host/flux_prop_apply.h"
#include "host/flux_host_picker_prop.h"
#include "controls/factory/flux_init_picker.h"
#include "render/flux_resource_intern.h"
#include "runtime/flux_time.h"
#include <controls/data/flux_list_view_data.h>
#include <controls/data/flux_pager_data.h>
#include <controls/data/flux_person_picture_data.h>
#include <controls/data/flux_radio_buttons_data.h>
#include <controls/data/flux_slider_data.h>
#include <controls/data/flux_title_bar_data.h>
#include "controls/behavior/flux_tree_internal.h"
#include <fluxent/flux_control_type.h>
#include <xtk/xtk_navmodel.h>
#include <xtk/xtk_treemodel.h>

#include <limits.h>
#include <math.h>
#include "controls/behavior/flux_calendar.h"
#include <stdlib.h>
#include <string.h>

typedef void (*FluxStrPropFn)(FluxStore *store, XentNodeId node, char const *s);
typedef void (*FluxBoolPropFn)(FluxStore *store, XentNodeId node, bool v);
typedef void (*FluxF32PropFn)(FluxStore *store, XentNodeId node, float v);
typedef void (*FluxIndexPropFn)(FluxStore *store, XentNodeId node, int index);
typedef bool (*FluxNativePropFn)(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v);

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

static void apply_dialog_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	float width   = stored_f32(R, control, XTK_PROP_WIDTH);
	float content = stored_f32(R, control, XTK_PROP_CONTENT_HEIGHT);
	flux_dialog_configure(
	  R->store, node, stored_string(R, control, XTK_PROP_TEXT), stored_string(R, control, XTK_PROP_PRIMARY_TEXT),
	  stored_string(R, control, XTK_PROP_SECONDARY_TEXT), stored_string(R, control, XTK_PROP_CLOSE_TEXT),
	  width > 0.0f ? width : 400.0f, content > 0.0f ? content : 120.0f
	);
}

static void apply_teachtip_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_teachtip_configure(
	  R->store, node, stored_string(R, control, XTK_PROP_ICON), stored_string(R, control, XTK_PROP_ACTION_TEXT),
	  stored_bool(R, control, XTK_PROP_ACTION_ACCENT), stored_string(R, control, XTK_PROP_CLOSE_TEXT),
	  stored_bool(R, control, XTK_PROP_LIGHT_DISMISS), ( int ) stored_enum(R, control, XTK_PROP_TAIL_VISIBILITY),
	  stored_f32(R, control, XTK_PROP_PLACEMENT_MARGIN)
	);
}

static void apply_badge_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_badge_configure(
	  R->store, node, ( FluxInfoBadgeMode ) stored_enum(R, control, XTK_PROP_BADGE_MODE),
	  stored_i32(R, control, XTK_PROP_BADGE_VALUE), stored_string(R, control, XTK_PROP_ICON)
	);
}

static void apply_pager_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_pager_configure(
	  R->store, node, stored_i32(R, control, XTK_PROP_ITEM_COUNT), stored_i32(R, control, XTK_PROP_SELECTED_INDEX),
	  ( int ) stored_enum(R, control, XTK_PROP_DISPLAY_MODE)
	);
}

static void apply_pager_buttons(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_pager_setbuttons(
	  R->store, node, ( int ) stored_enum(R, control, XTK_PROP_FIRST_BUTTON),
	  ( int ) stored_enum(R, control, XTK_PROP_PREVIOUS_BUTTON), ( int ) stored_enum(R, control, XTK_PROP_NEXT_BUTTON),
	  ( int ) stored_enum(R, control, XTK_PROP_LAST_BUTTON)
	);
}

static void apply_splitview_config(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_splitview_configure(
	  R->store, node, ( int ) stored_enum(R, control, XTK_PROP_DISPLAY_MODE),
	  ( int ) stored_enum(R, control, XTK_PROP_PANE_PLACEMENT), stored_f32(R, control, XTK_PROP_OPEN_PANE_LENGTH),
	  stored_f32(R, control, XTK_PROP_COMPACT_PANE_LENGTH)
	);
}

static void apply_avatar_badge(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_avatar_set_badge(
	  R->store, node, stored_string(R, control, XTK_PROP_BADGE_GLYPH), stored_i32(R, control, XTK_PROP_BADGE_VALUE)
	);
}

static void apply_rating_behavior(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_rating_setbehavior(
	  R->store, node, stored_f64(R, control, XTK_PROP_PLACEHOLDER_F64, -1.0),
	  stored_i32(R, control, XTK_PROP_INITIAL_VALUE), stored_bool(R, control, XTK_PROP_CLEAR_ENABLED)
	);
}

static void apply_titlebar_back(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	flux_titlebar_set_back(
	  R->store, node, stored_bool(R, control, XTK_PROP_SHOW_BACK), stored_bool(R, control, XTK_PROP_BACK_DISABLED)
	);
}

static bool apply_width(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_DIALOG) {
		apply_dialog_config(R, control, node);
		return true;
	}
	if (type == XTK_CTRL_EXPANDER) {
		flux_prop_expander_width(R->store, node, v->u.f32);
		return true;
	}
	if (type == XTK_CTRL_AVATAR) {
		FluxNodeData          *nd = flux_store_find(R->store, node);
		FluxPersonPictureData *d  = nd ? ( FluxPersonPictureData * ) nd->component_data : NULL;
		if (d) {
			d->diameter = v->u.f32;
			xent_setsize(R->ctx, node, (XentSize) {d->diameter, d->diameter});
			return true;
		}
	}
	( void ) type;
	return xent_setw(flux_store_ctx(R->store), node, v->u.f32);
}

static bool apply_height(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_AVATAR) {
		FluxNodeData          *nd = flux_store_find(R->store, node);
		FluxPersonPictureData *d  = nd ? ( FluxPersonPictureData * ) nd->component_data : NULL;
		if (d) {
			d->diameter = v->u.f32;
			xent_setsize(R->ctx, node, (XentSize) {d->diameter, d->diameter});
			return true;
		}
	}
	( void ) type;
	return xent_seth(R->ctx, node, v->u.f32);
}

static bool apply_grow(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	if (!xent_setgrow(R->ctx, node, v->u.f32)) return false;
	return xent_setbasis(R->ctx, node, v->u.f32 > 0.0f ? 0.0f : NAN);
}

static bool apply_shrink(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	return xent_setshrink(R->ctx, node, v->u.f32);
}

static bool apply_align_self(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	return xent_setself(R->ctx, node, ( XentFlexAlign ) v->u.u32);
}

static bool apply_margin(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	XentInsets insets = {
	  stored_f32(R, control, XTK_PROP_MARGIN_LEFT), stored_f32(R, control, XTK_PROP_MARGIN_TOP),
	  stored_f32(R, control, XTK_PROP_MARGIN_RIGHT), stored_f32(R, control, XTK_PROP_MARGIN_BOTTOM)};
	return xent_setm(R->ctx, node, insets);
}

#define MARGIN_APPLY(name)                                                                  \
	static bool apply_margin_##name(                                                        \
	  FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v \
	) {                                                                                     \
		( void ) type;                                                                      \
		( void ) v;                                                                         \
		return apply_margin(R, control, node);                                              \
	}

MARGIN_APPLY(left)
MARGIN_APPLY(top)
MARGIN_APPLY(right)
MARGIN_APPLY(bottom)

static void apply_slider_orientation(FluxHostState *R, XtkCtrl control, XentNodeId node, bool vertical) {
	flux_slider_setvertical(R->store, node, vertical);
	if (!flux_host_prop_findc(R, control, XTK_PROP_WIDTH)) xent_setw(R->ctx, node, vertical ? 32.0f : NAN);
	if (!flux_host_prop_findc(R, control, XTK_PROP_HEIGHT)) xent_seth(R->ctx, node, vertical ? NAN : 32.0f);
}

static bool apply_orientation(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_SLIDER) {
		apply_slider_orientation(R, control, node, v->u.u32 != XTK_STACK_ROW);
		return true;
	}
	if (type == XTK_CTRL_FLIP) {
		flux_flip_set_vertical(R->store, node, v->u.u32 != 0u);
		return true;
	}
	if (type == XTK_CTRL_PIPS) {
		flux_pips_set_vertical(R->store, node, v->u.u32 != 0u);
		return true;
	}
	if (type == XTK_CTRL_SCROLLBAR) {
		flux_scroll_bar_setorientation(R->ctx, R->store, node, v->u.u32);
		return true;
	}
	if (!xent_setproto(R->ctx, node, XENT_PROTOCOL_FLEX)) return false;
	return xent_setflexdir(R->ctx, node, v->u.u32 == XTK_STACK_ROW ? XENT_FLEX_ROW : XENT_FLEX_COLUMN);
}

static bool apply_align_items(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	return xent_setitems(R->ctx, node, ( XentFlexAlign ) v->u.u32);
}

static bool
apply_justify_content(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	return xent_setjustify(R->ctx, node, ( XentFlexJustify ) v->u.u32);
}

static bool apply_gap(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	return xent_setgap(R->ctx, node, v->u.f32);
}

static void progress_bar_min_height(FluxHostState *R, XentNodeId node, FluxNodeData const *nd) {
	FluxProgressData const *bar    = ( FluxProgressData const * ) nd->component_data;
	float                   height = 3.0f + bar->padding.top + bar->padding.bottom + 2.0f * nd->visuals.border_width;
	xent_setminsize(R->ctx, node, (XentSize) {0.0f, height});
}

static void progress_bar_padding(FluxHostState *R, XentNodeId node, XentInsets insets) {
	FluxNodeData     *nd  = flux_store_find(R->store, node);
	FluxProgressData *bar = nd ? ( FluxProgressData * ) nd->component_data : NULL;
	if (!bar) return;
	bar->padding = (XentInsets) {
	  fmaxf(0.0f, insets.left), fmaxf(0.0f, insets.top), fmaxf(0.0f, insets.right), fmaxf(0.0f, insets.bottom)};
	progress_bar_min_height(R, node, nd);
}

static bool apply_padding(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type) {
	XentInsets insets = {
	  stored_f32(R, control, XTK_PROP_PADDING_LEFT), stored_f32(R, control, XTK_PROP_PADDING_TOP),
	  stored_f32(R, control, XTK_PROP_PADDING_RIGHT), stored_f32(R, control, XTK_PROP_PADDING_BOTTOM)};
	if (type == XTK_CTRL_PROGRESS) progress_bar_padding(R, node, insets);
	return xent_setp(R->ctx, node, insets);
}

#define PADDING_APPLY(name)                                                                 \
	static bool apply_padding_##name(                                                       \
	  FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v \
	) {                                                                                     \
		( void ) v;                                                                         \
		return apply_padding(R, control, node, type);                                       \
	}

PADDING_APPLY(left)
PADDING_APPLY(top)
PADDING_APPLY(right)
PADDING_APPLY(bottom)

static bool apply_fill(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	bool row    = stored_enum(R, control, XTK_PROP_ORIENTATION) == XTK_STACK_ROW;
	bool wrap_w = type == XTK_CTRL_CARD ? false : !v->u.b;
	bool wrap_h = type == XTK_CTRL_CARD ? !v->u.b : row || !v->u.b;
	return xent_setfit(R->ctx, node, wrap_w, wrap_h);
}

static bool apply_fit_width(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) type;
	return xent_setfit(R->ctx, node, v->u.b, stored_bool(R, control, XTK_PROP_FIT_HEIGHT));
}

static bool apply_fit_height(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) type;
	return xent_setfit(R->ctx, node, stored_bool(R, control, XTK_PROP_FIT_WIDTH), v->u.b);
}

static bool apply_wrap(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	return xent_setflexwrap(R->ctx, node, ( XentFlexWrap ) v->u.u32);
}

static char const *str_or_empty(XtkValue const *v) { return v->u.string.data ? v->u.string.data : ""; }

static bool        apply_text(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_DIALOG) {
		apply_dialog_config(R, control, node);
		return true;
	}
	if (type == XTK_CTRL_TEACHTIP) {
		flux_teachtip_set_texts(R->store, node, str_or_empty(v), stored_string(R, control, XTK_PROP_SUBTITLE));
		return true;
	}
	if (type == XTK_CTRL_TREEITEM) {
		xent_settext(R->ctx, node, str_or_empty(v));
		return true;
	}
	if (type == XTK_CTRL_TITLEBAR) {
		FluxNodeData     *nd = flux_store_find(R->store, node);
		FluxTitleBarData *d  = nd ? ( FluxTitleBarData * ) nd->component_data : NULL;
		flux_titlebar_set_title(R->store, node, str_or_empty(v), d ? d->subtitle : NULL);
		return true;
	}
	static FluxStrPropFn const by_type [XTK_CTRL_TYPE_COUNT] = {
	  [XTK_CTRL_BUTTON]      = flux_prop_button_label,
	  [XTK_CTRL_REPEAT]      = flux_prop_repeat_label,
	  [XTK_CTRL_DROPDOWN]    = flux_prop_button_label,
	  [XTK_CTRL_SPLIT]       = flux_prop_button_label,
	  [XTK_CTRL_TOGGLESPLIT] = flux_prop_button_label,
	  [XTK_CTRL_LINK]        = flux_prop_hyperlink_label,
	  [XTK_CTRL_TEXT]        = flux_prop_text_content,
	  [XTK_CTRL_CHECKBOX]    = flux_prop_checkbox_label,
	  [XTK_CTRL_RADIO]       = flux_prop_checkbox_label,
	  [XTK_CTRL_TEXTBOX]     = flux_prop_textbox_content,
	  [XTK_CTRL_PASSWORD]    = flux_prop_password_content,
	  [XTK_CTRL_RATING]      = flux_rating_set_caption,
	  [XTK_CTRL_MENUFLY]     = flux_host_menufly_set_text,
	  [XTK_CTRL_MENUITEM]    = flux_host_menuitem_set_text,
	  [XTK_CTRL_EXPANDER]    = flux_prop_expander_header,
	  [XTK_CTRL_INFOBAR]     = flux_prop_infobar_title,
	  [XTK_CTRL_AVATAR]      = flux_avatar_setname,
	  [XTK_CTRL_SUGGEST]     = flux_suggest_set_content,
	  [XTK_CTRL_COMBO]       = flux_prop_combo_text,
	};
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return true;
	FluxStrPropFn fn = by_type [type];
	if (fn) fn(R->store, node, str_or_empty(v));
	return true;
}

static bool apply_icon(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_BADGE) {
		apply_badge_config(R, control, node);
		return true;
	}
	if (type == XTK_CTRL_TEACHTIP) {
		apply_teachtip_config(R, control, node);
		return true;
	}
	if (type == XTK_CTRL_MENUITEM) {
		flux_host_menuitem_set_icon(R->store, node, str_or_empty(v));
		return true;
	}
	if (type == XTK_CTRL_SUGGEST) {
		flux_suggest_set_query_icon(R->store, node, str_or_empty(v));
		return true;
	}
	if (type == XTK_CTRL_TITLEBAR) {
		flux_titlebar_set_icon(R->store, node, str_or_empty(v));
		return true;
	}
	if (type == XTK_CTRL_REPEAT) flux_prop_repeat_icon(R->store, node, str_or_empty(v));
	else flux_prop_button_icon(R->store, node, str_or_empty(v));
	return true;
}

static bool apply_button_style(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_REPEAT) flux_prop_repeat_style(R->store, node, ( FluxButtonStyle ) v->u.u32);
	else flux_prop_button_style(R->store, node, ( FluxButtonStyle ) v->u.u32);
	return true;
}

static bool apply_enabled(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_MENUITEM) {
		flux_host_menuitem_set_enabled(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_DROPDOWN || type == XTK_CTRL_SPLIT || type == XTK_CTRL_TOGGLESPLIT) {
		flux_prop_button_enabled(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_PASSWORD) {
		flux_prop_password_enabled(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_NUMBER) {
		flux_prop_numberbox_enabled(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_RADIOGROUP) {
		flux_radiogroup_set_enabled(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_BREADCRUMB) {
		flux_breadcrumb_set_disabled(R->store, node, !v->u.b);
		return true;
	}
	static FluxBoolPropFn const by_type [XTK_CTRL_TYPE_COUNT] = {
	  [XTK_CTRL_BUTTON]     = flux_prop_button_enabled,
	  [XTK_CTRL_REPEAT]     = flux_prop_repeat_enabled,
	  [XTK_CTRL_LINK]       = flux_prop_hyperlink_enabled,
	  [XTK_CTRL_CHECKBOX]   = flux_prop_checkbox_enabled,
	  [XTK_CTRL_RADIO]      = flux_prop_checkbox_enabled,
	  [XTK_CTRL_SWITCH]     = flux_prop_checkbox_enabled,
	  [XTK_CTRL_TEXTBOX]    = flux_prop_textbox_enabled,
	  [XTK_CTRL_SLIDER]     = flux_prop_slider_enabled,
	  [XTK_CTRL_SUGGEST]    = flux_suggest_set_enabled,
	  [XTK_CTRL_SCROLLBAR]  = flux_scroll_bar_setenabled,
	  [XTK_CTRL_LOOPING]    = flux_looping_setenabled,
	  [XTK_CTRL_DATEPICKER] = flux_picker_setenabled,
	  [XTK_CTRL_TIMEPICKER] = flux_picker_setenabled,
	};
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return true;
	FluxBoolPropFn fn = by_type [type];
	if (fn) fn(R->store, node, v->u.b);
	return true;
}

static FluxCheckboxData *checkbox_data(FluxHostState *R, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(R->store, node);
	return nd ? ( FluxCheckboxData * ) nd->component_data : NULL;
}

static void checkbox_apply_state(FluxHostState *R, XentNodeId node, FluxCheckState state) {
	FluxCheckboxData *cd = checkbox_data(R, node);
	if (!cd) return;
	( void ) node;
	flux_checkbox_setstate(cd, state);
}

static bool apply_three_state(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	FluxCheckboxData *cd = type == XTK_CTRL_CHECKBOX ? checkbox_data(R, node) : NULL;
	if (cd) cd->is_three_state = v->u.b;
	return cd != NULL;
}

static bool apply_checked(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_TOGGLESPLIT) {
		flux_togglesplit_set_checked(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_TITLEBAR) {
		flux_titlebar_settoggle(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_CHECKBOX || type == XTK_CTRL_RADIO || type == XTK_CTRL_SWITCH)
		checkbox_apply_state(R, node, v->u.b ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED);
	return true;
}

static bool apply_open(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_DIALOG) {
		flux_dialog_sync_host(R->store, node, v->u.b);
		return true;
	}
	if (type == XTK_CTRL_TEACHTIP) {
		if (v->u.b) flux_teachtip_show(R->store, node);
		else flux_teachtip_hide(R->store, node);
		return true;
	}
	if (type == XTK_CTRL_SPLITVIEW) flux_splitview_setopen(R->store, node, v->u.b);
	return true;
}

static bool apply_expanded(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_TREEITEM) xent_sem_setexpanded(R->ctx, node, v->u.b);
	else if (type == XTK_CTRL_EXPANDER) flux_prop_expander_expanded(R->store, node, R->ctx, v->u.b);
	return true;
}

static bool
apply_content_height(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_EXPANDER) flux_prop_expander_content_height(R->store, node, v->u.f32);
	else if (type == XTK_CTRL_DIALOG) apply_dialog_config(R, control, node);
	return true;
}

static bool apply_message(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_INFOBAR) flux_prop_infobar_message(R->store, node, str_or_empty(v));
	return true;
}

static bool apply_severity(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_INFOBAR) flux_prop_infobar_severity(R->store, node, ( FluxInfoBarSeverity ) v->u.u32);
	return true;
}

static bool apply_closable(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_INFOBAR) flux_prop_infobar_closable(R->store, node, v->u.b);
	return true;
}

static bool
apply_pull_direction(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_REFRESH) flux_refresh_set_direction(R->store, node, ( FluxPullDirection ) v->u.u32);
	return true;
}

static bool
apply_visualizer_size(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_REFRESH) flux_refresh_set_visualizer_size(R->store, node, v->u.f32);
	return true;
}

static bool
apply_threshold_ratio(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_REFRESH) flux_refresh_set_threshold_ratio(R->store, node, v->u.f32);
	return true;
}

static bool apply_refreshing(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_REFRESH) flux_refresh_set_refreshing(R->store, node, v->u.b);
	return true;
}

static bool apply_value_f32(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	static FluxF32PropFn const by_type [XTK_CTRL_TYPE_COUNT] = {
	  [XTK_CTRL_SLIDER]       = flux_prop_slider_value,
	  [XTK_CTRL_PROGRESS]     = flux_prop_progress_value,
	  [XTK_CTRL_PROGRESSRING] = flux_prop_progressring_value,
	};
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return false;
	FluxF32PropFn fn = by_type [type];
	if (!fn) return false;
	fn(R->store, node, v->u.f32);
	return true;
}

static bool apply_value_f64(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_SCROLLBAR) {
		flux_scroll_bar_setvalue(R->store, node, v->u.f64);
		return true;
	}
	if (type == XTK_CTRL_RATING) {
		flux_rating_set_value(R->store, node, v->u.f64);
		return true;
	}
	if (type != XTK_CTRL_NUMBER) return false;
	flux_prop_numberbox_value(R->store, node, v->u.f64);
	return true;
}

static bool apply_maximum_f64(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_SCROLLBAR) {
		flux_scroll_bar_setmaximum(R->store, node, v->u.f64);
		return true;
	}
	if (type == XTK_CTRL_RATING) {
		flux_rating_set_max(R->store, node, ( int ) v->u.f64);
		return true;
	}
	if (type != XTK_CTRL_NUMBER) return false;
	double min = stored_f64(R, control, XTK_PROP_MIN_F64, 0.0);
	flux_prop_numberbox_range(R->store, node, min, v->u.f64);
	return true;
}

static bool apply_minimum_f64(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_SCROLLBAR) {
		flux_scroll_bar_setminimum(R->store, node, v->u.f64);
		return true;
	}
	if (type != XTK_CTRL_NUMBER) return false;
	double max = stored_f64(R, control, XTK_PROP_MAX_F64, 100.0);
	flux_prop_numberbox_range(R->store, node, v->u.f64, max);
	return true;
}

static bool apply_step_f64(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_NUMBER) return false;
	flux_prop_numberbox_step(R->store, node, v->u.f64);
	return true;
}

static bool apply_progress_range(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type) {
	FluxNodeData    *data      = flux_store_find(R->store, node);
	PropEntry const *min_entry = flux_host_prop_findc(R, control, XTK_PROP_MIN_F32);
	PropEntry const *max_entry = flux_host_prop_findc(R, control, XTK_PROP_MAX_F32);
	float            min       = 0.0f;
	float            max       = 100.0f;
	if (type == XTK_CTRL_PROGRESS && data && data->component_data) {
		FluxProgressData *progress = ( FluxProgressData * ) data->component_data;
		min                        = progress->min_value;
		max                        = progress->max_value;
	}
	else if (type == XTK_CTRL_PROGRESSRING && data && data->component_data) {
		FluxProgressRingData *progress = ( FluxProgressRingData * ) data->component_data;
		min                            = progress->min_value;
		max                            = progress->max_value;
	}
	if (min_entry && min_entry->value.kind == XTK_VALUE_F32) min = min_entry->value.u.f32;
	if (max_entry && max_entry->value.kind == XTK_VALUE_F32) max = max_entry->value.u.f32;
	if (type == XTK_CTRL_PROGRESS) flux_prop_progress_range(R->store, node, min, max);
	else flux_prop_progressring_range(R->store, node, min, max);
	return true;
}

static bool apply_slider_minmax(FluxHostState *R, XtkCtrl control, XentNodeId node) {
	FluxNodeData   *data   = flux_store_find(R->store, node);
	FluxSliderData *slider = data ? ( FluxSliderData * ) data->component_data : NULL;
	if (!slider) return true;
	PropEntry const *min_entry = flux_host_prop_findc(R, control, XTK_PROP_MIN_F32);
	PropEntry const *max_entry = flux_host_prop_findc(R, control, XTK_PROP_MAX_F32);
	float min = min_entry && min_entry->value.kind == XTK_VALUE_F32 ? min_entry->value.u.f32 : slider->min_value;
	float max = max_entry && max_entry->value.kind == XTK_VALUE_F32 ? max_entry->value.u.f32 : slider->max_value;
	flux_prop_slider_range(R->store, node, min, max);
	return true;
}

static bool apply_slider_range(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type == XTK_CTRL_PROGRESS || type == XTK_CTRL_PROGRESSRING) return apply_progress_range(R, control, node, type);
	if (type != XTK_CTRL_SLIDER) return false;
	return apply_slider_minmax(R, control, node);
}

static bool apply_slider_step(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SLIDER) return false;
	flux_prop_slider_step(R->store, node, v->u.f32);
	return true;
}

static bool
apply_slider_tick_frequency(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SLIDER) return false;
	flux_prop_slider_tick_frequency(R->store, node, v->u.f32);
	return true;
}

static bool
apply_slider_tick_placement(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SLIDER) return false;
	flux_prop_slider_tick_placement(R->store, node, ( FluxTickPlacement ) v->u.u32);
	return true;
}

static bool
apply_slider_snaps_to(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SLIDER) return false;
	flux_prop_slider_snaps_to(R->store, node, ( FluxSliderSnapsTo ) v->u.u32);
	return true;
}

static bool
apply_slider_small_change(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_SCROLLBAR) {
		flux_scroll_bar_setsmallchange(R->store, node, v->u.f32);
		return true;
	}
	if (type != XTK_CTRL_SLIDER) return false;
	flux_prop_slider_small_change(R->store, node, v->u.f32);
	return true;
}

static bool
apply_slider_large_change(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_SCROLLBAR) {
		flux_scroll_bar_setlargechange(R->store, node, v->u.f32);
		return true;
	}
	if (type != XTK_CTRL_SLIDER) return false;
	flux_prop_slider_large_change(R->store, node, v->u.f32);
	return true;
}

static bool apply_viewport_size(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SCROLLBAR) return false;
	flux_scroll_bar_setviewport(R->store, node, v->u.f64);
	return true;
}

static bool
apply_indicator_mode(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SCROLLBAR) return false;
	flux_scroll_bar_setindicator(R->store, node, v->u.u32);
	return true;
}

static bool apply_checkbox_indeterminate(FluxHostState *R, XentNodeId node, bool on) {
	FluxCheckboxData *cd = checkbox_data(R, node);
	if (!cd) return false;
	if (on) checkbox_apply_state(R, node, FLUX_CHECK_INDETERMINATE);
	else if (cd->state == FLUX_CHECK_INDETERMINATE) checkbox_apply_state(R, node, FLUX_CHECK_UNCHECKED);
	return true;
}

static bool apply_indeterminate(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_CHECKBOX) return apply_checkbox_indeterminate(R, node, v->u.b);
	if (type == XTK_CTRL_PROGRESS) flux_prop_progress_indeterminate(R->store, node, v->u.b);
	else if (type == XTK_CTRL_PROGRESSRING) flux_prop_progressring_indeterminate(R->store, node, v->u.b);
	else return false;
	return true;
}

static bool
apply_progress_state(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	FluxNodeData     *nd = type == XTK_CTRL_PROGRESS ? flux_store_find(R->store, node) : NULL;
	FluxProgressData *pd = nd ? ( FluxProgressData * ) nd->component_data : NULL;
	if (!pd) return false;
	if (pd->state == v->u.u32) return true;
	pd->state = ( uint8_t ) v->u.u32;
	return true;
}

static bool
apply_progress_border_width(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	FluxNodeData *nd = type == XTK_CTRL_PROGRESS ? flux_store_find(R->store, node) : NULL;
	if (!nd || !nd->component_data) return false;
	nd->visuals.border_width = fmaxf(0.0f, v->u.f32);
	progress_bar_min_height(R, node, nd);
	return true;
}

static bool
apply_progress_active(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	FluxNodeData         *nd   = type == XTK_CTRL_PROGRESSRING ? flux_store_find(R->store, node) : NULL;
	FluxProgressRingData *ring = nd ? ( FluxProgressRingData * ) nd->component_data : NULL;
	if (!ring) return false;
	ring->inactive = !v->u.b;
	return true;
}

static bool apply_is_range(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SLIDER) return false;
	flux_slider_setrange(R->store, node, v->u.b);
	return true;
}

static bool apply_range_start(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SLIDER) return false;
	flux_slider_setthumb(R->store, node, 0, v->u.f32);
	return true;
}

static bool apply_range_end(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_SLIDER) return false;
	flux_slider_setthumb(R->store, node, 1, v->u.f32);
	return true;
}

static bool apply_is_editable(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_COMBO) return false;
	flux_prop_combo_editable(R->store, node, v->u.b);
	return true;
}

static bool apply_font_size(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	flux_prop_text_fontsize(R->store, node, v->u.f32);
	return true;
}

static bool apply_font_weight(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	flux_prop_text_weight(R->store, node, ( FluxFontWeight ) v->u.u32);
	return true;
}

static bool apply_text_align(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	flux_prop_text_alignment(R->store, node, ( FluxTextAlign ) v->u.u32);
	return true;
}

static bool apply_foreground(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	flux_prop_text_color(R->store, node, v->u.color);
	return true;
}

static bool apply_tooltip(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	( void ) type;
	flux_prop_node_tooltip(R->store, node, str_or_empty(v));
	return true;
}

static bool apply_url(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_LINK) return false;
	flux_prop_hyperlink_url(R->store, node, str_or_empty(v));
	return true;
}

static bool apply_placeholder(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	char const *placeholder = str_or_empty(v);
	if (type == XTK_CTRL_COMBO) {
		flux_prop_combo_placeholder(R->store, node, placeholder);
		return true;
	}
	if (type == XTK_CTRL_PASSWORD) {
		flux_prop_password_placeholder(R->store, node, placeholder);
		return true;
	}
	if (type == XTK_CTRL_SUGGEST) {
		flux_suggest_set_placeholder(R->store, node, placeholder);
		return true;
	}
	flux_prop_textbox_placeholder(R->store, node, placeholder);
	return true;
}

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

static int stored_selected(FluxHostState const *R, XtkCtrl control) {
	PropEntry const *selected = flux_host_prop_findc(R, control, XTK_PROP_SELECTED_INDEX);
	return selected && selected->value.kind == XTK_VALUE_I32 ? selected->value.u.i32 : -1;
}

static bool apply_tab_items(FluxHostState *R, XentNodeId node, FluxWireItems const *wire) {
	if ((wire->count & 1) != 0 || wire->count / 2 > FLUX_TAB_VIEW_MAX_TABS) return false;
	char const *icons [FLUX_TAB_VIEW_MAX_TABS];
	char const *labels [FLUX_TAB_VIEW_MAX_TABS];
	for (int i = 0; i < wire->count / 2; i++) {
		icons [i]  = wire->items [i * 2][0] ? wire->items [i * 2] : NULL;
		labels [i] = wire->items [i * 2 + 1];
	}
	return flux_tabs_set_items(R->store, node, icons, labels, wire->count / 2);
}

static bool apply_selector_items(FluxHostState *R, XtkCtrl control, XentNodeId node, FluxWireItems const *wire) {
	if (wire->count % 3 != 0) return false;
	int              count = wire->count / 3;
	XtkSelectorItem *items = count ? ( XtkSelectorItem * ) malloc(sizeof(*items) * ( size_t ) count) : NULL;
	if (count && !items) return false;
	for (int i = 0; i < count; i++)
		items [i] = (XtkSelectorItem) {
		  .icon     = wire->items [i * 3],
		  .text     = wire->items [i * 3 + 1],
		  .disabled = wire->items [i * 3 + 2][0] == '1',
		};
	flux_selector_set_items(R->store, node, items, count, stored_selected(R, control));
	free(items);
	return true;
}

static bool
apply_plain_items(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, FluxWireItems const *wire) {
	if (type == XTK_CTRL_SUGGEST) {
		flux_suggest_set_suggestions(R->store, node, wire->items, wire->count);
		return true;
	}
	if (type == XTK_CTRL_BREADCRUMB) {
		flux_breadcrumb_set_items(R->store, node, wire->items, wire->count);
		return true;
	}
	if (type == XTK_CTRL_COMBO) {
		flux_prop_combo_items(R->store, node, wire->items, wire->count);
		flux_prop_combo_selected(R->store, node, stored_selected(R, control));
		return true;
	}
	if (type == XTK_CTRL_LOOPING) {
		flux_looping_setitems(R->store, node, wire->items, wire->count);
		flux_looping_setselected(R->store, node, stored_selected(R, control));
		return true;
	}
	return false;
}

static bool apply_tree_items(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkValue const *v) {
	XtkTreeModelView model;
	if (!xtk_treemodel_open(v->u.bytes.data, v->u.bytes.len, &model) || !flux_tree_set_model(R->store, node, &model))
		return false;
	int flat     = stored_selected(R, control);
	int selected = flat < 0 ? -1 : flux_tree_flat_node(R->store, node, flat);
	flux_tree_set_selected(R->store, node, selected, true);
	return true;
}

static bool apply_items(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_TREE) return apply_tree_items(R, control, node, v);
	FluxWireItems wire = {0};
	if (!wire_items_open(v, &wire)) return false;
	bool ok = type == XTK_CTRL_TABS     ? apply_tab_items(R, node, &wire)
	        : type == XTK_CTRL_SELECTOR ? apply_selector_items(R, control, node, &wire)
	                                    : apply_plain_items(R, control, node, type, &wire);
	free(wire.items);
	return ok;
}

static bool apply_should_loop(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_LOOPING) flux_looping_setloop(R->store, node, v->u.b);
	return true;
}

static bool apply_image_source(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	char const *path = flux_resource_path(( XentResourceId ) v->u.resource);
	if (type == XTK_CTRL_AVATAR) flux_avatar_set_image(R->store, node, path);
	else if (type == XTK_CTRL_IMAGE) flux_prop_image_source(R->store, node, path);
	else return false;
	return true;
}

static bool apply_image_stretch(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_IMAGE) return false;
	flux_prop_image_stretch(R->store, node, ( FluxImageStretch ) v->u.u32);
	return true;
}

static bool list_type(XtkCtrlId type) {
	return type == XTK_CTRL_LIST
	    || type == XTK_CTRL_LISTBOX
	    || type == XTK_CTRL_GRIDVIEW
	    || type == XTK_CTRL_REPEATER
	    || type == XTK_CTRL_ITEMSVIEW;
}

static FluxListViewData *host_list_data(FluxHostState *R, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(R->store, node);
	return nd ? ( FluxListViewData * ) nd->component_data : NULL;
}

static void apply_list_count(FluxHostState *R, XentNodeId node, int count) {
	FluxListViewData *ld = host_list_data(R, node);
	float             h  = ld && ld->item_height > 0.0f ? ld->item_height : 40.0f;
	float             w  = ld ? ld->item_width : 0.0f;
	int               c  = ld ? ld->columns : 0;
	flux_list_set_extent(R->store, node, count, h, w, c);
}

static void apply_pager_count(FluxHostState *R, XentNodeId node, int count) {
	FluxNodeData  *nd = flux_store_find(R->store, node);
	FluxPagerData *pd = nd ? ( FluxPagerData * ) nd->component_data : NULL;
	if (pd) flux_pager_configure(R->store, node, count, pd->selected, pd->display_mode);
}

static bool apply_item_count(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (list_type(type)) {
		apply_list_count(R, node, v->u.i32);
		return true;
	}
	if (type == XTK_CTRL_PAGER) {
		apply_pager_count(R, node, v->u.i32);
		return true;
	}
	if (type == XTK_CTRL_PIPS) flux_pips_set_count(R->store, node, v->u.i32);
	return true;
}

static void apply_list_height(FluxHostState *R, XentNodeId node, float height) {
	FluxListViewData *ld = host_list_data(R, node);
	if (!ld) return;
	flux_list_set_extent(R->store, node, ld->count, height, ld->item_width, ld->columns);
}

static void apply_tree_height(FluxHostState *R, XentNodeId node, float height) {
	FluxNodeData     *nd = flux_store_find(R->store, node);
	FluxTreeViewData *td = nd ? ( FluxTreeViewData * ) nd->component_data : NULL;
	if (td) td->row_height = height > 0.0f ? height : FLUX_TREE_ROW_H;
}

static bool apply_item_height(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (list_type(type)) apply_list_height(R, node, v->u.f32);
	if (type == XTK_CTRL_TREE) apply_tree_height(R, node, v->u.f32);
	if (type == XTK_CTRL_LOOPING) flux_looping_setitemheight(R->store, node, v->u.f32);
	return true;
}

static bool apply_item_width(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_LOOPING) {
		flux_looping_setitemwidth(R->store, node, v->u.f32);
		return true;
	}
	if (!list_type(type)) return true;
	FluxNodeData     *nd = flux_store_find(R->store, node);
	FluxListViewData *ld = nd ? ( FluxListViewData * ) nd->component_data : NULL;
	if (!ld) return true;
	flux_list_set_extent(
	  R->store, node, ld->count, ld->item_height > 0.0f ? ld->item_height : 40.0f, v->u.f32, ld->columns
	);
	return true;
}

static bool
apply_selected_index(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_TREE) {
		int selected = v->u.i32 < 0 ? -1 : flux_tree_flat_node(R->store, node, v->u.i32);
		flux_tree_set_selected(R->store, node, selected, true);
		return true;
	}
	static FluxIndexPropFn const by_type [XTK_CTRL_TYPE_COUNT] = {
	  [XTK_CTRL_LIST]       = flux_list_set_selected,
	  [XTK_CTRL_LISTBOX]    = flux_list_set_selected,
	  [XTK_CTRL_GRIDVIEW]   = flux_list_set_selected,
	  [XTK_CTRL_REPEATER]   = flux_list_set_selected,
	  [XTK_CTRL_ITEMSVIEW]  = flux_list_set_selected,
	  [XTK_CTRL_RADIOGROUP] = flux_radiogroup_set_selected,
	  [XTK_CTRL_COMBO]      = flux_prop_combo_selected,
	  [XTK_CTRL_SELECTOR]   = flux_selector_set_selected,
	  [XTK_CTRL_PAGER]      = flux_pager_set_selected,
	  [XTK_CTRL_NAV]        = flux_nav_set_selected,
	  [XTK_CTRL_FLIP]       = flux_flip_select,
	  [XTK_CTRL_PIPS]       = flux_pips_set_selected,
	  [XTK_CTRL_TABS]       = flux_tabs_select,
	  [XTK_CTRL_LOOPING]    = flux_looping_setselected,
	};
	if (( unsigned ) type >= XTK_CTRL_TYPE_COUNT) return true;
	FluxIndexPropFn fn = by_type [type];
	if (fn) fn(R->store, node, v->u.i32);
	return true;
}

static int append_nav_item(FluxHostState *R, XentNodeId node, XtkNavModelItemView const *item) {
	switch (item->kind) {
	case XTK_NAV_ITEM      : return flux_nav_add_item(R->store, node, item->icon, item->label);
	case XTK_NAV_HEADER    : return flux_nav_add_header(R->store, node, item->label);
	case XTK_NAV_SEPARATOR : return flux_nav_add_separator(R->store, node);
	case XTK_NAV_FOOTER    : return flux_nav_addfooter(R->store, node, item->icon, item->label);
	case XTK_NAV_CHILD     : return flux_nav_addchild(R->store, node, item->parent, item->icon, item->label);
	default                : return -1;
	}
}

static bool apply_nav_model(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_NAV) return false;
	XtkNavModelView model;
	if (!xtk_navmodel_open(v->u.bytes.data, v->u.bytes.len, &model)) return false;
	if (model.count > FLUX_NAV_VIEW_MAX_ITEMS) return false;
	if (!flux_nav_reset_items(R->store, node)) return false;
	if (!flux_nav_set_chrome(R->store, node, (model.flags & XTK_NAV_MODEL_WINDOW_TITLE_BAR) != 0u, model.app_title))
		return false;
	for (uint32_t i = 0; i < model.count; i++) {
		XtkNavModelItemView item;
		if (!xtk_navmodel_item(&model, i, &item) || append_nav_item(R, node, &item) < 0) return false;
	}
	return true;
}

static void apply_list_columns(FluxHostState *R, XentNodeId node, int columns) {
	FluxListViewData *ld = host_list_data(R, node);
	if (!ld) return;
	flux_list_set_extent(
	  R->store, node, ld->count, ld->item_height > 0.0f ? ld->item_height : 40.0f, ld->item_width, columns
	);
}

static void apply_radio_columns(FluxHostState *R, XentNodeId node, int columns) {
	FluxNodeData         *nd = flux_store_find(R->store, node);
	FluxRadioButtonsData *g  = nd ? ( FluxRadioButtonsData * ) nd->component_data : NULL;
	if (g) g->columns = columns > 0 ? columns : 1;
}

static bool apply_column_count(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (list_type(type)) apply_list_columns(R, node, v->u.i32);
	else if (type == XTK_CTRL_RADIOGROUP) apply_radio_columns(R, node, v->u.i32);
	return true;
}

static bool apply_selected(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_LISTITEM) fluxlistitem_set_state(R->store, node, v->u.b, false);
	return true;
}

static bool
apply_selection_mode(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_CALENDAR) return flux_calendar_set(R->store, node, XTK_PROP_SELECTION_MODE, v);
	if (list_type(type)) flux_list_setmode(R->store, node, ( XtkListSelMode ) v->u.u32);
	if (type == XTK_CTRL_TREE) flux_tree_setmode(R->store, node, ( XtkTreeSelMode ) v->u.u32);
	return true;
}

static bool
apply_item_invoked_enabled(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_ITEMSVIEW) return true;
	FluxNodeData     *nd = flux_store_find(R->store, node);
	FluxListViewData *ld = nd ? ( FluxListViewData * ) nd->component_data : NULL;
	if (ld) ld->item_invoked_enabled = v->u.b;
	return true;
}

static bool apply_max_visible(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_PIPS) flux_pips_set_max_visible(R->store, node, v->u.i32);
	return true;
}

static bool
apply_nav_visibility(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_PIPS) flux_pips_set_nav_visibility(R->store, node, ( int ) v->u.u32);
	return true;
}

static bool apply_subtitle(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	if (type == XTK_CTRL_TITLEBAR) {
		FluxNodeData     *nd = flux_store_find(R->store, node);
		FluxTitleBarData *d  = nd ? ( FluxTitleBarData * ) nd->component_data : NULL;
		flux_titlebar_set_title(R->store, node, d ? d->title : NULL, str_or_empty(v));
		return true;
	}
	if (type == XTK_CTRL_TEACHTIP) {
		flux_teachtip_set_texts(R->store, node, stored_string(R, control, XTK_PROP_TEXT), str_or_empty(v));
		return true;
	}
	return false;
}

static bool apply_dialog_text(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_DIALOG) return false;
	apply_dialog_config(R, control, node);
	return true;
}

static bool apply_close_text(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type == XTK_CTRL_DIALOG) apply_dialog_config(R, control, node);
	else if (type == XTK_CTRL_TEACHTIP) apply_teachtip_config(R, control, node);
	else return false;
	return true;
}

static bool
apply_teachtip_config_prop(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_TEACHTIP) return false;
	apply_teachtip_config(R, control, node);
	return true;
}

static bool apply_pager_text(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_PAGER) return false;
	flux_pager_settext(
	  R->store, node, stored_string(R, control, XTK_PROP_PREFIX), stored_string(R, control, XTK_PROP_SUFFIX)
	);
	return true;
}

static bool apply_initials(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_AVATAR) return false;
	flux_avatar_set_initials(R->store, node, str_or_empty(v));
	return true;
}

static bool apply_badge_glyph(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_AVATAR) return false;
	apply_avatar_badge(R, control, node);
	return true;
}

static bool apply_badge_mode(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_BADGE) return false;
	apply_badge_config(R, control, node);
	return true;
}

static bool apply_badge_value(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type == XTK_CTRL_BADGE) apply_badge_config(R, control, node);
	else if (type == XTK_CTRL_AVATAR) apply_avatar_badge(R, control, node);
	else return false;
	return true;
}

static bool apply_display_mode(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type == XTK_CTRL_PAGER) apply_pager_config(R, control, node);
	else if (type == XTK_CTRL_SPLITVIEW) apply_splitview_config(R, control, node);
	else return false;
	return true;
}

static bool apply_placement(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TEACHTIP) return false;
	flux_teachtip_set_placement(R->store, node, ( FluxTeachingTipPlacement ) v->u.u32);
	return true;
}

static bool apply_pager_button(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_PAGER) return false;
	apply_pager_buttons(R, control, node);
	return true;
}

static bool apply_pane_config(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_SPLITVIEW) return false;
	apply_splitview_config(R, control, node);
	return true;
}

static bool apply_untargeted(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TEACHTIP) return false;
	XentNodeId target = v->u.b ? XENT_NODE_INVALID : xent_node_parent(R->ctx, node);
	flux_teachtip_set_target(R->store, node, target);
	return true;
}

static bool apply_group(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_AVATAR) return false;
	flux_avatar_set_group(R->store, node, v->u.b);
	return true;
}

static bool apply_read_only(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type == XTK_CTRL_TEXTBOX) {
		flux_prop_textbox_readonly(R->store, node, v->u.b);
		return true;
	}
	if (type != XTK_CTRL_RATING) return false;
	flux_rating_setreadonly(R->store, node, v->u.b);
	return true;
}

static bool apply_multiline(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TEXTBOX) return false;
	flux_prop_textbox_multiline(R->store, node, v->u.b);
	return true;
}

static bool apply_max_lines(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TEXT) return false;
	flux_prop_text_maxlines(R->store, node, v->u.i32 > 0 ? ( uint32_t ) v->u.i32 : 0u);
	return true;
}

static bool apply_text_trim(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TEXT || v->u.u32 > XENT_TEXT_TRIM_WORD_ELLIPSIS) return false;
	flux_prop_text_trim(R->store, node, ( XentTextTrim ) v->u.u32);
	return true;
}

static bool apply_text_wrap(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TEXT) return false;
	flux_prop_text_wrap(R->store, node, v->u.b);
	return true;
}

static bool apply_selectable(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TEXT) return false;
	flux_prop_text_selectable(R->store, node, v->u.b);
	return true;
}

static bool
apply_rating_behavior_prop(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_RATING) return false;
	apply_rating_behavior(R, control, node);
	return true;
}

static bool apply_back_state(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) v;
	if (type != XTK_CTRL_TITLEBAR) return false;
	apply_titlebar_back(R, control, node);
	return true;
}

static bool apply_show_toggle(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TITLEBAR) return false;
	flux_titlebar_settoggle(R->store, node, v->u.b);
	return true;
}

static bool
apply_integrate_window(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TITLEBAR) return false;
	flux_titlebar_set_window_integration(R->store, node, v->u.b);
	return true;
}

static bool apply_hide_spin(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_NUMBER) return false;
	flux_prop_numberbox_spin_enabled(R->store, node, !v->u.b);
	return true;
}

static bool apply_add_visible(FluxHostState *R, XtkCtrl control, XentNodeId node, XtkCtrlId type, XtkValue const *v) {
	( void ) control;
	if (type != XTK_CTRL_TABS) return false;
	flux_tabs_setaddvisible(R->store, node, v->u.b);
	return true;
}

typedef struct FluxNativePropEntry {
	XtkPropertyId    id;
	XtkValueKind     kind;
	FluxNativePropFn fn;
} FluxNativePropEntry;

static FluxNativePropEntry const native_props [] = {
  {XTK_PROP_WIDTH,                 XTK_VALUE_F32,      apply_width                     },
  {XTK_PROP_HEIGHT,                XTK_VALUE_F32,      apply_height                    },
  {XTK_PROP_GROW,                  XTK_VALUE_F32,      apply_grow                      },
  {XTK_PROP_SHRINK,                XTK_VALUE_F32,      apply_shrink                    },
  {XTK_PROP_ALIGN_SELF,            XTK_VALUE_ENUM,     apply_align_self                },
  {XTK_PROP_MARGIN_LEFT,           XTK_VALUE_F32,      apply_margin_left               },
  {XTK_PROP_MARGIN_TOP,            XTK_VALUE_F32,      apply_margin_top                },
  {XTK_PROP_MARGIN_RIGHT,          XTK_VALUE_F32,      apply_margin_right              },
  {XTK_PROP_MARGIN_BOTTOM,         XTK_VALUE_F32,      apply_margin_bottom             },
  {XTK_PROP_TEXT,                  XTK_VALUE_STRING,   apply_text                      },
  {XTK_PROP_ICON,                  XTK_VALUE_STRING,   apply_icon                      },
  {XTK_PROP_ENABLED,               XTK_VALUE_BOOL,     apply_enabled                   },
  {XTK_PROP_BUTTON_STYLE,          XTK_VALUE_ENUM,     apply_button_style              },
  {XTK_PROP_CHECKED,               XTK_VALUE_BOOL,     apply_checked                   },
  {XTK_PROP_MIN_F32,               XTK_VALUE_F32,      apply_slider_range              },
  {XTK_PROP_MAX_F32,               XTK_VALUE_F32,      apply_slider_range              },
  {XTK_PROP_VALUE_F32,             XTK_VALUE_F32,      apply_value_f32                 },
  {XTK_PROP_VALUE_F64,             XTK_VALUE_F64,      apply_value_f64                 },
  {XTK_PROP_MIN_F64,               XTK_VALUE_F64,      apply_minimum_f64               },
  {XTK_PROP_MAX_F64,               XTK_VALUE_F64,      apply_maximum_f64               },
  {XTK_PROP_STEP_F32,              XTK_VALUE_F32,      apply_slider_step               },
  {XTK_PROP_TICK_FREQUENCY,        XTK_VALUE_F32,      apply_slider_tick_frequency     },
  {XTK_PROP_TICK_PLACEMENT,        XTK_VALUE_ENUM,     apply_slider_tick_placement     },
  {XTK_PROP_SLIDER_SNAPS_TO,       XTK_VALUE_ENUM,     apply_slider_snaps_to           },
  {XTK_PROP_SMALL_CHANGE,          XTK_VALUE_F32,      apply_slider_small_change       },
  {XTK_PROP_LARGE_CHANGE,          XTK_VALUE_F32,      apply_slider_large_change       },
  {XTK_PROP_VIEWPORT_SIZE,         XTK_VALUE_F64,      apply_viewport_size             },
  {XTK_PROP_INDICATOR_MODE,        XTK_VALUE_ENUM,     apply_indicator_mode            },
  {XTK_PROP_STEP_F64,              XTK_VALUE_F64,      apply_step_f64                  },
  {XTK_PROP_INDETERMINATE,         XTK_VALUE_BOOL,     apply_indeterminate             },
  {XTK_PROP_FONT_SIZE,             XTK_VALUE_F32,      apply_font_size                 },
  {XTK_PROP_FONT_WEIGHT,           XTK_VALUE_ENUM,     apply_font_weight               },
  {XTK_PROP_TEXT_ALIGN,            XTK_VALUE_ENUM,     apply_text_align                },
  {XTK_PROP_FOREGROUND,            XTK_VALUE_COLOR,    apply_foreground                },
  {XTK_PROP_TOOLTIP,               XTK_VALUE_STRING,   apply_tooltip                   },
  {XTK_PROP_ORIENTATION,           XTK_VALUE_ENUM,     apply_orientation               },
  {XTK_PROP_ALIGN_ITEMS,           XTK_VALUE_ENUM,     apply_align_items               },
  {XTK_PROP_JUSTIFY_CONTENT,       XTK_VALUE_ENUM,     apply_justify_content           },
  {XTK_PROP_ITEM_COUNT,            XTK_VALUE_I32,      apply_item_count                },
  {XTK_PROP_ITEMS,                 XTK_VALUE_BYTES,    apply_items                     },
  {XTK_PROP_SHOULD_LOOP,           XTK_VALUE_BOOL,     apply_should_loop               },
  {XTK_PROP_ITEM_HEIGHT,           XTK_VALUE_F32,      apply_item_height               },
  {XTK_PROP_ITEM_WIDTH,            XTK_VALUE_F32,      apply_item_width                },
  {XTK_PROP_COLUMN_COUNT,          XTK_VALUE_I32,      apply_column_count              },
  {XTK_PROP_SELECTED_INDEX,        XTK_VALUE_I32,      apply_selected_index            },
  {XTK_PROP_SELECTED,              XTK_VALUE_BOOL,     apply_selected                  },
  {XTK_PROP_OPEN,                  XTK_VALUE_BOOL,     apply_open                      },
  {XTK_PROP_EXPANDED,              XTK_VALUE_BOOL,     apply_expanded                  },
  {XTK_PROP_CONTENT_HEIGHT,        XTK_VALUE_F32,      apply_content_height            },
  {XTK_PROP_MESSAGE,               XTK_VALUE_STRING,   apply_message                   },
  {XTK_PROP_SEVERITY,              XTK_VALUE_ENUM,     apply_severity                  },
  {XTK_PROP_CLOSABLE,              XTK_VALUE_BOOL,     apply_closable                  },
  {XTK_PROP_PULL_DIRECTION,        XTK_VALUE_ENUM,     apply_pull_direction            },
  {XTK_PROP_VISUALIZER_SIZE,       XTK_VALUE_F32,      apply_visualizer_size           },
  {XTK_PROP_THRESHOLD_RATIO,       XTK_VALUE_F32,      apply_threshold_ratio           },
  {XTK_PROP_REFRESHING,            XTK_VALUE_BOOL,     apply_refreshing                },
  {XTK_PROP_SELECTION_MODE,        XTK_VALUE_ENUM,     apply_selection_mode            },
  {XTK_PROP_ITEM_INVOKED_ENABLED,  XTK_VALUE_BOOL,     apply_item_invoked_enabled      },
  {XTK_PROP_MAX_VISIBLE,           XTK_VALUE_I32,      apply_max_visible               },
  {XTK_PROP_NAV_VISIBILITY,        XTK_VALUE_ENUM,     apply_nav_visibility            },
  {XTK_PROP_GAP,                   XTK_VALUE_F32,      apply_gap                       },
  {XTK_PROP_PADDING_LEFT,          XTK_VALUE_F32,      apply_padding_left              },
  {XTK_PROP_PADDING_TOP,           XTK_VALUE_F32,      apply_padding_top               },
  {XTK_PROP_PADDING_RIGHT,         XTK_VALUE_F32,      apply_padding_right             },
  {XTK_PROP_PADDING_BOTTOM,        XTK_VALUE_F32,      apply_padding_bottom            },
  {XTK_PROP_FILL,                  XTK_VALUE_BOOL,     apply_fill                      },
  {XTK_PROP_FIT_WIDTH,             XTK_VALUE_BOOL,     apply_fit_width                 },
  {XTK_PROP_FIT_HEIGHT,            XTK_VALUE_BOOL,     apply_fit_height                },
  {XTK_PROP_WRAP,                  XTK_VALUE_ENUM,     apply_wrap                      },
  {XTK_PROP_PLACEHOLDER,           XTK_VALUE_STRING,   apply_placeholder               },
  {XTK_PROP_IMAGE_SOURCE,          XTK_VALUE_RESOURCE, apply_image_source              },
  {XTK_PROP_IMAGE_STRETCH,         XTK_VALUE_ENUM,     apply_image_stretch             },
  {XTK_PROP_NAV_MODEL,             XTK_VALUE_BYTES,    apply_nav_model                 },
  {XTK_PROP_SUBTITLE,              XTK_VALUE_STRING,   apply_subtitle                  },
  {XTK_PROP_PRIMARY_TEXT,          XTK_VALUE_STRING,   apply_dialog_text               },
  {XTK_PROP_SECONDARY_TEXT,        XTK_VALUE_STRING,   apply_dialog_text               },
  {XTK_PROP_CLOSE_TEXT,            XTK_VALUE_STRING,   apply_close_text                },
  {XTK_PROP_ACTION_TEXT,           XTK_VALUE_STRING,   apply_teachtip_config_prop      },
  {XTK_PROP_PREFIX,                XTK_VALUE_STRING,   apply_pager_text                },
  {XTK_PROP_SUFFIX,                XTK_VALUE_STRING,   apply_pager_text                },
  {XTK_PROP_INITIALS,              XTK_VALUE_STRING,   apply_initials                  },
  {XTK_PROP_BADGE_GLYPH,           XTK_VALUE_STRING,   apply_badge_glyph               },
  {XTK_PROP_BADGE_MODE,            XTK_VALUE_ENUM,     apply_badge_mode                },
  {XTK_PROP_BADGE_VALUE,           XTK_VALUE_I32,      apply_badge_value               },
  {XTK_PROP_DISPLAY_MODE,          XTK_VALUE_ENUM,     apply_display_mode              },
  {XTK_PROP_PLACEMENT,             XTK_VALUE_ENUM,     apply_placement                 },
  {XTK_PROP_TAIL_VISIBILITY,       XTK_VALUE_ENUM,     apply_teachtip_config_prop      },
  {XTK_PROP_FIRST_BUTTON,          XTK_VALUE_ENUM,     apply_pager_button              },
  {XTK_PROP_PREVIOUS_BUTTON,       XTK_VALUE_ENUM,     apply_pager_button              },
  {XTK_PROP_NEXT_BUTTON,           XTK_VALUE_ENUM,     apply_pager_button              },
  {XTK_PROP_LAST_BUTTON,           XTK_VALUE_ENUM,     apply_pager_button              },
  {XTK_PROP_PANE_PLACEMENT,        XTK_VALUE_ENUM,     apply_pane_config               },
  {XTK_PROP_ACTION_ACCENT,         XTK_VALUE_BOOL,     apply_teachtip_config_prop      },
  {XTK_PROP_UNTARGETED,            XTK_VALUE_BOOL,     apply_untargeted                },
  {XTK_PROP_LIGHT_DISMISS,         XTK_VALUE_BOOL,     apply_teachtip_config_prop      },
  {XTK_PROP_IS_GROUP,              XTK_VALUE_BOOL,     apply_group                     },
  {XTK_PROP_READ_ONLY,             XTK_VALUE_BOOL,     apply_read_only                 },
  {XTK_PROP_MULTILINE,             XTK_VALUE_BOOL,     apply_multiline                 },
  {XTK_PROP_MAX_LINES,             XTK_VALUE_I32,      apply_max_lines                 },
  {XTK_PROP_TEXT_TRIM,             XTK_VALUE_ENUM,     apply_text_trim                 },
  {XTK_PROP_TEXT_WRAP,             XTK_VALUE_BOOL,     apply_text_wrap                 },
  {XTK_PROP_SELECTABLE,            XTK_VALUE_BOOL,     apply_selectable                },
  {XTK_PROP_CLEAR_ENABLED,         XTK_VALUE_BOOL,     apply_rating_behavior_prop      },
  {XTK_PROP_BACK_DISABLED,         XTK_VALUE_BOOL,     apply_back_state                },
  {XTK_PROP_INTEGRATE_WINDOW,      XTK_VALUE_BOOL,     apply_integrate_window          },
  {XTK_PROP_HIDE_SPIN,             XTK_VALUE_BOOL,     apply_hide_spin                 },
  {XTK_PROP_SHOW_BACK,             XTK_VALUE_BOOL,     apply_back_state                },
  {XTK_PROP_SHOW_PANE_TOGGLE,      XTK_VALUE_BOOL,     apply_show_toggle               },
  {XTK_PROP_OPEN_PANE_LENGTH,      XTK_VALUE_F32,      apply_pane_config               },
  {XTK_PROP_COMPACT_PANE_LENGTH,   XTK_VALUE_F32,      apply_pane_config               },
  {XTK_PROP_PLACEMENT_MARGIN,      XTK_VALUE_F32,      apply_teachtip_config_prop      },
  {XTK_PROP_PLACEHOLDER_F64,       XTK_VALUE_F64,      apply_rating_behavior_prop      },
  {XTK_PROP_INITIAL_VALUE,         XTK_VALUE_I32,      apply_rating_behavior_prop      },
  {XTK_PROP_URL,                   XTK_VALUE_STRING,   apply_url                       },
  {XTK_PROP_ADD_VISIBLE,           XTK_VALUE_BOOL,     apply_add_visible               },
  {XTK_PROP_HEADER,                XTK_VALUE_STRING,   flux_host_apply_header          },
  {XTK_PROP_DATE,                  XTK_VALUE_I32,      flux_host_apply_date            },
  {XTK_PROP_TIME,                  XTK_VALUE_I32,      flux_host_apply_time            },
  {XTK_PROP_MIN_YEAR,              XTK_VALUE_I32,      flux_host_apply_min_year        },
  {XTK_PROP_MAX_YEAR,              XTK_VALUE_I32,      flux_host_apply_max_year        },
  {XTK_PROP_MIN_DATE,              XTK_VALUE_I32,      flux_host_apply_min_date        },
  {XTK_PROP_MAX_DATE,              XTK_VALUE_I32,      flux_host_apply_max_date        },
  {XTK_PROP_FIRST_DAY,             XTK_VALUE_I32,      flux_host_apply_first_day       },
  {XTK_PROP_CLOCK,                 XTK_VALUE_ENUM,     flux_host_apply_clock           },
  {XTK_PROP_MINUTE_INCREMENT,      XTK_VALUE_I32,      flux_host_apply_minute_increment},
  {XTK_PROP_COLOR,                 XTK_VALUE_COLOR,    flux_host_apply_color           },
  {XTK_PROP_ALPHA_ENABLED,         XTK_VALUE_BOOL,     flux_host_apply_alpha_enabled   },
  {XTK_PROP_MORE_BUTTON_VISIBLE,   XTK_VALUE_BOOL,     flux_host_apply_more_button     },
  {XTK_PROP_SPECTRUM,              XTK_VALUE_ENUM,     flux_host_apply_spectrum        },
  {XTK_PROP_IS_THREE_STATE,        XTK_VALUE_BOOL,     apply_three_state               },
  {XTK_PROP_PROGRESS_STATE,        XTK_VALUE_ENUM,     apply_progress_state            },
  {XTK_PROP_PROGRESS_ACTIVE,       XTK_VALUE_BOOL,     apply_progress_active           },
  {XTK_PROP_PROGRESS_BORDER_WIDTH, XTK_VALUE_F32,      apply_progress_border_width     },
  {XTK_PROP_IS_EDITABLE,           XTK_VALUE_BOOL,     apply_is_editable               },
  {XTK_PROP_IS_RANGE,              XTK_VALUE_BOOL,     apply_is_range                  },
  {XTK_PROP_RANGE_START,           XTK_VALUE_F32,      apply_range_start               },
  {XTK_PROP_RANGE_END,             XTK_VALUE_F32,      apply_range_end                 },
};

static FluxNativePropEntry const *native_prop_entry(XtkPropPatch const *patch) {
	for (size_t i = 0; i < sizeof(native_props) / sizeof(native_props [0]); i++)
		if (native_props [i].id == patch->id && native_props [i].kind == patch->value.kind) return &native_props [i];
	return NULL;
}

bool flux_host_can_project(XtkCtrlId type, XtkPropPatch const *patch) {
	if (!native_prop_entry(patch)) return false;
	if (patch->id == XTK_PROP_MIN_F32)
		return type == XTK_CTRL_SLIDER || type == XTK_CTRL_PROGRESS || type == XTK_CTRL_PROGRESSRING;
	return true;
}

bool flux_host_apply_native(FluxHostState *R, XtkCtrl control, XtkPropPatch const *patch) {
	XentNodeId                 node  = flux_host_as_node(control);
	XtkCtrlId                  type  = flux_host_types_get(R, node);
	XtkValue const            *v     = &patch->value;
	FluxNativePropEntry const *entry = native_prop_entry(patch);
	return entry && entry->fn(R, control, node, type, v);
}
