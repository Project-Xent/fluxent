#include "app.h"
#include <string.h>

static void slider_example(
  PageBuild *b, XtkNodeRef parent, char const *caption, int tag, float min, float max, float value, XtkSliderProps extra
) {
	XtkNodeRef ex      = page_example(b, parent, caption);
	PageDemo   demo    = page_demo(b, ex);
	extra.set         |= XTK_SLIDER_W_MASK | XTK_SLIDER_MIN_MASK | XTK_SLIDER_MAX_MASK | XTK_SLIDER_VALUE_MASK;
	extra.w            = 300.0f;
	extra.min          = min;
	extra.max          = max;
	extra.value        = value;
	XtkLocalId slider  = xtk_slider_create(&b->txn, demo.items, &extra);
	xtk_slider_onchange(&b->txn, xtk_ref_local(slider), page_post, page_bind(b, tag));
	XtkNodeRef output = page_output(b, demo.row, gallery_fmt(b->app, "%.0f", value));
	page_keepout(b, tag, output.u.local);
}

bool page_slider_build(PageBuild *b) {
	page_sync(b, page_slider_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "Slider", "Use a Slider when users should be able to set a value along a continuous or stepped range."
	);
	slider_example(b, parent, "A simple Slider.", MSG_SLIDER_BASIC, 0, 100, m->slider_basic, (XtkSliderProps) {0});
	slider_example(
	  b, parent, "A Slider with range and steps specified.", MSG_SLIDER_STEP, 500, 1000, m->slider_step,
	  (XtkSliderProps) {
		.set         = XTK_SLIDER_STEP_MASK | XTK_SLIDER_SMALLCHANGE_MASK | XTK_SLIDER_LARGECHANGE_MASK,
		.step        = 10.0f,
		.smallchange = 10.0f,
		.largechange = 100.0f,
	  }
	);
	slider_example(
	  b, parent, "A Slider with tick marks.", MSG_SLIDER_TICK, 0, 100, m->slider_tick,
	  (XtkSliderProps) {
		.set       = XTK_SLIDER_TICK_MASK | XTK_SLIDER_TICKPLACE_MASK | XTK_SLIDER_SNAPSTO_MASK,
		.tick      = 20.0f,
		.tickplace = XTK_TICK_OUTSIDE,
		.snapsto   = XTK_SLIDER_SNAP_TICKS,
	  }
	);
	return true;
}

static void slider_text(GalleryApp *g, int tag, float value) {
	char const *text = gallery_fmt(g, "%.0f", value);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [tag], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}

void page_slider_sync(GalleryApp *g) {
	slider_text(g, MSG_SLIDER_BASIC, g->model.slider_basic);
	slider_text(g, MSG_SLIDER_STEP, g->model.slider_step);
	slider_text(g, MSG_SLIDER_TICK, g->model.slider_tick);
}
