#include "wizard.h"

#include <math.h>

static char const *const kStepNames [PAGE_COUNT]
  = {"License", "Location", "Components", "Options", "Installing", "Finish"};

static char const *const kStepTitles [PAGE_COUNT] = {
  "License agreement",
  "Choose install location",
  "Select components",
  "Update options",
  "Installing",
  "Setup complete",
};

static char const *const kStepHints [PAGE_COUNT] = {
  "Read the terms below. You must accept them to continue.",
  "Setup will install Xent Sample into the folder below.",
  "Check the parts you want. A partly checked group installs only what is checked inside it.",
  "Choose how Xent Sample keeps itself up to date.",
  "Please wait while Xent Sample is installed.",
  "Xent Sample is ready to use.",
};

static XtkEl *steps_pane(XtkUi *ui, Model const *m) {
	XtkEl **kids = xtk_children_alloc(ui, PAGE_COUNT);
	if (!kids) return NULL;
	for (int i = 0; i < PAGE_COUNT; i++) {
		bool current = i == m->page;
		kids [i]     = xtk_text(
		  ui,
		  xtk_fmt(
			ui, "%s  %s",
			i < m->page ? "\xE2\x9C\x93"
			: current   ? "\xE2\x80\xBA"
			            : " ",
			kStepNames [i]
		  ),
		  (XtkTextDesc) {.size = 14, .weight = current ? XTK_FONT_SEMI_BOLD : XTK_FONT_REGULAR}
		);
	}
	return xtk_sized(
	  xtk_column(
		ui,
		(XtkStackDesc) {
		  .gap = 14, .padding = {24, 28, 12, 24}
    },
		kids
	  ),
	  168.0f, NAN
	);
}

static XtkEl *page_license(XtkUi *ui, Model const *m) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
	  xtk_kids(
		xtk_grow(
		  xtk_textbox(ui, NULL, (XtkTextBoxDesc) {.content = kLicense, .multiline = true, .readonly = true}), 1.0f
		),
		xtk_checkbox(
		  ui, "I accept the terms in the license agreement",
		  (XtkToggleDesc) {.checked = m->accepted, .change = xtk_msg(MSG_ACCEPT)}
		)
	  )
	);
}

static XtkEl *page_location(XtkUi *ui, Model const *m) {
	XtkRadioItem items [FOLDER_COUNT];
	for (int i = 0; i < FOLDER_COUNT; i++) items [i] = (XtkRadioItem) {.label = kFolders [i]};
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_STRETCH},
	  xtk_kids(
		xtk_text(ui, "Destination folder", (XtkTextDesc) {.size = 14, .weight = XTK_FONT_SEMI_BOLD}),
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_CENTER},
		  xtk_kids(
			xtk_grow(
			  xtk_textbox(ui, "Folder path", (XtkTextBoxDesc) {.content = m->dir, .change = xtk_msg(MSG_DIR)}), 1.0f
			),
			xtk_button(ui, "Browse\xE2\x80\xA6", (XtkButtonDesc) {.click = xtk_msg(MSG_BROWSE)})
		  )
		),
		xtk_text(
		  ui, xtk_fmt(ui, "At least %d MB of free disk space is required.", wizard_required_mb(m)),
		  (XtkTextDesc) {.size = 13}
		),
		xtk_dialog(
		  ui, "Choose a folder",
		  (XtkDialogDesc) {
			.open     = m->browse_open,
			.primary  = "Select folder",
			.close    = "Cancel",
			.w        = 440,
			.contenth = 176,
			.result   = xtk_msg(MSG_BROWSE_RESULT)},
		  xtk_kids(xtk_radiogroup(
			ui, (XtkRadioButtonsDesc) {
				  .header = "Locations",
				  .items  = items,
				  .nitem  = FOLDER_COUNT,
				  .sel    = m->browse_sel,
				  .select = xtk_msg(MSG_BROWSE_PICK)}
		  ))
		)
	  )
	);
}

static XtkEl *page_components(XtkUi *ui, Model const *m) {
	XtkTreeNodeDesc nodes [COMP_COUNT] = {0};
	XtkTreeNodeDesc roots [COMP_COUNT] = {0};
	int             nroot              = 0;
	for (int i = 0; i < COMP_COUNT; i++) {
		WizardComp const *c = &kComps [i];
		nodes [i]           = (XtkTreeNodeDesc) {
		  .text = c->mb ? xtk_fmt(ui, "%s (%d MB)", c->name, c->mb) : c->name, .checked = m->checked [i]};
		if (c->parent >= 0) nodes [c->parent].child_count++;
	}
	for (int i = 0; i < COMP_COUNT; i++) {
		if (kComps [i].parent >= 0) continue;
		nodes [i].expanded = m->expanded [i];
		nodes [i].children = nodes [i].child_count ? &nodes [i + 1] : NULL;
		roots [nroot++]    = nodes [i];
	}
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
	  xtk_kids(
		xtk_grow(
		  xtk_tree(
			ui,
			(XtkTreeDesc) {
			  .roots    = roots,
			  .nroot    = nroot,
			  .sel_mode = XTK_TREE_SELECT_MULTIPLE,
			  .sel      = -1,
			  .select   = xtk_msg(MSG_COMP_SELECT),
			  .expand   = xtk_msg(MSG_COMP_EXPAND),
			}
		  ),
		  1.0f
		),
		xtk_text(ui, xtk_fmt(ui, "Space required: %d MB", wizard_required_mb(m)), (XtkTextDesc) {.size = 13})
	  )
	);
}

