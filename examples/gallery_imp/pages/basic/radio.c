#include "app.h"
#include <string.h>

static void              page_radio_sync(GalleryApp *g);

static char const *const kRadioOptions [] = {"Option 1", "Option 2", "Option 3"};

bool                     page_radio_build(PageBuild *b) {
	page_sync(b, page_radio_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "RadioButton", "RadioButtons present a set of mutually exclusive options — the model holds the index."
	);
	{
		XtkNodeRef ex   = page_example(b, parent, "A group of RadioButtons.");
		PageDemo   demo = page_demo(b, ex);
		XtkLocalId col  = xtk_container_create(
		  &b->txn, demo.items,
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
			.gap    = 8.0f,
			.fill   = true,
			.fitw   = false,
			.fith   = true,
		  }
		);
		for (int i = 0; i < 3; i++) {
			XtkLocalId radio = xtk_radio_create(
			  &b->txn, xtk_ref_local(col),
			  &(XtkToggleProps) {
				.set     = XTK_TOGGLE_TEXT_MASK | XTK_TOGGLE_CHECKED_MASK,
				.text    = kRadioOptions [i],
				.checked = m->radio == i,
			  }
			);
			xtk_radio_onchange(&b->txn, xtk_ref_local(radio), page_post, page_bind(b, MSG_RADIO_0 + i));
			page_keep(b, MSG_RADIO_0 + i, radio);
		}
		XtkLocalId output = page_output(b, demo.row, gallery_fmt(b->app, "You selected %s", kRadioOptions [m->radio]));
		page_keepout(b, MSG_RADIO_0, output);
	}
	return true;
}

static void page_radio_sync(GalleryApp *g) {
	for (int i = 0; i < 3; i++)
		xtk_radio_setchecked(
		  &g->scene, ( XtkCtrl ) g->live_control [MSG_RADIO_0 + i], i == g->model.radio, (XtkChangeStamp) {0}
		);
	char const *text = gallery_fmt(g, "You selected %s", kRadioOptions [g->model.radio]);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_RADIO_0], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}
