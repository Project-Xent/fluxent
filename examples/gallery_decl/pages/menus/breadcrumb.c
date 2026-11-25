#include "gallery.h"

static char const *const kCrumbs [] = {"Home", "Documents", "Projects", "FluXent"};

static XtkEl            *breadcrumb_overflow_example(XtkUi *ui) {
	char const *folders [150];
	int         count = ( int ) (sizeof(folders) / sizeof(folders [0]));
	for (int i = 0; i < count; i++) folders [i] = xtk_fmt(ui, "Folder %d", i + 1);
	XtkEl *bar
	  = xtk_breadcrumb(ui, (XtkBreadcrumbDesc) {.items = folders, .nitem = count, .click = xtk_msg(MSG_BREADCRUMB)});
	bar = xtk_sized(bar, 300.0f, NAN);
	return example(
	  ui, "A path with 150 segments. Open the ellipsis to scroll through the hidden folders.",
	  demo_card(ui, bar, NULL, NULL)
	);
}

XtkEl *page_breadcrumb(XtkUi *ui, Model const *m) {
	char const *status = m->breadcrumb_click >= 0 ? xtk_fmt(ui, "Last clicked segment: %d", m->breadcrumb_click + 1)
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
			NULL, NULL
		  )
		),
		breadcrumb_overflow_example(ui), xtk_text(ui, status, (XtkTextDesc) {.size = 13})
	  )
	);
}
