#include "gallery.h"

XtkEl *page_dialog(XtkUi *ui, Model const *m) {
	static char const *const kResults [] = {"Cancelled", "Deleted", "Secondary"};
	char const *result = m->dialog_result >= 0 && m->dialog_result <= 2 ? kResults [m->dialog_result] : "(none yet)";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "ContentDialog", "A modal dialog controlled by the model: open while a flag is true."),
		example(
		  ui, "A dialog with Delete / Cancel buttons.",
		  demo_card(
			ui, xtk_button(ui, "Show dialog", (XtkButtonDesc) {.click = xtk_msg(MSG_DIALOG_OPEN)}),
			xtk_fmt(ui, "Last result: %s", result), NULL
		  )
		),
		xtk_dialog(
		  ui, "Delete file?",
		  (XtkDialogDesc) {
			.open     = m->dialog_open,
			.primary  = "Delete",
			.close    = "Cancel",
			.w        = 400,
			.contenth = 56,
			.result   = xtk_msg(MSG_DIALOG_RESULT)},
		  xtk_kids(
			xtk_text(ui, "This file will be permanently removed. This cannot be undone.", (XtkTextDesc) {.size = 14})
		  )
		)
	  )
	);
}
