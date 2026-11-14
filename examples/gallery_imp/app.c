#include "app.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char const *gallery_fmt(GalleryApp *g, char const *fmt, ...) {
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(g->fmt_buf, sizeof(g->fmt_buf), fmt, ap);
	va_end(ap);
	return g->fmt_buf;
}

void gallery_settext(GalleryApp *g, XtkCtrl text, char const *value) {
	xtk_text_settext(&g->scene, text, value, ( uint32_t ) strlen(value), (XtkChangeStamp) {0});
}

static void bind_blocks_destroy(GalleryBindBlock *block) {
	while (block) {
		GalleryBindBlock *next = block->next;
		free(block);
		block = next;
	}
}

static void allocs_destroy(GalleryPageAlloc *alloc) {
	while (alloc) {
		GalleryPageAlloc *next = alloc->next;
		free(alloc);
		alloc = next;
	}
}

GalleryBind *page_bind_i(PageBuild *b, int tag, int i) {
	GalleryBindBlock *block = b->bind_cursor;
	if (!block || block->used == ( int ) (sizeof(block->items) / sizeof(block->items [0]))) {
		GalleryBindBlock *next = ( GalleryBindBlock * ) calloc(1, sizeof(*next));
		if (!next) return NULL;
		if (block) block->next = next;
		else b->bind_blocks = next;
		b->bind_cursor = block = next;
	}
	GalleryBind *bind = &block->items [block->used++];
	*bind             = (GalleryBind) {.app = b->app, .tag = tag, .i = i};
	return bind;
}

GalleryBind *page_bind(PageBuild *b, int tag) { return page_bind_i(b, tag, 0); }

void        *page_alloc(PageBuild *b, size_t size) {
	GalleryPageAlloc *alloc = ( GalleryPageAlloc * ) calloc(1, sizeof(*alloc) + size);
	if (!alloc) return NULL;
	alloc->next = b->allocs;
	b->allocs   = alloc;
	return alloc->data;
}

void page_on(PageBuild *b, XtkLocalId local, XtkEventId event, int tag, int i) {
	xtk_txn_on(&b->txn, xtk_ref_local(local), event, XTK_EVENT_PHASE_MASK_TARGET, page_post, page_bind_i(b, tag, i));
}

void          page_sync(PageBuild *b, PageSyncFn sync) { b->sync = sync; }

void          page_keep(PageBuild *b, int tag, XtkLocalId local) { b->controls [tag] = local; }

void          page_keepout(PageBuild *b, int tag, XtkLocalId local) { b->outputs [tag] = local; }

void          page_keepx(PageBuild *b, int slot, XtkLocalId local) { b->extra [slot] = local; }

static XtkMsg event_pair_msg(XtkMsg msg, XtkEventPayload const *p) {
	switch (p->kind) {
	case XTK_EVENT_PAYLOAD_QUERY :
		msg.ptr = ( void * ) p->u.query.text;
		msg.i   = p->u.query.index;
		break;
	case XTK_EVENT_PAYLOAD_RATING :
		msg.d = p->u.rating.value;
		msg.i = p->u.rating.count;
		break;
	case XTK_EVENT_PAYLOAD_INDEX_BOOL :
		msg.i = p->u.index_bool.index;
		msg.b = p->u.index_bool.value;
		break;
	case XTK_EVENT_PAYLOAD_RANGE :
		msg.f = p->u.range.start;
		msg.d = p->u.range.end;
		break;
	default : break;
	}
	return msg;
}

static XtkMsg event_msg(GalleryBind const *bind, XtkEvent const *event) {
	XtkMsg                 msg = {.tag = bind->tag, .i = bind->i};
	XtkEventPayload const *p   = &event->payload;
	switch (p->kind) {
	case XTK_EVENT_PAYLOAD_BOOL   : msg.b = p->u.b; break;
	case XTK_EVENT_PAYLOAD_I32    : msg.i = p->u.i32; break;
	case XTK_EVENT_PAYLOAD_F32    : msg.f = p->u.f32; break;
	case XTK_EVENT_PAYLOAD_F64    : msg.d = p->u.f64; break;
	case XTK_EVENT_PAYLOAD_STRING : msg.ptr = ( void * ) p->u.string.data; break;
	default                       : msg = event_pair_msg(msg, p); break;
	}
	return msg;
}

