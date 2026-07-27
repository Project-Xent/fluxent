#include "gallery.h"

static XtkSelectorItem const kSelectors [] = {
  {.text = "All",    .icon = "List"     },
  {.text = "Music",  .icon = "MusicNote"},
  {.text = "Video",  .icon = "Video"    },
  {.text = "Photos", .icon = "Pictures" },
};

XtkEl *page_selector_bar(XtkUi *ui, Model const *m) {
	char const *status = m->selector_sel >= 0 && m->selector_sel < 4
	                     ? xtk_fmt(ui, "Filter: %s", kSelectors [m->selector_sel].text)
	                     : "No filter selected.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "SelectorBar", "A horizontal strip of pill toggles — one selected item at a time."),
		example(
		  ui, "Icon+text selector items.",
		  demo_card(
			ui,
			xtk_selector(
			  ui,
			  (XtkSelectorBarDesc) {
				.items  = kSelectors,
				.nitem  = 4,
				.sel    = m->selector_sel,
				.select = xtk_msg(MSG_SELECTOR_BAR),
			  }
			),
			status, NULL
		  )
		)
	  )
	);
}
