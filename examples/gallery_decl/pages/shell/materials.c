#include "gallery.h"

#include <fluxent/flux_app.h>
#include <fluxent/flux_theme.h>

static char const *const kBackdropNames [] = {"None", "Mica", "Mica Alt", "Acrylic"};

static char const *const kBackdropDwm []
  = {"DWMSBT_NONE", "DWMSBT_MAINWINDOW", "DWMSBT_TABBEDWINDOW", "DWMSBT_TRANSIENTWINDOW"};

static char const *const kBackdropWhat [] = {
  "No material: the window paints solid_background and is fully opaque.",
  "Mica: the desktop wallpaper is blurred behind the window.",
  "Mica Alt (Tabbed): Mica with a stronger tint, for tabbed shells.",
  "Acrylic: a live blur of whatever sits behind the window.",
};

#define BACKDROP_COUNT 4

static XtkEl *backdrop_button(XtkUi *ui, Model const *m, int type) {
	return xtk_button(
	  ui, kBackdropNames [type],
	  (XtkButtonDesc) {
		.style = m->backdrop == type ? XTK_BUTTON_ACCENT : XTK_BUTTON_STANDARD,
		.click = xtk_msg_i(MSG_BACKDROP, type),
	  }
	);
}

typedef XtkEl *(*PatchFn)(XtkUi *ui, XtkStackDesc desc, XtkEl *children []);

static XtkEl  *patch(XtkUi *ui, PatchFn make, char const *caption, char const *note) {
	return xtk_grow(
	  make(
		ui,
		(XtkStackDesc) {
		  .gap = 6, .padding = {16, 14, 16, 14}
    },
		xtk_kids(
		  xtk_text(ui, caption, (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
		  xtk_text(ui, note, (XtkTextDesc) {.size = 12})
		)
	  ),
	  1
	);
}

XtkEl *page_materials(XtkUi *ui, Model const *m) {
	int const cur = (m->backdrop >= 0 && m->backdrop < BACKDROP_COUNT) ? m->backdrop : 0;

	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Materials",
		  "System backdrops (Mica, Mica Alt, Acrylic) applied to the real window. These are DWM "
		  "materials: Fluxent asks for one, then keeps the client area transparent wherever "
		  "nothing is painted."
		),

		example(
		  ui, "Backdrop",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
			xtk_kids(
			  backdrop_button(ui, m, 0), backdrop_button(ui, m, 1), backdrop_button(ui, m, 2), backdrop_button(ui, m, 3)
			)
		  )
		),

		xtk_column(
		  ui, (XtkStackDesc) {.gap = 4},
		  xtk_kids(
			xtk_text(
			  ui, xtk_fmt(ui, "Active: %s  (%s)", kBackdropNames [cur], kBackdropDwm [cur]),
			  (XtkTextDesc) {.size = 13, .weight = FLUX_FONT_SEMI_BOLD}
			),
			xtk_text(ui, kBackdropWhat [cur], (XtkTextDesc) {.size = 12})
		  )
		),

		example(
		  ui, "See-through vs painted",
		  xtk_row(
			ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
			xtk_kids(
			  patch(ui, xtk_column, "Bare page", "Nothing paints here - the backdrop is the background."),
			  patch(ui, xtk_card, "Card", "Cards paint card_bg_default, which hides the material.")
			)
		  )
		),

		xtk_text(
		  ui,
		  "The backdrop belongs to the window, not to elements inside it: anything that paints an "
		  "opaque background covers it. Requires Windows 11 22H2 (build 22621) or newer; below that "
		  "Fluxent clamps the backdrop to None. Your choice is written to fluxent-gallery.ini under "
		  "LOCALAPPDATA and restored on the next run.",
		  (XtkTextDesc) {.size = 12}
		)
	  )
	);
}
