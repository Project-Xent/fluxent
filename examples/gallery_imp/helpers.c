#include "app.h"

#include <math.h>
#include <string.h>
#include <xent/xent.h>

static XtkNodeRef page_text(PageBuild *b, XtkNodeRef parent, char const *text, float size, uint32_t weight) {
	XtkLocalId local = xtk_text_create(
	  &b->txn, parent,
	  &(XtkTextProps) {
		.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
		.text       = text,
		.fontsize   = size,
		.fontweight = weight,
	  }
	);
	return xtk_ref_local(local);
}

static XtkNodeRef page_column(PageBuild *b, XtkNodeRef parent, float gap) {
	XtkLocalId local = xtk_container_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set    = XTK_CONTAINER_SHRINK_MASK
	            | XTK_CONTAINER_DIR_MASK
	            | XTK_CONTAINER_ITEMS_MASK
	            | XTK_CONTAINER_GAP_MASK
	            | XTK_CONTAINER_FILL_MASK
	            | XTK_CONTAINER_FITW_MASK
	            | XTK_CONTAINER_FITH_MASK,
		.shrink = 0.0f,
		.dir    = XTK_STACK_COLUMN,
		.items  = XENT_FLEX_ALIGN_STRETCH,
		.gap    = gap,
		.fill   = true,
		.fitw   = false,
		.fith   = true,
	  }
	);
	return xtk_ref_local(local);
}

static XtkNodeRef page_row(PageBuild *b, XtkNodeRef parent, bool fill, XentFlexJustify justify) {
	XtkLocalId local = xtk_container_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set     = XTK_CONTAINER_DIR_MASK
	             | XTK_CONTAINER_ITEMS_MASK
	             | XTK_CONTAINER_JUSTIFY_MASK
	             | XTK_CONTAINER_GAP_MASK
	             | XTK_CONTAINER_FILL_MASK
	             | XTK_CONTAINER_WRAP_MASK,
		.dir     = XTK_STACK_ROW,
		.items   = XENT_FLEX_ALIGN_CENTER,
		.justify = justify,
		.gap     = 16.0f,
		.fill    = fill,
		.wrap    = XENT_FLEX_WRAP,
	  }
	);
	return xtk_ref_local(local);
}

static XtkNodeRef page_card(PageBuild *b, XtkNodeRef parent) {
	XtkLocalId local = xtk_card_create(
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
	return xtk_ref_local(local);
}

XtkNodeRef page_header(PageBuild *b, XtkNodeRef parent, char const *title, char const *blurb) {
	XtkNodeRef col = page_column(b, parent, 8.0f);
	page_text(b, col, title, 28.0f, FLUX_FONT_SEMI_BOLD);
	page_text(b, col, blurb, 13.0f, FLUX_FONT_REGULAR);
	return col;
}

XtkNodeRef page_example(PageBuild *b, XtkNodeRef parent, char const *caption) {
	XtkNodeRef col = page_column(b, parent, 8.0f);
	page_text(b, col, caption, 14.0f, FLUX_FONT_REGULAR);
	return col;
}

PageDemo page_demo(PageBuild *b, XtkNodeRef parent) {
	XtkNodeRef card  = page_card(b, parent);
	XtkNodeRef row   = page_row(b, card, true, XENT_FLEX_JUSTIFY_SPACE_BETWEEN);
	XtkNodeRef items = page_row(b, row, false, XENT_FLEX_JUSTIFY_START);
	return (PageDemo) {.row = row, .items = items};
}

XtkNodeRef page_output(PageBuild *b, XtkNodeRef parent, char const *value) {
	XtkLocalId col = xtk_container_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set    = XTK_CONTAINER_W_MASK
	            | XTK_CONTAINER_SHRINK_MASK
	            | XTK_CONTAINER_DIR_MASK
	            | XTK_CONTAINER_ITEMS_MASK
	            | XTK_CONTAINER_GAP_MASK
	            | XTK_CONTAINER_FILL_MASK
	            | XTK_CONTAINER_FITW_MASK
	            | XTK_CONTAINER_FITH_MASK,
		.w      = 260.0f,
		.shrink = 0.0f,
		.dir    = XTK_STACK_COLUMN,
		.items  = XENT_FLEX_ALIGN_STRETCH,
		.gap    = 4.0f,
		.fill   = true,
		.fitw   = false,
		.fith   = true,
	  }
	);
	XtkNodeRef ref = xtk_ref_local(col);
	page_text(b, ref, "Output:", 12.0f, FLUX_FONT_SEMI_BOLD);
	return page_text(b, ref, value, 14.0f, FLUX_FONT_REGULAR);
}

