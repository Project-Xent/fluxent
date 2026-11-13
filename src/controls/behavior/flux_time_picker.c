#include "controls/behavior/flux_picker_internal.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_picker.h"
#include "runtime/flux_date.h"
#include "runtime/flux_str.h"
#include <xtk/xtk.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
	COL_HOUR   = 0,
	COL_MINUTE = 1,
	COL_PERIOD = 2,
};

#define TIME_MAX_ITEMS 60

static FluxTimePickerData *time_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_TIME_PICKER) return NULL;
	return ( FluxTimePickerData * ) nd->component_data;
}

static bool time_resolve_h12(int clock) {
	if (clock == XTK_CLOCK_12H) return true;
	if (clock == XTK_CLOCK_24H) return false;
	return !flux_locale_isclock24();
}

static int  time_step(FluxTimePickerData const *td) { return td->step > 0 ? td->step : 60; }

static void time_resolve_order(FluxTimePickerData *td) {
	int all [3];
	flux_locale_timeorder(all);
	td->columns = 0;
	for (int i = 0; i < 3; ++i)
		if (all [i] != COL_PERIOD || td->h12) td->order [td->columns++] = all [i];
}

static int time_column(FluxTimePickerData const *td, int kind) {
	for (int i = 0; i < td->columns; ++i)
		if (td->order [i] == kind) return i;
	return -1;
}

static int time_hour_label(FluxTimePickerData const *td, int hour) {
	if (!td->h12) return hour;
	return hour % 12 == 0 ? 12 : hour % 12;
}

static void time_face_cell(FluxTimePickerData const *td, int kind, char const *am, char const *pm, char *out) {
	bool        has   = td->base.has_value;
	int         hour  = has ? td->minutes / 60 : 0;
	size_t      cap   = sizeof(td->base.text [0]);
	char const *blank = kind == COL_HOUR ? "hour" : "minute";
	if (kind == COL_PERIOD) {
		snprintf(out, cap, "%s", has && hour >= 12 ? pm : am);
		return;
	}
	if (!has) {
		snprintf(out, cap, "%s", blank);
		return;
	}
	snprintf(
	  out, cap, kind == COL_HOUR ? "%d" : "%02d", kind == COL_HOUR ? time_hour_label(td, hour) : td->minutes % 60
	);
}

static int time_coerce(FluxTimePickerData const *td, int minutes) {
	if (minutes < 0) return -1;
	int value = minutes % (24 * 60);
	return value - value % 60 % time_step(td);
}

static void time_face(FluxPickerData *pd) {
	FluxTimePickerData *td = ( FluxTimePickerData * ) pd;
	char                am [16], pm [16];
	flux_locale_ampm(am, pm, sizeof(am));

	td->minutes   = time_coerce(td, td->minutes);
	pd->value     = td->minutes;
	pd->has_value = td->minutes >= 0;
	pd->columns   = td->columns;
	for (int i = 0; i < pd->columns; i++) {
		pd->weight [i] = 1.0f;
		pd->left [i]   = false;
		time_face_cell(td, td->order [i], am, pm, pd->text [i]);
	}
}

typedef struct TimeItems {
	char        text [TIME_MAX_ITEMS][32];
	char const *view [TIME_MAX_ITEMS];
	int         count;
} TimeItems;

static void time_items_hour(FluxTimePickerData const *td, TimeItems *it) {
	it->count = td->h12 ? 12 : 24;
	for (int i = 0; i < it->count; i++) {
		snprintf(it->text [i], sizeof(it->text [i]), "%d", td->h12 && i == 0 ? 12 : i);
		it->view [i] = it->text [i];
	}
}

static void time_items_minute(FluxTimePickerData const *td, TimeItems *it) {
	it->count = 0;
	for (int m = 0; m < 60 && it->count < TIME_MAX_ITEMS; m += time_step(td)) {
		snprintf(it->text [it->count], sizeof(it->text [0]), "%02d", m);
		it->view [it->count] = it->text [it->count];
		it->count++;
	}
}

static void time_items_period(TimeItems *it) {
	flux_locale_ampm(it->text [0], it->text [1], sizeof(it->text [0]));
	it->view [0] = it->text [0];
	it->view [1] = it->text [1];
	it->count    = 2;
}

