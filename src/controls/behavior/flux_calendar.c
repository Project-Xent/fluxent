#include "controls/behavior/flux_calendar.h"
#include "controls/factory/flux_factory.h"
#include "render/flux_calendar_geom.h"
#include "render/flux_anim.h"
#include "bridge/flux_internal.h"
#include "runtime/flux_date.h"
#include "runtime/flux_time.h"
#include "runtime/flux_anim_driver.h"
#include <fluxent/flux_input.h>

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CAL_SCROLL_S     0.25
#define CAL_WHEEL_IDLE_S 0.12

static double            calendar_now(void) { return flux_perf_seconds(flux_perf_now()); }

static FluxCalendarData *calendar_data(FluxStore *store, XentNodeId node) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->component_type != FLUX_CONTROL_CALENDAR_VIEW) return NULL;
	return nd->component_data;
}

static int calendar_coerce(FluxCalendarData const *d, int date) {
	return date < d->min_date ? d->min_date : (date > d->max_date ? d->max_date : date);
}

static int calendar_units(int view, int a, int b) {
	if (view == FLUX_CAL_MONTH) return flux_date_between(a, b);
	int months = (flux_date_year(b) - flux_date_year(a)) * 12 + flux_date_month(b) - flux_date_month(a);
	return view == FLUX_CAL_YEAR ? months : flux_date_year(b) - flux_date_year(a);
}

static int calendar_add_units(int view, int date, int n) {
	if (view == FLUX_CAL_MONTH) return flux_date_shift(date, n);
	return flux_date_shiftmonth(date, view == FLUX_CAL_YEAR ? n : 12 * n);
}

static int calendar_scope_first(int view, int date) {
	int y = flux_date_year(date);
	if (view == FLUX_CAL_MONTH) return flux_date_pack(y, flux_date_month(date), 1);
	return flux_date_pack(view == FLUX_CAL_YEAR ? y : y / 10 * 10, 1, 1);
}

static int calendar_scope_last(int view, int date) {
	int y = flux_date_year(date), m = flux_date_month(date);
	if (view == FLUX_CAL_MONTH) return flux_date_pack(y, m, flux_date_daysin(y, m));
	return flux_date_pack(view == FLUX_CAL_YEAR ? y : y / 10 * 10 + 9, 12, 31);
}

static int calendar_add_scopes(int view, int date, int n) {
	static int const months [3] = {1, 12, 120};
	return view == FLUX_CAL_MONTH ? flux_date_shiftmonth(date, n) : flux_date_shiftmonth(date, n * months [view]);
}

int flux_calendar_size(FluxCalendarData const *d, int view) {
	return calendar_units(view, d->min_date, d->max_date) + 1;
}

int flux_calendar_start(FluxCalendarData const *d, int view) {
	return view == FLUX_CAL_MONTH ? (flux_date_weekday(d->min_date) - d->first_day + 7) % 7 : 0;
}

int flux_calendar_date_at(FluxCalendarData const *d, int view, int index) {
	return calendar_add_units(view, d->min_date, index);
}

int flux_calendar_index_of(FluxCalendarData const *d, int view, int date) {
	return calendar_units(view, d->min_date, date);
}

static void calendar_dims(FluxCalendarData const *d, int view, int *cols, int *rows) {
	XentRect rect = {0};
	xent_layout_rect(d->ctx, d->node, &rect);
	FluxRect         bounds = {0.0f, 0.0f, rect.w, rect.h};
	FluxCalendarGeom g      = flux_calendar_layout(d, view, &bounds);
	*cols                   = g.cols;
	*rows                   = g.rows;
}

static int calendar_row_of(FluxCalendarData const *d, int view, int index) {
	int cols, rows;
	calendar_dims(d, view, &cols, &rows);
	return (index + flux_calendar_start(d, view)) / cols;
}

static float calendar_max_row(FluxCalendarData const *d, int view) {
	int cols, rows;
	calendar_dims(d, view, &cols, &rows);
	int total = (flux_calendar_size(d, view) + flux_calendar_start(d, view) + cols - 1) / cols;
	return total > rows ? ( float ) (total - rows) : 0.0f;
}

static float calendar_scroll_progress(FluxCalendarScroll const *s, double now) {
	if (s->start < 0.0) return 1.0f;
	double t = (now - s->start) / CAL_SCROLL_S;
	return t >= 1.0 ? 1.0f : flux_ease_out_cubic(( float ) t);
}

