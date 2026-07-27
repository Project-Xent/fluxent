#include "app.h"

bool page_tooltip_build(PageBuild *b) {
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "ToolTip", "Attach a tooltip to any element with xtk_tooltip; it shows after a short hover."
	);
	XtkNodeRef ex   = page_example(b, parent, "A Button with a ToolTip.");
	XtkLocalId card = xtk_card_create(
	  &b->txn, ex,
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
	XtkLocalId row = xtk_container_create(
	  &b->txn, xtk_ref_local(card),
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_FILL_MASK
	           | XTK_CONTAINER_WRAP_MASK,
		.dir   = XTK_STACK_ROW,
		.items = XENT_FLEX_ALIGN_CENTER,
		.gap   = 8.0f,
		.fill  = true,
		.wrap  = XENT_FLEX_WRAP,
	  }
	);
	xtk_button_create(
	  &b->txn, xtk_ref_local(row),
	  &(XtkButtonProps) {
		.set     = XTK_BUTTON_TEXT_MASK | XTK_BUTTON_TIP_MASK | XTK_BUTTON_ENABLED_MASK,
		.text    = "Hover me",
		.tip     = "A simple ToolTip on a Button",
		.enabled = true,
	  }
	);
	xtk_button_create(
	  &b->txn, xtk_ref_local(row),
	  &(XtkButtonProps) {
		.set     = XTK_BUTTON_ICON_MASK | XTK_BUTTON_TIP_MASK | XTK_BUTTON_ENABLED_MASK,
		.icon    = "Save",
		.tip     = "Icon-only buttons especially need one",
		.enabled = true,
	  }
	);
	return true;
}
