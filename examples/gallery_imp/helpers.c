#include "app.h"

#include <math.h>

XtkLocalId ui_text(XtkTxn *txn, XtkNodeRef parent, char const *text, float size, uint32_t weight) {
	return xtk_text_create(
	  txn, parent,
	  &(XtkTextProps) {
		.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
		.text       = text,
		.fontsize   = size,
		.fontweight = weight,
	  }
	);
}

XtkLocalId ui_column(XtkTxn *txn, XtkNodeRef parent, float gap, uint32_t align) {
	return xtk_container_create(
	  txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_SHRINK_MASK
	           | XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_FITH_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = align,
		.gap   = gap,
		.fith  = true,
	  }
	);
}

XtkLocalId ui_row(XtkTxn *txn, XtkNodeRef parent, float gap, bool wrap) {
	return xtk_container_create(
	  txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK | XTK_CONTAINER_ITEMS_MASK | XTK_CONTAINER_GAP_MASK | XTK_CONTAINER_WRAP_MASK,
		.dir   = XTK_STACK_ROW,
		.items = XENT_FLEX_ALIGN_CENTER,
		.gap   = gap,
		.wrap  = wrap ? XENT_FLEX_WRAP : XENT_FLEX_NO_WRAP,
	  }
	);
}

XtkLocalId ui_card(XtkTxn *txn, XtkNodeRef parent, float gap, uint32_t align, float pad) {
	return xtk_card_create(
	  txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = align,
		.gap   = gap,
		.pl    = pad,
		.pt    = pad,
		.pr    = pad,
		.pb    = pad,
	  }
	);
}

XtkLocalId ui_button(XtkTxn *txn, XtkNodeRef parent, char const *text, char const *icon, uint32_t style) {
	return xtk_button_create(
	  txn, parent,
	  &(XtkButtonProps) {
		.set   = (text ? XTK_BUTTON_TEXT_MASK : 0u) | (icon ? XTK_BUTTON_ICON_MASK : 0u) | XTK_BUTTON_STYLE_MASK,
		.text  = text,
		.icon  = icon,
		.style = style,
	  }
	);
}

void ui_size(XtkTxn *txn, XtkLocalId local, float w, float h) {
	XtkNodeRef ref = xtk_ref_local(local);
	if (w == w)
		xtk_txn_set(txn, ref, XTK_PROP_WIDTH, (XtkValue) {.kind = XTK_VALUE_F32, .u.f32 = w}, (XtkChangeStamp) {0});
	if (h == h)
		xtk_txn_set(txn, ref, XTK_PROP_HEIGHT, (XtkValue) {.kind = XTK_VALUE_F32, .u.f32 = h}, (XtkChangeStamp) {0});
}

void ui_enable(XtkTxn *txn, XtkLocalId local, bool enabled) {
	xtk_txn_set(
	  txn, xtk_ref_local(local), XTK_PROP_ENABLED, (XtkValue) {.kind = XTK_VALUE_BOOL, .u.b = enabled},
	  (XtkChangeStamp) {0}
	);
}

void ui_center(XtkTxn *txn, XtkLocalId local) {
	xtk_txn_set(
	  txn, xtk_ref_local(local), XTK_PROP_JUSTIFY_CONTENT,
	  (XtkValue) {.kind = XTK_VALUE_ENUM, .u.u32 = XENT_FLEX_JUSTIFY_CENTER}, (XtkChangeStamp) {0}
	);
}

XtkNodeRef page_header(PageBuild *b, XtkNodeRef parent, char const *title, char const *blurb) {
	XtkNodeRef col = xtk_ref_local(ui_column(&b->txn, parent, 8.0f, XENT_FLEX_ALIGN_STRETCH));
	ui_text(&b->txn, col, title, 28.0f, FLUX_FONT_SEMI_BOLD);
	ui_text(&b->txn, col, blurb, 13.0f, FLUX_FONT_REGULAR);
	return col;
}

XtkNodeRef page_example(PageBuild *b, XtkNodeRef parent, char const *caption) {
	XtkNodeRef col = xtk_ref_local(ui_column(&b->txn, parent, 8.0f, XENT_FLEX_ALIGN_STRETCH));
	ui_text(&b->txn, col, caption, 14.0f, FLUX_FONT_REGULAR);
	return col;
}