float flux_calendar_rowpos(FluxCalendarData const *d, int view, double now) {
	if (view != d->view || d->scroll.start < 0.0) return d->row [view];
	float e = calendar_scroll_progress(&d->scroll, now);
	return d->scroll.from + (d->scroll.to - d->scroll.from) * e;
}

static void calendar_visible(FluxCalendarData const *d, int view, float rowpos, int *first, int *last) {
	int cols, rows;
	calendar_dims(d, view, &cols, &rows);
	int start = flux_calendar_start(d, view), size = flux_calendar_size(d, view);
	int r0 = ( int ) floorf(rowpos + 0.001f), r1 = ( int ) ceilf(rowpos + ( float ) rows - 0.001f) - 1;
	*first = r0 * cols - start;
	*last  = (r1 + 1) * cols - 1 - start;
	if (*first < 0) *first = 0;
	if (*last > size - 1) *last = size - 1;
}

static void calendar_scope_of(FluxCalendarData const *d, int view, int first, int last, int out [2]) {
	int a0 = calendar_scope_first(view, first), a1 = calendar_scope_last(view, first);
	int pick = first;
	if (last > a1) {
		int    b0     = calendar_add_units(view, a1, 1);
		int    b1     = calendar_scope_last(view, b0);
		int    seen2  = calendar_units(view, b0, last < b1 ? last : b1) + 1;
		double share1 = ( double ) (calendar_units(view, first, a1) + 1) / (calendar_units(view, a0, a1) + 1);
		double share2 = ( double ) seen2 / (calendar_units(view, b0, b1) + 1);
		if (share1 < share2) pick = b1;
	}
	out [0] = calendar_coerce(d, calendar_scope_first(view, pick));
	out [1] = calendar_coerce(d, calendar_scope_last(view, pick));
}

static void calendar_scope(FluxCalendarData const *d, int view, float rowpos, int out [2]) {
	int first, last;
	calendar_visible(d, view, rowpos, &first, &last);
	calendar_scope_of(d, view, flux_calendar_date_at(d, view, first), flux_calendar_date_at(d, view, last), out);
}

static void calendar_scroll_settle(FluxCalendarData *d, double now) {
	if (d->scroll.start < 0.0 || calendar_scroll_progress(&d->scroll, now) < 1.0f) return;
	d->row [d->view] = d->scroll.to;
	d->scroll.start  = -1.0;
}

static void calendar_scroll_to(FluxCalendarData *d, float row, bool animate) {
	double now    = calendar_now();
	d->wheel_free = false;
	calendar_scroll_settle(d, now);
	float max = calendar_max_row(d, d->view);
	float at  = flux_calendar_rowpos(d, d->view, now);
	row       = row < 0.0f ? 0.0f : (row > max ? max : row);
	if (!animate) {
		d->row [d->view] = row;
		d->scroll.start  = -1.0;
		return;
	}
	if (d->scroll.start < 0.0) {
		int scope [2];
		calendar_scope(d, d->view, at, scope);
		d->frozen = (FluxCalendarFrozen) {scope [0], scope [1], 0, -1};
		calendar_visible(d, d->view, at, &d->frozen.first, &d->frozen.last);
	}
	d->scroll = (FluxCalendarScroll) {at, row, now};
}

static void calendar_bring_into_view(FluxCalendarData *d) {
	int first = calendar_coerce(d, calendar_scope_first(d->view, d->displayed));
	calendar_scroll_to(d, ( float ) calendar_row_of(d, d->view, flux_calendar_index_of(d, d->view, first)), false);
}

bool flux_calendar_is_selected(FluxCalendarData const *d, int date) {
	for (size_t i = 0; i < d->selected_count; ++i)
		if (d->selected [i] == date) return true;
	return false;
}

bool flux_calendar_is_today(int view, int date) {
	int today = flux_date_today();
	if (view == FLUX_CAL_MONTH) return date == today;
	if (view == FLUX_CAL_YEAR) return date / 100 == today / 100;
	return flux_date_year(date) == flux_date_year(today);
}

bool flux_calendar_out_of_scope(FluxCalendarData const *d, int view, int index, double now) {
	if (!d->out_of_scope_enabled) return false;
	int  date   = flux_calendar_date_at(d, view, index);
	bool moving = d->wheel_free || (d->scroll.start >= 0.0 && calendar_scroll_progress(&d->scroll, now) < 1.0f);
	if (view == d->view && moving) {
		if (index < d->frozen.first || index > d->frozen.last) return false;
		return date < d->frozen.scope_min || date > d->frozen.scope_max;
	}
	int scope [2];
	calendar_scope(d, view, flux_calendar_rowpos(d, view, now), scope);
	return date < scope [0] || date > scope [1];
}

