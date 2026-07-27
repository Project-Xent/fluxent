#include "app.h"
#include <math.h>
#include <stdio.h>

static XentNodeId
stretch_col(GalleryApp *g, XentNodeId parent, char const *source, XtkImageStretch stretch, char const *label) {
	XentNodeId col = g_col(g, parent, 6.0f, 0, 0, 0, 0);
	XentNodeId img = flux_image_init(&(FluxImageDesc) {
	  .ctx     = gallery_ctx(g),
	  .store   = gallery_store(g),
	  .parent  = col,
	  .source  = source,
	  .stretch = stretch,
	});
	xent_setsize(gallery_ctx(g), img, (XentSize) {180.0f, 110.0f});
	g_text(g, col, label, 12.0f, FLUX_FONT_REGULAR);
	return col;
}

void page_image(GalleryApp *g, XentNodeId parent) {
	static char const *const kSource = "C:/Windows/Web/Wallpaper/Windows/img0.jpg";
	g_page_header(g, parent, "Image", "WIC-decoded images with the WinUI stretch modes.");
	{
		XentNodeId ex  = g_example(g, parent, "The same source under each Stretch mode.");
		XentNodeId row = g_wraprow(g, g_card(g, ex), 24.0f);
		stretch_col(g, row, kSource, XTK_IMAGE_UNIFORM, "Uniform");
		stretch_col(g, row, kSource, XTK_IMAGE_FILL, "Fill");
		stretch_col(g, row, kSource, XTK_IMAGE_UNIFORM_TO_FILL, "UniformToFill");
	}
}
