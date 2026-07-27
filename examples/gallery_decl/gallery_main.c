#include "gallery.h"
#include <fluxent/flux_nav_trace.h>


#include <windows.h>

void gallery_decl_update(void *model, XtkMsg msg) {
	Model *m = ( Model * ) model;
	flux_nav_trace_adopt(msg.trace_id);
	flux_nav_trace_log("model.update", "msg=%d i=%d page.before=%d", msg.tag, msg.i, m->page);
	gallery_model_update(m, msg);
	flux_nav_trace_log("model.update", "msg=%d page.after=%d", msg.tag, m->page);
}

typedef XtkEl *(*GalleryPageFn)(XtkUi *ui, Model const *m);

static XtkEl  *page_cat_basic(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Basic input", "Buttons, checkboxes, radios, combos, sliders, and switches.");
}

static XtkEl *page_cat_text(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Text", "Text entry and display: TextBox, NumberBox, Typography.");
}

static XtkEl *page_cat_status(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Status & info", "InfoBar, InfoBadge, progress, and tooltips.");
}

static XtkEl *page_cat_media(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Media", "Images decoded through WIC.");
}

static XtkEl *page_cat_menus(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Menus & layout", "Menus, tabs, links, and layout containers.");
}

static XtkEl *page_cat_dialogs(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Dialogs & flyouts", "Modal dialogs and popup surfaces.");
}

static XtkEl *page_cat_shell(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Layout & shell", "SplitView and window scaffolding.");
}

static XtkEl *page_cat_collections(XtkUi *ui, Model const *m) {
	( void ) m;
	return page_category(ui, "Collections", "ListView, GridView, ListBox, TreeView, ItemsView, and PullToRefresh.");
}

static GalleryPageFn const k_page_fn [PAGE_COUNT] = {
  [PAGE_HOME]            = page_home,
  [PAGE_CAT_BASIC]       = page_cat_basic,
  [PAGE_CAT_TEXT]        = page_cat_text,
  [PAGE_CAT_STATUS]      = page_cat_status,
  [PAGE_CAT_MEDIA]       = page_cat_media,
  [PAGE_CAT_MENUS]       = page_cat_menus,
  [PAGE_CAT_DIALOGS]     = page_cat_dialogs,
  [PAGE_CAT_SHELL]       = page_cat_shell,
  [PAGE_CAT_COLLECTIONS] = page_cat_collections,
  [PAGE_BUTTON]          = page_button,
  [PAGE_SPLIT]           = page_split,
  [PAGE_CHECKBOX]        = page_checkbox,
  [PAGE_RADIO]           = page_radio,
  [PAGE_COMBO]           = page_combo,
  [PAGE_SLIDER]          = page_slider,
  [PAGE_TOGGLE]          = page_toggle,
  [PAGE_RADIO_BUTTONS]   = page_radio_buttons,
  [PAGE_RATING]          = page_rating,
  [PAGE_TOGGLE_SPLIT]    = page_toggle_split,
  [PAGE_TEXTBOX]         = page_textbox,
  [PAGE_NUMBERBOX]       = page_numberbox,
  [PAGE_TYPOGRAPHY]      = page_typography,
  [PAGE_INFOBAR]         = page_infobar,
  [PAGE_BADGE]           = page_badge,
  [PAGE_PROGRESS]        = page_progress,
  [PAGE_TOOLTIP]         = page_tooltip,
  [PAGE_IMAGE]           = page_image,
  [PAGE_PERSON_PICTURE]  = page_person_picture,
  [PAGE_PAGER]           = page_pager,
  [PAGE_SPLIT_VIEW]      = page_split_view,
  [PAGE_TITLE_BAR]       = page_title_bar,
  [PAGE_MENUS]           = page_menus,
  [PAGE_TABVIEW]         = page_tabview,
  [PAGE_EXPANDER]        = page_expander,
  [PAGE_BREADCRUMB]      = page_breadcrumb,
  [PAGE_SELECTOR_BAR]    = page_selector_bar,
  [PAGE_LISTVIEW]        = page_listview,
  [PAGE_GRIDVIEW]        = page_gridview,
  [PAGE_LISTBOX]         = page_listbox,
  [PAGE_TREE_VIEW]       = page_tree_view,
  [PAGE_ITEMS_VIEW]      = page_items_view,
  [PAGE_PULL_REFRESH]    = page_pull_refresh,
  [PAGE_FLIPVIEW]        = page_flipview,
  [PAGE_AUTOSUGGEST]     = page_autosuggest,
  [PAGE_DIALOG]          = page_dialog,
  [PAGE_TEACHING_TIP]    = page_teaching_tip,
  [PAGE_SETTINGS]        = page_settings,
};

static XtkEl *page_view(XtkUi *ui, Model const *m) {
	if (( unsigned ) m->page >= PAGE_COUNT) return page_home(ui, m);
	GalleryPageFn fn = k_page_fn [m->page];
	return fn ? fn(ui, m) : page_home(ui, m);
}

XtkEl *gallery_decl_page_view(XtkUi *ui, void *model) {
	Model *m = ( Model * ) model;
	return xtk_grow(
	  xtk_keyed(
		ui, xtk_fmt(ui, "page-%d", m->page),
		xtk_scroll(
		  ui, xtk_kids(xtk_column(
				ui,
				(XtkStackDesc) {
				  .padding = {36, 28, 36, 28},
                    .align = XENT_FLEX_ALIGN_STRETCH, .fill = true
    },
				xtk_kids(page_view(ui, m))
			  ))
		)
	  ),
	  1.0f
	);
}

XtkEl *gallery_decl_view(XtkUi *ui, void *model) {
	Model *m = ( Model * ) model;
	flux_nav_trace_log("view", "build page=%d", m->page);
	return xtk_grow(
	  xtk_nav(
		ui,
		(XtkNavDesc) {
		  .items            = kNavItems,
		  .nitem            = PAGE_COUNT,
		  .sel              = m->page,
		  .window_title_bar = true,
		  .app_title        = "FluXent Gallery",
		  .select           = xtk_msg(MSG_NAV),
		},
		xtk_kids(gallery_decl_page_view(ui, model))
	  ),
	  1.0f
	);
}

#ifndef GALLERY_DECL_NO_MAIN
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
	( void ) hInst;
	( void ) hPrev;
	( void ) cmdLine;
	( void ) showCmd;

	Model model;
	gallery_model_init(&model);

	FluxAppCfg cfg = {
	  .title     = L"FluXent Gallery (Declarative)",
	  .width     = 1100,
	  .height    = 760,
	  .dark_mode = flux_theme_systemdark(),
	  .backdrop  = FLUX_BACKDROP_MICA,
	};
	return flux_run(&cfg, &model, gallery_decl_update, gallery_decl_view);
}
#endif
