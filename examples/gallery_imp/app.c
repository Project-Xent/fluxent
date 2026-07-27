#include "app.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

XentCtx                 *gallery_ctx(GalleryApp *g) { return g->bctx.ctx; }

FluxStore               *gallery_store(GalleryApp *g) { return g->bctx.store; }

XtkScene                *gallery_scene(GalleryApp *g) { return &g->scene; }

GalleryBind             *gallery_bind(GalleryApp *g, int tag) { return gallery_bind_i(g, tag, 0); }

static GalleryBindBlock *gallery_bind_block(GalleryApp *g) {
	GalleryBindBlock *block = g->bind_cursor;
	if (block && block->used < ( int ) (sizeof(block->items) / sizeof(block->items [0]))) return block;
	if (block && block->next) block = block->next;
	else {
		GalleryBindBlock *next = ( GalleryBindBlock * ) calloc(1, sizeof(*next));
		if (!next) return NULL;
		if (block) block->next = next;
		else g->bind_blocks = next;
		block = next;
	}
	g->bind_cursor = block;
	return block;
}

GalleryBind *gallery_bind_i(GalleryApp *g, int tag, int i) {
	GalleryBindBlock *block = gallery_bind_block(g);
	if (!block) return NULL;
	GalleryBind *b = &block->items [block->used++];
	g->bind_n++;
	b->app = g;
	b->tag = tag;
	b->i   = i;
	return b;
}

int gallery_bind_tag(GalleryApp const *g, int index) {
	for (GalleryBindBlock const *block = g->bind_blocks; block; block = block->next) {
		if (index < block->used) return block->items [index].tag;
		index -= block->used;
	}
	return MSG_NONE;
}

static void gallery_binds_reset(GalleryApp *g) {
	for (GalleryBindBlock *block = g->bind_blocks; block; block = block->next) block->used = 0;
	g->bind_cursor = g->bind_blocks;
	g->bind_n      = 0;
}

static void bind_blocks_destroy(GalleryBindBlock *block) {
	while (block) {
		GalleryBindBlock *next = block->next;
		free(block);
		block = next;
	}
}

static void gallery_binds_destroy(GalleryApp *g) {
	bind_blocks_destroy(g->bind_blocks);
	g->bind_blocks = NULL;
	g->bind_cursor = NULL;
	g->bind_n      = 0;
}

void *gallery_page_alloc(GalleryApp *g, size_t size) {
	if (size > SIZE_MAX - sizeof(GalleryPageAlloc)) return NULL;
	GalleryPageAlloc *alloc = ( GalleryPageAlloc * ) malloc(sizeof(*alloc) + size);
	if (!alloc) return NULL;
	alloc->next    = g->page_allocs;
	g->page_allocs = alloc;
	return alloc->data;
}

static void page_allocs_destroy(GalleryPageAlloc *alloc) {
	while (alloc) {
		GalleryPageAlloc *next = alloc->next;
		free(alloc);
		alloc = next;
	}
}

static void gallery_page_allocs_destroy(GalleryApp *g) {
	page_allocs_destroy(g->page_allocs);
	g->page_allocs = NULL;
}

static GalleryBindBlock *page_bind_block(PageBuild *b) {
	GalleryBindBlock *block = b->bind_cursor;
	if (block && block->used < ( int ) (sizeof(block->items) / sizeof(block->items [0]))) return block;
	if (block && block->next) block = block->next;
	else {
		GalleryBindBlock *next = ( GalleryBindBlock * ) calloc(1, sizeof(*next));
		if (!next) return NULL;
		if (block) block->next = next;
		else b->bind_blocks = next;
		block = next;
	}
	b->bind_cursor = block;
	return block;
}

GalleryBind *page_bind_i(PageBuild *b, int tag, int i) {
	GalleryBindBlock *block = page_bind_block(b);
	if (!block) return NULL;
	GalleryBind *bind = &block->items [block->used++];
	bind->app         = b->app;
	bind->tag         = tag;
	bind->i           = i;
	return bind;
}

GalleryBind *page_bind(PageBuild *b, int tag) { return page_bind_i(b, tag, 0); }

