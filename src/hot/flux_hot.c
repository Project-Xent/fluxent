#include "hot/flux_hot_internal.h"

#include <fluxent/flux_controls.h>
#include <fluxent/flux_host.h>
#include <xtk/xtk_hot.h>
#include <xtk/xtk_reconcile.h>

#include <stdlib.h>
#include <string.h>

typedef struct HotRoots {
	XtkCtrl *items;
	uint32_t count;
} HotRoots;

struct FluxHot {
	FluxApp       *app;
	FluxHotLoader *loader;
	XtkHost       *host;
	XtkRt         *rt;
	XtkScene      *scene;
	XtkCtrl        slot;
	void          *page;
	HotRoots       roots;
	bool           page_mode;
	void          *model;
};

bool                       flux_host_replacecomp(XtkHost *host, XtkCompType const *type);
void                       flux_host_dropcomp(XtkHost *host, XtkCtrlId id);

static XtkHotModule const *desc_of(void *lib) {
	XtkHotEntryFn entry = ( XtkHotEntryFn ) ( void (*)(void) ) flux_hot_symbol(lib, XTK_HOT_SYMBOL);
	return entry ? entry() : NULL;
}

static bool frame_ok(XtkFrameStatus status) { return status == XTK_FRAME_COMMITTED || status == XTK_FRAME_CLEAN; }

static bool desc_has(XtkHotModule const *desc, XtkCtrlId id) {
	for (uint32_t i = 0; i < desc->comp_count; i++)
		if (desc->comps [i]->control->id == id) return true;
	return false;
}

static void comps_drop_missing(FluxHot *hot, XtkHotModule const *from, XtkHotModule const *keep) {
	for (uint32_t i = 0; i < from->comp_count; i++) {
		XtkCtrlId id = from->comps [i]->control->id;
		if (!desc_has(keep, id)) flux_host_dropcomp(hot->host, id);
	}
}

static bool comps_install(FluxHot *hot, XtkHotModule const *desc) {
	for (uint32_t i = 0; i < desc->comp_count; i++) {
		XtkCompType const *type = desc->comps [i];
		if (!flux_host_replacecomp(hot->host, type) || !xtk_rt_remount(hot->rt, type->control->id)) return false;
	}
	return true;
}

static bool comps_register(FluxHot *hot, XtkHotModule const *desc) {
	for (uint32_t i = 0; i < desc->comp_count; i++)
		if (!flux_host_registercomp(hot->host, desc->comps [i])) return false;
	return true;
}

static FluxHotEvent view_swap(FluxHot *hot, XtkHotModule const *cur, XtkHotModule const *next) {
	bool ok = comps_install(hot, next) && xtk_rt_swap(hot->rt, next->update, next->view);
	if (ok && frame_ok(xtk_rt_frame(hot->rt).status)) {
		comps_drop_missing(hot, cur, next);
		return FLUX_HOT_RELOADED;
	}
	comps_drop_missing(hot, next, cur);
	( void ) comps_install(hot, cur);
	( void ) xtk_rt_swap(hot->rt, cur->update, cur->view);
	( void ) xtk_rt_frame(hot->rt);
	return FLUX_HOT_FRAME_FAILED;
}

static bool page_drop_roots(FluxHot *hot, XtkTxn *txn) {
	for (uint32_t i = 0; i < hot->roots.count; i++)
		if (!xtk_txn_drop(txn, xtk_ref_existing(hot->roots.items [i]))) return false;
	return true;
}

static bool slot_create(FluxHot const *hot, XtkBatchOp const *op) {
	if (op->kind != XTK_BATCH_OP_CREATE || op->u.create.parent.kind != XTK_REF_EXISTING) return false;
	return op->u.create.parent.u.existing == hot->slot;
}

static bool roots_collect(FluxHot const *hot, XtkBatch const *batch, HotRoots *out) {
	uint32_t n = 0;
	for (uint32_t i = 0; i < batch->count; i++) n += slot_create(hot, &batch->ops [i]);
	out->items = n ? ( XtkCtrl * ) calloc(n, sizeof(XtkCtrl)) : NULL;
	if (n && !out->items) return false;
	for (uint32_t i = 0; i < batch->count; i++)
		if (slot_create(hot, &batch->ops [i])) out->items [out->count++] = ( XtkCtrl ) batch->ops [i].u.create.local;
	return true;
}

