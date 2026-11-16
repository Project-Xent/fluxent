#include "gallery.h"

static XtkEl *drop_target(XtkUi *ui, Model const *m) {
	return xtk_dragdrop(
	  ui,
	  (XtkDragDropDesc) {
		.drop       = XTK_DROP_COPY,
		.hover      = xtk_msg(MSG_DD_HOVER),
		.drop_files = xtk_msg(MSG_DD_FILES),
		.drop_text  = xtk_msg(MSG_DD_TEXT),
	  },
	  xtk_card(
		ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH},
		xtk_kids(
		  xtk_text(ui, gallery_dd_prompt(m), (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
		  xtk_text(ui, m->dd_files ? "Files:" : "Text:", (XtkTextDesc) {.size = 12}),
		  xtk_text(ui, gallery_dd_result(m), (XtkTextDesc) {.size = 14})
		)
	  )
	);
}

static XtkEl *source(XtkUi *ui, char const *label, char const *text, char const *files) {
	return xtk_dragdrop(
	  ui, (XtkDragDropDesc) {.drag_text = text, .drag_files = files, .dragged = xtk_msg(MSG_DD_DONE)},
	  xtk_card(ui, (XtkStackDesc) {0}, xtk_kids(xtk_text(ui, label, (XtkTextDesc) {.size = 14})))
	);
}

XtkEl *page_drag_drop(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Drag and drop", "OLE drag and drop: drop targets, drag sources, and the system drag image."),
		example(ui, "A drop target that copies files or text.", drop_target(ui, m)),
		example(
		  ui, "Drag sources. The file card offers this gallery's executable.",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_START},
			xtk_kids(
			  xtk_row(
				ui, (XtkStackDesc) {.gap = 12},
				xtk_kids(
				  source(ui, "Drag this text", "Hello from Xent", NULL),
				  source(ui, "Drag this program file", NULL, m->dd_exe)
				)
			  ),
			  xtk_text(ui, gallery_dd_completed(m), (XtkTextDesc) {.size = 14})
			)
		  )
		)
	  )
	);
}
