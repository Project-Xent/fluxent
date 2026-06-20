/**
 * @file test_fx_crossaxis.c
 * @brief Cross-axis hug + definite-size honoring regressions.
 *
 * Guards the layout contract that lets xtk stacks size correctly in both
 * orientations:
 *  - A column hugs its width to content (like a row hugs its height), so the
 *    Image gallery's stretch-mode columns sit side by side in a row instead of
 *    collapsing to width 0 and overlapping — with no explicit width.
 *  - A column that fills its parent (cross-axis stretch) is given that width as
 *    a DEFINITE size by the engine, so a card's text wraps to the parent width
 *    instead of inflating to its max-content and overflowing.
 *  - A SplitButton reserves icon glyph+gap padding on mount without clobbering
 *    its trailing divider+secondary chevron zone.
 */
#include "flux_internal.h"

#include <math.h>
#include <stdio.h>

static void update(void *model, XtkMsg msg) {
	( void ) model;
	( void ) msg;
}

static XtkEl *stretch_col(XtkUi *ui, FluxImageStretch stretch, char const *label) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 6},
	  (XtkEl *[]) {
		xtk_sized(
		  xtk_image(ui, "C:/Windows/Web/Wallpaper/Windows/img0.jpg", (XtkImageDesc) {.stretch = stretch}), 180, 110
		),
		xtk_text(ui, label, (XtkTextDesc) {.size = 12}), XTK_END}
	);
}

static char const *const kLongText
  = "This is a deliberately long paragraph that must wrap to the width its card is "
    "stretched to, rather than inflating the card to its single-line max-content width.";

static XtkEl *view(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 24},
		  (XtkEl *[]) {
			stretch_col(ui, FLUX_IMAGE_UNIFORM, "Uniform"),
			stretch_col(ui, FLUX_IMAGE_FILL, "Fill"),
			stretch_col(ui, FLUX_IMAGE_UNIFORM_TO_FILL, "UniformToFill"), XTK_END}
		),
		xtk_split_button(
		  ui, "Save",
		  (XtkSplitDesc) {
			.icon = "Save",
			.items =
			  (XtkMenuItemDesc []) {
				{.label = "Save as..."},
				{.label = "Save all"},
			  },
			.item_count = 2}
		),
		/* A 360px-wide stretch column wrapping a card of long text. */
		xtk_sized(
		  xtk_column(
			ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH},
			(XtkEl *[]) {
			  xtk_card(ui, (XtkStackDesc) {0}, (XtkEl *[]) {xtk_text(ui, kLongText, (XtkTextDesc) {.size = 14}), XTK_END}),
			  XTK_END}
		  ),
		  360, NAN
		),
		XTK_END}
	);
}

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

static XentRect node_rect(XentContext *ctx, XtkNode *n) {
	XentRect r = {0};
	xent_get_layout_rect(ctx, n->node, &r);
	return r;
}

int main(void) {
	XentConfig     config = {0};
	XentContext   *ctx    = xent_create_context(&config);
	FluxNodeStore *store  = flux_node_store_create(64);
	flux_node_store_bind_context(store, ctx);

	XentNodeId host = xent_create_node(ctx);
	xent_set_protocol(ctx, host, XENT_PROTOCOL_FLEX);
	xent_set_flex_direction(ctx, host, XENT_FLEX_COLUMN);
	xent_set_flex_align_items(ctx, host, XENT_FLEX_ALIGN_STRETCH);

	int        m  = 0;
	FluxBackendCtx bctx = {.ctx = ctx, .store = store, .app = NULL, .runtime = NULL};
	XtkBackend be = flux_xtk_backend(&bctx);
	XtkRuntime *rt = xtk_runtime_create(ctx, &be, host, &m, update, view);
	if (rt) bctx.runtime = rt;
	EXPECT(rt, "runtime");

	xtk_runtime_frame(rt);
	xent_layout(ctx, host, 1100.0f, 760.0f);

	/* Image row: three columns hug their 180px image, laid side by side
	 * (180 + 24 gap = 204 stride) without overlap — no explicit width. */
	XtkNode  *row        = rt->root->children [0];
	float    prev_right = -1.0f;
	EXPECT(row->child_count == 3, "image row has three columns");
	for (int i = 0; i < row->child_count; i++) {
		XentRect c = node_rect(ctx, row->children [i]);
		EXPECT(c.w == 180.0f, "image column hugs its 180px child width");
		EXPECT(c.x >= prev_right, "image columns do not overlap");
		prev_right = c.x + c.w;
	}

	/* SplitButton: total width clears left icon padding + label + the trailing
	 * 47px (divider + secondary) zone. Without the reservation it was ~90. */
	XentRect split = node_rect(ctx, rt->root->children [1]);
	EXPECT(split.h == 32.0f, "split button is one line tall");
	EXPECT(split.w - 47.0f >= 60.0f, "split button primary zone fits icon + label");

	/* Long text wraps to the 360px-stretched card, not its max-content width.
	 * The card stretches to 360; its text must stay within the content box. */
	XtkNode  *wrap_col = rt->root->children [2];
	XentRect wrap     = node_rect(ctx, wrap_col);
	EXPECT(wrap.w == 360.0f, "wrap column keeps its pinned 360px width");
	XtkNode  *card     = wrap_col->children [0];
	XentRect card_r   = node_rect(ctx, card);
	EXPECT(card_r.w == 360.0f, "card stretches to the column width, not its max-content");
	XentRect text_r = node_rect(ctx, card->children [0]);
	EXPECT(text_r.w <= card_r.w, "wrapped text stays within the card content box");

	xtk_runtime_destroy(rt);
	flux_node_store_destroy(store);
	xent_destroy_context(ctx);
	printf("PASS: xtk cross-axis hug + definite-size wrap + split button icon\n");
	return 0;
}
