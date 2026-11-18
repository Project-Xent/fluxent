#include "gallery.h"

#include <fluxent/flux_app.h>
#include <fluxent/flux_theme.h>
#include <fluxent/flux_window.h>

/*
 * System backdrop materials.
 *
 * Fluxent maps FluxBackdropType onto DWMWA_SYSTEMBACKDROP_TYPE, which is the DWM material
 * behind the window; the client area is cleared to a fully transparent colour whenever a
 * backdrop is active, so the material shows wherever the UI does not paint:
 *   - a plain Container paints layer_default, a semi-transparent colour;
 *   - a Card paints card_bg_default, which covers the material.
 *
 * Popups cannot take the material either: a popup is a tool window without a DWM redirection
 * surface, so it stays opaque (see the note at the bottom of the page).
 */

static char const *const kBackdropNames [] = {"None", "Mica", "Mica Alt", "Acrylic"};

static char const *const kBackdropDwm []
  = {"DWMSBT_NONE", "DWMSBT_MAINWINDOW", "DWMSBT_TABBEDWINDOW", "DWMSBT_TRANSIENTWINDOW"};

static char const *const kBackdropWhat [] = {
  "No material: the window paints solid_background and is fully opaque.",
  "Mica: the desktop wallpaper is blurred behind the window.",
  "Mica Alt (Tabbed): Mica with a stronger tint, for tabbed shells.",
  "Acrylic: a live blur of whatever sits behind the window.",
};

static char const *const kPopupItems [] = {"First item", "Second item", "Third item"};

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

/* Bare page: nothing paints here, so the material is the page background. */
static XtkEl *bare_patch(XtkUi *ui, char const *caption, char const *note) {
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 6, .padding = {16, 14, 16, 14}
    },
	  xtk_kids(
		xtk_text(ui, caption, (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, note, (XtkTextDesc) {.size = 12})
	  )
	);
}

/* Container: paints layer_default, so the material reads as a tint. */
static XtkEl *layered_patch(XtkUi *ui, char const *caption, char const *note) {
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 6, .padding = {16, 14, 16, 14}
    },
	  xtk_kids(
		xtk_text(ui, caption, (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, note, (XtkTextDesc) {.size = 12})
	  )
	);
}

/* Card: paints card_bg_default, which mostly hides the material. */
static XtkEl *carded_patch(XtkUi *ui, char const *caption, char const *note) {
	return xtk_card(
	  ui,
	  (XtkStackDesc) {
		.gap = 6, .padding = {16, 14, 16, 14}
    },
	  xtk_kids(
		xtk_text(ui, caption, (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, note, (XtkTextDesc) {.size = 12})
	  )
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
			  xtk_grow(
				bare_patch(ui, "Bare page", "Nothing paints here - the system backdrop is the page background."), 1
			  ),
			  xtk_grow(
				layered_patch(ui, "Container layer", "Containers paint layer_default, so it reads as a tint."), 1
			  ),
			  xtk_grow(carded_patch(ui, "Card", "Cards paint card_bg_default, which hides the material."), 1)
			)
		  )
		),

		example(
		  ui, "Popup surface",
		  xtk_column(
			ui, (XtkStackDesc) {.gap = 10, .align = XENT_FLEX_ALIGN_START},
			xtk_kids(
			  xtk_combo(ui, "Open the drop-down popup", (XtkComboDesc) {.items = kPopupItems, .nitem = 3, .sel = 0}),
			  xtk_text(
				ui,
				"The drop-down is a real OS popup (flux_popup_create). A popup cannot take the system "
				"backdrop: it is a tool window without a DWM redirection surface, so the acrylic brush "
				"has nothing to blur there. It stays opaque by design.",
				(XtkTextDesc) {.size = 12}
			  )
			)
		  )
		),

		example(
		  ui, "Popups and the system backdrop",
		  xtk_text(
			ui,
			"The system backdrop belongs to the window, not to elements inside it. An element that "
			"wants a material of its own needs a backdrop brush behind it, which the Composition "
			"renderer exposes and the Direct2D renderer does not; the window material below is what "
			"both renderers honour.",
			(XtkTextDesc) {.size = 12}
		  )
		),

		xtk_text(
		  ui,
		  "Requires Windows 11 22H2 (build 22621) or newer; below that Fluxent clamps the backdrop "
		  "to None. Your choice is written to fluxent-gallery.ini under LOCALAPPDATA and restored on "
		  "the next run.",
		  (XtkTextDesc) {.size = 12}
		)
	  )
	);
}
