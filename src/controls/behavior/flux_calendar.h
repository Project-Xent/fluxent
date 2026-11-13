#ifndef FLUX_CALENDAR_H
#define FLUX_CALENDAR_H

#include <fluxent/flux_node_store.h>
#include <fluxent/flux_types.h>
#include <xent/xent_display.h>
#include <xtk/xtk.h>
#include <xtk/xtk_batch.h>

typedef struct FluxRenderCtx    FluxRenderCtx;
typedef struct FluxRenderSnap   FluxRenderSnap;
typedef struct FluxControlState FluxControlState;

enum
{
	FLUX_CAL_MONTH,
	FLUX_CAL_YEAR,
	FLUX_CAL_DECADE,
};

enum
{
	FLUX_CAL_NONE   = -1,
	FLUX_CAL_HEADER = -2,
	FLUX_CAL_PREV   = -3,
	FLUX_CAL_NEXT   = -4,
};

typedef struct FluxCalendarScroll {
	float  from, to;
	double start;
} FluxCalendarScroll;

typedef struct FluxCalendarFrozen {
	int scope_min, scope_max;
	int first, last;
} FluxCalendarFrozen;

typedef struct FluxCalendarData {
	XentCtx           *ctx;
	FluxStore         *store;
	XentNodeId         node;
	int                min_date, max_date;
	int                mode;
	int                view;
	int                first_day;
	int                displayed;
	bool               out_of_scope_enabled;
	bool               today_highlighted;
	bool               group_label;
	float              row [3];
	FluxCalendarScroll scroll;
	FluxCalendarFrozen frozen;
	bool               wheel_free;
	double             wheel_last;
	int                old_view;
	double             view_since;
	double             header_since;
	int                hover, pressed;
	int                focus;
	bool               focus_visual;
	float              header_line, caption_line, nav_line;
	XentSize           item [3];
	char               weekday [7][16];
	int               *selected;
	size_t             selected_count, selected_capacity;
} FluxCalendarData;

typedef struct FluxCalendarSnapshot {
	FluxCalendarData const *data;
} FluxCalendarSnapshot;

XentNodeId flux_calendar_init(XentCtx *ctx, FluxStore *store);
void       flux_calendar_retarget(XentCtx *ctx, FluxStore *store, XentNodeId node);
bool       flux_calendar_set(FluxStore *store, XentNodeId node, XtkPropertyId property, XtkValue const *value);
void       flux_calendar_snapshot(FluxCalendarSnapshot *out, FluxCalendarData const *data);
bool       flux_calendar_handle_wheel(FluxStore *store, XentNodeId node, float wheel_y);

int        flux_calendar_size(FluxCalendarData const *d, int view);
int        flux_calendar_start(FluxCalendarData const *d, int view);
int        flux_calendar_date_at(FluxCalendarData const *d, int view, int index);
int        flux_calendar_index_of(FluxCalendarData const *d, int view, int date);
float      flux_calendar_rowpos(FluxCalendarData const *d, int view, double now);
bool       flux_calendar_is_selected(FluxCalendarData const *d, int date);
bool       flux_calendar_is_today(int view, int date);
bool       flux_calendar_out_of_scope(FluxCalendarData const *d, int view, int index, double now);
void       flux_calendar_header(FluxCalendarData const *d, double now, char *out, size_t size);
void       flux_calendar_nav(FluxCalendarData const *d, double now, bool *before, bool *after);

XentDl    *flux_calendar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
);

#endif