static bool msg_owns_string(int tag) {
	return tag == MSG_NAME
	    || tag == MSG_PASSWORD
	    || tag == MSG_ASB_TEXT
	    || tag == MSG_ASB_QUERY
	    || tag == MSG_COMBO_SUBMIT;
}

static void dispatch(GalleryApp *g, XtkMsg msg) {
	if (g->rebuilding || msg.tag <= MSG_NONE || msg.tag >= MSG_COUNT) return;
	GalleryPending *pending = ( GalleryPending * ) calloc(1, sizeof(*pending));
	if (!pending) return;
	pending->msg = msg;
	if (msg_owns_string(msg.tag) && msg.ptr) {
		pending->owned = _strdup(( char const * ) msg.ptr);
		if (!pending->owned) {
			free(pending);
			return;
		}
		pending->msg.ptr = pending->owned;
	}
	if (g->pending_tail) g->pending_tail->next = pending;
	else g->pending_head = pending;
	g->pending_tail = pending;
	flux_app_redraw(g->flux);
}

uint32_t page_post(void *arg, XtkEvent const *event) {
	GalleryBind *bind = ( GalleryBind * ) arg;
	if (!bind || !event) return XTK_EVENT_CONTINUE;
	dispatch(bind->app, event_msg(bind, event));
	return XTK_EVENT_HANDLED;
}

static XtkCtrl result_ctrl(XtkBatchResult const *result, XtkLocalId local) {
	if (local == XTK_LOCAL_INVALID) return XTK_CTRL_INVALID;
	for (uint32_t i = 0; i < result->control_count; i++)
		if (result->controls [i].local == local) return result->controls [i].control;
	return XTK_CTRL_INVALID;
}

typedef struct ListWindow {
	int first;
	int count;
	int keep_lo;
	int keep_hi;
} ListWindow;

static ListWindow list_window(GalleryList const *l, XtkEvent const *event) {
	ListWindow w = {.first = event->payload.u.window.first};
	w.count      = event->payload.u.window.last - w.first + 1;
	if (w.count < 0) w.count = 0;
	if (w.count > GALLERY_LIST_WINDOW) w.count = GALLERY_LIST_WINDOW;
	w.keep_lo = l->first > w.first ? l->first : w.first;
	w.keep_hi = l->first + l->count < w.first + w.count ? l->first + l->count : w.first + w.count;
	if (w.keep_hi < w.keep_lo) w.keep_hi = w.keep_lo;
	return w;
}

static void list_drop_outside(GalleryList const *l, XtkTxn *txn, ListWindow const *w) {
	for (int i = 0; i < l->count; i++) {
		int index = l->first + i;
		if (index < w->keep_lo || index >= w->keep_hi) xtk_txn_drop(txn, xtk_ref_existing(l->items [i]));
	}
}

static XtkNodeRef list_slot(GalleryList *l, XtkTxn *txn, XtkNodeRef list, XtkNodeRef before, int index) {
	XtkLocalId id = xtk_txn_create(txn, XTK_CTRL_LISTITEM, list, before, NULL, 0);
	l->fill(l->app, txn, xtk_ref_local(id), index);
	return xtk_ref_local(id);
}

static XtkNodeRef list_ref(GalleryList *l, XtkTxn *txn, XtkNodeRef list, ListWindow const *w, int index) {
	if (index >= w->keep_lo && index < w->keep_hi) return xtk_ref_existing(l->items [index - l->first]);
	XtkNodeRef anchor = w->keep_hi > w->keep_lo ? xtk_ref_existing(l->items [w->keep_lo - l->first]) : xtk_ref_none();
	return list_slot(l, txn, list, index < w->keep_lo ? anchor : xtk_ref_none(), index);
}