void flux_calendar_header(FluxCalendarData const *d, double now, char *out, size_t size) {
	int scope [2];
	calendar_scope(d, d->view, flux_calendar_rowpos(d, d->view, now), scope);
	if (d->view == FLUX_CAL_MONTH) flux_locale_formattemplate(scope [0], L"month year", out, size);
	else if (d->view == FLUX_CAL_YEAR) flux_locale_formattemplate(scope [1], L"year", out, size);
	else snprintf(out, size, "%d - %d", flux_date_year(scope [0]), flux_date_year(scope [1]));
}

void flux_calendar_nav(FluxCalendarData const *d, double now, bool *before, bool *after) {
	int first, last;
	calendar_visible(d, d->view, flux_calendar_rowpos(d, d->view, now), &first, &last);
	*before = first > 0;
	*after  = last + 1 < flux_calendar_size(d, d->view);
}

static int calendar_copy_date(FluxCalendarData const *d, int view, int source, int target) {
	if (view == FLUX_CAL_MONTH) return calendar_coerce(d, source);
	int y    = flux_date_year(source);
	int m    = view == FLUX_CAL_YEAR ? flux_date_month(source) : flux_date_month(target);
	int days = flux_date_daysin(y, m), day = flux_date_day(target);
	return calendar_coerce(d, flux_date_pack(y, m, day > days ? days : day));
}

static void calendar_update_displayed(FluxCalendarData *d, int view, double now) {
	int   first, last, scope [2];
	float rowpos = flux_calendar_rowpos(d, view, now);
	calendar_visible(d, view, rowpos, &first, &last);
	int first_date = flux_calendar_date_at(d, view, first), last_date = flux_calendar_date_at(d, view, last);
	if (calendar_units(view, first_date, d->displayed) >= 0 && calendar_units(view, d->displayed, last_date) >= 0)
		return;
	calendar_scope(d, view, rowpos, scope);
	int pick     = scope [0] < first_date ? first_date : scope [0];
	d->displayed = calendar_copy_date(d, view, pick, d->displayed);
}

static void calendar_set_view(FluxCalendarData *d, int view, bool keyboard) {
	if (view == d->view || view < FLUX_CAL_MONTH || view > FLUX_CAL_DECADE) return;
	double now = calendar_now();
	calendar_scroll_settle(d, now);
	calendar_update_displayed(d, d->view, now);
	d->scroll.start = -1.0;
	d->old_view     = d->view;
	d->view         = view;
	d->view_since   = now;
	d->header_since = now;
	calendar_bring_into_view(d);
	d->focus        = d->focus >= 0 || keyboard ? flux_calendar_index_of(d, view, d->displayed) : d->focus;
	d->focus_visual = keyboard;
}

static void calendar_notify(FluxCalendarData *d, int date, bool selected) {
	flux_input_route_event(
	  flux_store_host(d->store), d->node, XTK_EVENT_DATE_SELECT,
	  (XtkEventPayload) {
		.kind = XTK_EVENT_PAYLOAD_INDEX_BOOL, .u.index_bool = {date, selected}
    },
	  XTK_EVENT_CAUSE_USER
	);
}

static bool calendar_append(FluxCalendarData *d, int date) {
	if (d->selected_count == d->selected_capacity) {
		size_t capacity = d->selected_capacity ? d->selected_capacity * 2 : 8;
		int   *grown    = realloc(d->selected, capacity * sizeof(*grown));
		if (!grown) return false;
		d->selected          = grown;
		d->selected_capacity = capacity;
	}
	d->selected [d->selected_count++] = date;
	return true;
}

static void calendar_remove(FluxCalendarData *d, int date) {
	size_t kept = 0;
	for (size_t i = 0; i < d->selected_count; ++i)
		if (d->selected [i] != date) d->selected [kept++] = d->selected [i];
	d->selected_count = kept;
}

static void calendar_select(FluxCalendarData *d, int date) {
	if (d->mode == XTK_CALENDAR_NONE) return;
	if (flux_calendar_is_selected(d, date)) {
		calendar_remove(d, date);
		calendar_notify(d, date, false);
		return;
	}
	if (d->mode == XTK_CALENDAR_SINGLE && d->selected_count == 1) {
		int old           = d->selected [0];
		d->selected_count = 0;
		calendar_notify(d, old, false);
	}
	if (calendar_append(d, date)) calendar_notify(d, date, true);
}

