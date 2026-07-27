#include "app.h"
#include <string.h>

static XtkNodeRef button_row(PageBuild *b, XtkNodeRef parent) {
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
	return xtk_ref_local(row);
}

static void button_add(PageBuild *b, XtkNodeRef parent, char const *text, char const *icon, uint32_t style) {
	xtk_button_create(
	  &b->txn, parent,
	  &(XtkButtonProps) {
		.set     = (text ? XTK_BUTTON_TEXT_MASK : 0u)
	             | (icon ? XTK_BUTTON_ICON_MASK : 0u)
	             | XTK_BUTTON_STYLE_MASK
	             | XTK_BUTTON_ENABLED_MASK,
		.text    = text,
		.icon    = icon,
		.style   = style,
		.enabled = true,
	  }
	);
}

bool page_button_build(PageBuild *b) {
	page_sync(b, page_button_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "Button",
	  "The Button control provides a Click event to respond to user input. It posts an XtkMsg; nothing calls "
	  "back into your code."
	);
	{
		XtkNodeRef ex   = page_example(b, parent, "A simple Button with text content.");
		PageDemo   demo = page_demo(b, ex);
		XtkLocalId btn  = xtk_button_create(
		  &b->txn, demo.items,
		  &(XtkButtonProps) {
			.set     = XTK_BUTTON_TEXT_MASK | XTK_BUTTON_STYLE_MASK | XTK_BUTTON_ENABLED_MASK,
			.text    = "Standard button",
			.style   = XTK_BUTTON_STANDARD,
			.enabled = !m->button_disabled,
		  }
		);
		xtk_button_oninvoke(&b->txn, xtk_ref_local(btn), page_post, page_bind(b, MSG_CLICK));
		XtkLocalId dis = xtk_checkbox_create(
		  &b->txn, demo.items,
		  &(XtkToggleProps) {
			.set     = XTK_TOGGLE_TEXT_MASK | XTK_TOGGLE_CHECKED_MASK | XTK_TOGGLE_ENABLED_MASK,
			.text    = "Disable button",
			.checked = m->button_disabled,
			.enabled = true,
		  }
		);
		xtk_checkbox_onchange(&b->txn, xtk_ref_local(dis), page_post, page_bind(b, MSG_BUTTON_DISABLE));
		page_keep(b, MSG_BUTTON_DISABLE, btn);
		XtkNodeRef output
		  = page_output(b, demo.row, gallery_fmt(b->app, "Clicked %d time%s", m->clicks, m->clicks == 1 ? "" : "s"));
		page_keepout(b, MSG_CLICK, output.u.local);
	}
	{
		XtkNodeRef ex  = page_example(b, parent, "Built-in styles applied to Button.");
		XtkNodeRef row = button_row(b, ex);
		button_add(b, row, "Accent style button", NULL, XTK_BUTTON_ACCENT);
		button_add(b, row, "Standard style button", NULL, XTK_BUTTON_STANDARD);
		button_add(b, row, "Subtle style button", NULL, XTK_BUTTON_SUBTLE);
		button_add(b, row, "Text style button", NULL, XTK_BUTTON_TEXT);
	}
	{
		XtkNodeRef ex  = page_example(b, parent, "A Button with graphical content.");
		XtkNodeRef row = button_row(b, ex);
		button_add(b, row, "Save", "Save", XTK_BUTTON_STANDARD);
		button_add(b, row, "Share", "Share", XTK_BUTTON_STANDARD);
		button_add(b, row, NULL, "Mail", XTK_BUTTON_STANDARD);
	}
	return true;
}

void page_button_sync(GalleryApp *g) {
	xtk_button_setenabled(
	  &g->scene, ( XtkCtrl ) g->live_control [MSG_BUTTON_DISABLE], !g->model.button_disabled, (XtkChangeStamp) {0}
	);
	char const *text = gallery_fmt(g, "Clicked %d time%s", g->model.clicks, g->model.clicks == 1 ? "" : "s");
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_CLICK], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}
