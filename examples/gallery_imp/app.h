#ifndef GALLERY_IMP_APP_H
#define GALLERY_IMP_APP_H

#include "gallery_model.h"

#include <fluxent/fluxent.h>
#include <fluxent/flux_host.h>
#include <xtk/xtk_controls.h>
#include <xtk/xtk_scene.h>

#include <stddef.h>

#define PAGE_EXTRA          4
#define PAGE_LISTS          4
#define GALLERY_LIST_WINDOW 256

typedef struct GalleryApp GalleryApp;

typedef struct GalleryBind {
	GalleryApp *app;
	int         tag;
	int         i;
} GalleryBind;

typedef struct GalleryBindBlock {
	struct GalleryBindBlock *next;
	int                      used;
	GalleryBind              items [64];
} GalleryBindBlock;

typedef struct GalleryPageAlloc {
	struct GalleryPageAlloc *next;
	uintptr_t                align;
	unsigned char            data [];
} GalleryPageAlloc;

typedef struct GalleryPending {
	struct GalleryPending *next;
	XtkMsg                 msg;
	char                  *owned;
} GalleryPending;

typedef void (*PageFillFn)(GalleryApp *g, XtkTxn *txn, XtkNodeRef item, int index);

typedef struct GalleryList {
	GalleryApp *app;
	XtkLocalId  local;
	PageFillFn  fill;
	XtkCtrl     items [GALLERY_LIST_WINDOW];
	int         count;
} GalleryList;

typedef void (*PageSyncFn)(GalleryApp *g);

typedef struct PageBuild {
	GalleryApp       *app;
	XtkTxn            txn;
	GalleryBindBlock *bind_blocks;
	GalleryBindBlock *bind_cursor;
	GalleryPageAlloc *allocs;
	PageSyncFn        sync;
	XtkLocalId        root;
	XtkLocalId        content;
	XtkLocalId        controls [MSG_COUNT];
	XtkLocalId        outputs [MSG_COUNT];
	XtkLocalId        extra [PAGE_EXTRA];
	GalleryList      *lists [PAGE_LISTS];
	int               nlist;
	bool              active;
} PageBuild;

typedef struct PageDemo {
	XtkNodeRef row;
	XtkNodeRef items;
} PageDemo;

struct GalleryApp {
	FluxApp          *flux;
	XtkHost          *host;
	XtkScene          scene;
	Model             model;
	GalleryBind       nav_bind;
	XtkCtrl           nav;
	XtkCtrl           page_root;
	GalleryBindBlock *bind_blocks;
	GalleryPageAlloc *page_allocs;
	GalleryPending   *pending_head;
	GalleryPending   *pending_tail;
	PageSyncFn        sync_page;
	char              fmt_buf [256];
	bool              rebuilding;
	bool              rebuild_request;
	XtkCtrl           live_control [MSG_COUNT];
	XtkCtrl           live_output [MSG_COUNT];
	XtkCtrl           live_extra [PAGE_EXTRA];
};

bool         gallery_app_create(GalleryApp *g);
void         gallery_app_destroy(GalleryApp *g);
int          gallery_app_run(GalleryApp *g);
char const  *gallery_fmt(GalleryApp *g, char const *fmt, ...);
void         gallery_settext(GalleryApp *g, XtkCtrl text, char const *value);

GalleryBind *page_bind(PageBuild *b, int tag);
GalleryBind *page_bind_i(PageBuild *b, int tag, int i);
void        *page_alloc(PageBuild *b, size_t size);
uint32_t     page_post(void *arg, XtkEvent const *event);
void         page_on(PageBuild *b, XtkLocalId local, XtkEventId event, int tag, int i);
void         page_sync(PageBuild *b, PageSyncFn sync);
void         page_keep(PageBuild *b, int tag, XtkLocalId local);
void         page_keepout(PageBuild *b, int tag, XtkLocalId local);
void         page_keepx(PageBuild *b, int slot, XtkLocalId local);
void         page_list(PageBuild *b, XtkLocalId list, PageFillFn fill);

