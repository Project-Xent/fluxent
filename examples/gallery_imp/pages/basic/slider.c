#include "app.h"
#include <string.h>

static void page_slider_sync(GalleryApp *g);

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
	XtkLocalId output = page_output(b, demo.row, gallery_fmt(b->app, "%.0f", value));
	page_keepout(b, tag, output);
}

static void slider_vertical_example(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex   = page_example(b, parent, "A vertical Slider: arrow keys move it, the minimum sits at the bottom.");
	PageDemo   demo = page_demo(b, ex);
	XtkLocalId slider = xtk_slider_create(
	  &b->txn, demo.items,
	  &(XtkSliderProps) {
		.set       = XTK_SLIDER_H_MASK
	               | XTK_SLIDER_DIR_MASK
	               | XTK_SLIDER_MIN_MASK
	               | XTK_SLIDER_MAX_MASK
	               | XTK_SLIDER_VALUE_MASK
	               | XTK_SLIDER_TICK_MASK
	               | XTK_SLIDER_TICKPLACE_MASK,
		.h         = 180.0f,
		.dir       = XTK_STACK_COLUMN,
		.min       = 0.0f,
		.max       = 100.0f,
		.value     = m->slider_vert,
		.tick      = 20.0f,
		.tickplace = XTK_TICK_OUTSIDE,
	  }
	);
	xtk_slider_onchange(&b->txn, xtk_ref_local(slider), page_post, page_bind(b, MSG_SLIDER_VERT));
	XtkLocalId output = page_output(b, demo.row, gallery_fmt(b->app, "%.0f", m->slider_vert));
	page_keepout(b, MSG_SLIDER_VERT, output);
}

static void slider_range_example(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex   = page_example(b, parent, "A range Slider: two thumbs that cannot cross, each reachable with Tab.");
	PageDemo   demo = page_demo(b, ex);
	XtkLocalId slider = xtk_slider_create(
	  &b->txn, demo.items,
	  &(XtkSliderProps) {
		.set        = XTK_SLIDER_W_MASK
	                | XTK_SLIDER_MIN_MASK
	                | XTK_SLIDER_MAX_MASK
	                | XTK_SLIDER_RANGE_MASK
	                | XTK_SLIDER_RANGESTART_MASK
	                | XTK_SLIDER_RANGEEND_MASK
	                | XTK_SLIDER_TICK_MASK
	                | XTK_SLIDER_TICKPLACE_MASK,
		.w          = 300.0f,
		.min        = 0.0f,
		.max        = 100.0f,
		.range      = true,
		.rangestart = m->slider_range_lo,
		.rangeend   = m->slider_range_hi,
		.tick       = 10.0f,
		.tickplace  = XTK_TICK_OUTSIDE,
	  }
	);
	xtk_slider_onrange(&b->txn, xtk_ref_local(slider), page_post, page_bind(b, MSG_SLIDER_RANGE));
	XtkLocalId output
	  = page_output(b, demo.row, gallery_fmt(b->app, "%.0f - %.0f", m->slider_range_lo, m->slider_range_hi));
	page_keepout(b, MSG_SLIDER_RANGE, output);
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
	slider_vertical_example(b, parent, m);
	slider_range_example(b, parent, m);
	return true;
}

static void slider_text(GalleryApp *g, int tag, float value) {
	char const *text = gallery_fmt(g, "%.0f", value);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [tag], text, ( uint32_t ) strlen(text), (XtkChangeStamp) {0}
	);
}

static void page_slider_sync(GalleryApp *g) {
	slider_text(g, MSG_SLIDER_BASIC, g->model.slider_basic);
	slider_text(g, MSG_SLIDER_STEP, g->model.slider_step);
	slider_text(g, MSG_SLIDER_TICK, g->model.slider_tick);
	slider_text(g, MSG_SLIDER_VERT, g->model.slider_vert);
	char const *range = gallery_fmt(g, "%.0f - %.0f", g->model.slider_range_lo, g->model.slider_range_hi);
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_SLIDER_RANGE], range, ( uint32_t ) strlen(range), (XtkChangeStamp) {0}
	);
}
