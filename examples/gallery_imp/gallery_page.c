#include "app.h"

#include <xtk/xtk_hot.h>

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
	xtk_txn_on(b->txn, xtk_ref_local(local), event, XTK_EVENT_PHASE_MASK_TARGET, page_post, page_bind_i(b, tag, i));
}

void page_sync(PageBuild *b, PageSyncFn sync) { b->sync = sync; }

void page_keep(PageBuild *b, int tag, XtkLocalId local) { b->controls [tag] = local; }

void page_keepout(PageBuild *b, int tag, XtkLocalId local) { b->outputs [tag] = local; }

void page_keepx(PageBuild *b, int slot, XtkLocalId local) { b->extra [slot] = local; }

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
		l->items [i]
		  = refs [i].kind == XTK_REF_LOCAL ? gallery_result_ctrl(result, refs [i].u.local) : refs [i].u.existing;
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
	xtk_txn_on(b->txn, xtk_ref_local(list), XTK_EVENT_REALIZE, XTK_EVENT_PHASE_MASK_TARGET, list_realize, l);
}

static int groups_header_row(GalleryGroups const *l, int group) {
	int row = 0;
	for (int g = 0; g < group; g++) row += 1 + l->sizes [g];
	return row;
}

static int groups_window(GalleryGroups const *l, XtkEvent const *event, int *rows) {
	int        first = event->payload.u.window.first, n = 0;
	XtkListRow at = xtk_list_row(l->sizes, l->nsize, first);
	if (at.item >= 0) rows [n++] = groups_header_row(l, at.group);
	for (int r = first; r <= event->payload.u.window.last && n < GALLERY_LIST_WINDOW; r++) rows [n++] = r;
	return n;
}

static int groups_find(GalleryGroups const *l, int row) {
	for (int i = 0; i < l->count; i++)
		if (l->rows [i] == row) return i;
	return -1;
}

static bool groups_wanted(int const *rows, int n, int row) {
	for (int i = 0; i < n; i++)
		if (rows [i] == row) return true;
	return false;
}

static XtkNodeRef groups_before(GalleryGroups const *l, int const *rows, int n, int row) {
	for (int i = 0; i < l->count; i++)
		if (l->rows [i] > row && groups_wanted(rows, n, l->rows [i])) return xtk_ref_existing(l->nodes [i]);
	return xtk_ref_none();
}

static XtkNodeRef groups_ref(GalleryGroups *l, XtkTxn *txn, XtkNodeRef list, int const *rows, int n, int row) {
	int kept = groups_find(l, row);
	if (kept >= 0) return xtk_ref_existing(l->nodes [kept]);
	XtkListRow  at   = xtk_list_row(l->sizes, l->nsize, row);
	XtkCtrlType type = at.item < 0 ? XTK_CTRL_GROUPHEADER : XTK_CTRL_LISTITEM;
	XtkLocalId  id   = xtk_txn_create(txn, type, list, groups_before(l, rows, n, row), NULL, 0);
	(at.item < 0 ? l->header : l->item)(l->app, txn, xtk_ref_local(id), at.item < 0 ? at.group : at.item);
	return xtk_ref_local(id);
}

static uint32_t groups_realize(void *arg, XtkEvent const *event) {
	GalleryGroups *l = ( GalleryGroups * ) arg;
	int            rows [GALLERY_LIST_WINDOW];
	XtkNodeRef     refs [GALLERY_LIST_WINDOW];
	int            n    = groups_window(l, event, rows);
	XtkNodeRef     list = xtk_ref_existing(event->target);
	XtkTxn         txn;
	if (!xtk_txn_begin(&l->app->scene, &txn)) return XTK_EVENT_HANDLED;
	for (int i = 0; i < l->count; i++)
		if (!groups_wanted(rows, n, l->rows [i])) xtk_txn_drop(&txn, xtk_ref_existing(l->nodes [i]));
	for (int i = 0; i < n; i++) refs [i] = groups_ref(l, &txn, list, rows, n, rows [i]);
	xtk_txn_set(
	  &txn, list, XTK_PROP_REALIZED_FIRST, (XtkValue) {.kind = XTK_VALUE_I32, .u.i32 = event->payload.u.window.first},
	  (XtkChangeStamp) {0}
	);
	XtkBatchResult *result = xtk_txn_commit(&txn, NULL);
	if (!result) return XTK_EVENT_HANDLED;
	for (int i = 0; i < n; i++) {
		l->rows [i] = rows [i];
		l->nodes [i]
		  = refs [i].kind == XTK_REF_LOCAL ? gallery_result_ctrl(result, refs [i].u.local) : refs [i].u.existing;
	}
	l->count = n;
	xtk_batchresult_destroy(result);
	return XTK_EVENT_HANDLED;
}

void page_groups(PageBuild *b, XtkLocalId list, int32_t const *sizes, int nsize, PageFillFn header, PageFillFn item) {
	GalleryGroups *l = ( GalleryGroups * ) page_alloc(b, sizeof(*l));
	if (!l) return;
	*l = (GalleryGroups) {.app = b->app, .header = header, .item = item, .sizes = sizes, .nsize = nsize};
	xtk_txn_set(
	  b->txn, xtk_ref_local(list), XTK_PROP_GROUPS,
	  (XtkValue) {
		.kind = XTK_VALUE_BYTES, .u.bytes = {( uint8_t * ) ( uintptr_t ) sizes, sizeof(int32_t) * ( uint32_t ) nsize}
    },
	  (XtkChangeStamp) {0}
	);
	xtk_txn_on(b->txn, xtk_ref_local(list), XTK_EVENT_REALIZE, XTK_EVENT_PHASE_MASK_TARGET, groups_realize, l);
}

