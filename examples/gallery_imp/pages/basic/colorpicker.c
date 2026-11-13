#include "app.h"

static char const *color_text(GalleryApp *g, unsigned rgba) {
	return gallery_fmt(g, "#%02X%02X%02X%02X", rgba >> 24, (rgba >> 16) & 0xffu, (rgba >> 8) & 0xffu, rgba & 0xffu);
}

static void color_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_COLOR_PICKER], color_text(g, g->model.color_pick));
	gallery_settext(g, g->live_output [MSG_COLOR_ALPHA], color_text(g, g->model.color_alpha));
}

static void color_example(PageBuild *b, XtkNodeRef parent, char const *caption, unsigned rgba, bool alpha, int tag) {
	PageDemo   demo   = page_demo(b, page_example(b, parent, caption));
	XtkLocalId picker = xtk_colorpicker_create(
	  &b->txn, demo.items,
	  &(XtkColorPickerProps) {
		.set   = XTK_COLORPICKER_COLOR_MASK | XTK_COLORPICKER_ALPHA_MASK,
		.color = (XtkColor) {rgba},
		.alpha = alpha,
	  }
	);
	xtk_colorpicker_onvalue(&b->txn, xtk_ref_local(picker), page_post, page_bind(b, tag));
	page_keepout(b, tag, page_output(b, demo.row, color_text(b->app, rgba)));
}

bool page_color_picker_build(PageBuild *b) {
	page_sync(b, color_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "ColorPicker",
	  "A spectrum, a hue slider and channel inputs. Drag in the spectrum or type a value; the color updates live."
	);
	color_example(b, parent, "A ColorPicker without alpha.", b->app->model.color_pick, false, MSG_COLOR_PICKER);
	color_example(b, parent, "With the alpha channel enabled.", b->app->model.color_alpha, true, MSG_COLOR_ALPHA);
	return true;
}