static void calendar_activate(FluxCalendarData *d, int index, bool keyboard) {
	int date = flux_calendar_date_at(d, d->view, index);
	if (d->view == FLUX_CAL_MONTH) {
		calendar_select(d, date);
		return;
	}
	d->displayed = calendar_copy_date(d, d->view, date, d->displayed);
	d->focus     = 0;
	calendar_set_view(d, d->view - 1, keyboard);
}

static void calendar_focus_index(FluxCalendarData *d, int index) {
	int size = flux_calendar_size(d, d->view);
	index    = index < 0 ? 0 : (index >= size ? size - 1 : index);
	calendar_scroll_settle(d, calendar_now());
	d->displayed    = flux_calendar_date_at(d, d->view, index);
	d->focus        = index;
	d->focus_visual = true;
	calendar_bring_into_view(d);
}

static bool calendar_full_scope(FluxCalendarData const *d, int view) {
	static int const longest [3] = {31, 12, 10};
	int              cols, rows;
	calendar_dims(d, view, &cols, &rows);
	return (rows - 1) * cols + 1 >= longest [view];
}

static int calendar_next_scope(FluxCalendarData const *d, float rowpos, int first_date, bool forward) {
	int scope [2];
	calendar_scope(d, d->view, rowpos, scope);
	int adjust = forward ? 1 : (scope [0] >= first_date ? -1 : 0);
	if (adjust == 0) return scope [0];
	return calendar_coerce(d, calendar_scope_first(d->view, calendar_add_scopes(d->view, scope [0], adjust)));
}

static void calendar_navigate(FluxCalendarData *d, bool forward) {
	double now    = calendar_now();
	float  rowpos = d->scroll.start >= 0.0 ? d->scroll.to : flux_calendar_rowpos(d, d->view, now);
	int    first, last, cols, rows;
	calendar_visible(d, d->view, rowpos, &first, &last);
	int target;
	if (calendar_full_scope(d, d->view)) {
		target = calendar_next_scope(d, rowpos, flux_calendar_date_at(d, d->view, first), forward);
	}
	else {
		calendar_dims(d, d->view, &cols, &rows);
		int index = first + (forward ? cols * rows : -cols * rows);
		int size  = flux_calendar_size(d, d->view);
		target    = flux_calendar_date_at(d, d->view, index < 0 ? 0 : (index >= size ? size - 1 : index));
	}
	calendar_scroll_to(d, ( float ) calendar_row_of(d, d->view, flux_calendar_index_of(d, d->view, target)), true);
}

static void calendar_invoke(FluxCalendarData *d, int part, bool keyboard) {
	if (part >= 0) {
		calendar_activate(d, part, keyboard);
		return;
	}
	if (part == FLUX_CAL_HEADER) calendar_set_view(d, d->view + 1, keyboard);
	if (part == FLUX_CAL_PREV || part == FLUX_CAL_NEXT) calendar_navigate(d, part == FLUX_CAL_NEXT);
}

static bool calendar_part_enabled(FluxCalendarData const *d, int part, double now) {
	bool before, after;
	flux_calendar_nav(d, now, &before, &after);
	if (part == FLUX_CAL_HEADER) return d->view != FLUX_CAL_DECADE;
	if (part == FLUX_CAL_PREV) return before;
	if (part == FLUX_CAL_NEXT) return after;
	return part >= 0;
}

static int calendar_hit_item(FluxCalendarData const *d, FluxCalendarGeom const *g, float x, float y, double now) {
	if (!flux_rect_contains(&g->viewport, x, y)) return FLUX_CAL_NONE;
	float rowpos = flux_calendar_rowpos(d, d->view, now);
	int   row    = ( int ) floorf((y - g->viewport.y) / g->row_h + rowpos);
	int   col    = ( int ) ((x - g->viewport.x) / g->col_w);
	int   index  = row * g->cols + col - flux_calendar_start(d, d->view);
	return index >= 0 && index < flux_calendar_size(d, d->view) ? index : FLUX_CAL_NONE;
}

