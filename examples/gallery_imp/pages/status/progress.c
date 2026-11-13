#include "app.h"

#include <string.h>

static void       page_progress_sync(GalleryApp *g);

static XtkNodeRef progress_column(PageBuild *b, XtkNodeRef parent) {
	XtkLocalId column = xtk_container_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK | XTK_CONTAINER_ITEMS_MASK | XTK_CONTAINER_GAP_MASK | XTK_CONTAINER_FILL_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.gap   = 16.0f,
		.fill  = true,
	  }
	);
	return xtk_ref_local(column);
}

static XtkNodeRef progress_row(PageBuild *b, XtkNodeRef parent) {
	XtkLocalId row = xtk_container_create(
	  &b->txn, parent,
	  &(XtkContainerProps) {
		.set     = XTK_CONTAINER_DIR_MASK
	             | XTK_CONTAINER_ITEMS_MASK
	             | XTK_CONTAINER_JUSTIFY_MASK
	             | XTK_CONTAINER_GAP_MASK
	             | XTK_CONTAINER_FILL_MASK
	             | XTK_CONTAINER_WRAP_MASK,
		.dir     = XTK_STACK_ROW,
		.items   = XENT_FLEX_ALIGN_CENTER,
		.justify = XENT_FLEX_JUSTIFY_SPACE_BETWEEN,
		.gap     = 16.0f,
		.fill    = true,
		.wrap    = XENT_FLEX_WRAP,
	  }
	);
	return xtk_ref_local(row);
}

static void
progress_state_bar(PageBuild *b, XtkNodeRef parent, Model const *m, bool indeterminate, XtkProgressState state) {
	xtk_progress_create(
	  &b->txn, parent,
	  &(XtkProgressProps) {
		.set           = XTK_PROGRESS_W_MASK
	                   | XTK_PROGRESS_MIN_MASK
	                   | XTK_PROGRESS_MAX_MASK
	                   | XTK_PROGRESS_VALUE_MASK
	                   | XTK_PROGRESS_INDETERMINATE_MASK
	                   | XTK_PROGRESS_STATE_MASK,
		.w             = 400.0f,
		.min           = 0.0f,
		.max           = 100.0f,
		.value         = m->progress,
		.indeterminate = indeterminate,
		.state         = state,
	  }
	);
}

static void progress_state_example(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex     = page_example(b, parent, "ProgressBar can be paused or show an error, determinate or not.");
	PageDemo   demo   = page_demo(b, ex);
	XtkNodeRef column = progress_column(b, demo.items);
	progress_state_bar(b, column, m, false, XTK_PROGRESS_NORMAL);
	progress_state_bar(b, column, m, false, XTK_PROGRESS_PAUSED);
	progress_state_bar(b, column, m, false, XTK_PROGRESS_ERROR);
	progress_state_bar(b, column, m, true, XTK_PROGRESS_PAUSED);
	progress_state_bar(b, column, m, true, XTK_PROGRESS_ERROR);
}

bool page_progress_build(PageBuild *b) {
	page_sync(b, page_progress_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);

	page_header(
	  b, parent, "Progress", "ProgressBar and ProgressRing show determinate progress or indeterminate activity."
	);
	XtkNodeRef ex       = page_example(b, parent, "Bar and ring bound to the same slider.");
	PageDemo   demo     = page_demo(b, ex);
	XtkNodeRef controls = progress_column(b, demo.items);
	XtkNodeRef row      = progress_row(b, controls);
	XtkLocalId slider   = xtk_slider_create(
	  &b->txn, row,
	  &(XtkSliderProps) {
		.set     = XTK_SLIDER_W_MASK
	             | XTK_SLIDER_ENABLED_MASK
	             | XTK_SLIDER_MIN_MASK
	             | XTK_SLIDER_MAX_MASK
	             | XTK_SLIDER_VALUE_MASK,
		.w       = 300.0f,
		.enabled = true,
		.min     = 0.0f,
		.max     = 100.0f,
		.value   = m->progress,
	  }
	);
	xtk_slider_onchange(&b->txn, xtk_ref_local(slider), page_post, page_bind(b, MSG_PROGRESS));
	XtkLocalId value = xtk_text_create(
	  &b->txn, row,
	  &(XtkTextProps) {
		.set        = XTK_TEXT_TEXT_MASK | XTK_TEXT_FONTSIZE_MASK | XTK_TEXT_FONTWEIGHT_MASK,
		.text       = gallery_fmt(b->app, "%.0f%%", m->progress),
		.fontsize   = 14.0f,
		.fontweight = FLUX_FONT_REGULAR,
	  }
	);
	XtkLocalId indeterminate = xtk_checkbox_create(
	  &b->txn, row,
	  &(XtkCheckBoxProps) {
		.set     = XTK_CHECKBOX_TEXT_MASK | XTK_CHECKBOX_CHECKED_MASK | XTK_CHECKBOX_ENABLED_MASK,
		.text    = "Indeterminate",
		.checked = m->indeterminate,
		.enabled = true,
	  }
	);
	xtk_checkbox_onchange(&b->txn, xtk_ref_local(indeterminate), page_post, page_bind(b, MSG_INDETERMINATE));
	XtkProgressProps progress = {
	  .set = XTK_PROGRESS_MIN_MASK | XTK_PROGRESS_MAX_MASK | XTK_PROGRESS_VALUE_MASK | XTK_PROGRESS_INDETERMINATE_MASK,
	  .min = 0.0f,
	  .max = 100.0f,
	  .value         = m->progress,
	  .indeterminate = m->indeterminate,
	};
	XtkLocalId bar  = xtk_progress_create(&b->txn, controls, &progress);
	XtkLocalId ring = xtk_progressring_create(&b->txn, controls, &progress);
	page_keep(b, MSG_PROGRESS, bar);
	page_keep(b, MSG_INDETERMINATE, ring);
	page_keepout(b, MSG_PROGRESS, value);
	progress_state_example(b, parent, m);
	return true;
}

static void page_progress_sync(GalleryApp *g) {
	XtkChangeStamp change = {0};
	XtkCtrl        bar    = ( XtkCtrl ) g->live_control [MSG_PROGRESS];
	XtkCtrl        ring   = ( XtkCtrl ) g->live_control [MSG_INDETERMINATE];
	xtk_progress_setvalue(&g->scene, bar, g->model.progress, change);
	xtk_progress_setindeterminate(&g->scene, bar, g->model.indeterminate, change);
	xtk_progressring_setvalue(&g->scene, ring, g->model.progress, change);
	xtk_progressring_setindeterminate(&g->scene, ring, g->model.indeterminate, change);
	char const *text = gallery_fmt(g, "%.0f%%", g->model.progress);
	xtk_text_settext(&g->scene, ( XtkCtrl ) g->live_output [MSG_PROGRESS], text, ( uint32_t ) strlen(text), change);
}
