#include "gallery.h"

static char const *color_text(XtkUi *ui, unsigned rgba) {
	return xtk_fmt(ui, "#%02X%02X%02X%02X", rgba >> 24, (rgba >> 16) & 0xffu, (rgba >> 8) & 0xffu, rgba & 0xffu);
}

static XtkEl *color_example(XtkUi *ui, char const *caption, unsigned rgba, bool alpha, int tag) {
	return example(
	  ui, caption,
	  demo_card(
		ui,
		xtk_color_picker(ui, (XtkColorPickerDesc) {.color = (XtkColor) {rgba}, .alpha = alpha, .change = xtk_msg(tag)}),
		color_text(ui, rgba), NULL
	  )
	);
}

XtkEl *page_color_picker(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "ColorPicker",
		  "A spectrum, a hue slider and channel inputs. Drag in the spectrum or type a value; the color updates live."
		),
		color_example(ui, "A ColorPicker without alpha.", m->color_pick, false, MSG_COLOR_PICKER),
		color_example(ui, "With the alpha channel enabled.", m->color_alpha, true, MSG_COLOR_ALPHA)
	  )
	);
}
