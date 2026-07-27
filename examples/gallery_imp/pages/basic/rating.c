#include "app.h"

#include <string.h>

bool page_rating_build(PageBuild *b) {
	page_sync(b, page_rating_sync);
	Model const *m      = &b->app->model;
	XtkNodeRef   parent = xtk_ref_local(b->content);
	char const  *out    = m->rating_value >= 0.0 ? gallery_fmt(b->app, "Value: %.0f", m->rating_value)
	                                             : "Unset (-1). Click a star to rate.";

	page_header(
	  b, parent, "RatingControl",
	  "A star strip: hovering previews the prospective rating under the pointer, a click commits, "
	  "and re-clicking the current rating clears it back to unset (-1)."
	);
	{
		XtkNodeRef ex     = page_example(b, parent, "An interactive RatingControl with a caption.");
		PageDemo   demo   = page_demo(b, ex);
		XtkLocalId rating = xtk_rating_create(
		  &b->txn, demo.items,
		  &(XtkRatingProps) {
			.set   = XTK_RATING_VALUE_MASK
		           | XTK_RATING_MAX_MASK
		           | XTK_RATING_TEXT_MASK
		           | XTK_RATING_CLEAR_MASK
		           | XTK_RATING_READONLY_MASK,
			.value = m->rating_value,
			.max   = 5.0,
			.text  = "Your rating",
			.clear = true,
		  }
		);
		xtk_rating_onrating(&b->txn, xtk_ref_local(rating), page_post, page_bind(b, MSG_RATING));
		XtkNodeRef output = page_output(b, demo.row, out);
		page_keepout(b, MSG_RATING, output.u.local);
	}
	{
		XtkNodeRef ex   = page_example(b, parent, "Read-only, displaying a fractional average as a partial fill.");
		PageDemo   demo = page_demo(b, ex);
		xtk_rating_create(
		  &b->txn, demo.items,
		  &(XtkRatingProps) {
			.set      = XTK_RATING_VALUE_MASK | XTK_RATING_MAX_MASK | XTK_RATING_TEXT_MASK | XTK_RATING_READONLY_MASK,
			.value    = 3.5,
			.max      = 5.0,
			.text     = "154 ratings",
			.readonly = true,
		  }
		);
		page_output(b, demo.row, "Value: 3.5");
	}
	return true;
}

void page_rating_sync(GalleryApp *g) {
	char const *out = g->model.rating_value >= 0.0 ? gallery_fmt(g, "Value: %.0f", g->model.rating_value)
	                                               : "Unset (-1). Click a star to rate.";
	xtk_text_settext(
	  &g->scene, ( XtkCtrl ) g->live_output [MSG_RATING], out, ( uint32_t ) strlen(out), (XtkChangeStamp) {0}
	);
}
