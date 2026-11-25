#include "gallery.h"
#include "gallery_assets.h"

static XtkEl *stretch_col(XtkUi *ui, XtkResource source, XtkImageStretch stretch, char const *label) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 6},
	  xtk_kids(
		xtk_sized(xtk_image(ui, (XtkImageDesc) {.source = source, .stretch = stretch}), 180, 110),
		xtk_text(ui, label, (XtkTextDesc) {.size = 12})
	  )
	);
}

static XtkEl *labeled(XtkUi *ui, XtkEl *image, char const *label) {
	return xtk_column(ui, (XtkStackDesc) {.gap = 6}, xtk_kids(image, xtk_text(ui, label, (XtkTextDesc) {.size = 12})));
}

static XtkEl *asset(XtkUi *ui, wchar_t const *name, XtkImageDesc desc) {
	desc.source = gallery_asset(name);
	return xtk_image(ui, desc);
}

XtkEl *page_image(XtkUi *ui, Model const *m) {
	( void ) m;
	XtkResource source = gallery_wallpaper();
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH
    },
	  xtk_kids(
		page_header(ui, "Image", "WIC-decoded images with the WinUI stretch modes."),
		example(
		  ui, "The same source under each Stretch mode.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 24, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				stretch_col(ui, source, XTK_IMAGE_UNIFORM, "Uniform"), stretch_col(ui, source, XTK_IMAGE_FILL, "Fill"),
				stretch_col(ui, source, XTK_IMAGE_UNIFORM_TO_FILL, "UniformToFill")
			  )
			))
		  )
		),
		example(
		  ui,
		  "No Width or Height: each image measures to its natural size once its header is decoded off the UI thread.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 24, .wrap = XENT_FLEX_WRAP, .align = XENT_FLEX_ALIGN_END},
			  xtk_kids(
				labeled(ui, asset(ui, L"logo.svg", (XtkImageDesc) {.stretch = XTK_IMAGE_UNIFORM}), "SVG"),
				labeled(ui, asset(ui, L"spinner.gif", (XtkImageDesc) {.stretch = XTK_IMAGE_UNIFORM}), "Animated GIF"),
				labeled(ui, asset(ui, L"bounce.png", (XtkImageDesc) {.stretch = XTK_IMAGE_UNIFORM}), "APNG"),
				labeled(
				  ui, xtk_sized(asset(ui, L"logo.svg", (XtkImageDesc) {.stretch = XTK_IMAGE_UNIFORM}), 192, 192),
				  "SVG at 192 × 192"
				)
			  )
			))
		  )
		),
		example(
		  ui, "NineGrid = 12: corners keep their size, edges and center stretch.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 24, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				labeled(
				  ui, xtk_sized(asset(ui, L"frame.png", (XtkImageDesc) {.stretch = XTK_IMAGE_FILL}), 240, 80), "Fill"
				),
				labeled(
				  ui,
				  xtk_sized(
					asset(ui, L"frame.png", (XtkImageDesc) {.stretch = XTK_IMAGE_FILL, .nine = {12, 12, 12, 12}}), 240,
					80
				  ),
				  "Fill + NineGrid"
				)
			  )
			))
		  )
		)
	  )
	);
}
