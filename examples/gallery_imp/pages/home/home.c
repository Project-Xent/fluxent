#include "app.h"

static void home_card(PageBuild *b, XtkNodeRef parent, char const *icon, char const *title, char const *body) {
	XtkLocalId card = xtk_card_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_GROW_MASK
	           | XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK,
		.grow  = 1.0f,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.gap   = 8.0f,
		.pl    = 16.0f,
		.pt    = 16.0f,
		.pr    = 16.0f,
		.pb    = 16.0f,
	  }
	);
	XtkNodeRef ref = xtk_ref_local(card);
	xtk_button_create(
	  &b->txn, ref,
	  &(XtkButtonProps) {
		.set     = XTK_BUTTON_ICON_MASK | XTK_BUTTON_STYLE_MASK | XTK_BUTTON_ENABLED_MASK,
		.icon    = icon,
		.style   = XTK_BUTTON_SUBTLE,
		.enabled = false,
	  }
	);
	xtk_text_create(
	  &b->txn, ref,
	  &(XtkTextProps) {
		.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
		.text       = title,
		.fontsize   = 16.0f,
		.fontweight = FLUX_FONT_SEMI_BOLD,
	  }
	);
	xtk_text_create(
	  &b->txn, ref,
	  &(XtkTextProps) {
		.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
		.text       = body,
		.fontsize   = 13.0f,
		.fontweight = FLUX_FONT_REGULAR,
	  }
	);
}

bool page_home_build(PageBuild *b) {
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "FluXent Gallery",
	  "WinUI 3-style controls in pure C — every page is a live view function over one shared model."
	);
	xtk_infobar_create(
	  &b->txn, parent,
	  &(XtkInfoBarProps) {
		.set = XTK_INFOBAR_TEXT_MASK | XTK_INFOBAR_MESSAGE_MASK | XTK_INFOBAR_SEVERITY_MASK | XTK_INFOBAR_CLOSABLE_MASK,
		.text     = "Welcome",
		.message  = "Pick a control in the navigation pane; everything you see is reconciled per frame.",
		.severity = XTK_INFOBAR_SUCCESS,
		.closable = false,
	  }
	);
	XtkLocalId row = xtk_container_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK | XTK_CONTAINER_ITEMS_MASK | XTK_CONTAINER_GAP_MASK | XTK_CONTAINER_FILL_MASK,
		.dir   = XTK_STACK_ROW,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.gap   = 12.0f,
		.fill  = true,
	  }
	);
	XtkNodeRef cards = xtk_ref_local(row);
	home_card(b, cards, "Send", "Elm loop", "Controls post messages; update() evolves the model; view() rebuilds.");
	home_card(b, cards, "Sync", "Keyed diff", "The reconciler diffs the element tree and patches retained controls.");
	home_card(b, cards, "Edit", "Plain C23", "Designated initializers + compound literals; zero is the default.");
	return true;
}