static int calendar_hit(FluxCalendarData const *d, float x, float y) {
	XentRect rect = {0};
	xent_layout_rect(d->ctx, d->node, &rect);
	FluxRect         bounds = {0.0f, 0.0f, rect.w, rect.h};
	FluxCalendarGeom g      = flux_calendar_layout(d, d->view, &bounds);
	double           now    = calendar_now();
	int              part   = calendar_hit_item(d, &g, x, y, now);
	if (flux_rect_contains(&g.header, x, y)) part = FLUX_CAL_HEADER;
	if (flux_rect_contains(&g.prev, x, y)) part = FLUX_CAL_PREV;
	if (flux_rect_contains(&g.next, x, y)) part = FLUX_CAL_NEXT;
	return part != FLUX_CAL_NONE && calendar_part_enabled(d, part, now) ? part : FLUX_CAL_NONE;
}

static bool calendar_touch(FluxCalendarData const *d) {
	FluxNodeData const *nd = flux_store_find(d->store, d->node);
	return nd && nd->state.pointer_type == FLUX_POINTER_TOUCH;
}

static void calendar_pointer_move(void *ctx, float x, float y) {
	FluxCalendarData *d   = ctx;
	int               hit = calendar_hit(d, x, y);
	if (d->pressed != FLUX_CAL_NONE && d->pressed != hit) d->pressed = FLUX_CAL_NONE;
	d->hover = hit >= 0 && calendar_touch(d) ? FLUX_CAL_NONE : hit;
}

static void calendar_hover_changed(void *ctx, bool hovered) {
	FluxCalendarData *d = ctx;
	if (hovered) return;
	d->hover   = FLUX_CAL_NONE;
	d->pressed = FLUX_CAL_NONE;
}

static void calendar_press(void *ctx, float x, float y, int clicks) {
	( void ) clicks;
	FluxCalendarData *d = ctx;
	d->pressed          = calendar_hit(d, x, y);
	d->hover            = d->pressed >= 0 && calendar_touch(d) ? FLUX_CAL_NONE : d->pressed;
}

static int calendar_hit_pointer(FluxCalendarData const *d) {
	FluxNodeData const *nd = flux_store_find(d->store, d->node);
	return nd && nd->hover_local_x >= 0.0f ? calendar_hit(d, nd->hover_local_x, nd->hover_local_y) : FLUX_CAL_NONE;
}

static void calendar_click(void *ctx) {
	FluxCalendarData *d    = ctx;
	int               part = d->pressed;
	d->pressed             = FLUX_CAL_NONE;
	if (!xent_sem_enabled(d->ctx, d->node) || part == FLUX_CAL_NONE || part != calendar_hit_pointer(d)) return;
	if (part >= 0) {
		d->displayed    = flux_calendar_date_at(d, d->view, part);
		d->focus        = part;
		d->focus_visual = false;
	}
	calendar_invoke(d, part, false);
}

static void calendar_cancel(void *ctx) { (( FluxCalendarData * ) ctx)->pressed = FLUX_CAL_NONE; }

static bool calendar_wheel_step(void *ctx, unsigned long now_ms) {
	( void ) now_ms;
	FluxCalendarData *d = ctx;
	if (!d->wheel_free) return false;
	double now = calendar_now();
	if (now - d->wheel_last < CAL_WHEEL_IDLE_S) return true;
	int scope [2];
	calendar_scope(d, d->view, d->row [d->view], scope);
	d->wheel_free = false;
	calendar_scroll_to(d, ( float ) calendar_row_of(d, d->view, flux_calendar_index_of(d, d->view, scope [0])), true);
	return false;
}

static void calendar_wheel_pan(FluxCalendarData *d, float wheel_y) {
	double now = calendar_now();
	calendar_scroll_settle(d, now);
	float at = flux_calendar_rowpos(d, d->view, now);
	if (!d->wheel_free) {
		int scope [2];
		calendar_scope(d, d->view, at, scope);
		d->frozen = (FluxCalendarFrozen) {scope [0], scope [1], 0, -1};
		calendar_visible(d, d->view, at, &d->frozen.first, &d->frozen.last);
	}
	XentRect rect = {0};
	xent_layout_rect(d->ctx, d->node, &rect);
	FluxRect         bounds = {0.0f, 0.0f, rect.w, rect.h};
	FluxCalendarGeom g      = flux_calendar_layout(d, d->view, &bounds);
	float            max    = calendar_max_row(d, d->view);
	float            row = at - wheel_y * FLUX_SCROLLBAR_SMALL_CHANGE / (g.row_h > 0.0f ? g.row_h : FLUX_CAL_DAY_CELL);
	d->row [d->view]     = row < 0.0f ? 0.0f : (row > max ? max : row);
	d->scroll.start      = -1.0;
	d->wheel_free        = true;
	d->wheel_last        = now;
	flux_anim_register(d, calendar_wheel_step);
}

