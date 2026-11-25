#ifndef FLUX_PAGER_DATA_H
#define FLUX_PAGER_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxStore FluxStore;
typedef struct FluxText  FluxText;

typedef enum FluxPagerDisplayMode
{
	FLUX_PAGER_DISPLAY_AUTO = 0,
	FLUX_PAGER_DISPLAY_COMBO_BOX,
	FLUX_PAGER_DISPLAY_NUMBER_BOX,
	FLUX_PAGER_DISPLAY_BUTTON_PANEL,
} FluxPagerDisplayMode;

typedef enum FluxPagerButtonVis
{
	FLUX_PAGER_BUTTON_VISIBLE = 0,
	FLUX_PAGER_BUTTON_HIDDEN_ON_EDGE,
	FLUX_PAGER_BUTTON_HIDDEN,
} FluxPagerButtonVis;

enum
{
	FLUX_PAGER_EL_FIRST = 0,
	FLUX_PAGER_EL_PREV,
	FLUX_PAGER_EL_CELL,
	FLUX_PAGER_EL_BOX,
	FLUX_PAGER_EL_NEXT,
	FLUX_PAGER_EL_LAST,
};

enum
{
	FLUX_PAGER_NAV_HIDDEN = 0,
	FLUX_PAGER_NAV_ENABLED,
	FLUX_PAGER_NAV_DISABLED,
	FLUX_PAGER_NAV_INVISIBLE,
};

#define FLUX_PAGER_BTN              36.0f
#define FLUX_PAGER_SPACING          4.0f
#define FLUX_PAGER_HEIGHT           36.0f
#define FLUX_PAGER_ELLIPSIS_PAGE    (-1)
#define FLUX_PAGER_MAX_CELLS        12
#define FLUX_PAGER_AUTO_PAGES       10
#define FLUX_PAGER_INFINITE_ENTRIES 100

typedef struct FluxPagerElem {
	uint8_t kind;
	int32_t page;
	float   x, w;
} FluxPagerElem;

typedef struct FluxPagerData {
	XentCtx    *ctx;
	FluxStore  *store;
	FluxText   *text;
	XentNodeId  node;

	int         count;
	int         selected;
	uint8_t     display_mode;
	uint8_t     first_vis, prev_vis, next_vis, last_vis;
	char const *prefix;
	char const *suffix;
	bool        hide_ends;
	float       box_w;

	int         pressed_elem;
	XentNodeId  nav [4];
	XentNodeId  group;
	XentNodeId  box;
	XentNodeId  prefix_label;
	XentNodeId  number;
	XentNodeId  combo;
	XentNodeId  suffix_label;
	XentNodeId  total_label;
	XentNodeId  cells [FLUX_PAGER_MAX_CELLS];

	bool        (*box_key [2])(void *ctx, FluxKey key, bool down);
	void       *box_key_ctx [2];
	void        (*on_select)(void *ctx, int index);
	void       *on_select_ctx;
} FluxPagerData;

static bool inline flux_pager_nav_enabled(int slot, int sel, int count) {
	if (count < 0) return slot < 2 ? sel > 0 : slot == 2 && sel < INT32_MAX - 1;
	if (count <= 1) return false;
	if (sel == 0) return slot >= 2;
	if (sel >= count - 1) return slot < 2;
	return true;
}

static int inline flux_pager_nav_state(int slot, int vis, int sel, int count) {
	if (vis == FLUX_PAGER_BUTTON_HIDDEN || (slot == 3 && count < 0)) return FLUX_PAGER_NAV_HIDDEN;
	bool edge = slot < 2 ? sel == 0 : sel == count - 1;
	if (vis == FLUX_PAGER_BUTTON_HIDDEN_ON_EDGE && edge) return FLUX_PAGER_NAV_INVISIBLE;
	return flux_pager_nav_enabled(slot, sel, count) ? FLUX_PAGER_NAV_ENABLED : FLUX_PAGER_NAV_DISABLED;
}

static int inline flux_pager_cells_all(int count, int32_t *cells) {
	for (int i = 0; i < count; i++) cells [i] = i + 1;
	return count;
}

static int inline flux_pager_cells_start(int count, bool ends, int32_t *cells) {
	for (int i = 0; i < 5; i++) cells [i] = i + 1;
	if (!ends) return 5;
	cells [5] = FLUX_PAGER_ELLIPSIS_PAGE;
	cells [6] = count;
	return 7;
}