static void list_adopt(GalleryList *l, ListWindow const *w, XtkNodeRef const *refs, XtkBatchResult *result) {
	for (int i = 0; i < w->count; i++)
		l->items [i] = refs [i].kind == XTK_REF_LOCAL ? result_ctrl(result, refs [i].u.local) : refs [i].u.existing;
	l->first = w->first;
	l->count = w->count;
}

static uint32_t list_realize(void *arg, XtkEvent const *event) {
	GalleryList *l    = ( GalleryList * ) arg;
	ListWindow   w    = list_window(l, event);
	XtkNodeRef   list = xtk_ref_existing(event->target);
	XtkNodeRef   refs [GALLERY_LIST_WINDOW];
	XtkTxn       txn;
	if (!xtk_txn_begin(&l->app->scene, &txn)) return XTK_EVENT_HANDLED;
	list_drop_outside(l, &txn, &w);
	for (int i = 0; i < w.count; i++) refs [i] = list_ref(l, &txn, list, &w, w.first + i);
	xtk_txn_set(
	  &txn, list, XTK_PROP_REALIZED_FIRST, (XtkValue) {.kind = XTK_VALUE_I32, .u.i32 = w.first}, (XtkChangeStamp) {0}
	);
	XtkBatchResult *result = xtk_txn_commit(&txn, NULL);
	if (!result) return XTK_EVENT_HANDLED;
	list_adopt(l, &w, refs, result);
	xtk_batchresult_destroy(result);
	return XTK_EVENT_HANDLED;
}

void page_list(PageBuild *b, XtkLocalId list, PageFillFn fill) {
	GalleryList *l = ( GalleryList * ) page_alloc(b, sizeof(*l));
	if (!l) return;
	*l = (GalleryList) {.app = b->app, .fill = fill};
	xtk_txn_on(&b->txn, xtk_ref_local(list), XTK_EVENT_REALIZE, XTK_EVENT_PHASE_MASK_TARGET, list_realize, l);
}

static void page_reset(PageBuild *b) {
	if (b->active) xtk_txn_abort(&b->txn);
	bind_blocks_destroy(b->bind_blocks);
	allocs_destroy(b->allocs);
	memset(b, 0, sizeof(*b));
}

static XtkLocalId page_scroll(PageBuild *b, XtkCtrl parent) {
	b->root = xtk_scroll_create(
	  &b->txn, xtk_ref_existing(parent), &(XtkScrollProps) {.set = XTK_SCROLL_GROW_MASK, .grow = 1.0f}
	);
	return xtk_container_create(
	  &b->txn, xtk_ref_local(b->root),
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_SHRINK_MASK
	           | XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK
	           | XTK_CONTAINER_FILL_MASK
	           | XTK_CONTAINER_FITH_MASK,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_STRETCH,
		.gap   = 20.0f,
		.pl    = 36.0f,
		.pt    = 28.0f,
		.pr    = 36.0f,
		.pb    = 28.0f,
		.fill  = true,
		.fith  = true,
	  }
	);
}

static bool page_begin(PageBuild *b, GalleryApp *g) {
	memset(b, 0, sizeof(*b));
	b->app = g;
	for (int i = 0; i < MSG_COUNT; i++) b->controls [i] = b->outputs [i] = XTK_LOCAL_INVALID;
	for (int i = 0; i < PAGE_EXTRA; i++) b->extra [i] = XTK_LOCAL_INVALID;
	if (!xtk_txn_begin(&g->scene, &b->txn)) return false;
	b->active = true;
	if (g->page_root != XTK_CTRL_INVALID) xtk_txn_drop(&b->txn, xtk_ref_existing(g->page_root));
	b->content = page_scroll(b, g->nav);
	return b->content != XTK_LOCAL_INVALID;
}

static void page_adopt(GalleryApp *g, PageBuild *b, XtkBatchResult const *result) {
	bind_blocks_destroy(g->bind_blocks);
	allocs_destroy(g->page_allocs);
	g->page_root   = result_ctrl(result, b->root);
	g->bind_blocks = b->bind_blocks;
	g->page_allocs = b->allocs;
	g->sync_page   = b->sync;
	for (int i = 0; i < MSG_COUNT; i++) {
		g->live_control [i] = result_ctrl(result, b->controls [i]);
		g->live_output [i]  = result_ctrl(result, b->outputs [i]);
	}
	for (int i = 0; i < PAGE_EXTRA; i++) g->live_extra [i] = result_ctrl(result, b->extra [i]);
	b->bind_blocks = NULL;
	b->allocs      = NULL;
}

