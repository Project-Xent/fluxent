#include "app.h"

static char const *const kMonths [] = {
  "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November",
  "December"};

static char const *const kSpeeds [] = {"Slowest", "Slow", "Normal", "Fast", "Fastest"};

#define MONTH_COUNT ( int ) (sizeof(kMonths) / sizeof(kMonths [0]))
#define SPEED_COUNT ( int ) (sizeof(kSpeeds) / sizeof(kSpeeds [0]))

static void looping_sync(GalleryApp *g) {
	gallery_settext(g, g->live_output [MSG_LOOPING], kMonths [g->model.looping_sel % MONTH_COUNT]);
}

static XtkLocalId wheel(PageBuild *b, XtkNodeRef parent, char const *const *items, int count, int sel, bool loop) {
	XtkLocalId local = xtk_looping_create(
	  &b->txn, parent,
	  &(XtkLoopingProps) {
		.set   = XTK_LOOPING_W_MASK
	           | XTK_LOOPING_H_MASK
	           | XTK_LOOPING_NITEM_MASK
	           | XTK_LOOPING_SEL_MASK
	           | XTK_LOOPING_LOOP_MASK,
		.w     = 140.0f,
		.h     = 200.0f,
		.nitem = count,
		.sel   = sel,
		.loop  = loop,
	  }
	);
	xtk_txn_setitems(&b->txn, xtk_ref_local(local), items, count);
	return local;
}

bool page_looping_build(PageBuild *b) {
	page_sync(b, looping_sync);
	int        sel    = b->app->model.looping_sel;
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "LoopingSelector",
	  "A LoopingSelector is the wheel the date and time pickers spin. It snaps to whole items, so whichever item "
	  "comes to rest in the middle is the selection."
	);
	{
		PageDemo demo = page_demo(
		  b, page_example(b, parent, "A looping wheel. Spinning past either end carries on from the other.")
		);
		XtkLocalId w = wheel(b, demo.items, kMonths, MONTH_COUNT, sel, true);
		xtk_looping_onselect(&b->txn, xtk_ref_local(w), page_post, page_bind(b, MSG_LOOPING));
		page_keepout(b, MSG_LOOPING, page_output(b, demo.row, kMonths [sel % MONTH_COUNT]));
	}
	{
		PageDemo demo
		  = page_demo(b, page_example(b, parent, "Without looping the wheel stops at the first and last item."));
		wheel(b, demo.items, kSpeeds, SPEED_COUNT, 2, false);
	}
	{
		PageDemo demo = page_demo(b, page_example(b, parent, "A disabled LoopingSelector."));
		ui_enable(&b->txn, wheel(b, demo.items, kMonths, MONTH_COUNT, 0, true), false);
	}
	return true;
}