static XtkLocalId demo_row(XtkTxn *txn, XtkNodeRef parent, bool fill, XentFlexJustify justify) {
	return xtk_container_create(
	  txn, parent,
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
}

PageDemo page_demo(PageBuild *b, XtkNodeRef parent) {
	XtkNodeRef card  = xtk_ref_local(ui_card(&b->txn, parent, 0.0f, XENT_FLEX_ALIGN_STRETCH, 16.0f));
	XtkNodeRef row   = xtk_ref_local(demo_row(&b->txn, card, true, XENT_FLEX_JUSTIFY_SPACE_BETWEEN));
	XtkNodeRef items = xtk_ref_local(demo_row(&b->txn, row, false, XENT_FLEX_JUSTIFY_START));
	return (PageDemo) {.row = row, .items = items};
}

XtkLocalId page_output(PageBuild *b, XtkNodeRef parent, char const *value) {
	XtkLocalId col = ui_column(&b->txn, parent, 4.0f, XENT_FLEX_ALIGN_STRETCH);
	ui_size(&b->txn, col, 260.0f, NAN);
	ui_text(&b->txn, xtk_ref_local(col), "Output:", 12.0f, FLUX_FONT_SEMI_BOLD);
	return ui_text(&b->txn, xtk_ref_local(col), value, 14.0f, FLUX_FONT_REGULAR);
}

XtkLocalId page_button(PageBuild *b, XtkNodeRef parent, char const *text, int tag, int i) {
	XtkLocalId button = ui_button(&b->txn, parent, text, NULL, XTK_BUTTON_STANDARD);
	xtk_button_oninvoke(&b->txn, xtk_ref_local(button), page_post, page_bind_i(b, tag, i));
	return button;
}

XtkLocalId page_checkbox(PageBuild *b, XtkNodeRef parent, char const *text, bool checked, int tag) {
	XtkLocalId box = xtk_checkbox_create(
	  &b->txn, parent,
	  &(XtkCheckBoxProps) {.set = XTK_CHECKBOX_TEXT_MASK | XTK_CHECKBOX_CHECKED_MASK, .text = text, .checked = checked}
	);
	xtk_checkbox_onchange(&b->txn, xtk_ref_local(box), page_post, page_bind(b, tag));
	return box;
}

XtkLocalId page_note(PageBuild *b, XtkNodeRef parent, char const *text) {
	return ui_text(&b->txn, parent, text, 13.0f, FLUX_FONT_REGULAR);
}

static struct {
	char const *title;
	char const *blurb;
} const k_categories [PAGE_COUNT] = {
  [PAGE_CAT_BASIC]       = {"Basic input",       "Buttons, checkboxes, radios, combos, sliders, and switches."         },
  [PAGE_CAT_TEXT]        = {"Text",              "Text entry and display: TextBox, NumberBox, Typography."             },
  [PAGE_CAT_STATUS]      = {"Status & info",     "InfoBar, InfoBadge, progress, and tooltips."                         },
  [PAGE_CAT_MEDIA]       = {"Media",             "Images decoded through WIC."                                         },
  [PAGE_CAT_MENUS]       = {"Menus & layout",    "Menus, tabs, links, and layout containers."                          },
  [PAGE_CAT_DIALOGS]     = {"Dialogs & flyouts", "Modal dialogs and popup surfaces."                                   },
  [PAGE_CAT_SHELL]       = {"Layout & shell",    "SplitView and window scaffolding."                                   },
  [PAGE_CAT_COLLECTIONS] = {"Collections",       "ListView, GridView, ListBox, TreeView, ItemsView, and PullToRefresh."},
};

bool page_category_build(PageBuild *b) {
	XtkNodeRef parent = xtk_ref_local(b->content);
	int        page   = b->app->model.page;
	page_header(b, parent, k_categories [page].title, k_categories [page].blurb);
	page_note(b, parent, "Expand this group in the navigation pane to pick a control.");
	return true;
}
