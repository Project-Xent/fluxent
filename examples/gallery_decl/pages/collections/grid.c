#include "gallery.h"
#include <string.h>

static XtkEl *grid_cell(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_column(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_CENTER, .justify = XENT_FLEX_JUSTIFY_CENTER},
	  xtk_kids(
		xtk_text(ui, xtk_fmt(ui, "%d", index), (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, xtk_fmt(ui, "Item %d", index), (XtkTextDesc) {.size = 11})
	  )
	);
}

static int32_t const kGridGroups [] = {7, 12, 3, 18, 5, 9, 14, 2, 11, 6};

static XtkEl        *grid_header(XtkUi *ui, void *env, int group) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "Group %d", group + 1), (XtkTextDesc) {0});
}

XtkEl *page_gridview(XtkUi *ui, Model const *m) {
	char const *status = m->grid_sel >= 0 ? xtk_fmt(ui, "Selected cell: %d", m->grid_sel) : "Nothing selected yet.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "GridView",
		  "The same virtualized items host wrapping cells across auto-fit columns. "
		  "Arrow keys navigate in 2D; the "
		  "selection ring is the WinUI "
		  "accent border."
		),
		example(
		  ui, "10,000 uniform cells; columns follow the width.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_sized(
				xtk_gridview(
				  ui,
				  (XtkListDesc) {
					.nitem  = 10000,
					.itemh  = 104.0f,
					.itemw  = 152.0f,
					.item   = grid_cell,
					.sel    = m->grid_sel,
					.select = xtk_msg(MSG_GRID_SELECT),
				  }
				),
				NAN, 420.0f
			  ),
			  xtk_text(ui, status, (XtkTextDesc) {.size = 13})
			)
		  )
		),
		example(
		  ui, "Grouped: each group starts a new row under its header; headers stick while the group is in view.",
		  xtk_sized(
			xtk_gridview(
			  ui,
			  (XtkListDesc) {
				.itemh        = 104.0f,
				.itemw        = 152.0f,
				.item         = grid_cell,
				.sel          = -1,
				.groups       = kGridGroups,
				.ngroup       = 10,
				.group_header = grid_header,
			  }
			),
			NAN, 420.0f
		  )
		)
	  )
	);
}
