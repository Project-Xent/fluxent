#include "gallery.h"

static FluxEl *stretch_col(FxUi *ui, char const *source, FluxImageStretch stretch, char const *label) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 6},
	  (FluxEl *[]) {
		fx_sized(fx_image(ui, source, (FxImageDesc) {.stretch = stretch}), 180, 110),
		fx_text(ui, label, (FxTextDesc) {.size = 12}), FX_END}
	);
}

FluxEl *page_image(FxUi *ui, Model const *m) {
	( void ) m;
	static char const *const kSource = "C:/Windows/Web/Wallpaper/Windows/img0.jpg";
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, "Image", "WIC-decoded images with the WinUI stretch modes."),
		example(
		  ui, "The same source under each Stretch mode.",
		  fx_card(
			ui, (FxStackDesc) {0},
			(FluxEl *[]) {
			  fx_row(
				ui, (FxStackDesc) {.gap = 24},
				(FluxEl *[]) {
				  stretch_col(ui, kSource, FLUX_IMAGE_UNIFORM, "Uniform"),
				  stretch_col(ui, kSource, FLUX_IMAGE_FILL, "Fill"),
				  stretch_col(ui, kSource, FLUX_IMAGE_UNIFORM_TO_FILL, "UniformToFill"), FX_END}
			  ),
			  FX_END}
		  )
		),
		FX_END}
	);
}
