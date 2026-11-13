#ifndef FLUX_CALENDAR_GEOM_H
#define FLUX_CALENDAR_GEOM_H

#include "controls/behavior/flux_calendar.h"
#include "render/flux_scroll_geom.h"

#include <math.h>

#define FLUX_CAL_BORDER     1.0f
#define FLUX_CAL_DAY_CELL   42.0f
#define FLUX_CAL_WEEKS      6
#define FLUX_CAL_YD_DIM     4
#define FLUX_CAL_DAY_MARGIN 1.0f
#define FLUX_CAL_YD_MARGIN  9.0f
#define FLUX_CAL_NAV_GLYPH  8.0f

typedef struct FluxCalendarGeom {
	FluxRect header, prev, next;
	float    rule_y;
	FluxRect views;
	FluxRect weekdays;
	FluxRect viewport;
	int      cols, rows;
	float    col_w, row_h;
} FluxCalendarGeom;

static float inline flux_calendar_row_h(FluxCalendarData const *d) {
	float header = 6.0f + 7.0f + 7.0f + 8.0f + d->header_line;
	float nav    = 6.0f + 7.0f + 23.0f + d->nav_line;
	return header > nav ? header : nav;
}

static float inline flux_calendar_weekday_h(FluxCalendarData const *d) {
	return 1.0f + 12.0f + d->caption_line + 12.0f + 1.0f;
}

static int inline flux_calendar_fit(float extent, float item) {
	if (extent <= 0.0f || item <= 0.0f) return FLUX_CAL_YD_DIM;
	int n = ( int ) (extent / item);
	return n < 1 ? 1 : (n > FLUX_CAL_YD_DIM ? FLUX_CAL_YD_DIM : n);
}

static void inline flux_calendar_dims(
  FluxCalendarData const *d, int view, FluxRect const *viewport, int *cols, int *rows
) {
	if (view == FLUX_CAL_MONTH) {
		*cols = 7;
		*rows = FLUX_CAL_WEEKS;
		return;
	}
	*cols = flux_calendar_fit(viewport->w, d->item [view].w);
	*rows = flux_calendar_fit(viewport->h, d->item [view].h);
}

static FluxCalendarGeom inline flux_calendar_layout(FluxCalendarData const *d, int view, FluxRect const *bounds) {
	FluxCalendarGeom g     = {0};
	FluxRect         inner = {
	  bounds->x + FLUX_CAL_BORDER, bounds->y + FLUX_CAL_BORDER, bounds->w - 2.0f * FLUX_CAL_BORDER,
	  bounds->h - 2.0f * FLUX_CAL_BORDER};
	float row   = flux_calendar_row_h(d);
	float nav_w = 12.0f + 12.0f + FLUX_CAL_NAV_GLYPH;
	float right = inner.x + inner.w;
	g.next      = ( FluxRect ) {right - 7.0f - nav_w, inner.y + 6.0f, nav_w, row - 13.0f};
	g.prev      = ( FluxRect ) {g.next.x - 3.0f - 3.0f - nav_w, inner.y + 6.0f, nav_w, row - 13.0f};
	g.header    = ( FluxRect ) {inner.x + 7.0f, inner.y + 6.0f, g.prev.x - 3.0f - 3.0f - inner.x - 7.0f, row - 13.0f};
	g.rule_y    = inner.y + row;
	g.views     = ( FluxRect ) {inner.x, g.rule_y + 1.0f, inner.w, inner.y + inner.h - g.rule_y - 1.0f};
	float top   = g.views.y + 2.0f;
	g.weekdays  = ( FluxRect ) {g.views.x + 2.0f, top, g.views.w - 4.0f, flux_calendar_weekday_h(d)};
	if (view == FLUX_CAL_MONTH) top = g.weekdays.y + g.weekdays.h + 2.0f;
	g.viewport = ( FluxRect ) {g.views.x + 2.0f, top, g.views.w - 4.0f, g.views.y + g.views.h - 2.0f - top};
	flux_calendar_dims(d, view, &g.viewport, &g.cols, &g.rows);
	g.col_w = g.viewport.w / ( float ) g.cols;
	g.row_h = g.viewport.h / ( float ) g.rows;
	return g;
}

static XentSize inline flux_calendar_desired(FluxCalendarData const *d) {
	float w = 2.0f * FLUX_CAL_BORDER + 4.0f + 7.0f * FLUX_CAL_DAY_CELL;
	float h = 2.0f * FLUX_CAL_BORDER
	        + flux_calendar_row_h(d)
	        + 1.0f
	        + 2.0f
	        + flux_calendar_weekday_h(d)
	        + 2.0f
	        + FLUX_CAL_WEEKS * FLUX_CAL_DAY_CELL
	        + 2.0f;
	return ( XentSize ) {w, h};
}

static FluxRect inline flux_calendar_cell(FluxCalendarGeom const *g, int slot, float rowpos) {
	int row = slot / g->cols, col = slot % g->cols;
	return ( FluxRect ) {
	  g->viewport.x + ( float ) col * g->col_w, g->viewport.y + (( float ) row - rowpos) * g->row_h, g->col_w,
	  g->row_h};
}

static FluxRect inline flux_calendar_item(FluxRect const *cell, int view) {
	float m = view == FLUX_CAL_MONTH ? FLUX_CAL_DAY_MARGIN : FLUX_CAL_YD_MARGIN;
	return ( FluxRect ) {cell->x + m, cell->y + m, cell->w - 2.0f * m, cell->h - 2.0f * m};
}

static float inline flux_calendar_radius(FluxRect const *item) { return fminf(item->w, item->h) * 0.5f; }

#endif