XtkLocalId   ui_text(XtkTxn *txn, XtkNodeRef parent, char const *text, float size, uint32_t weight);
XtkLocalId   ui_column(XtkTxn *txn, XtkNodeRef parent, float gap, uint32_t align);
XtkLocalId   ui_row(XtkTxn *txn, XtkNodeRef parent, float gap, bool wrap);
XtkLocalId   ui_card(XtkTxn *txn, XtkNodeRef parent, float gap, uint32_t align, float pad);
XtkLocalId   ui_button(XtkTxn *txn, XtkNodeRef parent, char const *text, char const *icon, uint32_t style);
void         ui_size(XtkTxn *txn, XtkLocalId local, float w, float h);
void         ui_enable(XtkTxn *txn, XtkLocalId local, bool enabled);
void         ui_center(XtkTxn *txn, XtkLocalId local);

XtkNodeRef   page_header(PageBuild *b, XtkNodeRef parent, char const *title, char const *blurb);
XtkNodeRef   page_example(PageBuild *b, XtkNodeRef parent, char const *caption);
PageDemo     page_demo(PageBuild *b, XtkNodeRef parent);
XtkLocalId   page_output(PageBuild *b, XtkNodeRef parent, char const *value);
XtkLocalId   page_button(PageBuild *b, XtkNodeRef parent, char const *text, int tag, int i);
XtkLocalId   page_checkbox(PageBuild *b, XtkNodeRef parent, char const *text, bool checked, int tag);
XtkLocalId   page_note(PageBuild *b, XtkNodeRef parent, char const *text);

bool         page_category_build(PageBuild *b);
bool         page_home_build(PageBuild *b);
bool         page_button_build(PageBuild *b);
bool         page_split_build(PageBuild *b);
bool         page_checkbox_build(PageBuild *b);
bool         page_radio_build(PageBuild *b);
bool         page_combo_build(PageBuild *b);
bool         page_slider_build(PageBuild *b);
bool         page_scroll_bar_build(PageBuild *b);
bool         page_looping_build(PageBuild *b);
bool         page_date_picker_build(PageBuild *b);
bool         page_time_picker_build(PageBuild *b);
bool         page_calendar_build(PageBuild *b);
bool         page_color_picker_build(PageBuild *b);
bool         page_toggle_build(PageBuild *b);
bool         page_radio_buttons_build(PageBuild *b);
bool         page_rating_build(PageBuild *b);
bool         page_toggle_split_build(PageBuild *b);
bool         page_textbox_build(PageBuild *b);
bool         page_autosuggest_build(PageBuild *b);
bool         page_numberbox_build(PageBuild *b);
bool         page_typography_build(PageBuild *b);
bool         page_infobar_build(PageBuild *b);
bool         page_badge_build(PageBuild *b);
bool         page_progress_build(PageBuild *b);
bool         page_tooltip_build(PageBuild *b);
bool         page_image_build(PageBuild *b);
bool         page_person_picture_build(PageBuild *b);
bool         page_flipview_build(PageBuild *b);
bool         page_pager_build(PageBuild *b);
bool         page_menus_build(PageBuild *b);
bool         page_tabview_build(PageBuild *b);
bool         page_expander_build(PageBuild *b);
bool         page_breadcrumb_build(PageBuild *b);
bool         page_selector_bar_build(PageBuild *b);
bool         page_listview_build(PageBuild *b);
bool         page_gridview_build(PageBuild *b);
bool         page_listbox_build(PageBuild *b);
bool         page_tree_view_build(PageBuild *b);
bool         page_items_view_build(PageBuild *b);
bool         page_pull_refresh_build(PageBuild *b);
bool         page_split_view_build(PageBuild *b);
bool         page_title_bar_build(PageBuild *b);
bool         page_dialog_build(PageBuild *b);
bool         page_teaching_tip_build(PageBuild *b);
bool         page_settings_build(PageBuild *b);

#endif