static void roots_resolve(HotRoots *roots, XtkBatchResult const *result) {
	for (uint32_t i = 0; i < roots->count; i++) {
		XtkLocalId local = ( XtkLocalId ) roots->items [i];
		roots->items [i] = XTK_CTRL_INVALID;
		for (uint32_t j = 0; j < result->control_count; j++)
			if (result->controls [j].local == local) roots->items [i] = result->controls [j].control;
	}
}

static void page_release(XtkHotModule const *desc, void *page) {
	if (page && desc->page_release) desc->page_release(page);
}

static bool page_stage(FluxHot *hot, XtkHotModule const *desc, XtkTxn *txn, void **page, HotRoots *roots) {
	if (!page_drop_roots(hot, txn)) return false;
	if (!desc->page_build(hot->scene, txn, xtk_ref_existing(hot->slot), hot->model, page)) return false;
	return roots_collect(hot, xtk_txn_batch(txn), roots);
}

static bool page_commit(FluxHot *hot, XtkHotModule const *desc, void **out) {
	XtkTxn   txn;
	void    *page  = NULL;
	HotRoots fresh = {0};
	if (!xtk_txn_begin(hot->scene, &txn)) return false;
	XtkBatchResult *result = page_stage(hot, desc, &txn, &page, &fresh) ? xtk_txn_commit(&txn, NULL) : NULL;
	if (!result) {
		xtk_txn_abort(&txn);
		page_release(desc, page);
		free(fresh.items);
		return false;
	}
	roots_resolve(&fresh, result);
	if (desc->page_adopt) desc->page_adopt(page, result);
	xtk_batchresult_destroy(result);
	free(hot->roots.items);
	hot->roots = fresh;
	*out       = page;
	return true;
}

static FluxHotEvent page_swap(FluxHot *hot, XtkHotModule const *cur, XtkHotModule const *next) {
	void *old  = hot->page;
	void *page = NULL;
	if (!page_commit(hot, next, &page)) return FLUX_HOT_FRAME_FAILED;
	hot->page = page;
	page_release(cur, old);
	return FLUX_HOT_RELOADED;
}

static FluxHotEvent hot_start(FluxHot *hot, XtkHotModule const *desc) {
	if (hot->page_mode) return page_commit(hot, desc, &hot->page) ? FLUX_HOT_RELOADED : FLUX_HOT_FRAME_FAILED;
	hot->host  = flux_app_attachhost(hot->app);
	bool ready = hot->host && comps_register(hot, desc);
	hot->rt    = ready ? flux_app_createruntime(hot->app, hot->model, desc->update, desc->view) : NULL;
	return hot->rt ? FLUX_HOT_RELOADED : FLUX_HOT_LOAD_FAILED;
}

static FluxHotEvent compat_event(XtkHotCompat compat) {
	switch (compat) {
	case XTK_HOT_COMPATIBLE    : return FLUX_HOT_RELOADED;
	case XTK_HOT_ABI_CHANGED   : return FLUX_HOT_ABI_CHANGED;
	case XTK_HOT_MODEL_CHANGED : return FLUX_HOT_MODEL_CHANGED;
	default                    : return FLUX_HOT_LOAD_FAILED;
	}
}

static bool module_fits(FluxHot const *hot, XtkHotModule const *desc) {
	return hot->page_mode ? desc->page_build != NULL : desc->update && desc->view;
}

static FluxHotEvent hot_swap(void *ctx, void *prev, void *next) {
	FluxHot            *hot = ( FluxHot * ) ctx;
	XtkHotModule const *cur = prev ? desc_of(prev) : NULL;
	XtkHotModule const *nd  = desc_of(next);
	if (!nd || (prev && !cur)) return FLUX_HOT_LOAD_FAILED;
	FluxHotEvent event = compat_event(xtk_hot_compat(cur, nd));
	if (event == FLUX_HOT_RELOADED && !module_fits(hot, nd)) event = FLUX_HOT_LOAD_FAILED;
	if (event != FLUX_HOT_RELOADED) return event;
	if (nd->on_load) nd->on_load(hot->app);
	if (!cur) event = hot_start(hot, nd);
	else event = hot->page_mode ? page_swap(hot, cur, nd) : view_swap(hot, cur, nd);
	if (event != FLUX_HOT_RELOADED && nd->on_unload) nd->on_unload();
	return event;
}

static uintptr_t image_end(void *lib) {
	IMAGE_DOS_HEADER const *dos = ( IMAGE_DOS_HEADER const * ) lib;
	IMAGE_NT_HEADERS const *nt  = ( IMAGE_NT_HEADERS const * ) (( unsigned char const * ) lib + dos->e_lfanew);
	return ( uintptr_t ) lib + nt->OptionalHeader.SizeOfImage;
}

