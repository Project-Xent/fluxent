#include "gallery.h"

static char const *const kCrumbs [] = {"Home", "Documents", "Projects", "FluXent"};

XtkEl                   *page_breadcrumb(XtkUi *ui, Model const *m) {
	char const *status = m->breadcrumb_click >= 0 && m->breadcrumb_click < 4
	                     ? xtk_fmt(ui, "Clicked: %s", kCrumbs [m->breadcrumb_click])
	                     : "Click a crumb to navigate.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "BreadcrumbBar", "Shows the path to the current location. Every crumb posts ItemClicked."),
		example(
		  ui, "A four-segment breadcrumb.",
		  demo_card(
			ui,
			xtk_breadcrumb(
			  ui,
			  (XtkBreadcrumbDesc) {
				.items = kCrumbs,
				.nitem = 4,
				.click = xtk_msg(MSG_BREADCRUMB),
			  }
			),
			status, NULL
		  )
		)
	  )
	);
}
