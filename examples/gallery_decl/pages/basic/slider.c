#include "gallery.h"

XtkEl *page_slider(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "Slider", "Use a Slider when users should be able to set a value along a continuous or stepped range."
		),
		example(
		  ui, "A simple Slider.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(ui, (XtkSliderDesc) {.value = m->slider_basic, .change = xtk_msg(MSG_SLIDER_BASIC)}), 300, NAN
			),
			xtk_fmt(ui, "%.0f", m->slider_basic), NULL
		  )
		),
		example(
		  ui, "A Slider with range and steps specified.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(
				ui,
				(XtkSliderDesc) {
				  .min          = 500,
				  .max          = 1000,
				  .value        = m->slider_step,
				  .step         = 10,
				  .small_change = 10,
				  .large_change = 100,
				  .change       = xtk_msg(MSG_SLIDER_STEP)}
			  ),
			  300, NAN
			),
			xtk_fmt(ui, "%.0f", m->slider_step), NULL
		  )
		),
		example(
		  ui, "A Slider with tick marks.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(
				ui,
				(XtkSliderDesc) {
				  .value          = m->slider_tick,
				  .tick           = 20,
				  .tick_placement = XTK_TICK_OUTSIDE,
				  .snap_to_ticks  = true,
				  .change         = xtk_msg(MSG_SLIDER_TICK)}
			  ),
			  300, NAN
			),
			xtk_fmt(ui, "%.0f", m->slider_tick), NULL
		  )
		),
		example(
		  ui, "A vertical Slider: arrow keys move it, the minimum sits at the bottom.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(
				ui,
				(XtkSliderDesc) {
				  .value          = m->slider_vert,
				  .vertical       = true,
				  .tick           = 20,
				  .tick_placement = XTK_TICK_OUTSIDE,
				  .change         = xtk_msg(MSG_SLIDER_VERT)}
			  ),
			  NAN, 180
			),
			xtk_fmt(ui, "%.0f", m->slider_vert), NULL
		  )
		),
		example(
		  ui, "A range Slider: two thumbs that cannot cross, each reachable with Tab.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_slider(
				ui,
				(XtkSliderDesc) {
				  .range          = true,
				  .range_start    = m->slider_range_lo,
				  .range_end      = m->slider_range_hi,
				  .tick           = 10,
				  .tick_placement = XTK_TICK_OUTSIDE,
				  .change_range   = xtk_msg(MSG_SLIDER_RANGE)}
			  ),
			  300, NAN
			),
			xtk_fmt(ui, "%.0f - %.0f", m->slider_range_lo, m->slider_range_hi), NULL
		  )
		)
	  )
	);
}
