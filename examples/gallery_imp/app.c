#include "app.h"

#include <fluxent/flux_theme.h>
#include <xtk/xtk.h>

#ifdef GALLERY_HOT
  #include <fluxent/flux_hot.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool msg_owns_string(int tag) {
	return tag == MSG_NAME
	    || tag == MSG_PASSWORD
	    || tag == MSG_ASB_TEXT
	    || tag == MSG_ASB_QUERY
	    || tag == MSG_CONTROL_SEARCH_TEXT
	    || tag == MSG_CONTROL_SEARCH_QUERY
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

static void rebuild_page(GalleryApp *g) {
#ifdef GALLERY_HOT
	( void ) flux_hot_page_rebuild(( FluxHot * ) g->hot);
#else
	XtkTxn txn;
	void  *page = NULL;
	if (!xtk_txn_begin(&g->scene, &txn)) return;
	if (g->page_root != XTK_CTRL_INVALID) xtk_txn_drop(&txn, xtk_ref_existing(g->page_root));
	XtkBatchDiag    diag   = {0};
	XtkBatchResult *result = gallery_page_build(g, &txn, &page) ? xtk_txn_commit(&txn, &diag) : NULL;
	if (!result) {
		xtk_txn_abort(&txn);
		gallery_page_release(page);
		fprintf(stderr, "gallery page batch rejected: error=%d op=%u\n", ( int ) diag.error, diag.failed_op);
		return;
	}
	gallery_page_adopt(page, result);
	xtk_batchresult_destroy(result);
	gallery_page_release(g->page);
	g->page = page;
#endif
}

static void apply_flow(GalleryApp *g) {
	XtkTxn txn;
	if (!xtk_txn_begin(&g->scene, &txn)) return;
	XentDirection dir = g->model.rtl ? XENT_DIRECTION_RTL : XENT_DIRECTION_LTR;
	xtk_txn_set(
	  &txn, xtk_ref_existing(xtk_scene_root(&g->scene)), XTK_PROP_FLOW_DIRECTION,
	  (XtkValue) {.kind = XTK_VALUE_ENUM, .u.u32 = ( uint32_t ) dir}, (XtkChangeStamp) {0}
	);
	XtkBatchResult *result = xtk_txn_commit(&txn, NULL);
	if (result) xtk_batchresult_destroy(result);
}

static void sync_control_search(GalleryApp *g) {
	char const *items [CONTROL_SEARCH_LIMIT];
	int         count = gallery_search_items(g->model.control_query, items);
	xtk_ctrl_setitems(&g->scene, g->control_search, items, count);
}

static void clear_control_search(GalleryApp *g) {
	if (g->model.control_query [0]) return;
	xtk_suggest_settext(&g->scene, g->control_search, "", 0u, (XtkChangeStamp) {0});
	sync_control_search(g);
}

static void apply_pending_message(GalleryApp *g, XtkMsg msg) {
	gallery_model_update(&g->model, msg);
	switch (msg.tag) {
	case MSG_BACKDROP             : gallery_backdrop_apply(flux_app_window(g->flux), g->model.backdrop); break;
	case MSG_RTL                  : apply_flow(g); break;
	case MSG_CONTROL_SEARCH_TEXT  : sync_control_search(g); break;
	case MSG_CONTROL_SEARCH_QUERY : clear_control_search(g); break;
	case MSG_NAV_STYLE :
		xtk_nav_setdisplay(
		  &g->scene, g->nav, g->model.nav_style ? XTK_NAV_PANE_TOP : XTK_NAV_PANE_AUTO, (XtkChangeStamp) {0}
		);
		break;
	case MSG_MOTION_REDUCED : flux_set_animations_override(g->model.motion_reduced_override); break;
	default                 : break;
	}
}