void        *page_alloc(PageBuild *b, size_t size) {
	if (size > SIZE_MAX - sizeof(GalleryPageAlloc)) return NULL;
	GalleryPageAlloc *alloc = ( GalleryPageAlloc * ) malloc(sizeof(*alloc) + size);
	if (!alloc) return NULL;
	alloc->next = b->allocs;
	b->allocs   = alloc;
	return alloc->data;
}

char const *gallery_fmt(GalleryApp *g, char const *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(g->fmt_buf, sizeof(g->fmt_buf), fmt, ap);
	va_end(ap);
	return g->fmt_buf;
}

static XtkCtrl page_control(XtkBatchResult const *result, XtkLocalId local) {
	if (!result || local == XTK_LOCAL_INVALID) return XTK_CTRL_INVALID;
	for (uint32_t i = 0; i < result->control_count; i++)
		if (result->controls [i].local == local) return result->controls [i].control;
	return XTK_CTRL_INVALID;
}

static int page_bind_count(GalleryBindBlock const *block) {
	int count = 0;
	for (; block; block = block->next) count += block->used;
	return count;
}

void page_abort(PageBuild *b) {
	if (!b) return;
	if (b->active) xtk_txn_abort(&b->txn);
	bind_blocks_destroy(b->bind_blocks);
	page_allocs_destroy(b->allocs);
	memset(b, 0, sizeof(*b));
}

bool page_begin(PageBuild *b, GalleryApp *g) {
	if (!b || !g || g->content_host == XENT_NODE_INVALID) return false;
	memset(b, 0, sizeof(*b));
	b->app     = g;
	b->root    = XTK_LOCAL_INVALID;
	b->content = XTK_LOCAL_INVALID;
	for (int i = 0; i < MSG_COUNT; i++) b->controls [i] = b->outputs [i] = XTK_LOCAL_INVALID;
	if (!xtk_txn_begin(&g->scene, &b->txn)) return false;
	b->active = true;
	if (g->page_root != XENT_NODE_INVALID && !xtk_txn_drop(&b->txn, xtk_ref_existing(( XtkCtrl ) g->page_root))) {
		page_abort(b);
		return false;
	}
	b->root = xtk_scroll_create(&b->txn, xtk_ref_existing(( XtkCtrl ) g->content_host), NULL);
	if (b->root == XTK_LOCAL_INVALID) {
		page_abort(b);
		return false;
	}
	b->content = xtk_container_create(
	  &b->txn, xtk_ref_local(b->root),
	  &(XtkContainerProps) {
		.set    = XTK_CONTAINER_SHRINK_MASK
	            | XTK_CONTAINER_DIR_MASK
	            | XTK_CONTAINER_ITEMS_MASK
	            | XTK_CONTAINER_GAP_MASK
	            | XTK_CONTAINER_PL_MASK
	            | XTK_CONTAINER_PT_MASK
	            | XTK_CONTAINER_PR_MASK
	            | XTK_CONTAINER_PB_MASK
	            | XTK_CONTAINER_FILL_MASK
	            | XTK_CONTAINER_FITW_MASK
	            | XTK_CONTAINER_FITH_MASK,
		.shrink = 0.0f,
		.dir    = XTK_STACK_COLUMN,
		.items  = XENT_FLEX_ALIGN_STRETCH,
		.gap    = 20.0f,
		.pl     = 36.0f,
		.pt     = 28.0f,
		.pr     = 36.0f,
		.pb     = 28.0f,
		.fill   = true,
		.fitw   = false,
		.fith   = true,
	  }
	);
	if (b->content != XTK_LOCAL_INVALID) return true;
	page_abort(b);
	return false;
}

