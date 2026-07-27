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
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(b, parent, "Typography", "The WinUI 3 type ramp rendered by xtk_text.");
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
	return true;
}
