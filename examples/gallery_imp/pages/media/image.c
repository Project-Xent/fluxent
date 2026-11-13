#include "app.h"

static void stretch_col(XtkTxn *txn, XtkNodeRef parent, XtkImageStretch stretch, char const *label) {
	XtkNodeRef col = xtk_ref_local(ui_column(txn, parent, 6.0f, XENT_FLEX_ALIGN_START));
	xtk_image_create(
	  txn, col,
	  &(XtkImageProps) {
		.set = XTK_IMAGE_W_MASK | XTK_IMAGE_H_MASK | XTK_IMAGE_SRC_MASK | XTK_IMAGE_FIT_MASK,
		.w   = 180.0f,
		.h   = 110.0f,
		.src = flux_image_respath("C:/Windows/Web/Wallpaper/Windows/img0.jpg"),
		.fit = stretch,
	  }
	);
	ui_text(txn, col, label, 12.0f, FLUX_FONT_REGULAR);
}

bool page_image_build(PageBuild *b) {
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "Image", "WIC-decoded images with the WinUI stretch modes.");
	XtkNodeRef card = xtk_ref_local(ui_card(
	  &b->txn, page_example(b, parent, "The same source under each Stretch mode."), 0.0f, XENT_FLEX_ALIGN_STRETCH, 16.0f
	));
	XtkNodeRef row  = xtk_ref_local(ui_row(&b->txn, card, 24.0f, true));
	stretch_col(&b->txn, row, XTK_IMAGE_UNIFORM, "Uniform");
	stretch_col(&b->txn, row, XTK_IMAGE_FILL, "Fill");
	stretch_col(&b->txn, row, XTK_IMAGE_UNIFORM_TO_FILL, "UniformToFill");
	return true;
}
