#include "gallery.h"

static XtkEl *refresh_row(XtkUi *ui, void *env, int index) {
	( void ) env;
	return xtk_text(ui, xtk_fmt(ui, "Row %d", index), (XtkTextDesc) {0});
}

XtkEl *page_pull_refresh(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "PullToRefresh", "A RefreshContainer wrapping scrollable content. Pull from the top edge to refresh."
		),
		example(
		  ui, "List inside a top-edge pull container.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
			xtk_kids(
			  xtk_sized(
				xtk_refresh(
				  ui,
				  (XtkRefreshDesc) {
					.is_refreshing = m->refresh_busy,
					.on_refresh    = xtk_msg(MSG_REFRESH),
				  },
				  xtk_kids(xtk_grow(
					xtk_list(
					  ui,
					  (XtkListDesc) {
						.nitem = 40,
						.itemh = 40.0f,
						.item  = refresh_row,
					  }
					),
					1.0f
				  ))
				),
				NAN, 360.0f
			  ),
			  xtk_text(
				ui, xtk_fmt(ui, "Refresh count: %d. %s", m->refresh_count, m->refresh_busy ? "Refreshing…" : "Idle"),
				(XtkTextDesc) {.size = 13}
			  ),
			  xtk_button(
				ui, "End refresh", (XtkButtonDesc) {.click = xtk_msg(MSG_REFRESH_DONE), .disabled = !m->refresh_busy}
			  )
			)
		  )
		)
	  )
	);
}