bool page_commit(PageBuild *b) {
	if (!b || !b->active) return false;
	XtkBatchDiag    diagnostic = {0};
	XtkBatchResult *result     = xtk_txn_commit(&b->txn, &diagnostic);
	b->active                  = false;
	if (!result) {
		fprintf(
		  stderr, "gallery page batch rejected: error=%d op=%u\n", ( int ) diagnostic.error, diagnostic.failed_op
		);
		page_abort(b);
		return false;
	}

	GalleryApp *g = b->app;
	gallery_page_allocs_destroy(g);
	gallery_binds_destroy(g);
	g->page_root   = ( XentNodeId ) page_control(result, b->root);
	g->bind_blocks = b->bind_blocks;
	g->bind_cursor = b->bind_cursor;
	g->bind_n      = page_bind_count(b->bind_blocks);
	g->page_allocs = b->allocs;
	g->sync_page   = b->sync;
	for (int i = 0; i < MSG_COUNT; i++) {
		g->live_control [i] = ( XentNodeId ) page_control(result, b->controls [i]);
		g->live_output [i]  = ( XentNodeId ) page_control(result, b->outputs [i]);
		g->live_data [i]    = b->data [i];
	}
	b->bind_blocks = NULL;
	b->bind_cursor = NULL;
	b->allocs      = NULL;
	xtk_batch_result_destroy(result);
	memset(b, 0, sizeof(*b));
	return g->page_root != XENT_NODE_INVALID;
}

void page_sync(PageBuild *b, GalleryPageSyncFn sync) {
	if (b) b->sync = sync;
}

void page_keep(PageBuild *b, int tag, XtkLocalId local) {
	if (b && tag > MSG_NONE && tag < MSG_COUNT) b->controls [tag] = local;
}

void page_keepout(PageBuild *b, int tag, XtkLocalId local) {
	if (b && tag > MSG_NONE && tag < MSG_COUNT) b->outputs [tag] = local;
}

void page_keepdata(PageBuild *b, int tag, void *data) {
	if (b && tag > MSG_NONE && tag < MSG_COUNT) b->data [tag] = data;
}

uint32_t page_post(void *arg, XtkEvent const *event) {
	GalleryBind *bind = ( GalleryBind * ) arg;
	if (!bind) return XTK_EVENT_CONTINUE;
	XtkMsg msg = {.tag = bind->tag, .i = bind->i, .trace_id = event ? event->trace_id : 0};
	if (event) {
		switch (event->payload.kind) {
		case XTK_EVENT_PAYLOAD_BOOL   : msg.b = event->payload.u.b; break;
		case XTK_EVENT_PAYLOAD_I32    : msg.i = event->payload.u.i32; break;
		case XTK_EVENT_PAYLOAD_F32    : msg.f = event->payload.u.f32; break;
		case XTK_EVENT_PAYLOAD_F64    : msg.d = event->payload.u.f64; break;
		case XTK_EVENT_PAYLOAD_STRING : msg.ptr = ( void * ) event->payload.u.string.data; break;
		case XTK_EVENT_PAYLOAD_QUERY :
			msg.ptr = ( void * ) event->payload.u.query.text;
			msg.i   = event->payload.u.query.index;
			break;
		case XTK_EVENT_PAYLOAD_RATING :
			msg.d = event->payload.u.rating.value;
			msg.i = event->payload.u.rating.count;
			break;
		case XTK_EVENT_PAYLOAD_INDEX_BOOL :
			msg.i = event->payload.u.index_bool.index;
			msg.b = event->payload.u.index_bool.value;
			break;
		case XTK_EVENT_PAYLOAD_NONE : break;
		}
	}
	gallery_dispatch(bind->app, msg);
	return XTK_EVENT_HANDLED;
}

static bool gallery_msg_owns_string(int tag) {
	return tag == MSG_NAME || tag == MSG_PASSWORD || tag == MSG_ASB_TEXT || tag == MSG_ASB_QUERY;
}

static char *gallery_string_copy(char const *text) {
	size_t size = strlen(text) + 1;
	char  *copy = ( char * ) malloc(size);
	if (copy) memcpy(copy, text, size);
	return copy;
}

static GalleryPending *gallery_pending_new(XtkMsg msg) {
	GalleryPending *pending = ( GalleryPending * ) calloc(1, sizeof(*pending));
	if (!pending) return NULL;
	pending->msg = msg;
	if (!gallery_msg_owns_string(msg.tag) || !msg.ptr) return pending;

	pending->owned = gallery_string_copy(( char const * ) msg.ptr);
	if (!pending->owned) {
		free(pending);
		return NULL;
	}
	pending->msg.ptr = pending->owned;
	return pending;
}