static bool page_commit(PageBuild *b) {
	XtkBatchDiag    diag   = {0};
	XtkBatchResult *result = xtk_txn_commit(&b->txn, &diag);
	b->active              = false;
	if (!result) {
		fprintf(stderr, "gallery page batch rejected: error=%d op=%u\n", ( int ) diag.error, diag.failed_op);
		return false;
	}
	GalleryApp *g = b->app;
	page_adopt(g, b, result);
	xtk_batchresult_destroy(result);
	return true;
}

typedef bool             (*PageBuildFn)(PageBuild *b);

static PageBuildFn const k_pages [PAGE_COUNT] = {
  [PAGE_HOME]            = page_home_build,
  [PAGE_CAT_BASIC]       = page_category_build,
  [PAGE_CAT_TEXT]        = page_category_build,
  [PAGE_CAT_STATUS]      = page_category_build,
  [PAGE_CAT_MEDIA]       = page_category_build,
  [PAGE_CAT_MENUS]       = page_category_build,
  [PAGE_CAT_DIALOGS]     = page_category_build,
  [PAGE_CAT_SHELL]       = page_category_build,
  [PAGE_CAT_COLLECTIONS] = page_category_build,
  [PAGE_BUTTON]          = page_button_build,
  [PAGE_SPLIT]           = page_split_build,
  [PAGE_CHECKBOX]        = page_checkbox_build,
  [PAGE_RADIO]           = page_radio_build,
  [PAGE_COMBO]           = page_combo_build,
  [PAGE_SLIDER]          = page_slider_build,
  [PAGE_SCROLL_BAR]      = page_scroll_bar_build,
  [PAGE_LOOPING]         = page_looping_build,
  [PAGE_DATE_PICKER]     = page_date_picker_build,
  [PAGE_TIME_PICKER]     = page_time_picker_build,
  [PAGE_CALENDAR]        = page_calendar_build,
  [PAGE_COLOR_PICKER]    = page_color_picker_build,
  [PAGE_TOGGLE]          = page_toggle_build,
  [PAGE_RADIO_BUTTONS]   = page_radio_buttons_build,
  [PAGE_RATING]          = page_rating_build,
  [PAGE_TOGGLE_SPLIT]    = page_toggle_split_build,
  [PAGE_TEXTBOX]         = page_textbox_build,
  [PAGE_AUTOSUGGEST]     = page_autosuggest_build,
  [PAGE_NUMBERBOX]       = page_numberbox_build,
  [PAGE_TYPOGRAPHY]      = page_typography_build,
  [PAGE_INFOBAR]         = page_infobar_build,
  [PAGE_BADGE]           = page_badge_build,
  [PAGE_PROGRESS]        = page_progress_build,
  [PAGE_TOOLTIP]         = page_tooltip_build,
  [PAGE_IMAGE]           = page_image_build,
  [PAGE_PERSON_PICTURE]  = page_person_picture_build,
  [PAGE_FLIPVIEW]        = page_flipview_build,
  [PAGE_PAGER]           = page_pager_build,
  [PAGE_MENUS]           = page_menus_build,
  [PAGE_TABVIEW]         = page_tabview_build,
  [PAGE_EXPANDER]        = page_expander_build,
  [PAGE_BREADCRUMB]      = page_breadcrumb_build,
  [PAGE_SELECTOR_BAR]    = page_selector_bar_build,
  [PAGE_LISTVIEW]        = page_listview_build,
  [PAGE_GRIDVIEW]        = page_gridview_build,
  [PAGE_LISTBOX]         = page_listbox_build,
  [PAGE_TREE_VIEW]       = page_tree_view_build,
  [PAGE_ITEMS_VIEW]      = page_items_view_build,
  [PAGE_PULL_REFRESH]    = page_pull_refresh_build,
  [PAGE_SPLIT_VIEW]      = page_split_view_build,
  [PAGE_TITLE_BAR]       = page_title_bar_build,
  [PAGE_DIALOG]          = page_dialog_build,
  [PAGE_TEACHING_TIP]    = page_teaching_tip_build,
  [PAGE_SETTINGS]        = page_settings_build,
};

