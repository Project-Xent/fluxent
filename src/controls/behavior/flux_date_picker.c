#include "controls/behavior/flux_picker_internal.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_picker.h"
#include "runtime/flux_date.h"
#include "runtime/flux_str.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum
{
	COL_DAY   = 0,
	COL_MONTH = 1,
	COL_YEAR  = 2,
};

#define DATE_WEEK_DAY_WEIGHT   78.0f
#define DATE_WEEK_MONTH_WEIGHT 132.0f
#define DATE_MAX_YEARS         1000
#define DATE_CENTURY           100

static FluxDatePickerData *date_data(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_DATE_PICKER) return NULL;
	return ( FluxDatePickerData * ) nd->component_data;
}

static float date_weight(int kind) { return kind == COL_MONTH ? DATE_WEEK_MONTH_WEIGHT : DATE_WEEK_DAY_WEIGHT; }

static int   date_position(FluxDatePickerData const *dd, int kind) {
	for (int i = 0; i < FLUX_PICKER_COLUMNS; i++)
		if (dd->order [i] == kind) return i;
	return 0;
}

static int  date_clamp(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void date_kind_text(FluxDatePickerData const *dd, int kind, char *out, size_t size) {
	static wchar_t const *const picture [3] = {L"d", L"MMMM", L"yyyy"};
	flux_locale_formatdate(dd->date, picture [kind], out, size);
}

static void date_face(FluxPickerData *pd) {
	FluxDatePickerData      *dd        = ( FluxDatePickerData * ) pd;
	static char const *const label [3] = {"day", "month", "year"};

	pd->has_value                      = flux_date_valid(dd->date);
	if (pd->has_value)
		dd->date = date_clamp(dd->date, flux_date_pack(dd->min_year, 1, 1), flux_date_pack(dd->max_year, 12, 31));
	pd->value   = dd->date;
	pd->columns = FLUX_PICKER_COLUMNS;
	for (int i = 0; i < FLUX_PICKER_COLUMNS; i++) {
		int kind       = dd->order [i];
		pd->weight [i] = date_weight(kind);
		pd->left [i]   = kind == COL_MONTH;
		if (pd->has_value) date_kind_text(dd, kind, pd->text [i], sizeof(pd->text [i]));
		else snprintf(pd->text [i], sizeof(pd->text [i]), "%s", label [kind]);
	}
}

typedef struct DateItems {
	char        text [DATE_MAX_YEARS][64];
	char const *view [DATE_MAX_YEARS];
	int         count;
} DateItems;

static void date_items_number(DateItems *it, int first, int count) {
	if (count > DATE_MAX_YEARS) count = DATE_MAX_YEARS;
	for (int i = 0; i < count; i++) {
		snprintf(it->text [i], sizeof(it->text [i]), "%d", first + i);
		it->view [i] = it->text [i];
	}
	it->count = count;
}

static void date_items_month(DateItems *it) {
	for (int m = 1; m <= 12; m++) {
		flux_locale_formatdate(flux_date_pack(2000, m, 1), L"MMMM", it->text [m - 1], sizeof(it->text [0]));
		it->view [m - 1] = it->text [m - 1];
	}
	it->count = 12;
}

static void date_set_day_wheel(FluxDatePickerData *dd) {
	DateItems *it = ( DateItems * ) malloc(sizeof(*it));
	if (!it) return;
	date_items_number(it, 1, flux_date_daysin(dd->year, dd->month));
	flux_pflyout_setcolumn(dd->base.flyout, date_position(dd, COL_DAY), it->view, it->count, dd->day - 1);
	free(it);
}

static void date_load(FluxPickerData *pd) {
	FluxDatePickerData *dd = ( FluxDatePickerData * ) pd;
	int                 d  = flux_date_valid(dd->date) ? dd->date : flux_date_today();

	dd->loading            = true;
	dd->year               = date_clamp(flux_date_year(d), dd->min_year, dd->max_year);
	dd->month              = flux_date_month(d);
	dd->day                = date_clamp(flux_date_day(d), 1, flux_date_daysin(dd->year, dd->month));

	DateItems *it          = ( DateItems * ) malloc(sizeof(*it));
	if (it) {
		date_items_month(it);
		flux_pflyout_setcolumn(pd->flyout, date_position(dd, COL_MONTH), it->view, it->count, dd->month - 1);
		date_items_number(it, dd->min_year, dd->max_year - dd->min_year + 1);
		flux_pflyout_setcolumn(pd->flyout, date_position(dd, COL_YEAR), it->view, it->count, dd->year - dd->min_year);
		free(it);
	}
	date_set_day_wheel(dd);
	dd->loading = false;
}

static void date_select(FluxPickerData *pd, int column, int index) {
	FluxDatePickerData *dd = ( FluxDatePickerData * ) pd;
	if (dd->loading) return;

	int kind = dd->order [column];
	if (kind == COL_DAY) {
		dd->day = index + 1;
		return;
	}
	if (kind == COL_MONTH) dd->month = index + 1;
	else dd->year = dd->min_year + index;

	int last = flux_date_daysin(dd->year, dd->month);
	if (dd->day > last) dd->day = last;
	dd->loading = true;
	date_set_day_wheel(dd);
	dd->loading = false;
}

static int date_accept(FluxPickerData *pd) {
	FluxDatePickerData *dd = ( FluxDatePickerData * ) pd;
	dd->date               = flux_date_pack(dd->year, dd->month, dd->day);
	date_face(pd);
	return dd->date;
}

static void date_fill(FluxPickerData *pd, FluxPickerFlyout *fly) {
	FluxDatePickerData *dd = ( FluxDatePickerData * ) pd;
	for (int i = 0; i < FLUX_PICKER_COLUMNS; i++) {
		int kind = dd->order [i];
		flux_pflyout_addcolumn(
		  fly,
		  &(FluxPickerColumnSpec) {.weight = date_weight(kind), .loop = kind != COL_YEAR, .left = kind == COL_MONTH}
		);
	}
}

static FluxPickerOps const date_ops = {
  .width  = FLUX_DATE_PICKER_W,
  .face   = date_face,
  .fill   = date_fill,
  .load   = date_load,
  .select = date_select,
  .accept = date_accept,
};

XentNodeId flux_datepicker_init(FluxDatePickerDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_DATE_PICKER);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxDatePickerData *dd = ( FluxDatePickerData * ) calloc(1, sizeof(*dd));
	if (!dd) return flux_factory_fail_node(info->ctx, info->store, root);
	if (!flux_picker_setup(&dd->base, info->ctx, info->store, root, FLUX_CONTROL_DATE_PICKER, FLUX_DATE_PICKER_W)) {
		free(dd);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	int year               = flux_date_year(flux_date_today());
	dd->base.ops           = &date_ops;
	dd->base.input         = info->input;
	dd->base.window        = info->window;
	dd->base.overlay       = info->overlay;
	dd->base.header        = flux_str_dup(info->header && info->header [0] ? info->header : NULL);
	dd->base.on_change     = info->on_change;
	dd->base.on_change_ctx = info->userdata;
	dd->min_year           = info->min_year > 0 ? info->min_year : year - DATE_CENTURY;
	dd->max_year           = info->max_year >= dd->min_year ? info->max_year : dd->min_year + 2 * DATE_CENTURY;
	if (dd->max_year - dd->min_year >= DATE_MAX_YEARS) dd->max_year = dd->min_year + DATE_MAX_YEARS - 1;
	dd->date = info->date;
	flux_locale_dateorder(dd->order);

	date_face(&dd->base);
	flux_picker_refresh(&dd->base);
	return root;
}

void flux_datepicker_setdate(FluxStore *store, XentNodeId id, int date) {
	FluxDatePickerData *dd = date_data(store, id);
	if (!dd || dd->date == date) return;
	dd->date = date;
	date_face(&dd->base);
	flux_picker_refresh(&dd->base);
}

static void date_set_years(FluxDatePickerData *dd, int min_year, int max_year) {
	if (max_year < min_year || max_year - min_year >= DATE_MAX_YEARS) return;
	dd->min_year = min_year;
	dd->max_year = max_year;
	date_face(&dd->base);
	flux_picker_refresh(&dd->base);
}

void flux_datepicker_setminyear(FluxStore *store, XentNodeId id, int year) {
	FluxDatePickerData *dd = date_data(store, id);
	if (dd && year > 0) date_set_years(dd, year, dd->max_year < year ? year : dd->max_year);
}

void flux_datepicker_setmaxyear(FluxStore *store, XentNodeId id, int year) {
	FluxDatePickerData *dd = date_data(store, id);
	if (dd && year > 0) date_set_years(dd, dd->min_year > year ? year : dd->min_year, year);
}