void gallery_dispatch(GalleryApp *g, XtkMsg msg) {
	if (!g || g->rebuilding || msg.tag <= MSG_NONE || msg.tag >= MSG_COUNT) return;
	GalleryPending *pending = gallery_pending_new(msg);
	if (!pending) return;
	if (g->pending_tail) g->pending_tail->next = pending;
	else g->pending_head = pending;
	g->pending_tail = pending;
	flux_app_redraw(g->flux);
}

static void gallery_pending_free(GalleryPending *pending) {
	if (!pending) return;
	free(pending->owned);
	free(pending);
}

void gallery_flush(GalleryApp *g) {
	if (!g || !g->pending_head || g->rebuilding) return;
	int  initial_page = g->model.page;
	bool rebuild      = false;

	while (g->pending_head) {
		GalleryPending *pending = g->pending_head;
		g->pending_head         = pending->next;
		int page_before         = g->model.page;
		gallery_model_update(&g->model, pending->msg);
		if (g->model.page != page_before || !g->sync_page) rebuild = true;
		gallery_pending_free(pending);
	}
	g->pending_tail = NULL;

	if (rebuild) gallery_rebuild_page(g);
	else g->sync_page(g);
	if (g->model.page != initial_page && g->nav != XENT_NODE_INVALID)
		flux_nav_select(gallery_store(g), g->nav, g->model.page);
}

static void gallery_pending_destroy(GalleryApp *g) {
	while (g->pending_head) {
		GalleryPending *pending = g->pending_head;
		g->pending_head         = pending->next;
		gallery_pending_free(pending);
	}
	g->pending_tail = NULL;
}

static void     gallery_frame(void *ctx) { gallery_flush(( GalleryApp * ) ctx); }

void            gallery_page_sync(GalleryApp *g, GalleryPageSyncFn sync) { g->sync_page = sync; }

static uint32_t on_invoke(void *ud, XtkEvent const *ev) {
	( void ) ev;
	GalleryBind *b = ( GalleryBind * ) ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = b->i});
	return XTK_EVENT_HANDLED;
}

static uint32_t on_invoke_ev(void *ud, XtkEvent const *ev) {
	GalleryBind *b = ( GalleryBind * ) ud;
	int          i = (ev && ev->payload.kind == XTK_EVENT_PAYLOAD_I32) ? ev->payload.u.i32 : b->i;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = i});
	return XTK_EVENT_HANDLED;
}

static uint32_t on_bool(void *ud, XtkEvent const *ev) {
	GalleryBind *b = ( GalleryBind * ) ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .b = ev && ev->payload.u.b});
	return XTK_EVENT_HANDLED;
}

static uint32_t on_f32(void *ud, XtkEvent const *ev) {
	GalleryBind *b = ( GalleryBind * ) ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .f = ev ? ev->payload.u.f32 : 0.0f});
	return XTK_EVENT_HANDLED;
}

static uint32_t on_f64(void *ud, XtkEvent const *ev) {
	GalleryBind *b = ( GalleryBind * ) ud;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .d = ev ? ev->payload.u.f64 : 0.0});
	return XTK_EVENT_HANDLED;
}

static uint32_t on_string(void *ud, XtkEvent const *ev) {
	GalleryBind *b = ( GalleryBind * ) ud;
	char const  *s = (ev && ev->payload.kind == XTK_EVENT_PAYLOAD_STRING) ? ev->payload.u.string.data : NULL;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .ptr = ( void * ) s});
	return XTK_EVENT_HANDLED;
}

static uint32_t select(void *ud, XtkEvent const *ev) {
	GalleryBind *b = ( GalleryBind * ) ud;
	int          i = (ev && ev->payload.kind == XTK_EVENT_PAYLOAD_I32) ? ev->payload.u.i32 : 0;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = i});
	return XTK_EVENT_HANDLED;
}

static uint32_t close(void *ud, XtkEvent const *ev) {
	GalleryBind *b = ( GalleryBind * ) ud;
	int          i = (ev && ev->payload.kind == XTK_EVENT_PAYLOAD_I32) ? ev->payload.u.i32 : b->i;
	gallery_dispatch(b->app, (XtkMsg) {.tag = b->tag, .i = i});
	return XTK_EVENT_HANDLED;
}

