#include <fluxent/flux_controls.h>
#include <fluxent/flux_host.h>
#include <fluxent/flux_node_store.h>
#include <render/flux_display_paint.h>
#include <render/flux_engine.h>
#include <render/flux_render_cache.h>
#include <render/flux_render_internal.h>
#include <store/flux_node_data.h>

#include <xtk/xtk_alloc.h>
#include <xtk/xtk_batch.h>
#include <xtk/xtk_catalog.h>

#include <stdio.h>

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

/* The dirty-rect path is pure CPU: collect() reads layout and FluxNodeData, never D2D. */

static XentNodeId commit_one(XtkHost *host, XtkBatch *batch) {
	XtkPrepare  *prepare = NULL;
	XtkBatchDiag d;
	if (host->vt->prepare(host, batch, &prepare, &d) != XTK_BATCH_OK || !prepare) return XENT_NODE_INVALID;
	XtkBatchResult *result = host->vt->commit(host, prepare, &d);
	XentNodeId      id     = XENT_NODE_INVALID;
	if (result && result->control_count >= 1u) id = ( XentNodeId ) result->controls [0].control;
	if (result) xtk_batchresult_destroy(result);
	return id;
}

/* Mount a container, then insert two texts under it in a second transaction. */
static XentNodeId mount_row(XtkHost *host) {
	XtkBatch *first = xtk_batch_alloc(host->vt->revision(host));
	if (!first) return XENT_NODE_INVALID;
	XtkLocalId row = xtk_batch_create(first, XTK_CTRL_CONTAINER, xtk_ref_none(), xtk_ref_none(), NULL, 0u);
	if (row == XTK_LOCAL_INVALID) {
		xtk_batch_free(first);
		return XENT_NODE_INVALID;
	}
	XentNodeId row_id = commit_one(host, first);
	xtk_batch_free(first);
	if (row_id == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	XtkBatch *second = xtk_batch_alloc(host->vt->revision(host));
	if (!second) return row_id;
	XtkValue text = {
	  .kind = XTK_VALUE_STRING, .u.string = {.data = "x", .len = 1u}
    };
	XtkPropPatch props [] = {
	  {.id = XTK_PROP_TEXT, .value = text, .change = {0}}
    };
	XtkLocalId a = xtk_batch_create(second, XTK_CTRL_TEXT, xtk_ref_none(), xtk_ref_none(), props, 1u);
	XtkLocalId b = xtk_batch_create(second, XTK_CTRL_TEXT, xtk_ref_none(), xtk_ref_none(), props, 1u);
	if (a != XTK_LOCAL_INVALID
	    && b != XTK_LOCAL_INVALID
	    && xtk_batch_insert(second, xtk_ref_existing(( XtkCtrl ) row_id), xtk_ref_local(a), xtk_ref_none())
	    && xtk_batch_insert(second, xtk_ref_existing(( XtkCtrl ) row_id), xtk_ref_local(b), xtk_ref_none()))
	{
		( void ) commit_one(host, second);
	}
	xtk_batch_free(second);
	return row_id;
}

/* Run one collect + finish_frame pair so the engine has a baseline. */
static void settle_frame(FluxEngine *eng, XentCtx *ctx, XentNodeId root) {
	flux_engine_collect(eng, ctx, root);
	FluxRect ignored = {0};
	( void ) flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &ignored);
	flux_engine_finish_frame(eng, 0u);
}

static int
test_opacity_change_repaints(FluxEngine *eng, XentCtx *ctx, FluxStore *store, XentNodeId root, XentNodeId row) {
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	flux_engine_collect(eng, ctx, root);
	FluxRect calm = {0};
	EXPECT(flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &calm), "unchanged frame stays incremental");
	EXPECT(calm.w <= 0.0f && calm.h <= 0.0f, "an unchanged frame produces no dirty area");
	flux_engine_finish_frame(eng, 0u);

	FluxNodeData *nd = flux_store_find(store, row);
	EXPECT(nd, "row node data");
	nd->render_opacity = 0.5f;

	flux_engine_collect(eng, ctx, root);
	FluxRect animated = {0};
	/* A transform change repaints the frame in full, so the engine reports "no partial
	   rect" and the caller takes its full-present path. */
	EXPECT(
	  !flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &animated), "opacity change leaves the partial path"
	);
	flux_engine_finish_frame(eng, 0u);

	/* Rewriting the same value is not a change, or a node that recomputes its transform
	   every frame would repaint forever. */
	settle_frame(eng, ctx, root);
	nd                 = flux_store_find(store, row);
	nd->render_opacity = 0.5f;
	flux_engine_collect(eng, ctx, root);
	FluxRect steady = {0};
	EXPECT(flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &steady), "steady state stays incremental");
	EXPECT(steady.w <= 0.0f && steady.h <= 0.0f, "rewriting the same opacity does not dirty the frame");
	flux_engine_finish_frame(eng, 0u);
	return 0;
}