static XtkEl *page_options(XtkUi *ui, Model const *m) {
	return xtk_row(
	  ui, (XtkStackDesc) {.gap = 40, .align = XENT_FLEX_ALIGN_START},
	  xtk_kids(
		xtk_column(
		  ui, (XtkStackDesc) {.gap = 20},
		  xtk_kids(
			xtk_text(ui, "Update channel", (XtkTextDesc) {.size = 14, .weight = XTK_FONT_SEMI_BOLD}),
			xtk_sized(
			  xtk_combo(
				ui, "Pick or type a channel",
				(XtkComboDesc) {
				  .items    = kChannels,
				  .nitem    = CHANNEL_COUNT,
				  .sel      = m->channel_sel,
				  .editable = true,
				  .content  = m->channel_sel < 0 ? m->channel : NULL,
				  .select   = xtk_msg(MSG_CHANNEL_SELECT),
				  .submit   = xtk_msg(MSG_CHANNEL_SUBMIT)}
			  ),
			  240.0f, NAN
			),
			xtk_text(
			  ui, xtk_fmt(ui, "Channel: %s", m->channel && m->channel [0] ? m->channel : "(none)"),
			  (XtkTextDesc) {.size = 13}
			),
			xtk_date_picker(
			  ui,
			  (XtkDatePickerDesc) {
				.header = "Check for updates starting", .date = m->update_date, .change = xtk_msg(MSG_UPDATE_DATE)}
			)
		  )
		),
		xtk_column(
		  ui, (XtkStackDesc) {.gap = 12, .align = XENT_FLEX_ALIGN_CENTER},
		  xtk_kids(
			xtk_text(ui, "Download cache", (XtkTextDesc) {.size = 14, .weight = XTK_FONT_SEMI_BOLD}),
			xtk_sized(
			  xtk_slider(
				ui,
				(XtkSliderDesc) {
				  .min          = 0,
				  .max          = 2048,
				  .value        = m->cache_mb,
				  .step         = 64,
				  .small_change = 64,
				  .large_change = 256,
				  .vertical     = true,
				  .change       = xtk_msg(MSG_CACHE)}
			  ),
			  NAN, 200.0f
			),
			xtk_text(ui, xtk_fmt(ui, "%.0f MB", m->cache_mb), (XtkTextDesc) {.size = 13})
		  )
		)
	  )
	);
}

static XtkEl *page_progress(XtkUi *ui, Model const *m) {
	float            value   = m->file_count ? 100.0f * ( float ) m->file_done / ( float ) m->file_count : 0.0f;
	char const      *current = m->installing && m->file_done > 0 && m->file_done <= m->file_count
	                           ? xtk_fmt(ui, "Copying %s", m->files [m->file_done - 1])
	                         : m->finished  ? "All files were copied."
	                         : m->cancelled ? "Setup was cancelled."
	                                        : "Preparing\xE2\x80\xA6";
	XtkProgressState state   = m->cancelled ? XTK_PROGRESS_ERROR
	                         : m->exit_open ? XTK_PROGRESS_PAUSED
	                                        : XTK_PROGRESS_NORMAL;
	return xtk_column(
	  ui,
	  (XtkStackDesc) {
		.gap = 12, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true
    },
	  xtk_kids(
		xtk_text(
		  ui, current, (XtkTextDesc) {.size = 13, .max_lines = 1, .trim = XTK_TRIM_CHAR_ELLIPSIS, .nowrap = true}
		),
		xtk_progress(ui, (XtkProgressDesc) {.value = value, .max = 100, .state = state}),
		xtk_text(ui, xtk_fmt(ui, "%d of %d files", m->file_done, m->file_count), (XtkTextDesc) {.size = 13}),
		xtk_grow(
		  xtk_card(
			ui, (XtkStackDesc) {.padding = {4, 4, 4, 4}, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
			xtk_kids(xtk_grow(
			  xtk_scroll(
				ui, xtk_kids(xtk_column(
					  ui, (XtkStackDesc) {.padding = {8, 6, 8, 6}, .align = XENT_FLEX_ALIGN_STRETCH},
					  xtk_kids(xtk_text(ui, m->log ? m->log : "", (XtkTextDesc) {.size = 12, .selectable = true}))
					))
			  ),
			  1.0f
			))
		  ),
		  1.0f
		)
	  )
	);
}

static XtkEl *page_finish(XtkUi *ui, Model const *m) {
	char const *summary = xtk_fmt(
	  ui, "Installed to: %s\nUpdate channel: %s\nComponents: %d MB in %d files", m->dir,
	  m->channel ? m->channel : "Stable", wizard_required_mb(m), m->file_count
	);
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_START},
	  xtk_kids(
		xtk_text(ui, "Setup has finished installing Xent Sample on your computer.", (XtkTextDesc) {.size = 14}),
		xtk_text(ui, summary, (XtkTextDesc) {.size = 14, .selectable = true}),
		xtk_text(ui, "You can select the details above and copy them with Ctrl+C.", (XtkTextDesc) {.size = 12}),
		xtk_link(
		  ui, "Read the release notes", (XtkHyperlinkDesc) {.url = "https://example.com/xent-sample/release-notes"}
		)
	  )
	);
}

