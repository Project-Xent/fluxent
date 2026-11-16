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

static void asset_col(PageBuild *b, XtkNodeRef parent, char const *name, XtkImageProps props, char const *label) {
	XtkNodeRef col  = xtk_ref_local(ui_column(&b->txn, parent, 6.0f, XENT_FLEX_ALIGN_START));
	props.set      |= XTK_IMAGE_SRC_MASK | XTK_IMAGE_FIT_MASK;
	props.src       = flux_image_respath(gallery_fmt(b->app, "%s/%s", GALLERY_ASSETS, name));
	xtk_image_create(&b->txn, col, &props);
	ui_text(&b->txn, col, label, 12.0f, FLUX_FONT_REGULAR);
}

static XtkNodeRef image_card(PageBuild *b, XtkNodeRef parent, char const *caption) {
	XtkNodeRef card
	  = xtk_ref_local(ui_card(&b->txn, page_example(b, parent, caption), 0.0f, XENT_FLEX_ALIGN_STRETCH, 16.0f));
	return xtk_ref_local(ui_row(&b->txn, card, 24.0f, true));
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
	XtkNodeRef natural = image_card(
	  b, parent,
	  "No Width or Height: each image measures to its natural size once its header is decoded off the UI thread."
	);
	asset_col(b, natural, "logo.svg", (XtkImageProps) {.fit = XTK_IMAGE_UNIFORM}, "SVG");
	asset_col(b, natural, "spinner.gif", (XtkImageProps) {.fit = XTK_IMAGE_UNIFORM}, "Animated GIF");
	asset_col(b, natural, "bounce.png", (XtkImageProps) {.fit = XTK_IMAGE_UNIFORM}, "APNG");
	asset_col(
	  b, natural, "logo.svg",
	  (XtkImageProps) {.set = XTK_IMAGE_W_MASK | XTK_IMAGE_H_MASK, .w = 192.0f, .h = 192.0f, .fit = XTK_IMAGE_UNIFORM},
	  "SVG at 192 × 192"
	);
	XtkNodeRef    nine = image_card(b, parent, "NineGrid = 12: corners keep their size, edges and center stretch.");
	XtkImageProps fill = {.set = XTK_IMAGE_W_MASK | XTK_IMAGE_H_MASK, .w = 240.0f, .h = 80.0f, .fit = XTK_IMAGE_FILL};
	asset_col(b, nine, "frame.png", fill, "Fill");
	fill.set |= XTK_IMAGE_NL_MASK | XTK_IMAGE_NT_MASK | XTK_IMAGE_NR_MASK | XTK_IMAGE_NB_MASK;
	fill.nl = fill.nt = fill.nr = fill.nb = 12.0f;
	asset_col(b, nine, "frame.png", fill, "Fill + NineGrid");
	return true;
}
