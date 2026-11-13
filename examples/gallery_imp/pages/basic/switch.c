#include "app.h"
#include <string.h>

static void page_toggle_sync(GalleryApp *g);

bool        page_toggle_build(PageBuild *b) {
	page_sync(b, page_toggle_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "ToggleSwitch", "Use a ToggleSwitch for binary on/off settings that take effect immediately."
	);
	{
		XtkNodeRef ex    = page_example(b, parent, "A simple ToggleSwitch.");
		PageDemo   demo  = page_demo(b, ex);
		XtkLocalId inner = xtk_container_create(
		  &b->txn, demo.items,
		  &(XtkContainerProps) {
			.set = XTK_CONTAINER_DIR_MASK | XTK_CONTAINER_ITEMS_MASK | XTK_CONTAINER_GAP_MASK | XTK_CONTAINER_FILL_MASK,
			.dir = XTK_STACK_ROW,
			.items = XENT_FLEX_ALIGN_CENTER,
			.gap   = 12.0f,
			.fill  = true,
		  }
		);
		XtkLocalId sw = xtk_switch_create(
		  &b->txn, xtk_ref_local(inner), &(XtkToggleProps) {.set = XTK_TOGGLE_CHECKED_MASK, .checked = m->light_on}
		);
		xtk_switch_onchange(&b->txn, xtk_ref_local(sw), page_post, page_bind(b, MSG_LIGHT));
		XtkLocalId state = xtk_text_create(
		  &b->txn, xtk_ref_local(inner),
		  &(XtkTextProps) {
			.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
			.text       = m->light_on ? "On" : "Off",
			.fontsize   = 14.0f,
			.fontweight = FLUX_FONT_REGULAR,
		  }
		);
		page_keep(b, MSG_LIGHT, state);
		XtkLocalId output = page_output(b, demo.row, m->light_on ? "The lights are on." : "The lights are off.");
		page_keepout(b, MSG_LIGHT, output);
	}
	return true;
}

static void page_toggle_sync(GalleryApp *g) {
	char const *state = g->model.light_on ? "On" : "Off";
	char const *out   = g->model.light_on ? "The lights are on." : "The lights are off.";
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_control [MSG_LIGHT], state, ( uint32_t ) strlen(state), (XtkChangeStamp) {0}
	);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_LIGHT], out, ( uint32_t ) strlen(out), (XtkChangeStamp) {0}
	);
}