static void time_load(FluxPickerData *pd) {
	FluxTimePickerData *td  = ( FluxTimePickerData * ) pd;
	int                 now = td->minutes >= 0 ? td->minutes : flux_date_now();
	TimeItems          *it  = ( TimeItems * ) malloc(sizeof(*it));
	if (!it) return;

	td->loading = true;
	td->hour    = now / 60;
	td->minute  = now % 60 / time_step(td) * time_step(td);

	time_items_hour(td, it);
	flux_pflyout_setcolumn(
	  pd->flyout, time_column(td, COL_HOUR), it->view, it->count, td->h12 ? td->hour % 12 : td->hour
	);
	time_items_minute(td, it);
	flux_pflyout_setcolumn(pd->flyout, time_column(td, COL_MINUTE), it->view, it->count, td->minute / time_step(td));
	if (td->h12) {
		time_items_period(it);
		flux_pflyout_setcolumn(pd->flyout, time_column(td, COL_PERIOD), it->view, it->count, td->hour >= 12 ? 1 : 0);
	}
	td->loading = false;
	free(it);
}

static void time_select(FluxPickerData *pd, int column, int index) {
	FluxTimePickerData *td = ( FluxTimePickerData * ) pd;
	if (td->loading || column < 0 || column >= td->columns) return;
	int kind = td->order [column];
	if (kind == COL_MINUTE) {
		td->minute = index * time_step(td);
		return;
	}
	bool pm = td->hour >= 12;
	if (kind == COL_PERIOD) pm = index == 1;
	else td->hour = index;
	if (td->h12) td->hour = td->hour % 12 + (pm ? 12 : 0);
}

static int time_accept(FluxPickerData *pd) {
	FluxTimePickerData *td = ( FluxTimePickerData * ) pd;
	td->minutes            = td->hour * 60 + td->minute;
	time_face(pd);
	return td->minutes;
}

static void time_fill(FluxPickerData *pd, FluxPickerFlyout *fly) {
	FluxTimePickerData *td = ( FluxTimePickerData * ) pd;
	for (int i = 0; i < td->columns; ++i) {
		int  kind = td->order [i];
		bool loop = kind == COL_HOUR || (kind == COL_MINUTE && td->step > 0);
		flux_pflyout_addcolumn(fly, &(FluxPickerColumnSpec) {.weight = 1.0f, .loop = loop});
	}
}

static FluxPickerOps const time_ops = {
  .width  = FLUX_TIME_PICKER_W,
  .face   = time_face,
  .fill   = time_fill,
  .load   = time_load,
  .select = time_select,
  .accept = time_accept,
};

XentNodeId flux_timepicker_init(FluxTimePickerDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_TIME_PICKER);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxTimePickerData *td = ( FluxTimePickerData * ) calloc(1, sizeof(*td));
	if (!td) return flux_factory_fail_node(info->ctx, info->store, root);
	if (!flux_picker_setup(&td->base, info->ctx, info->store, root, FLUX_CONTROL_TIME_PICKER, FLUX_TIME_PICKER_W)) {
		free(td);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	td->base.ops           = &time_ops;
	td->base.input         = info->input;
	td->base.window        = info->window;
	td->base.overlay       = info->overlay;
	td->base.header        = flux_str_dup(info->header && info->header [0] ? info->header : NULL);
	td->base.on_change     = info->on_change;
	td->base.on_change_ctx = info->userdata;
	td->minutes            = info->minutes;
	td->clock              = info->clock;
	td->step               = info->step >= 0 && info->step < 60 ? info->step : 1;
	td->h12                = time_resolve_h12(td->clock);
	time_resolve_order(td);

	time_face(&td->base);
	flux_picker_refresh(&td->base);
	return root;
}

void flux_timepicker_settime(FluxStore *store, XentNodeId id, int minutes) {
	FluxTimePickerData *td = time_data(store, id);
	if (!td) return;
	int next = time_coerce(td, minutes);
	if (td->minutes == next) return;
	td->minutes = next;
	time_face(&td->base);
	flux_picker_refresh(&td->base);
}

static void time_reset_flyout(FluxTimePickerData *td) {
	flux_picker_close(td->base.store, td->base.root);
	flux_pflyout_destroy(td->base.flyout);
	td->base.flyout = NULL;
	time_face(&td->base);
	flux_picker_refresh(&td->base);
}

void flux_timepicker_setclock(FluxStore *store, XentNodeId id, int clock) {
	FluxTimePickerData *td = time_data(store, id);
	if (!td || td->clock == clock) return;
	td->clock = clock;
	td->h12   = time_resolve_h12(clock);
	time_resolve_order(td);
	time_reset_flyout(td);
}

void flux_timepicker_setstep(FluxStore *store, XentNodeId id, int step) {
	FluxTimePickerData *td = time_data(store, id);
	if (!td) return;
	td->step = step >= 0 && step < 60 ? step : 1;
	time_face(&td->base);
	flux_picker_refresh(&td->base);
}