static bool model_refers(void const *model, uint32_t size, void *lib) {
	uintptr_t            lo    = ( uintptr_t ) lib;
	uintptr_t            hi    = image_end(lib);
	unsigned char const *bytes = ( unsigned char const * ) model;
	for (uint32_t off = 0; off + sizeof(uintptr_t) <= size; off += ( uint32_t ) sizeof(uintptr_t)) {
		uintptr_t v;
		memcpy(&v, bytes + off, sizeof v);
		if (v >= lo && v < hi) return true;
	}
	return false;
}

static bool hot_retire(void *ctx, void *prev, void *cur) {
	FluxHot            *hot = ( FluxHot * ) ctx;
	XtkHotModule const *pd  = desc_of(prev);
	XtkHotModule const *cd  = desc_of(cur);
	if (!pd || !cd || !(pd->flags & cd->flags & XTK_HOT_UNLOADABLE)) return false;
	if (!model_refers(hot->model, cd->model_size, prev)) return true;
	flux_hot_print("[hot] Model still points into an older module; keeping it loaded\n");
	return false;
}

static void hot_unload(void *ctx, void *lib) {
	( void ) ctx;
	XtkHotModule const *desc = desc_of(lib);
	if (desc && desc->on_unload) desc->on_unload();
}

static FluxHotLoader *hot_open(FluxHot *hot, FluxHotCfg const *cfg) {
	FluxHotHooks hooks = {.swap = hot_swap, .retire = hot_retire, .unload = hot_unload, .ctx = hot};
	return flux_hot_loader_open(hot->app, cfg, &hooks);
}

static int hot_loop(FluxHot *hot, FluxAppCfg const *app_cfg) {
	if (app_cfg && app_cfg->on_start) app_cfg->on_start(hot->app, app_cfg->user);
	int rc = flux_app_run(hot->app);
	flux_app_onframe(hot->app, NULL, NULL);
	flux_hot_loader_stop(hot->loader);
	if (app_cfg && app_cfg->on_stop) app_cfg->on_stop(hot->app, app_cfg->user);
	return rc;
}

int flux_run_hot(FluxAppCfg const *app_cfg, FluxHotCfg const *cfg, void *model) {
	if (!cfg || !model) return 1;
	FluxHot hot = {.model = model};
	flux_controls_all();
	hot.app = flux_app_create(app_cfg);
	if (!hot.app) return 1;
	hot.loader = hot_open(&hot, cfg);
	int rc     = hot.loader ? hot_loop(&hot, app_cfg) : 1;
	flux_hot_loader_stop(hot.loader);
	if (hot.rt && !xtk_rt_destroy(hot.rt)) rc = 1;
	if (hot.host) flux_host_destroy(hot.host);
	flux_app_destroy(hot.app);
	flux_hot_loader_close(hot.loader);
	return rc;
}

FluxHot *flux_hot_page_open(FluxApp *app, XtkScene *scene, XtkCtrl slot, FluxHotCfg const *cfg, void *model) {
	if (!app || !scene || slot == XTK_CTRL_INVALID || !cfg || !model) return NULL;
	FluxHot *hot = ( FluxHot * ) calloc(1, sizeof(*hot));
	if (!hot) return NULL;
	*hot        = (FluxHot) {.app = app, .scene = scene, .slot = slot, .page_mode = true, .model = model};
	hot->loader = hot_open(hot, cfg);
	if (hot->loader) return hot;
	free(hot->roots.items);
	free(hot);
	return NULL;
}

bool flux_hot_page_rebuild(FluxHot *hot) {
	XtkHotModule const *desc = hot ? desc_of(flux_hot_loader_current(hot->loader)) : NULL;
	return desc && page_swap(hot, desc, desc) == FLUX_HOT_RELOADED;
}

static void page_drop(FluxHot *hot) {
	XtkTxn txn;
	if (!xtk_txn_begin(hot->scene, &txn)) return;
	if (!page_drop_roots(hot, &txn)) {
		xtk_txn_abort(&txn);
		return;
	}
	XtkBatchResult *result = xtk_txn_commit(&txn, NULL);
	if (result) xtk_batchresult_destroy(result);
}

void flux_hot_page_close(FluxHot *hot) {
	if (!hot) return;
	flux_hot_loader_stop(hot->loader);
	page_drop(hot);
	XtkHotModule const *desc = desc_of(flux_hot_loader_current(hot->loader));
	if (desc) page_release(desc, hot->page);
	flux_hot_loader_close(hot->loader);
	free(hot->roots.items);
	free(hot);
}
