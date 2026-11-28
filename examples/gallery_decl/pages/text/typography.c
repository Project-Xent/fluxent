#include "gallery.h"

XtkEl *page_typography(XtkUi *ui, Model const *m) {
	( void ) m;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(ui, "Typography", "The WinUI 3 type ramp, explicit font_family, and FontIcon."),
		xtk_card(
		  ui, (XtkStackDesc) {.gap = 10},
		  xtk_kids(
			xtk_text(ui, "Caption — 12", (XtkTextDesc) {.size = 12}),
			xtk_text(ui, "Body — 14", (XtkTextDesc) {.size = 14}),
			xtk_text(ui, "Body Strong — 14 SemiBold", (XtkTextDesc) {.size = 14, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Body Large — 18", (XtkTextDesc) {.size = 18}),
			xtk_text(ui, "Subtitle — 20 SemiBold", (XtkTextDesc) {.size = 20, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Title — 28 SemiBold", (XtkTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Title Large — 40 SemiBold", (XtkTextDesc) {.size = 40, .weight = FLUX_FONT_SEMI_BOLD}),
			xtk_text(ui, "Display — 68 SemiBold", (XtkTextDesc) {.size = 68, .weight = FLUX_FONT_SEMI_BOLD})
		  )
		),
		example(
		  ui, "Text with explicit font_family.",
		  xtk_card(
			ui, (XtkStackDesc) {.gap = 8},
			xtk_kids(
			  xtk_text(
				ui, "Cascadia Code — Monospace sample", (XtkTextDesc) {.font_family = "Cascadia Code", .size = 16}
			  ),
			  xtk_text(
				ui, "Consolas — Fallback monospace sample", (XtkTextDesc) {.font_family = "Consolas", .size = 16}
			  ),
			  xtk_text(ui, "Georgia — Serif editorial sample", (XtkTextDesc) {.font_family = "Georgia", .size = 16})
			)
		  )
		),
		example(
		  ui, "FontIcon glyphs, sizes, colors, and RTL mirroring.",
		  xtk_card(
			ui, (XtkStackDesc) {0},
			xtk_kids(xtk_row(
			  ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER, .wrap = XENT_FLEX_WRAP},
			  xtk_kids(
				xtk_fonticon(ui, "Home", (XtkFontIconDesc) {0}),
				xtk_automation(
				  ui, xtk_fonticon(ui, "Settings", (XtkFontIconDesc) {0}), &(XtkAutomation) {.name = "Settings icon"}
				),
				xtk_fonticon(ui, "Search", (XtkFontIconDesc) {0}),
				xtk_fonticon(ui, "Heart", (XtkFontIconDesc) {.size = 28, .color = flux_color_rgb(0xc4, 0x2b, 0x1c)}),
				xtk_fonticon(ui, "Refresh", (XtkFontIconDesc) {.size = 28, .color = flux_color_rgb(0x00, 0x78, 0xd4)}),
				xtk_fonticon(ui, "\xEE\x9C\xB4", (XtkFontIconDesc) {.size = 24}),
				xtk_fonticon(ui, "Forward", (XtkFontIconDesc) {.mirrored_when_rtl = true})
			  )
			))
		  )
		)
	  )
	);
}
