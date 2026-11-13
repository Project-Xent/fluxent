#include "gallery.h"

static XtkEl *stretch_col(XtkUi *ui, char const *source, XtkImageStretch stretch, char const *label) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 6},
	  xtk_kids(
		xtk_sized(xtk_image(ui, (XtkImageDesc) {.source = flux_image_respath(source), .stretch = stretch}), 180, 110),
		xtk_text(ui, label, (XtkTextDesc) {.size = 12})
	  )
	);
}

XtkEl *page_image(XtkUi *ui, Model const *m) {
	( void ) m;
	static char const *const kSource = "C:/Windows/Web/Wallpaper/Windows/img0.jpg";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Image", "WIC-decoded images with the WinUI stretch modes."),
		example(
		  ui, "The same source under each Stretch mode.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 24, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				stretch_col(ui, kSource, XTK_IMAGE_UNIFORM, "Uniform"),
				stretch_col(ui, kSource, XTK_IMAGE_FILL, "Fill"),
				stretch_col(ui, kSource, XTK_IMAGE_UNIFORM_TO_FILL, "UniformToFill")
			  )
			))
		  )
		)
	  )
	);
}