bool flux_calendar_handle_wheel(FluxStore *store, XentNodeId node, float wheel_y) {
	FluxCalendarData *d = calendar_data(store, node);
	if (!d || wheel_y == 0.0f) return false;
	if (d->wheel_free || fabsf(wheel_y - roundf(wheel_y)) > 0.001f) {
		calendar_wheel_pan(d, wheel_y);
		return true;
	}
	double now = calendar_now();
	bool   before, after;
	flux_calendar_nav(d, now, &before, &after);
	if ((wheel_y > 0.0f && before) || (wheel_y < 0.0f && after)) calendar_navigate(d, wheel_y < 0.0f);
	return true;
}

static int calendar_arrow_target(FluxCalendarData const *d, int index, FluxKey key) {
	int cols, rows;
	calendar_dims(d, d->view, &cols, &rows);
	int step = 0;
	if (key == FLUX_KEY_LEFT) step = -1;
	if (key == FLUX_KEY_RIGHT) step = 1;
	if (key == FLUX_KEY_UP) step = -cols;
	if (key == FLUX_KEY_DOWN) step = cols;
	int to = index + step;
	return step != 0 && to >= 0 && to < flux_calendar_size(d, d->view) ? to : index;
}

static int calendar_nav_target(FluxCalendarData *d, FluxKey key) {
	int index = flux_calendar_index_of(d, d->view, d->displayed);
	int scope [2];
	if (key == FLUX_KEY_HOME || key == FLUX_KEY_END) {
		calendar_scope(d, d->view, flux_calendar_rowpos(d, d->view, calendar_now()), scope);
		return flux_calendar_index_of(d, d->view, scope [key == FLUX_KEY_END]);
	}
	if (key == FLUX_KEY_PAGE_UP || key == FLUX_KEY_PAGE_DOWN) {
		int date = calendar_add_scopes(d->view, d->displayed, key == FLUX_KEY_PAGE_UP ? -1 : 1);
		return flux_calendar_index_of(d, d->view, calendar_coerce(d, date));
	}
	return calendar_arrow_target(d, index, key);
}

static bool calendar_is_nav_key(FluxKey key) {
	return key == FLUX_KEY_HOME
	    || key == FLUX_KEY_END
	    || key == FLUX_KEY_PAGE_UP
	    || key == FLUX_KEY_PAGE_DOWN
	    || key == FLUX_KEY_LEFT
	    || key == FLUX_KEY_RIGHT
	    || key == FLUX_KEY_UP
	    || key == FLUX_KEY_DOWN;
}

static bool calendar_key_mode(FluxCalendarData *d, FluxKey key) {
	if (key != FLUX_KEY_UP && key != FLUX_KEY_DOWN) return false;
	int view = d->view + (key == FLUX_KEY_UP ? 1 : -1);
	if (view < FLUX_CAL_MONTH || view > FLUX_CAL_DECADE) return false;
	calendar_set_view(d, view, true);
	return true;
}

static bool calendar_key_items(FluxCalendarData *d, FluxKey key) {
	if (flux_key_held(FLUX_KEY_CONTROL)) return calendar_key_mode(d, key);
	if (key == FLUX_KEY_SPACE || key == FLUX_KEY_ENTER) {
		d->focus_visual = true;
		calendar_activate(d, flux_calendar_index_of(d, d->view, d->displayed), true);
		return true;
	}
	if (!calendar_is_nav_key(key) || flux_key_held(FLUX_KEY_SHIFT) || flux_key_held(FLUX_KEY_ALT)) return false;
	int to = calendar_nav_target(d, key);
	if (to != flux_calendar_index_of(d, d->view, d->displayed)) calendar_focus_index(d, to);
	return true;
}

static bool calendar_key(void *ctx, FluxKey key, bool down) {
	FluxCalendarData *d = ctx;
	if (!down) return false;
	bool handled = false;
	if (d->focus >= 0) handled = calendar_key_items(d, key);
	else if (key == FLUX_KEY_SPACE || key == FLUX_KEY_ENTER) {
		calendar_invoke(d, d->focus, true);
		handled = true;
	}
	return handled;
}

static int calendar_tab_stop(FluxCalendarData const *d, int slot) {
	static int const parts [3] = {FLUX_CAL_HEADER, FLUX_CAL_PREV, FLUX_CAL_NEXT};
	if (slot < 3) return parts [slot];
	return flux_calendar_index_of(d, d->view, d->displayed);
}