static int test_collapse_dirties_whole_subtree(FluxEngine *eng, XentCtx *ctx, XentNodeId root, XentNodeId row) {
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	XentNodeId a = xent_node_first(ctx, row);
	XentNodeId b = a == XENT_NODE_INVALID ? XENT_NODE_INVALID : xent_node_next(ctx, a);
	EXPECT(a != XENT_NODE_INVALID && b != XENT_NODE_INVALID, "row has two children");
	EXPECT(xent_node_first(ctx, a) == XENT_NODE_INVALID, "children are leaves");

	/* Collapse the container, not one leaf: both descendants kept their own bounds. */
	xent_setcollapsed(ctx, row, true);

	flux_engine_collect(eng, ctx, root);
	FluxRect hidden = {0};
	EXPECT(flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &hidden), "collapse frame stays incremental");
	EXPECT(hidden.w > 0.0f && hidden.h > 0.0f, "collapsing a container repaints the area it covered");
	flux_engine_finish_frame(eng, 0u);

	/* Every descendant record was cleared, so the next frame marks nothing again. */
	flux_engine_collect(eng, ctx, root);
	FluxRect settled = {0};
	( void ) flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &settled);
	EXPECT(settled.w <= 0.0f && settled.h <= 0.0f, "a collapsed subtree is marked dirty only once");
	flux_engine_finish_frame(eng, 0u);

	xent_setcollapsed(ctx, row, false);
	return 0;
}

/* The tween gate lives behind a predicate, so it is checkable without a device. */
static int test_tween_blocks_reuse(FluxStore *store, XentNodeId node) {
	FluxRenderCache *cache = flux_render_cache_create(64);
	EXPECT(cache, "render cache");
	FluxCacheEntry *entry = flux_rcache_ensure(cache, ( uint64_t ) node);
	EXPECT(entry, "cache entry");

	/* Give the node a real reusable list so the tween gate is what decides. */
	XentDlBuilder *b = xent_dl_begin();
	EXPECT(b, "display list builder");
	EXPECT(xent_dl_fillrect(b, (XentRect) {0.0f, 0.0f, 10.0f, 10.0f}, xent_color_rgba(0, 0, 0, 255)), "record fill");
	XentDl *list = xent_dl_end(b);
	EXPECT(list, "display list");
	flux_node_adoptdl(store, node, list);

	FluxNodeData *nd = flux_store_find(store, node);
	EXPECT(nd && nd->display_list && nd->display_list_owned, "node adopted the list");
	uint64_t hash = 0x1234u;
	float    w = 10.0f, h = 10.0f;
	nd->dl_hash    = hash;
	nd->dl_w       = w;
	nd->dl_h       = h;
	nd->dl_dynamic = 0u;

	EXPECT(flux_display_paint_can_reuse(nd, hash, w, h, false, entry), "a settled entry reuses the list");

	entry->press_anim.active = true;
	EXPECT(!flux_display_paint_can_reuse(nd, hash, w, h, false, entry), "a live press tween blocks reuse");
	entry->press_anim.active = false;

	entry->color_anim.active = true;
	EXPECT(!flux_display_paint_can_reuse(nd, hash, w, h, false, entry), "a live colour tween blocks reuse");
	entry->color_anim.active = false;

	EXPECT(!flux_rcache_entry_animating(entry), "settled entry reports no animation");
	EXPECT(flux_display_paint_can_reuse(nd, hash, w, h, false, entry), "reuse resumes once the tween settles");
	EXPECT(!flux_display_paint_can_reuse(nd, hash + 1u, w, h, false, entry), "a different hash is not reusable");

	nd->dl_dynamic = 1u;
	EXPECT(!flux_display_paint_can_reuse(nd, hash, w, h, false, entry), "a dynamic list is not reusable");

	flux_render_cache_destroy(cache);
	return 0;
}

int main(void) {
	flux_controls_all();

	XtkHost *host = flux_host_create();
	EXPECT(host, "host");

	XentNodeId row = mount_row(host);
	EXPECT(row != XENT_NODE_INVALID, "mounted row");

	XentCtx   *ctx   = flux_host_context(host);
	FluxStore *store = flux_host_store(host);
	EXPECT(ctx && store, "host context and store");

	FluxEngine *eng = flux_engine_create(store);
	EXPECT(eng, "engine");
	flux_engine_bind(eng, NULL);

	XentNodeId root = xent_node_parent(ctx, row);
	EXPECT(root != XENT_NODE_INVALID, "row parent");

	xent_setsize(ctx, root, (XentSize) {200.0f, 100.0f});
	xent_layout(ctx, root, 200.0f, 100.0f);

	if (test_opacity_change_repaints(eng, ctx, store, root, row)) return 1;
	if (test_collapse_dirties_whole_subtree(eng, ctx, root, row)) return 1;
	if (test_tween_blocks_reuse(store, row)) return 1;

	flux_engine_destroy(eng);
	flux_host_destroy(host);
	printf("PASS: dirty rects follow transforms, collapse the whole subtree and honour tweens\n");
	return 0;
}