static void sub(GalleryApp *g, XentNodeId node, XtkEventId id, XtkEventFn fn, GalleryBind *b) {
	if (!b || node == XENT_NODE_INVALID) return;
	flux_host_subscribe(g->host, ( XtkCtrl ) node, id, XTK_EVENT_PHASE_MASK_TARGET, fn, b);
}

void g_on_invoke(GalleryApp *g, XentNodeId node, int tag) {
	sub(g, node, XTK_EVENT_INVOKE, on_invoke, gallery_bind(g, tag));
}

void g_on_invoke_i(GalleryApp *g, XentNodeId node, int tag, int i) {
	sub(g, node, XTK_EVENT_INVOKE, on_invoke, gallery_bind_i(g, tag, i));
}

void g_on_change_bool(GalleryApp *g, XentNodeId node, int tag) {
	sub(g, node, XTK_EVENT_CHANGE_BOOL, on_bool, gallery_bind(g, tag));
}

void g_on_change_f32(GalleryApp *g, XentNodeId node, int tag) {
	sub(g, node, XTK_EVENT_CHANGE_F32, on_f32, gallery_bind(g, tag));
}

void g_on_change_f64(GalleryApp *g, XentNodeId node, int tag) {
	sub(g, node, XTK_EVENT_CHANGE_F64, on_f64, gallery_bind(g, tag));
}

void g_on_change_string(GalleryApp *g, XentNodeId node, int tag) {
	sub(g, node, XTK_EVENT_CHANGE_STRING, on_string, gallery_bind(g, tag));
}

void g_on_select(GalleryApp *g, XentNodeId node, int tag) {
	sub(g, node, XTK_EVENT_SELECT, select, gallery_bind(g, tag));
}

void g_on_close(GalleryApp *g, XentNodeId node, int tag) { sub(g, node, XTK_EVENT_CLOSE, close, gallery_bind(g, tag)); }

typedef void (*PageFn)(GalleryApp *g, XentNodeId parent);
typedef bool (*PageBuildFn)(PageBuild *b);

static void  page_cat_basic(GalleryApp *g, XentNodeId p) {
	page_category(g, p, "Basic input", "Buttons, checkboxes, radios, combos, sliders, and switches.");
}

static void page_cat_text(GalleryApp *g, XentNodeId p) {
	page_category(g, p, "Text", "Text entry and display: TextBox, NumberBox, Typography.");
}

static void page_cat_status(GalleryApp *g, XentNodeId p) {
	page_category(g, p, "Status & info", "InfoBar, InfoBadge, progress, and tooltips.");
}

static void page_cat_media(GalleryApp *g, XentNodeId p) { page_category(g, p, "Media", "Images decoded through WIC."); }

static void page_cat_menus(GalleryApp *g, XentNodeId p) {
	page_category(g, p, "Menus & layout", "Menus, tabs, links, and layout containers.");
}

static void page_cat_dialogs(GalleryApp *g, XentNodeId p) {
	page_category(g, p, "Dialogs & flyouts", "Modal dialogs and popup surfaces.");
}

static void page_cat_shell(GalleryApp *g, XentNodeId p) {
	page_category(g, p, "Layout & shell", "SplitView and window scaffolding.");
}

static void page_cat_collections(GalleryApp *g, XentNodeId p) {
	page_category(g, p, "Collections", "ListView, GridView, ListBox, TreeView, ItemsView, and PullToRefresh.");
}