static int calendar_tab_slot(int part) {
	if (part >= 0) return 3;
	return part == FLUX_CAL_HEADER ? 0 : (part == FLUX_CAL_PREV ? 1 : 2);
}

static bool calendar_on_tab(void *ctx, bool shift, bool entering) {
	FluxCalendarData *d    = ctx;
	double            now  = calendar_now();
	int               slot = entering ? (shift ? 4 : -1) : calendar_tab_slot(d->focus);
	for (slot += shift ? -1 : 1; slot >= 0 && slot <= 3; slot += shift ? -1 : 1) {
		int part = calendar_tab_stop(d, slot);
		if (!calendar_part_enabled(d, part, now)) continue;
		d->focus        = part;
		d->focus_visual = true;
		return true;
	}
	return false;
}

static void calendar_on_blur(void *ctx) {
	FluxCalendarData *d = ctx;
	d->focus_visual     = false;
}

static float calendar_line(XentCtx *ctx, float size, uint16_t weight) {
	XentTextMeasureReq request
	  = {.text = "Ag", .font_size = size, .font_weight = weight, .max_lines = 1u, .width_mode = XENT_MEASURE_UNDEFINED};
	XentTextMetrics metrics = {0};
	return xent_text_measure(ctx, &request, &metrics) ? ceilf(metrics.height) : ceilf(size * 1.33f);
}

static void calendar_grow_item(XentSize *item, XentCtx *ctx, char const *text) {
	XentTextMeasureReq request
	  = {.text = text, .font_size = 14.0f, .font_weight = 400u, .max_lines = 1u, .width_mode = XENT_MEASURE_UNDEFINED};
	XentTextMetrics metrics = {0};
	if (!xent_text_measure(ctx, &request, &metrics)) return;
	item->w = fmaxf(item->w, ceilf(metrics.width) + 2.0f * FLUX_CAL_YD_MARGIN);
	item->h = fmaxf(item->h, ceilf(metrics.height) + 2.0f * FLUX_CAL_YD_MARGIN);
}

static void calendar_measure_items(FluxCalendarData *d) {
	char text [32];
	d->item [FLUX_CAL_YEAR] = d->item [FLUX_CAL_DECADE] = (XentSize) {0};
	for (int m = 1; m <= 12; ++m) {
		flux_locale_formatdate(flux_date_pack(2000, m, 1), L"MMM", text, sizeof(text));
		calendar_grow_item(&d->item [FLUX_CAL_YEAR], d->ctx, text);
	}
	snprintf(text, sizeof(text), "%d", flux_date_year(flux_date_today()));
	calendar_grow_item(&d->item [FLUX_CAL_DECADE], d->ctx, text);
}

static void calendar_measure(FluxCalendarData *d) {
	calendar_measure_items(d);
	d->header_line  = calendar_line(d->ctx, 14.0f, 600u);
	d->caption_line = calendar_line(d->ctx, 12.0f, 600u);
	d->nav_line     = calendar_line(d->ctx, FLUX_CAL_NAV_GLYPH, 400u);
	for (int i = 0; i < 7; ++i) flux_locale_shortestday(i, d->weekday [i], sizeof(d->weekday [i]));
	XentSize size = flux_calendar_desired(d);
	xent_setsize(d->ctx, d->node, size);
}

static void calendar_destroy(void *ctx) {
	FluxCalendarData *d = ctx;
	flux_anim_unregister(d);
	free(d->selected);
	free(d);
}

static void calendar_bind(FluxNodeData *nd, FluxCalendarData *d) {
	nd->component_data                = d;
	nd->destroy_component_data        = calendar_destroy;
	nd->behavior.on_pointer_down      = calendar_press;
	nd->behavior.on_pointer_down_ctx  = d;
	nd->behavior.on_pointer_move      = calendar_pointer_move;
	nd->behavior.on_pointer_move_ctx  = d;
	nd->behavior.on_hover_changed     = calendar_hover_changed;
	nd->behavior.on_hover_changed_ctx = d;
	nd->behavior.on_click             = calendar_click;
	nd->behavior.on_click_ctx         = d;
	nd->behavior.on_cancel            = calendar_cancel;
	nd->behavior.on_cancel_ctx        = d;
	nd->behavior.on_key               = calendar_key;
	nd->behavior.on_key_ctx           = d;
	nd->behavior.on_tab               = calendar_on_tab;
	nd->behavior.on_tab_ctx           = d;
	nd->behavior.on_blur              = calendar_on_blur;
	nd->behavior.on_blur_ctx          = d;
}