static void rebuild_page(GalleryApp *g) {
	int page = g->model.page;
	if (( unsigned ) page >= PAGE_COUNT || !k_pages [page]) page = PAGE_HOME;
	g->rebuilding = true;
	PageBuild b;
	if (page_begin(&b, g) && k_pages [page](&b)) page_commit(&b);
	page_reset(&b);
	g->rebuilding = false;
}

static void flush(void *ctx) {
	GalleryApp *g = ( GalleryApp * ) ctx;
	if (!g->pending_head || g->rebuilding) return;
	int  initial = g->model.page;
	bool rebuild = !g->sync_page;
	while (g->pending_head) {
		GalleryPending *pending = g->pending_head;
		g->pending_head         = pending->next;
		gallery_model_update(&g->model, pending->msg);
		free(pending->owned);
		free(pending);
	}
	g->pending_tail = NULL;
	if (g->model.page != initial) {
		rebuild = true;
		xtk_nav_setsel(&g->scene, g->nav, g->model.page, (XtkChangeStamp) {0});
	}
	if (!rebuild) g->sync_page(g);
	if (rebuild || g->rebuild_request) rebuild_page(g);
	g->rebuild_request = false;
}

static bool shell_build(GalleryApp *g) {
	XtkTxn txn;
	if (!xtk_txn_begin(&g->scene, &txn)) return false;
	XtkLocalId nav = xtk_nav_create(
	  &txn, xtk_ref_existing(xtk_scene_root(&g->scene)),
	  &(XtkNavProps) {.set = XTK_NAV_GROW_MASK | XTK_NAV_SEL_MASK, .grow = 1.0f, .sel = g->model.page}
	);
	xtk_txn_setnav(
	  &txn, xtk_ref_local(nav),
	  &(XtkNavDesc) {
		.items            = kNavItems,
		.nitem            = PAGE_COUNT,
		.sel              = g->model.page,
		.window_title_bar = true,
		.app_title        = "FluXent Gallery",
	  }
	);
	g->nav_bind = (GalleryBind) {.app = g, .tag = MSG_NAV};
	xtk_nav_onselect(&txn, xtk_ref_local(nav), page_post, &g->nav_bind);
	XtkBatchResult *result = xtk_txn_commit(&txn, NULL);
	if (!result) return false;
	g->nav = result_ctrl(result, nav);
	xtk_batchresult_destroy(result);
	return g->nav != XTK_CTRL_INVALID;
}

bool gallery_app_create(GalleryApp *g) {
	memset(g, 0, sizeof(*g));
	gallery_model_init(&g->model);
	g->flux = flux_app_create(&(FluxAppCfg) {
	  .title    = L"FluXent Gallery (Imperative)",
	  .width    = 1100,
	  .height   = 760,
	  .backdrop = FLUX_BACKDROP_MICA,
	});
	if (!g->flux) return false;
	g->host = flux_app_attachhost(g->flux);
	if (!g->host) return false;
	g->scene = xtk_scene_fromhost(g->host);
	flux_app_onframe(g->flux, flush, g);
	if (!shell_build(g)) return false;
	rebuild_page(g);
	return true;
}

void gallery_app_destroy(GalleryApp *g) {
	if (g->flux) flux_app_onframe(g->flux, NULL, NULL);
	while (g->pending_head) {
		GalleryPending *next = g->pending_head->next;
		free(g->pending_head->owned);
		free(g->pending_head);
		g->pending_head = next;
	}
	if (g->host) flux_host_destroy(g->host);
	if (g->flux) flux_app_destroy(g->flux);
	bind_blocks_destroy(g->bind_blocks);
	allocs_destroy(g->page_allocs);
	memset(g, 0, sizeof(*g));
}

int gallery_app_run(GalleryApp *g) { return flux_app_run(g->flux); }