static XtkLocalId page_scroll(PageBuild *b, XtkCtrl parent) {
	b->root = xtk_scroll_create(
	  b->txn, xtk_ref_existing(parent), &(XtkScrollProps) {.set = XTK_SCROLL_GROW_MASK, .grow = 1.0f}
	);
	return xtk_container_create(
	  b->txn, xtk_ref_local(b->root),
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

static bool page_begin(PageBuild *b, GalleryApp *g, XtkTxn *txn) {
	b->txn = txn;
	for (int i = 0; i < MSG_COUNT; i++) b->controls [i] = b->outputs [i] = XTK_LOCAL_INVALID;
	for (int i = 0; i < PAGE_EXTRA; i++) b->extra [i] = XTK_LOCAL_INVALID;
	XtkNavPaneMode       mode = g->model.nav_style ? XTK_NAV_PANE_TOP : XTK_NAV_PANE_AUTO;
	XtkNavTransitionKind tr   = xtk_nav_recommended_transition(g->model.prev_page, g->model.page, mode);
	XtkNodeRef           fref = xtk_ref_existing(g->page_frame);
	xtk_txn_set(
	  txn, fref, XTK_PROP_FRAME_TRANSITION, (XtkValue) {.kind = XTK_VALUE_ENUM, .u.u32 = ( uint32_t ) tr},
	  (XtkChangeStamp) {0}
	);
	xtk_txn_set(
	  txn, fref, XTK_PROP_FRAME_PAGE_KEY, (XtkValue) {.kind = XTK_VALUE_I32, .u.i32 = g->model.page + 1},
	  (XtkChangeStamp) {0}
	);
	b->content = page_scroll(b, g->page_frame);
	return b->content != XTK_LOCAL_INVALID;
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
  [PAGE_COMMANDBAR]      = page_commandbar_build,
  [PAGE_TABVIEW]         = page_tabview_build,
  [PAGE_EXPANDER]        = page_expander_build,
  [PAGE_BREADCRUMB]      = page_breadcrumb_build,
  [PAGE_SELECTOR_BAR]    = page_selector_bar_build,
  [PAGE_LISTVIEW]        = page_listview_build,
  [PAGE_GRIDVIEW]        = page_gridview_build,
  [PAGE_LISTBOX]         = page_listbox_build,
  [PAGE_DETAILS]         = page_details_build,
  [PAGE_TREE_VIEW]       = page_tree_view_build,
  [PAGE_ITEMS_VIEW]      = page_items_view_build,
  [PAGE_PULL_REFRESH]    = page_pull_refresh_build,
  [PAGE_SPLIT_VIEW]      = page_split_view_build,
  [PAGE_NAVIGATION_VIEW] = page_navigation_view_build,
  [PAGE_TITLE_BAR]       = page_title_bar_build,
  [PAGE_DRAG_DROP]       = page_drag_drop_build,
  [PAGE_MATERIALS]       = page_materials_build,
  [PAGE_MOTION]          = page_motion_build,
  [PAGE_DIALOG]          = page_dialog_build,
  [PAGE_TEACHING_TIP]    = page_teaching_tip_build,
  [PAGE_SETTINGS]        = page_settings_build,
};

bool gallery_page_build(GalleryApp *g, XtkTxn *txn, void **out) {
	PageBuild *b = ( PageBuild * ) calloc(1, sizeof(*b));
	if (!b) return false;
	*out          = b;
	b->app        = g;
	g->rebuilding = true;
	int page      = g->model.page;
	if (( unsigned ) page >= PAGE_COUNT || !k_pages [page]) page = PAGE_HOME;
	return page_begin(b, g, txn) && k_pages [page](b);
}

void gallery_page_adopt(void *page, XtkBatchResult const *result) {
	PageBuild  *b  = ( PageBuild * ) page;
	GalleryApp *g  = b->app;
	g->page_root   = gallery_result_ctrl(result, b->root);
	g->page_allocs = b->allocs;
	g->sync_page   = b->sync;
	for (int i = 0; i < MSG_COUNT; i++) {
		g->live_control [i] = gallery_result_ctrl(result, b->controls [i]);
		g->live_output [i]  = gallery_result_ctrl(result, b->outputs [i]);
	}
	for (int i = 0; i < PAGE_EXTRA; i++) g->live_extra [i] = gallery_result_ctrl(result, b->extra [i]);
	g->rebuilding = false;
}

void gallery_page_release(void *page) {
	PageBuild *b = ( PageBuild * ) page;
	if (!b) return;
	b->app->rebuilding = false;
	bind_blocks_destroy(b->bind_blocks);
	allocs_destroy(b->allocs);
	free(b);
}

#ifdef GALLERY_HOT_MODULE
static bool hot_build(XtkScene *scene, XtkTxn *txn, XtkNodeRef slot, void *model, void **page) {
	( void ) scene;
	( void ) slot;
	return gallery_page_build(( GalleryApp * ) model, txn, page);
}

XTK_HOT_EXPORT_MODULE(
  GalleryApp, GALLERY_MODEL_VERSION, 0u, .page_build = hot_build, .page_adopt = gallery_page_adopt,
  .page_release = gallery_page_release
)
#endif
