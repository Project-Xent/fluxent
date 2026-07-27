#include "gallery.h"

XtkEl *page_rating(XtkUi *ui, Model const *m) {
	char const *out
	  = m->rating_value >= 0.0 ? xtk_fmt(ui, "Value: %.0f", m->rating_value) : "Unset (-1). Click a star to rate.";
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "RatingControl",
		  "A star strip: hovering previews the prospective rating under the pointer, "
		  "a click commits, and re-clicking the current rating clears it back to "
		  "unset (-1)."
		),
		example(
		  ui, "An interactive RatingControl with a caption.",
		  demo_card(
			ui,
			xtk_rating(
			  ui,
			  (XtkRatingDesc) {
				.value            = m->rating_value,
				.is_clear_enabled = true,
				.caption          = "Your rating",
				.change           = xtk_msg(MSG_RATING),
			  }
			),
			out, NULL
		  )
		),
		example(
		  ui, "Read-only, displaying a fractional average as a partial fill.",
		  demo_card(
			ui, xtk_rating(ui, (XtkRatingDesc) {.value = 3.5, .is_read_only = true, .caption = "154 ratings"}),
			"Value: 3.5", NULL
		  )
		)
	  )
	);
}