static PageFn const k_pages [PAGE_COUNT] = {
  [PAGE_CAT_BASIC]       = page_cat_basic,
  [PAGE_CAT_TEXT]        = page_cat_text,
  [PAGE_CAT_STATUS]      = page_cat_status,
  [PAGE_CAT_MEDIA]       = page_cat_media,
  [PAGE_CAT_MENUS]       = page_cat_menus,
  [PAGE_CAT_DIALOGS]     = page_cat_dialogs,
  [PAGE_CAT_SHELL]       = page_cat_shell,
  [PAGE_CAT_COLLECTIONS] = page_cat_collections,
  [PAGE_SPLIT]           = page_split,
  [PAGE_COMBO]           = page_combo,
  [PAGE_RADIO_BUTTONS]   = page_radio_buttons,
  [PAGE_TOGGLE_SPLIT]    = page_toggle_split,
  [PAGE_INFOBAR]         = page_infobar,
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

static PageBuildFn const k_page_builders [PAGE_COUNT] = {
  [PAGE_HOME]       = page_home_build,
  [PAGE_BUTTON]     = page_button_build,
  [PAGE_CHECKBOX]   = page_checkbox_build,
  [PAGE_RADIO]      = page_radio_build,
  [PAGE_SLIDER]     = page_slider_build,
  [PAGE_TOGGLE]     = page_toggle_build,
  [PAGE_TYPOGRAPHY] = page_typography_build,
  [PAGE_TOOLTIP]    = page_tooltip_build,
  [PAGE_NUMBERBOX]  = page_numberbox_build,
  [PAGE_RATING]     = page_rating_build,
  [PAGE_TEXTBOX]    = page_textbox_build,
  [PAGE_BADGE]      = page_badge_build,
  [PAGE_PROGRESS]   = page_progress_build,
};

void gallery_rebuild_page(GalleryApp *g) {
	if (g->content_host == XENT_NODE_INVALID) return;
	g->rebuilding = 1;

	int page      = g->model.page;
	if (( unsigned ) page >= PAGE_COUNT || (!k_page_builders [page] && !k_pages [page])) page = PAGE_HOME;
	if (k_page_builders [page]) {
		PageBuild build = {0};
		bool      ok    = page_begin(&build, g) && k_page_builders [page](&build) && page_commit(&build);
		if (!ok && build.active) page_abort(&build);
		g->rebuilding = 0;
		return;
	}

	if (g->page_root != XENT_NODE_INVALID) {
		flux_tree_drop(gallery_store(g), g->page_root);
		g->page_root = XENT_NODE_INVALID;
	}
	gallery_page_allocs_destroy(g);
	gallery_binds_reset(g);
	memset(g->live_control, 0, sizeof(g->live_control));
	memset(g->live_output, 0, sizeof(g->live_output));
	memset(g->live_data, 0, sizeof(g->live_data));
	g->sync_page      = NULL;

	XentNodeId scroll = g_scroll(g, g->content_host);
	XentNodeId col    = g_col(g, scroll, 20.0f, 36.0f, 28.0f, 36.0f, 28.0f);
	g->page_root      = scroll;

	k_pages [page](g, col);
	g->rebuilding = 0;
}

bool gallery_app_create(GalleryApp *g) {
	memset(g, 0, sizeof(*g));
	gallery_model_init(&g->model);

	FluxAppCfg cfg = {
	  .title     = L"FluXent Gallery (Imperative)",
	  .width     = 1100,
	  .height    = 760,
	  .dark_mode = flux_theme_systemdark(),
	  .backdrop  = FLUX_BACKDROP_MICA,
	};
	g->flux = flux_app_create(&cfg);
	if (!g->flux) return false;

	g->bctx.ctx   = flux_app_ctx(g->flux);
	g->bctx.store = flux_app_store(g->flux);
	g->bctx.app   = g->flux;
	g->host       = flux_host_adopt(g->bctx.ctx, g->bctx.store, flux_app_root(g->flux));
	if (!g->host) return false;
	g->bctx.host = g->host;

	g->backend   = flux_xtk_backend_create(&g->bctx, g->host);
	if (!g->backend) return false;
	flux_app_sethost(g->flux, g->host, g->backend);
	g->scene = xtk_scene_from_host(g->host);
	flux_app_onframe(g->flux, gallery_frame, g);

	if (!gallery_shell_build(g)) return false;
	gallery_rebuild_page(g);
	return true;
}

void gallery_app_destroy(GalleryApp *g) {
	if (g->flux) flux_app_onframe(g->flux, NULL, NULL);
	gallery_pending_destroy(g);
	if (g->host) flux_host_destroy(g->host);
	g->host = NULL;
	if (g->flux) flux_app_destroy(g->flux);
	gallery_page_allocs_destroy(g);
	gallery_binds_destroy(g);
	memset(g, 0, sizeof(*g));
}

int gallery_app_run(GalleryApp *g) { return flux_app_run(g->flux); }
