#include "app.h"

typedef struct TypeSample {
	char const *text;
	float       size;
	uint32_t    weight;
} TypeSample;

bool page_typography_build(PageBuild *b) {
	static TypeSample const samples [] = {
	  {"Caption — 12",              12.0f, FLUX_FONT_REGULAR  },
	  {"Body — 14",                 14.0f, FLUX_FONT_REGULAR  },
	  {"Body Strong — 14 SemiBold", 14.0f, FLUX_FONT_SEMI_BOLD},
	  {"Body Large — 18",           18.0f, FLUX_FONT_REGULAR  },
	  {"Subtitle — 20 SemiBold",    20.0f, FLUX_FONT_SEMI_BOLD},
	  {"Title — 28 SemiBold",       28.0f, FLUX_FONT_SEMI_BOLD},
	  {"Title Large — 40 SemiBold", 40.0f, FLUX_FONT_SEMI_BOLD},
	  {"Display — 68 SemiBold",     68.0f, FLUX_FONT_SEMI_BOLD},
	};
	static char const *const fonts [][2] = {
	  {"Cascadia Code", "Cascadia Code — Monospace sample"    },
	  {"Consolas",      "Consolas — Fallback monospace sample"},
	  {"Georgia",       "Georgia — Serif editorial sample"    },
	};
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "Typography", "The WinUI 3 type ramp, explicit font_family, and FontIcon.");
	XtkLocalId card = xtk_card_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.pl    = 16.0f,
		.pt    = 16.0f,
		.pr    = 16.0f,
		.pb    = 16.0f,
	  }
	);
	for (size_t i = 0; i < sizeof(samples) / sizeof(samples [0]); i++)
		xtk_text_create(
		  &b->txn, xtk_ref_local(card),
		  &(XtkTextProps) {
			.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
			.text       = samples [i].text,
			.fontsize   = samples [i].size,
			.fontweight = samples [i].weight,
		  }
		);
	XtkLocalId font_card = xtk_card_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.gap   = 8.0f,
		.pl    = 16.0f,
		.pt    = 16.0f,
		.pr    = 16.0f,
		.pb    = 16.0f,
	  }
	);
	for (size_t i = 0; i < sizeof(fonts) / sizeof(fonts [0]); i++)
		xtk_text_create(
		  &b->txn, xtk_ref_local(font_card),
		  &(XtkTextProps) {
			.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTFAMILY_MASK | XTK_TEXT_FONTSIZE_MASK,
			.text       = fonts [i][1],
			.fontfamily = fonts [i][0],
			.fontsize   = 16.0f,
		  }
		);
	XtkLocalId icon_card = xtk_card_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_WRAP_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK,
		.dir   = XTK_STACK_ROW,
		.items = XENT_FLEX_ALIGN_CENTER,
		.gap   = 16.0f,
		.wrap  = XENT_FLEX_WRAP,
		.pl    = 16.0f,
		.pt    = 16.0f,
		.pr    = 16.0f,
		.pb    = 16.0f,
	  }
	);
	xtk_fonticon_create(
	  &b->txn, xtk_ref_local(icon_card), &(XtkFontIconProps) {.set = XTK_FONTICON_GLYPH_MASK, .glyph = "Home"}
	);
	xtk_fonticon_create(
	  &b->txn, xtk_ref_local(icon_card),
	  &(XtkFontIconProps) {
		.set = XTK_FONTICON_GLYPH_MASK | XTK_FONTICON_ANAME_MASK, .glyph = "Settings", .aname = "Settings icon"}
	);
	xtk_fonticon_create(
	  &b->txn, xtk_ref_local(icon_card), &(XtkFontIconProps) {.set = XTK_FONTICON_GLYPH_MASK, .glyph = "Search"}
	);
	xtk_fonticon_create(
	  &b->txn, xtk_ref_local(icon_card),
	  &(XtkFontIconProps) {
		.set      = XTK_FONTICON_GLYPH_MASK | XTK_FONTICON_FONTSIZE_MASK | XTK_FONTICON_FG_MASK,
		.glyph    = "Heart",
		.fontsize = 28.0f,
		.fg       = flux_color_rgb(0xc4, 0x2b, 0x1c),
	  }
	);
	xtk_fonticon_create(
	  &b->txn, xtk_ref_local(icon_card),
	  &(XtkFontIconProps) {
		.set      = XTK_FONTICON_GLYPH_MASK | XTK_FONTICON_FONTSIZE_MASK | XTK_FONTICON_FG_MASK,
		.glyph    = "Refresh",
		.fontsize = 28.0f,
		.fg       = flux_color_rgb(0x00, 0x78, 0xd4),
	  }
	);
	xtk_fonticon_create(
	  &b->txn, xtk_ref_local(icon_card),
	  &(XtkFontIconProps) {
		.set = XTK_FONTICON_GLYPH_MASK | XTK_FONTICON_FONTSIZE_MASK, .glyph = "\xEE\x9C\xB4", .fontsize = 24.0f}
	);
	xtk_fonticon_create(
	  &b->txn, xtk_ref_local(icon_card),
	  &(XtkFontIconProps) {
		.set = XTK_FONTICON_GLYPH_MASK | XTK_FONTICON_MIRRORRTL_MASK, .glyph = "Forward", .mirrorrtl = true}
	);
	return true;
}
