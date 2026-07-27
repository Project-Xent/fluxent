/**
 * @file app.h
 * @brief Imperative FluXent Gallery application context.
 */
#ifndef GALLERY_IMP_APP_H
#define GALLERY_IMP_APP_H

#include "gallery_model.h"

#include <fluxent/fluxent.h>
#include <fluxent/flux_host.h>
#include <fluxent/flux_node_store.h>
#include <xtk/xtk_controls.h>
#include <xtk/xtk_scene.h>

#include <stddef.h>

#include "bridge/flux_internal.h"
#include "render/flux_dl_replay.h"

typedef struct GalleryBind {
	struct GalleryApp *app;
	int                tag;
	int                i;
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

typedef struct GalleryApp GalleryApp;

typedef void              (*GalleryPageSyncFn)(GalleryApp *app);

typedef struct PageBuild {
	GalleryApp       *app;
	XtkTxn            txn;
	GalleryBindBlock *bind_blocks;
	GalleryBindBlock *bind_cursor;
	GalleryPageAlloc *allocs;
	GalleryPageSyncFn sync;
	XtkLocalId        root;
	XtkLocalId        content;
	XtkLocalId        controls [MSG_COUNT];
	XtkLocalId        outputs [MSG_COUNT];
	void             *data [MSG_COUNT];
	bool              active;
} PageBuild;

typedef struct PageDemo {
	XtkNodeRef row;
	XtkNodeRef items;
} PageDemo;

typedef struct GalleryDemo {
	XentNodeId row;
	XentNodeId items;
} GalleryDemo;

struct GalleryApp {
	FluxApp          *flux;
	FluxBackendCtx    bctx;
	XtkHost          *host;
	XtkBackend       *backend;
	XtkScene          scene;
	Model             model;
	XentNodeId        nav;
	XentNodeId        content_host;
	XentNodeId        page_root;
	GalleryBindBlock *bind_blocks;
	GalleryBindBlock *bind_cursor;
	GalleryPageAlloc *page_allocs;
	GalleryPending   *pending_head;
	GalleryPending   *pending_tail;
	GalleryPageSyncFn sync_page;
	int               bind_n;
	char              fmt_buf [256];
	int               rebuilding;
	XentNodeId        live_control [MSG_COUNT];
	XentNodeId        live_output [MSG_COUNT];
	void             *live_data [MSG_COUNT];
};

bool         gallery_app_create(GalleryApp *g);
void         gallery_app_destroy(GalleryApp *g);
int          gallery_app_run(GalleryApp *g);

void         gallery_dispatch(GalleryApp *g, XtkMsg msg);
void         gallery_flush(GalleryApp *g);
void         gallery_rebuild_page(GalleryApp *g);
void         gallery_page_sync(GalleryApp *g, GalleryPageSyncFn sync);

GalleryBind *gallery_bind(GalleryApp *g, int tag);
GalleryBind *gallery_bind_i(GalleryApp *g, int tag, int i);
int          gallery_bind_tag(GalleryApp const *g, int index);
void        *gallery_page_alloc(GalleryApp *g, size_t size);
char const  *gallery_fmt(GalleryApp *g, char const *fmt, ...);

XentCtx     *gallery_ctx(GalleryApp *g);
FluxStore   *gallery_store(GalleryApp *g);
XtkScene    *gallery_scene(GalleryApp *g);

bool         page_begin(PageBuild *b, GalleryApp *g);
bool         page_commit(PageBuild *b);
void         page_abort(PageBuild *b);
GalleryBind *page_bind(PageBuild *b, int tag);
GalleryBind *page_bind_i(PageBuild *b, int tag, int i);
void        *page_alloc(PageBuild *b, size_t size);
uint32_t     page_post(void *arg, XtkEvent const *event);
void         page_sync(PageBuild *b, GalleryPageSyncFn sync);
void         page_keep(PageBuild *b, int tag, XtkLocalId local);
void         page_keepout(PageBuild *b, int tag, XtkLocalId local);
void         page_keepdata(PageBuild *b, int tag, void *data);
XtkNodeRef   page_header(PageBuild *b, XtkNodeRef parent, char const *title, char const *blurb);
XtkNodeRef   page_example(PageBuild *b, XtkNodeRef parent, char const *caption);
PageDemo     page_demo(PageBuild *b, XtkNodeRef parent);
XtkNodeRef   page_output(PageBuild *b, XtkNodeRef parent, char const *value);

/* Shell */
bool         gallery_shell_build(GalleryApp *g);

/* Layout helpers — create under @p parent (existing node). Return column/card/etc. node. */
XentNodeId   g_col(GalleryApp *g, XentNodeId parent, float gap, float pad_l, float pad_t, float pad_r, float pad_b);
XentNodeId   g_row(GalleryApp *g, XentNodeId parent, float gap);
XentNodeId   g_wraprow(GalleryApp *g, XentNodeId parent, float gap);
XentNodeId   g_card(GalleryApp *g, XentNodeId parent);
XentNodeId   g_scroll(GalleryApp *g, XentNodeId parent);
XentNodeId   g_text(GalleryApp *g, XentNodeId parent, char const *text, float size, uint32_t weight);
XentNodeId   g_spacer(GalleryApp *g, XentNodeId parent);
XentNodeId   g_page_header(GalleryApp *g, XentNodeId parent, char const *title, char const *blurb);
XentNodeId   g_example(GalleryApp *g, XentNodeId parent, char const *caption);
GalleryDemo  g_demo_card(GalleryApp *g, XentNodeId parent);
XentNodeId   g_output(GalleryApp *g, XentNodeId parent, char const *value);
XentNodeId
g_button(GalleryApp *g, XentNodeId parent, char const *label, char const *icon, uint32_t style, bool enabled);
XentNodeId g_checkbox(GalleryApp *g, XentNodeId parent, char const *label, bool checked, bool enabled);
XentNodeId g_radio(GalleryApp *g, XentNodeId parent, char const *label, bool checked);
XentNodeId g_switch(GalleryApp *g, XentNodeId parent, bool checked);
XentNodeId g_slider(GalleryApp *g, XentNodeId parent, float min, float max, float value, float width);

void       g_on_invoke(GalleryApp *g, XentNodeId node, int tag);
void       g_on_invoke_i(GalleryApp *g, XentNodeId node, int tag, int i);
void       g_on_change_bool(GalleryApp *g, XentNodeId node, int tag);
void       g_on_change_f32(GalleryApp *g, XentNodeId node, int tag);
void       g_on_change_f64(GalleryApp *g, XentNodeId node, int tag);
void       g_on_change_string(GalleryApp *g, XentNodeId node, int tag);
void       g_on_select(GalleryApp *g, XentNodeId node, int tag);
void       g_on_close(GalleryApp *g, XentNodeId node, int tag);

/* Pages — build into @p parent column. */
bool       page_home_build(PageBuild *b);
void       page_category(GalleryApp *g, XentNodeId parent, char const *title, char const *blurb);
bool       page_button_build(PageBuild *b);
void       page_split(GalleryApp *g, XentNodeId parent);
bool       page_checkbox_build(PageBuild *b);
bool       page_radio_build(PageBuild *b);
void       page_combo(GalleryApp *g, XentNodeId parent);
bool       page_slider_build(PageBuild *b);
bool       page_toggle_build(PageBuild *b);
void       page_radio_buttons(GalleryApp *g, XentNodeId parent);
bool       page_rating_build(PageBuild *b);
void       page_toggle_split(GalleryApp *g, XentNodeId parent);
bool       page_textbox_build(PageBuild *b);
bool       page_numberbox_build(PageBuild *b);
bool       page_typography_build(PageBuild *b);
void       page_autosuggest(GalleryApp *g, XentNodeId parent);
void       page_combo_sync(GalleryApp *g);
void       page_button_sync(GalleryApp *g);
void       page_split_sync(GalleryApp *g);
void       page_checkbox_sync(GalleryApp *g);
void       page_radio_sync(GalleryApp *g);
void       page_slider_sync(GalleryApp *g);
void       page_toggle_sync(GalleryApp *g);
void       page_radio_buttons_sync(GalleryApp *g);
void       page_rating_sync(GalleryApp *g);
void       page_toggle_split_sync(GalleryApp *g);
void       page_infobar_sync(GalleryApp *g);
void       page_badge_sync(GalleryApp *g);
void       page_progress_sync(GalleryApp *g);
void       page_person_picture_sync(GalleryApp *g);
void       page_flipview_sync(GalleryApp *g);
void       page_pager_sync(GalleryApp *g);
void       page_menus_sync(GalleryApp *g);
void       page_expander_sync(GalleryApp *g);
void       page_breadcrumb_sync(GalleryApp *g);
void       page_selector_bar_sync(GalleryApp *g);
void       page_textbox_sync(GalleryApp *g);
void       page_numberbox_sync(GalleryApp *g);
void       page_autosuggest_sync(GalleryApp *g);
void       page_listview_sync(GalleryApp *g);
void       page_gridview_sync(GalleryApp *g);
void       page_listbox_sync(GalleryApp *g);
void       page_tree_view_sync(GalleryApp *g);
void       page_items_view_sync(GalleryApp *g);
void       page_pull_refresh_sync(GalleryApp *g);
void       page_split_view_sync(GalleryApp *g);
void       page_title_bar_sync(GalleryApp *g);
void       page_dialog_sync(GalleryApp *g);
void       page_teaching_tip_sync(GalleryApp *g);
void       page_infobar(GalleryApp *g, XentNodeId parent);
bool       page_badge_build(PageBuild *b);
bool       page_progress_build(PageBuild *b);
bool       page_tooltip_build(PageBuild *b);
void       page_image(GalleryApp *g, XentNodeId parent);
void       page_person_picture(GalleryApp *g, XentNodeId parent);
void       page_flipview(GalleryApp *g, XentNodeId parent);
void       page_pager(GalleryApp *g, XentNodeId parent);
void       page_menus(GalleryApp *g, XentNodeId parent);
void       page_tabview(GalleryApp *g, XentNodeId parent);
void       page_tabview_sync(GalleryApp *g);
void       page_expander(GalleryApp *g, XentNodeId parent);
void       page_breadcrumb(GalleryApp *g, XentNodeId parent);
void       page_selector_bar(GalleryApp *g, XentNodeId parent);
void       page_listview(GalleryApp *g, XentNodeId parent);
void       page_gridview(GalleryApp *g, XentNodeId parent);
void       page_listbox(GalleryApp *g, XentNodeId parent);
void       page_tree_view(GalleryApp *g, XentNodeId parent);
void       page_items_view(GalleryApp *g, XentNodeId parent);
void       page_pull_refresh(GalleryApp *g, XentNodeId parent);
void       page_split_view(GalleryApp *g, XentNodeId parent);
void       page_title_bar(GalleryApp *g, XentNodeId parent);
void       page_dialog(GalleryApp *g, XentNodeId parent);
void       page_teaching_tip(GalleryApp *g, XentNodeId parent);
void       page_settings(GalleryApp *g, XentNodeId parent);

#endif