static void flush(void *ctx) {
	GalleryApp *g = ( GalleryApp * ) ctx;
	if (!g->pending_head || g->rebuilding) return;
	int  initial = g->model.page;
	bool rebuild = !g->sync_page;
	while (g->pending_head) {
		GalleryPending *pending = g->pending_head;
		g->pending_head         = pending->next;
		apply_pending_message(g, pending->msg);
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

static XtkLocalId create_control_search(GalleryApp *g, XtkTxn *txn, XtkNodeRef nav) {
	XtkLocalId slot
	  = xtk_navslot_create(txn, nav, &(XtkNavSlotProps) {.set = XTK_NAVSLOT_SLOT_MASK, .slot = XTK_NAV_SLOT_SEARCH});
	XtkLocalId box = xtk_suggest_create(
	  txn, xtk_ref_local(slot),
	  &(XtkSuggestProps) {
		.set         = XTK_SUGGEST_PLACEHOLDER_MASK | XTK_SUGGEST_ICON_MASK,
		.placeholder = "Search controls",
		.icon        = "Search",
	  }
	);
	g->search_text_bind  = (GalleryBind) {.app = g, .tag = MSG_CONTROL_SEARCH_TEXT};
	g->search_query_bind = (GalleryBind) {.app = g, .tag = MSG_CONTROL_SEARCH_QUERY};
	xtk_suggest_onchange(txn, xtk_ref_local(box), page_post, &g->search_text_bind);
	xtk_suggest_onquery(txn, xtk_ref_local(box), page_post, &g->search_query_bind);
	return box;
}

static bool shell_build(GalleryApp *g) {
	XtkTxn txn;
	if (!xtk_txn_begin(&g->scene, &txn)) return false;
	XtkLocalId nav = xtk_nav_create(
	  &txn, xtk_ref_existing(xtk_scene_root(&g->scene)),
	  &(XtkNavProps) {
		.set         = XTK_NAV_GROW_MASK | XTK_NAV_SEL_MASK | XTK_NAV_BACK_BUTTON_MASK,
		.grow        = 1.0f,
		.sel         = g->model.page,
		.back_button = XTK_NAV_BACK_COLLAPSED}
	);
	xtk_txn_setnav(
	  &txn, xtk_ref_local(nav),
	  &(XtkNavDesc) {
		.items            = kNavItems,
		.nitem            = PAGE_COUNT,
		.sel              = g->model.page,
		.window_title_bar = XTK_NAV_TITLE_BAR_TALL,
		.app_title        = "FluXent Gallery",
	  }
	);
	g->nav_bind = (GalleryBind) {.app = g, .tag = MSG_NAV};
	xtk_nav_onselect(&txn, xtk_ref_local(nav), page_post, &g->nav_bind);
	XtkLocalId search = create_control_search(g, &txn, xtk_ref_local(nav));
	XtkLocalId frame  = xtk_frame_create(
	  &txn, xtk_ref_local(nav),
	  &(XtkFrameProps) {
		.set        = XTK_FRAME_GROW_MASK | XTK_FRAME_TRANSITION_MASK,
		.grow       = 1.0f,
		.transition = XTK_NAV_TRANSITION_ENTRANCE,
	  }
	);
	XtkBatchResult *result = xtk_txn_commit(&txn, NULL);
	if (!result) return false;
	g->nav            = gallery_result_ctrl(result, nav);
	g->control_search = gallery_result_ctrl(result, search);
	g->page_frame     = gallery_result_ctrl(result, frame);
	xtk_batchresult_destroy(result);
	if (g->nav != XTK_CTRL_INVALID) xtk_nav_setsettings_visible(&g->scene, g->nav, false, (XtkChangeStamp) {0});
	return g->nav != XTK_CTRL_INVALID && g->control_search != XTK_CTRL_INVALID && g->page_frame != XTK_CTRL_INVALID;
}

bool gallery_app_create(GalleryApp *g) {
	memset(g, 0, sizeof(*g));
	gallery_model_init(&g->model);
	g->model.backdrop = gallery_backdrop_load();
	g->flux           = flux_app_create(&(FluxAppCfg) {
	  .title    = L"FluXent Gallery (Imperative)",
	  .width    = 1100,
	  .height   = 760,
	  .backdrop = ( FluxBackdropType ) g->model.backdrop,
	});
	if (!g->flux) return false;
	g->host = flux_app_attachhost(g->flux);
	if (!g->host) return false;
	g->scene    = xtk_scene_fromhost(g->host);
	g->dispatch = dispatch;
	flux_app_onframe(g->flux, flush, g);
	if (!shell_build(g)) return false;
#ifdef GALLERY_HOT
	FluxHotCfg cfg = {
	  .module    = L"fluxent_gallery_imp_view.dll",
	  .watch     = GALLERY_ROOT L"/examples",
	  .build_dir = GALLERY_ROOT,
	  .build     = L"xmake build -y fluxent_gallery_imp_view",
	};
	g->hot = flux_hot_page_open(g->flux, &g->scene, g->page_frame, &cfg, g);
	return g->hot != NULL;
#else
	rebuild_page(g);
	return true;
#endif
}

void gallery_app_destroy(GalleryApp *g) {
	if (g->flux) flux_app_onframe(g->flux, NULL, NULL);
	while (g->pending_head) {
		GalleryPending *next = g->pending_head->next;
		free(g->pending_head->owned);
		free(g->pending_head);
		g->pending_head = next;
	}
#ifdef GALLERY_HOT
	flux_hot_page_close(( FluxHot * ) g->hot);
#else
	gallery_page_release(g->page);
#endif
	if (g->host) flux_host_destroy(g->host);
	if (g->flux) flux_app_destroy(g->flux);
	memset(g, 0, sizeof(*g));
}

int gallery_app_run(GalleryApp *g) { return flux_app_run(g->flux); }