static XentNodeId map_first(XtkBatchResult *r) {
	if (!r || r->control_count == 0) return XENT_NODE_INVALID;
	return ( XentNodeId ) r->controls [0].control;
}

static void flex_col(GalleryApp *g, XentNodeId n, float gap) {
	if (n == XENT_NODE_INVALID) return;
	xent_setproto(gallery_ctx(g), n, XENT_PROTOCOL_FLEX);
	xent_setflexdir(gallery_ctx(g), n, XENT_FLEX_COLUMN);
	xent_setitems(gallery_ctx(g), n, XENT_FLEX_ALIGN_STRETCH);
	xent_setgap(gallery_ctx(g), n, gap);
}

XentNodeId g_scroll(GalleryApp *g, XentNodeId parent) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_scroll_create(&sb, xtk_ref_existing(( XtkCtrl ) parent), NULL);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}

XentNodeId g_col(GalleryApp *g, XentNodeId parent, float gap, float pad_l, float pad_t, float pad_r, float pad_b) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_container_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent),
	  &(XtkContainerProps) {
		.set  = XTK_CONTAINER_GAP_MASK
	          | XTK_CONTAINER_PL_MASK
	          | XTK_CONTAINER_PT_MASK
	          | XTK_CONTAINER_PR_MASK
	          | XTK_CONTAINER_PB_MASK
	          | XTK_CONTAINER_FILL_MASK,
		.gap  = gap,
		.pl   = pad_l,
		.pt   = pad_t,
		.pr   = pad_r,
		.pb   = pad_b,
		.fill = true,
	  }
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	flex_col(g, n, gap);
	if (n != XENT_NODE_INVALID) {
		xent_setp(gallery_ctx(g), n, (XentInsets) {pad_l, pad_t, pad_r, pad_b});
		xent_setfit(gallery_ctx(g), n, false, true);
		xent_setshrink(gallery_ctx(g), n, 0.0f);
	}
	return n;
}

static XentNodeId
row_create(GalleryApp *g, XentNodeId parent, float gap, bool fill, bool wrap, XentFlexJustify justify) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_container_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent),
	  &(XtkContainerProps) {
		.set     = XTK_CONTAINER_DIR_MASK
	             | XTK_CONTAINER_ITEMS_MASK
	             | XTK_CONTAINER_JUSTIFY_MASK
	             | XTK_CONTAINER_GAP_MASK
	             | XTK_CONTAINER_FILL_MASK
	             | XTK_CONTAINER_WRAP_MASK,
		.dir     = XTK_STACK_ROW,
		.items   = XENT_FLEX_ALIGN_CENTER,
		.justify = justify,
		.gap     = gap,
		.fill    = fill,
		.wrap    = wrap ? XENT_FLEX_WRAP : XENT_FLEX_NO_WRAP,
	  }
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}

XentNodeId g_row(GalleryApp *g, XentNodeId parent, float gap) {
	return row_create(g, parent, gap, true, false, XENT_FLEX_JUSTIFY_START);
}

XentNodeId g_wraprow(GalleryApp *g, XentNodeId parent, float gap) {
	return row_create(g, parent, gap, true, true, XENT_FLEX_JUSTIFY_START);
}

XentNodeId g_card(GalleryApp *g, XentNodeId parent) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_card_create(&sb, xtk_ref_existing(( XtkCtrl ) parent), NULL);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	flex_col(g, n, 0.0f);
	if (n != XENT_NODE_INVALID) xent_setp(gallery_ctx(g), n, (XentInsets) {16, 16, 16, 16});
	return n;
}

XentNodeId g_text(GalleryApp *g, XentNodeId parent, char const *text, float size, uint32_t weight) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_text_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent),
	  &(XtkTextProps) {
		.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
		.text       = text,
		.fontsize   = size,
		.fontweight = weight,
	  }
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}

XentNodeId g_spacer(GalleryApp *g, XentNodeId parent) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_container_create(&sb, xtk_ref_existing(( XtkCtrl ) parent), NULL);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	if (n != XENT_NODE_INVALID) xent_setgrow(gallery_ctx(g), n, 1.0f);
	return n;
}

