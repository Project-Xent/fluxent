#include "gallery.h"

static char const *const kMonths [] = {
  "January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November",
  "December"};

static char const *const kSpeeds [] = {"Slowest", "Slow", "Normal", "Fast", "Fastest"};

XtkEl                   *page_looping(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		page_header(
		  ui, "LoopingSelector",
		  "A LoopingSelector is the wheel the date and time pickers spin. It snaps to whole items, so whichever "
		  "item comes to rest in the middle is the selection."
		),
		example(
		  ui, "A looping wheel. Spinning past either end carries on from the other.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_looping_selector(
				ui,
				(XtkLoopingDesc) {
				  .items  = kMonths,
				  .nitem  = ( int ) (sizeof(kMonths) / sizeof(kMonths [0])),
				  .sel    = m->looping_sel,
				  .loop   = true,
				  .select = xtk_msg(MSG_LOOPING)}
			  ),
			  140, 200
			),
			kMonths [m->looping_sel % ( int ) (sizeof(kMonths) / sizeof(kMonths [0]))], NULL
		  )
		),
		example(
		  ui, "Without looping the wheel stops at the first and last item.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_looping_selector(
				ui,
				(XtkLoopingDesc) {.items = kSpeeds, .nitem = ( int ) (sizeof(kSpeeds) / sizeof(kSpeeds [0])), .sel = 2}
			  ),
			  140, 200
			),
			NULL, NULL
		  )
		),
		example(
		  ui, "A disabled LoopingSelector.",
		  demo_card(
			ui,
			xtk_sized(
			  xtk_looping_selector(
				ui,
				(XtkLoopingDesc) {
				  .items    = kMonths,
				  .nitem    = ( int ) (sizeof(kMonths) / sizeof(kMonths [0])),
				  .sel      = 0,
				  .loop     = true,
				  .disabled = true}
			  ),
			  140, 200
			),
			NULL, NULL
		  )
		)
	  )
	);
}