static int calendar_default_min(void) { return flux_date_pack(flux_date_year(flux_date_today()) - 100, 1, 1); }

static int calendar_default_max(void) { return flux_date_pack(flux_date_year(flux_date_today()) + 100, 12, 31); }

XentNodeId flux_calendar_init(XentCtx *ctx, FluxStore *store) {
	XentNodeId node = flux_factory_create_node(ctx, store, XENT_NODE_INVALID, FLUX_CONTROL_CALENDAR_VIEW);
	if (node == XENT_NODE_INVALID) return node;
	FluxNodeData     *nd = flux_store_find(store, node);
	FluxCalendarData *d  = calloc(1, sizeof(*d));
	if (!d || !nd) {
		free(d);
		return flux_factory_fail_node(ctx, store, node);
	}
	*d = (FluxCalendarData) {
	  .ctx                  = ctx,
	  .store                = store,
	  .node                 = node,
	  .min_date             = calendar_default_min(),
	  .max_date             = calendar_default_max(),
	  .mode                 = XTK_CALENDAR_SINGLE,
	  .first_day            = flux_locale_firstday(),
	  .displayed            = flux_date_today(),
	  .out_of_scope_enabled = true,
	  .today_highlighted    = true,
	  .scroll               = {.start = -1.0},
	  .view_since           = -1.0,
	  .header_since         = -1.0,
	  .hover                = FLUX_CAL_NONE,
	  .pressed              = FLUX_CAL_NONE,
	  .focus                = FLUX_CAL_HEADER};
	calendar_bind(nd, d);
	xent_set_focusable(ctx, node, true);
	xent_setself(ctx, node, XENT_FLEX_ALIGN_START);
	xent_sem_setlabel(ctx, node, "calendar");
	xent_sem_setrole(ctx, node, XENT_SEM_ROLE_CONTAINER);
	calendar_measure(d);
	calendar_bring_into_view(d);
	return node;
}

void flux_calendar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node) {
	FluxCalendarData *d = calendar_data(store, node);
	if (!d) return;
	d->ctx   = ctx;
	d->store = store;
	d->node  = node;
}

static void calendar_range_changed(FluxCalendarData *d) {
	size_t kept = 0;
	for (size_t i = 0; i < d->selected_count; ++i)
		if (d->selected [i] >= d->min_date && d->selected [i] <= d->max_date) d->selected [kept++] = d->selected [i];
	d->selected_count = kept;
	d->displayed      = calendar_coerce(d, d->displayed);
	calendar_bring_into_view(d);
}

static void calendar_set_mode(FluxCalendarData *d, int mode) {
	d->mode = mode;
	if (mode == XTK_CALENDAR_NONE) d->selected_count = 0;
	if (mode == XTK_CALENDAR_SINGLE && d->selected_count > 1) d->selected_count = 1;
}

static void calendar_set_date(FluxCalendarData *d, int date) {
	d->selected_count = 0;
	if (d->mode == XTK_CALENDAR_NONE || !flux_date_valid(date) || date < d->min_date || date > d->max_date) return;
	if (!calendar_append(d, date)) return;
	d->displayed = date;
	calendar_bring_into_view(d);
}

static void calendar_set_range(FluxCalendarData *d, XtkPropertyId property, int value) {
	if (property == XTK_PROP_MIN_DATE) d->min_date = value ? value : calendar_default_min();
	if (property == XTK_PROP_MAX_DATE) d->max_date = value ? value : calendar_default_max();
	if (property == XTK_PROP_FIRST_DAY) d->first_day = value ? (value - 1) % 7 : flux_locale_firstday();
	calendar_range_changed(d);
}

bool flux_calendar_set(FluxStore *store, XentNodeId node, XtkPropertyId property, XtkValue const *value) {
	FluxCalendarData *d = calendar_data(store, node);
	if (!d) return false;
	switch (property) {
	case XTK_PROP_DATE           : calendar_set_date(d, value->u.i32); return true;
	case XTK_PROP_MIN_DATE       :
	case XTK_PROP_MAX_DATE       :
	case XTK_PROP_FIRST_DAY      : calendar_set_range(d, property, value->u.i32); return true;
	case XTK_PROP_SELECTION_MODE : calendar_set_mode(d, ( int ) value->u.u32); return true;
	default                      : return false;
	}
}

void flux_calendar_snapshot(FluxCalendarSnapshot *out, FluxCalendarData const *data) { out->data = data; }