XentNodeId g_page_header(GalleryApp *g, XentNodeId parent, char const *title, char const *blurb) {
	XentNodeId col = g_col(g, parent, 8.0f, 0, 0, 0, 0);
	g_text(g, col, title, 28.0f, FLUX_FONT_SEMI_BOLD);
	g_text(g, col, blurb, 13.0f, FLUX_FONT_REGULAR);
	return col;
}

XentNodeId g_example(GalleryApp *g, XentNodeId parent, char const *caption) {
	XentNodeId col = g_col(g, parent, 8.0f, 0, 0, 0, 0);
	g_text(g, col, caption, 14.0f, FLUX_FONT_REGULAR);
	return col;
}

GalleryDemo g_demo_card(GalleryApp *g, XentNodeId parent) {
	XentNodeId card  = g_card(g, parent);
	XentNodeId row   = row_create(g, card, 16.0f, true, true, XENT_FLEX_JUSTIFY_SPACE_BETWEEN);
	XentNodeId items = row_create(g, row, 16.0f, false, true, XENT_FLEX_JUSTIFY_START);
	return (GalleryDemo) {.row = row, .items = items};
}

XentNodeId g_output(GalleryApp *g, XentNodeId parent, char const *value) {
	XentNodeId col = g_col(g, parent, 4.0f, 0, 0, 0, 0);
	xent_setsize(gallery_ctx(g), col, (XentSize) {260.0f, NAN});
	g_text(g, col, "Output:", 12.0f, FLUX_FONT_SEMI_BOLD);
	return g_text(g, col, value, 14.0f, FLUX_FONT_REGULAR);
}

void page_category(GalleryApp *g, XentNodeId parent, char const *title, char const *blurb) {
	g_page_header(g, parent, title, blurb);
	g_text(g, parent, "Expand this group in the navigation pane to pick a control.", 13.0f, FLUX_FONT_REGULAR);
}

/* ---- control create helpers (scene) ------------------------------------ */

XentNodeId
g_button(GalleryApp *g, XentNodeId parent, char const *label, char const *icon, uint32_t style, bool enabled) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_button_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent),
	  &(XtkButtonProps) {
		.set     = (label ? XTK_BUTTON_TEXT_MASK : 0u)
	             | (icon ? XTK_BUTTON_ICON_MASK : 0u)
	             | XTK_BUTTON_STYLE_MASK
	             | XTK_BUTTON_ENABLED_MASK,
		.text    = label,
		.icon    = icon,
		.style   = style,
		.enabled = enabled,
	  }
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}

XentNodeId g_checkbox(GalleryApp *g, XentNodeId parent, char const *label, bool checked, bool enabled) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_checkbox_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent),
	  &(XtkToggleProps) {
		.set     = XTK_TOGGLE_TEXT_MASK | XTK_TOGGLE_CHECKED_MASK | XTK_TOGGLE_ENABLED_MASK,
		.text    = label,
		.checked = checked,
		.enabled = enabled,
	  }
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}

XentNodeId g_radio(GalleryApp *g, XentNodeId parent, char const *label, bool checked) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_radio_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent),
	  &(XtkToggleProps) {.set = XTK_TOGGLE_TEXT_MASK | XTK_TOGGLE_CHECKED_MASK, .text = label, .checked = checked}
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}

XentNodeId g_switch(GalleryApp *g, XentNodeId parent, bool checked) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_switch_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent), &(XtkToggleProps) {.set = XTK_TOGGLE_CHECKED_MASK, .checked = checked}
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}

XentNodeId g_slider(GalleryApp *g, XentNodeId parent, float min, float max, float value, float width) {
	XtkTxn sb;
	if (!xtk_txn_begin(&g->scene, &sb)) return XENT_NODE_INVALID;
	xtk_slider_create(
	  &sb, xtk_ref_existing(( XtkCtrl ) parent),
	  &(XtkSliderProps) {
		.set = XTK_SLIDER_MIN_MASK | XTK_SLIDER_MAX_MASK | XTK_SLIDER_VALUE_MASK | (width > 0 ? XTK_SLIDER_W_MASK : 0u),
		.min = min,
		.max = max,
		.value = value,
		.w     = width,
	  }
	);
	XtkBatchResult *r = xtk_txn_commit(&sb, NULL);
	XentNodeId      n = map_first(r);
	if (r) xtk_batch_result_destroy(r);
	return n;
}
