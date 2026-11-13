#include "app.h"
#include <string.h>

static void        page_checkbox_sync(GalleryApp *g);

static char const *check_state_text(int state) {
	if (state == XTK_CHECK_CHECKED) return "Checked";
	return state == XTK_CHECK_INDETERMINATE ? "Indeterminate" : "Unchecked";
}

bool page_checkbox_build(PageBuild *b) {
	page_sync(b, page_checkbox_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(b, parent, "CheckBox", "CheckBox controls let the user select a combination of binary options.");
	{
		XtkNodeRef ex   = page_example(b, parent, "A 2-state CheckBox.");
		PageDemo   demo = page_demo(b, ex);
		XtkLocalId cb   = xtk_checkbox_create(
		  &b->txn, demo.items,
		  &(XtkCheckBoxProps) {
			.set     = XTK_CHECKBOX_TEXT_MASK | XTK_CHECKBOX_CHECKED_MASK | XTK_CHECKBOX_ENABLED_MASK,
			.text    = "Two-state CheckBox",
			.checked = m->check_a,
			.enabled = true,
		  }
		);
		xtk_checkbox_onchange(&b->txn, xtk_ref_local(cb), page_post, page_bind(b, MSG_CHECK_A));
		XtkLocalId output = page_output(b, demo.row, m->check_a ? "Checked" : "Unchecked");
		page_keepout(b, MSG_CHECK_A, output);
	}
	{
		XtkNodeRef ex
		  = page_example(b, parent, "A 3-state CheckBox: a click cycles unchecked, checked, indeterminate.");
		PageDemo   demo = page_demo(b, ex);
		XtkLocalId cb   = xtk_checkbox_create(
		  &b->txn, demo.items,
		  &(XtkCheckBoxProps) {
			.set           = XTK_CHECKBOX_TEXT_MASK
		                   | XTK_CHECKBOX_CHECKED_MASK
		                   | XTK_CHECKBOX_INDETERMINATE_MASK
		                   | XTK_CHECKBOX_THREESTATE_MASK
		                   | XTK_CHECKBOX_ENABLED_MASK,
			.text          = "Three-state CheckBox",
			.checked       = m->check_state == XTK_CHECK_CHECKED,
			.indeterminate = m->check_state == XTK_CHECK_INDETERMINATE,
			.threestate    = true,
			.enabled       = true,
		  }
		);
		xtk_checkbox_onstate(&b->txn, xtk_ref_local(cb), page_post, page_bind(b, MSG_CHECK_TRI));
		XtkLocalId output = page_output(b, demo.row, check_state_text(m->check_state));
		page_keepout(b, MSG_CHECK_TRI, output);
	}
	{
		XtkNodeRef ex   = page_example(b, parent, "CheckBoxes can be disabled.");
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
		XtkLocalId cb = xtk_checkbox_create(
		  &b->txn, xtk_ref_local(card),
		  &(XtkCheckBoxProps) {
			.set     = XTK_CHECKBOX_TEXT_MASK | XTK_CHECKBOX_CHECKED_MASK | XTK_CHECKBOX_ENABLED_MASK,
			.text    = "Enabled option",
			.checked = m->check_b,
			.enabled = true,
		  }
		);
		xtk_checkbox_onchange(&b->txn, xtk_ref_local(cb), page_post, page_bind(b, MSG_CHECK_B));
		xtk_checkbox_create(
		  &b->txn, xtk_ref_local(card),
		  &(XtkCheckBoxProps) {
			.set     = XTK_CHECKBOX_TEXT_MASK | XTK_CHECKBOX_CHECKED_MASK | XTK_CHECKBOX_ENABLED_MASK,
			.text    = "Disabled option",
			.checked = false,
			.enabled = false,
		  }
		);
		xtk_checkbox_create(
		  &b->txn, xtk_ref_local(card),
		  &(XtkCheckBoxProps) {
			.set     = XTK_CHECKBOX_TEXT_MASK | XTK_CHECKBOX_CHECKED_MASK | XTK_CHECKBOX_ENABLED_MASK,
			.text    = "Disabled and checked",
			.checked = true,
			.enabled = false,
		  }
		);
	}
	return true;
}

static void page_checkbox_sync(GalleryApp *g) {
	char const *tri = check_state_text(g->model.check_state);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_CHECK_TRI], tri, ( uint32_t ) strlen(tri), (XtkChangeStamp) {0}
	);
	char const *text = g->model.check_a ? "Checked" : "Unchecked";
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_CHECK_A], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}
