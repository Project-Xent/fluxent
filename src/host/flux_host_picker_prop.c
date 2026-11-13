#include "host/flux_host_picker_prop.h"

#include "controls/factory/flux_init_picker.h"
#include "controls/behavior/flux_calendar.h"
#include "controls/behavior/flux_color_picker.h"

static bool is_date_picker(XtkCtrlId type) { return type == XTK_CTRL_DATEPICKER; }

static bool is_time_picker(XtkCtrlId type) { return type == XTK_CTRL_TIMEPICKER; }

FLUX_PICKER_PROP(flux_host_apply_header) {
	( void ) control;
	if (!is_date_picker(type) && !is_time_picker(type)) return false;
	flux_picker_setheader(R->store, node, v->u.string.data ? v->u.string.data : "");
	return true;
}

FLUX_PICKER_PROP(flux_host_apply_date) {
	( void ) control;
	if (type == XTK_CTRL_CALENDAR) return flux_calendar_set(R->store, node, XTK_PROP_DATE, v);
	if (!is_date_picker(type)) return false;
	flux_datepicker_setdate(R->store, node, v->u.i32);
	return true;
}

FLUX_PICKER_PROP(flux_host_apply_time) {
	( void ) control;
	if (!is_time_picker(type)) return false;
	flux_timepicker_settime(R->store, node, v->u.i32);
	return true;
}

FLUX_PICKER_PROP(flux_host_apply_min_year) {
	( void ) control;
	if (!is_date_picker(type)) return false;
	flux_datepicker_setminyear(R->store, node, v->u.i32);
	return true;
}

FLUX_PICKER_PROP(flux_host_apply_max_year) {
	( void ) control;
	if (!is_date_picker(type)) return false;
	flux_datepicker_setmaxyear(R->store, node, v->u.i32);
	return true;
}

FLUX_PICKER_PROP(flux_host_apply_clock) {
	( void ) control;
	if (!is_time_picker(type)) return false;
	flux_timepicker_setclock(R->store, node, ( int ) v->u.u32);
	return true;
}

FLUX_PICKER_PROP(flux_host_apply_minute_increment) {
	( void ) control;
	if (!is_time_picker(type)) return false;
	flux_timepicker_setstep(R->store, node, v->u.i32);
	return true;
}

FLUX_PICKER_PROP(flux_host_apply_min_date) {
	( void ) control;
	return type == XTK_CTRL_CALENDAR && flux_calendar_set(R->store, node, XTK_PROP_MIN_DATE, v);
}

FLUX_PICKER_PROP(flux_host_apply_max_date) {
	( void ) control;
	return type == XTK_CTRL_CALENDAR && flux_calendar_set(R->store, node, XTK_PROP_MAX_DATE, v);
}

FLUX_PICKER_PROP(flux_host_apply_first_day) {
	( void ) control;
	return type == XTK_CTRL_CALENDAR && flux_calendar_set(R->store, node, XTK_PROP_FIRST_DAY, v);
}

FLUX_PICKER_PROP(flux_host_apply_color) {
	( void ) control;
	return type == XTK_CTRL_COLORPICKER && flux_colorpicker_set(R->store, node, XTK_PROP_COLOR, v);
}

FLUX_PICKER_PROP(flux_host_apply_alpha_enabled) {
	( void ) control;
	return type == XTK_CTRL_COLORPICKER && flux_colorpicker_set(R->store, node, XTK_PROP_ALPHA_ENABLED, v);
}

FLUX_PICKER_PROP(flux_host_apply_more_button) {
	( void ) control;
	return type == XTK_CTRL_COLORPICKER && flux_colorpicker_set(R->store, node, XTK_PROP_MORE_BUTTON_VISIBLE, v);
}

FLUX_PICKER_PROP(flux_host_apply_spectrum) {
	( void ) control;
	return type == XTK_CTRL_COLORPICKER && flux_colorpicker_set(R->store, node, XTK_PROP_SPECTRUM, v);
}
