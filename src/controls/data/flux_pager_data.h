/**
 * @file flux_pager_data.h
 * @brief Data for FluxPager — a numeric/arrow pager (WinUI 3 PagerControl).
 *
 * A single leaf hosting up to four navigation buttons (First / Previous / Next
 * / Last) around a page display. The default NumberPanel display lays out
 * numbered page buttons with ellipses following PagerControl's windowing rules;
 * a NumberText display shows a "prefix n suffix N" label instead. Layout is
 * shared between the renderer and hit-testing via flux_pager_layout so clicks
 * resolve against exactly the cells that are drawn.
 */
#ifndef FLUX_PAGER_DATA_H
#define FLUX_PAGER_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>


typedef struct FluxStore FluxStore;

/** @brief Display mode; values match XtkPagerDisplayMode. */
typedef enum FluxPagerDisplayMode
{
	FLUX_PAGER_DISPLAY_AUTO = 0,
	FLUX_PAGER_DISPLAY_NUMBER_PANEL,
	FLUX_PAGER_DISPLAY_NUMBER_TEXT,
} FluxPagerDisplayMode;

/** @brief Per-button visibility; values match XtkPagerButtonVis. */
typedef enum FluxPagerButtonVis
{
	FLUX_PAGER_BUTTON_AUTO = 0,
	FLUX_PAGER_BUTTON_ALWAYS,
	FLUX_PAGER_BUTTON_HIDDEN_EDGE,
	FLUX_PAGER_BUTTON_NONE,
} FluxPagerButtonVis;

/** @brief Element kinds emitted by flux_pager_layout. */
enum
{
	FLUX_PAGER_EL_FIRST = 0,
	FLUX_PAGER_EL_PREV,
	FLUX_PAGER_EL_CELL, /**< A number cell or ellipsis (see .page). */
	FLUX_PAGER_EL_TEXT, /**< NumberText label (non-interactive). */
	FLUX_PAGER_EL_NEXT,
	FLUX_PAGER_EL_LAST,
};

/** @brief Nav-button render/hit state. */
enum
{
	FLUX_PAGER_NAV_HIDDEN = 0, /**< Collapsed / edge-hidden. */
	FLUX_PAGER_NAV_ENABLED,
	FLUX_PAGER_NAV_DISABLED,
};

#define FLUX_PAGER_BTN           36.0f  /**< Nav button + number cell square. */
#define FLUX_PAGER_SPACING       4.0f   /**< Gap between number cells (StackLayout Spacing). */
#define FLUX_PAGER_TEXT_W        120.0f /**< NumberText label width. */
#define FLUX_PAGER_HEIGHT        36.0f
#define FLUX_PAGER_ELLIPSIS_PAGE (-1)   /**< cells[] sentinel: an ellipsis. */
#define FLUX_PAGER_MAX_CELLS     12

/** @brief One laid-out element (nav button, number cell, or label). */
typedef struct FluxPagerElem {
	uint8_t kind; /**< FLUX_PAGER_EL_*. */
	int16_t page; /**< CELL: 1-based page or FLUX_PAGER_ELLIPSIS_PAGE; else 0. */
	float   x, w;
} FluxPagerElem;

/** @brief Retained state for a FLUX_CONTROL_PAGER node. */
typedef struct FluxPagerData {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId node;

	int        count;                                   /**< NumberOfPages (>= 1). */
	int        selected;                                /**< SelectedPageIndex (0-based). */
	uint8_t    display_mode;                            /**< FluxPagerDisplayMode. */
	uint8_t    first_vis, prev_vis, next_vis, last_vis; /**< FluxPagerButtonVis. */
	char      *prefix;                                  /**< Owned NumberText prefix, or NULL. */
	char      *suffix;                                  /**< Owned NumberText suffix, or NULL. */

	int        pressed_elem;                            /**< Element index under an active press, or -1. */

	void       (*on_select)(void *ctx, int index);
	void      *on_select_ctx;
} FluxPagerData;