static int inline flux_pager_cells_end(int count, bool ends, int32_t *cells) {
	int n = 0;
	if (ends) cells [n++] = 1;
	if (ends) cells [n++] = FLUX_PAGER_ELLIPSIS_PAGE;
	for (int i = 0; i < 5; i++) cells [n++] = count - 4 + i;
	return n;
}

static int inline flux_pager_cells_middle(int count, int selected, bool ends, int32_t *cells) {
	int n = 0;
	if (ends) cells [n++] = 1;
	if (ends) cells [n++] = FLUX_PAGER_ELLIPSIS_PAGE;
	for (int i = 0; i < 3; i++) cells [n++] = selected + i;
	if (ends) cells [n++] = FLUX_PAGER_ELLIPSIS_PAGE;
	if (ends) cells [n++] = count;
	return n;
}

static int inline flux_pager_cells_infinite(int sel0, int32_t *cells) {
	int           start    = sel0 > INT32_MAX - 2 ? INT32_MAX - 2 : sel0;
	int32_t const head [5] = {1, 2, 3, 4, 5};
	int32_t const tail [5] = {1, FLUX_PAGER_ELLIPSIS_PAGE, start, start + 1, start + 2};
	for (int i = 0; i < 5; i++) cells [i] = sel0 < 3 ? head [i] : tail [i];
	return 5;
}

static int inline flux_pager_build_cells(int count, int sel0, bool ends, int32_t *cells) {
	if (count < 0) return flux_pager_cells_infinite(sel0, cells);
	if (count < 8) return flux_pager_cells_all(count, cells);
	if (sel0 < 4) return flux_pager_cells_start(count, ends, cells);
	if (sel0 >= count - 4) return flux_pager_cells_end(count, ends, cells);
	return flux_pager_cells_middle(count, sel0, ends, cells);
}

static void inline flux_pager_append_elem(
  FluxPagerElem *elems, int *count, float *x, uint8_t kind, int page, float width
) {
	elems [(*count)++]  = ( FluxPagerElem ) {kind, page, *x, width};
	*x                 += width;
}

static void inline flux_pager_append_nav(FluxPagerElem *elems, int *count, float *x, int state, uint8_t kind) {
	if (state != FLUX_PAGER_NAV_HIDDEN) flux_pager_append_elem(elems, count, x, kind, 0, FLUX_PAGER_BTN);
}

float flux_pager_cell_width(FluxText *text, int32_t page);

static void inline flux_pager_append_cells(
  FluxPagerElem *elems, int *count, float *x, int32_t const *cells, int cell_count, FluxText *text
) {
	for (int i = 0; i < cell_count; i++) {
		if (i > 0) *x += FLUX_PAGER_SPACING;
		flux_pager_append_elem(elems, count, x, FLUX_PAGER_EL_CELL, cells [i], flux_pager_cell_width(text, cells [i]));
	}
}

static bool inline flux_pager_boxed(int display_mode) { return display_mode != FLUX_PAGER_DISPLAY_BUTTON_PANEL; }

static bool inline flux_pager_combo(int display_mode, int count) {
	if (display_mode == FLUX_PAGER_DISPLAY_AUTO) return count < FLUX_PAGER_AUTO_PAGES;
	return display_mode == FLUX_PAGER_DISPLAY_COMBO_BOX;
}

static int inline flux_pager_layout(
  int display_mode, int first_st, int prev_st, int next_st, int last_st, int32_t const *cells, int cell_count,
  float box_w, FluxPagerElem *elems, float *total_w, FluxText *text
) {
	int   n = 0;
	float x = 0.0f;
	flux_pager_append_nav(elems, &n, &x, first_st, FLUX_PAGER_EL_FIRST);
	flux_pager_append_nav(elems, &n, &x, prev_st, FLUX_PAGER_EL_PREV);
	if (flux_pager_boxed(display_mode)) flux_pager_append_elem(elems, &n, &x, FLUX_PAGER_EL_BOX, 0, box_w);
	else flux_pager_append_cells(elems, &n, &x, cells, cell_count, text);
	flux_pager_append_nav(elems, &n, &x, next_st, FLUX_PAGER_EL_NEXT);
	flux_pager_append_nav(elems, &n, &x, last_st, FLUX_PAGER_EL_LAST);
	if (total_w) *total_w = x;
	return n;
}

#endif