typedef XtkEl      *(*PageFn)(XtkUi *ui, Model const *m);

static PageFn const kPages [PAGE_COUNT] = {
  page_license,
  page_location,
  page_components,
  page_options,
  page_progress,
  page_finish,
};

static XtkEl *footer(XtkUi *ui, Model const *m) {
	bool        back   = m->page > PAGE_LICENSE && m->page < PAGE_PROGRESS;
	char const *next   = m->page == PAGE_OPTIONS ? "Install" : m->page == PAGE_FINISH ? "Finish" : "Next";
	bool        cancel = m->page != PAGE_FINISH;
	return xtk_row(
	  ui,
	  (XtkStackDesc) {
		.gap = 8, .padding = {24, 16, 24, 16},
             .justify = XENT_FLEX_JUSTIFY_END
    },
	  xtk_kids(
		xtk_sized(xtk_button(ui, "Back", (XtkButtonDesc) {.disabled = !back, .click = xtk_msg(MSG_BACK)}), 96.0f, NAN),
		xtk_sized(
		  xtk_button(
			ui, next,
			(XtkButtonDesc) {.style = XTK_BUTTON_ACCENT, .disabled = !wizard_can_next(m), .click = xtk_msg(MSG_NEXT)}
		  ),
		  96.0f, NAN
		),
		cancel ? xtk_sized(xtk_button(ui, "Cancel", (XtkButtonDesc) {.click = xtk_msg(MSG_CANCEL)}), 96.0f, NAN) : NULL
	  )
	);
}

XtkEl *wizard_view(XtkUi *ui, void *model) {
	Model const *m = ( Model const * ) model;
	return xtk_grow(
	  xtk_column(
		ui,
		(XtkStackDesc) {
		  .align = XENT_FLEX_ALIGN_STRETCH, .fill = true
    },
		xtk_kids(
		  xtk_grow(
			xtk_row(
			  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
			  xtk_kids(
				steps_pane(ui, m),
				xtk_grow(
				  xtk_column(
					ui,
					(XtkStackDesc) {
					  .gap = 6, .padding = {12, 24, 28, 8}, .align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
					xtk_kids(
					  xtk_text(ui, kStepTitles [m->page], (XtkTextDesc) {.size = 20, .weight = XTK_FONT_SEMI_BOLD}),
					  xtk_text(ui, kStepHints [m->page], (XtkTextDesc) {.size = 13}),
					  xtk_grow(
						xtk_margin(
						  xtk_column(
							ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH, .fill = true},
							xtk_kids(
							  xtk_grow(xtk_keyed(ui, xtk_fmt(ui, "page-%d", m->page), kPages [m->page](ui, m)), 1.0f)
							)
						  ),
						  (XentInsets) {0, 14, 0, 0}
						),
						1.0f
					  )
					)
				  ),
				  1.0f
				)
			  )
			),
			1.0f
		  ),
		  xtk_divider(ui), footer(ui, m),
		  xtk_dialog(
			ui, m->installing ? "Cancel installation?" : "Exit setup?",
			(XtkDialogDesc) {
			  .open     = m->exit_open,
			  .primary  = m->installing ? "Cancel installation" : "Exit setup",
			  .close    = m->installing ? "Keep installing" : "Continue",
			  .w        = 420,
			  .contenth = 48,
			  .result   = xtk_msg(MSG_EXIT_RESULT)},
			xtk_kids(xtk_text(
			  ui, m->installing ? "Files copied so far will be removed." : "Xent Sample has not been installed yet.",
			  (XtkTextDesc) {.size = 14}
			))
		  )
		)
	  ),
	  1.0f
	);
}