/** @brief Nav-button state from a visibility mode and whether it sits at its edge. */
static int inline flux_pager_nav_state(int vis, bool at_edge) {
	switch (vis) {
	case FLUX_PAGER_BUTTON_NONE        : return FLUX_PAGER_NAV_HIDDEN;
	case FLUX_PAGER_BUTTON_HIDDEN_EDGE : return at_edge ? FLUX_PAGER_NAV_HIDDEN : FLUX_PAGER_NAV_ENABLED;
	case FLUX_PAGER_BUTTON_ALWAYS      : return FLUX_PAGER_NAV_ENABLED;
	default                            : return at_edge ? FLUX_PAGER_NAV_DISABLED : FLUX_PAGER_NAV_ENABLED; /* Auto */
	}
}

static int inline flux_pager_cells_all(int count, int16_t *cells) {
	for (int i = 0; i < count; i++) cells [i] = ( int16_t ) (i + 1);
	return count;
}

static int inline flux_pager_cells_start(int count, int16_t *cells) {
	int16_t const start [] = {1, 2, 3, 4, 5, FLUX_PAGER_ELLIPSIS_PAGE};
	for (int i = 0; i < 6; i++) cells [i] = start [i];
	cells [6] = ( int16_t ) count;
	return 7;
}

static int inline flux_pager_cells_end(int count, int16_t *cells) {
	cells [0] = 1;
	cells [1] = FLUX_PAGER_ELLIPSIS_PAGE;
	for (int i = 0; i < 5; i++) cells [i + 2] = ( int16_t ) (count - 4 + i);
	return 7;
}

static int inline flux_pager_cells_middle(int count, int selected, int16_t *cells) {
	cells [0] = 1;
	cells [1] = FLUX_PAGER_ELLIPSIS_PAGE;
	cells [2] = ( int16_t ) selected;
	cells [3] = ( int16_t ) (selected + 1);
	cells [4] = ( int16_t ) (selected + 2);
	cells [5] = FLUX_PAGER_ELLIPSIS_PAGE;
	cells [6] = ( int16_t ) count;
	return 7;
}

/**
 * @brief Build the NumberPanel cell list (PagerControl windowing).
 *
 * Writes 1-based page numbers and FLUX_PAGER_ELLIPSIS_PAGE sentinels into
 * @p cells, returning the count. Mirrors UpdateNumberPanel with
 * ButtonPanelAlwaysShowFirstLastPageIndex enabled.
 */
static int inline flux_pager_build_cells(int count, int sel0, int16_t *cells) {
	if (count < 8) return flux_pager_cells_all(count, cells);
	if (sel0 < 4) return flux_pager_cells_start(count, cells);
	if (sel0 >= count - 4) return flux_pager_cells_end(count, cells);
	return flux_pager_cells_middle(count, sel0, cells);
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

static void inline flux_pager_append_cells(
  FluxPagerElem *elems, int *count, float *x, int16_t const *cells, int cell_count
) {
	for (int i = 0; i < cell_count; i++) {
		if (i > 0) *x += FLUX_PAGER_SPACING;
		flux_pager_append_elem(elems, count, x, FLUX_PAGER_EL_CELL, cells [i], FLUX_PAGER_BTN);
	}
}

/**
 * @brief Lay out the pager left-to-right into @p elems (nav buttons, then the
 * display, then nav buttons). Returns the element count; @p total_w receives
 * the overall width. @p cells / @p cell_count come from flux_pager_build_cells.
 */
static int inline flux_pager_layout(
  int display_mode, int first_st, int prev_st, int next_st, int last_st, int16_t const *cells, int cell_count,
  FluxPagerElem *elems, float *total_w
) {
	int   n = 0;
	float x = 0.0f;
	flux_pager_append_nav(elems, &n, &x, first_st, FLUX_PAGER_EL_FIRST);
	flux_pager_append_nav(elems, &n, &x, prev_st, FLUX_PAGER_EL_PREV);
	if (display_mode == FLUX_PAGER_DISPLAY_NUMBER_TEXT)
		flux_pager_append_elem(elems, &n, &x, FLUX_PAGER_EL_TEXT, 0, FLUX_PAGER_TEXT_W);
	else flux_pager_append_cells(elems, &n, &x, cells, cell_count);
	flux_pager_append_nav(elems, &n, &x, next_st, FLUX_PAGER_EL_NEXT);
	flux_pager_append_nav(elems, &n, &x, last_st, FLUX_PAGER_EL_LAST);
	if (total_w) *total_w = x;
	return n;
}


#endif
