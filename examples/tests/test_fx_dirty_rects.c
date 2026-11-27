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

/* Mount a chain of `depth` nested containers under `parent`, one transaction each. */
static XentNodeId mount_chain(XtkHost *host, XentNodeId parent, int depth) {
	XentNodeId top = XENT_NODE_INVALID;
	for (int i = 0; i < depth; i++) {
		XtkBatch *b = xtk_batch_alloc(host->vt->revision(host));
		if (!b) return top;
		XtkNodeRef par = parent == XENT_NODE_INVALID ? xtk_ref_none() : xtk_ref_existing(( XtkCtrl ) parent);
		XtkLocalId id  = xtk_batch_create(b, XTK_CTRL_CONTAINER, par, xtk_ref_none(), NULL, 0u);
		if (id == XTK_LOCAL_INVALID) {
			xtk_batch_free(b);
			return top;
		}
		XentNodeId made = commit_one(host, b);
		xtk_batch_free(b);
		if (made == XENT_NODE_INVALID) return top;
		top    = made;
		parent = made;
	}
	return top;
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

/* A transform that steps by less than the epsilon must still cross it eventually: the
   baseline may only advance on a frame that was actually dirtied. */
static int
test_sub_epsilon_steps_accumulate(FluxEngine *eng, XentCtx *ctx, FluxStore *store, XentNodeId root, XentNodeId row) {
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	FluxNodeData *nd = flux_store_find(store, row);
	EXPECT(nd, "row node data");
	nd->render_opacity = 1.0f;
	settle_frame(eng, ctx, root);

	/* Two steps of 0.0005 each stay below the 0.001 epsilon on their own, but sum to
	   0.001 against the last dirtied value. */
	bool crossed = false;
	for (int step = 0; step < 4 && !crossed; step++) {
		nd->render_opacity -= 0.0005f;
		flux_engine_collect(eng, ctx, root);
		FluxRect r = {0};
		/* Leaving the partial path means the frame repaints in full. */
		if (!flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &r)) crossed = true;
		flux_engine_finish_frame(eng, 0u);
	}
	EXPECT(crossed, "sub-epsilon steps accumulate into a repaint");
	return 0;
}

/* A node that has some other dirty change every frame must still let sub-epsilon
   transform steps accumulate. The baseline may only advance when the transform itself
   is what dirtied the frame. */
static int test_sub_epsilon_steps_with_other_dirt(
  FluxEngine *eng, XentCtx *ctx, FluxStore *store, XentNodeId root, XentNodeId row
) {
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	FluxNodeData *nd = flux_store_find(store, row);
	EXPECT(nd, "row node data");
	nd->render_opacity = 1.0f;
	settle_frame(eng, ctx, root);

	bool crossed = false;
	for (int step = 0; step < 6 && !crossed; step++) {
		/* An unrelated visual change dirties the node every frame: the snapshot background
		   feeds flux_snap_hash, so the collect hash moves with it. */
		nd->visuals.background.rgba  = 0xff000000u | ( uint32_t ) (step * 0x101010u);
		nd->render_opacity          -= 0.0005f;

		flux_engine_collect(eng, ctx, root);

		/* Prove the unrelated change alone dirties the frame: opacity is still within
		   epsilon of the baseline on the early steps, so a dirty rect here is not from it. */
		FluxRect r       = {0};
		bool     partial = flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &r);
		bool     dirtied = !partial || r.w > 0.0f || r.h > 0.0f;
		EXPECT(dirtied, "each unrelated change dirties the frame on its own");
		if (!partial) crossed = true;
		flux_engine_finish_frame(eng, 0u);
	}
	EXPECT(crossed, "sub-epsilon steps accumulate even with other dirty changes");
	return 0;
}

/* A scaled or translated subtree painted outside its layout bounds, so collapsing it
   must not rely on those bounds alone. */
static int
test_collapse_transformed_subtree(FluxEngine *eng, XentCtx *ctx, FluxStore *store, XentNodeId root, XentNodeId row) {
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	FluxNodeData *nd = flux_store_find(store, row);
	EXPECT(nd, "row node data");
	nd->render_scale       = 2.0f;
	nd->render_translate_x = 40.0f;
	settle_frame(eng, ctx, root);

	/* The transform must have painted, and its extent is not in prev_bounds. */
	xent_setcollapsed(ctx, row, true);
	flux_engine_collect(eng, ctx, root);
	FluxRect hidden = {0};
	/* force_full means the caller takes the full-present path, which covers the
	   pixels the transform painted beyond the recorded layout bounds. */
	EXPECT(
	  !flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &hidden),
	  "collapsing a transformed subtree forces a full repaint"
	);
	flux_engine_finish_frame(eng, 0u);

	xent_setcollapsed(ctx, row, false);
	nd = flux_store_find(store, row);
	if (nd) {
		nd->render_scale       = 1.0f;
		nd->render_translate_x = 0.0f;
	}
	settle_frame(eng, ctx, root);
	return 0;
}

/* The cleanup walks the subtree with an explicit stack, so depth must not be a limit.
   Builds a chain deeper than the walker's initial stack capacity (64). */
static int test_collapse_deep_subtree(FluxEngine *eng, XentCtx *ctx, XtkHost *host, XentNodeId root, XentNodeId row) {
	XentNodeId deep = mount_chain(host, row, 128);
	EXPECT(deep != XENT_NODE_INVALID && deep != row, "mounted a deep chain");

	/* Count the chain so a silently failed mount cannot pass this test. */
	int depth = 0;
	for (XentNodeId n = deep; n != XENT_NODE_INVALID && n != row; n = xent_node_parent(ctx, n)) depth++;
	EXPECT(depth >= 100, "the chain is deeper than the walker's initial capacity");

	xent_layout(ctx, root, 200.0f, 100.0f);
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	xent_setcollapsed(ctx, row, true);
	flux_engine_collect(eng, ctx, root);
	FluxRect hidden = {0};
	EXPECT(flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &hidden), "deep collapse stays incremental");
	EXPECT(hidden.w > 0.0f && hidden.h > 0.0f, "deep collapse repaints the covered area");
	flux_engine_finish_frame(eng, 0u);

	/* A second collect must not re-mark the same area for every descendant. */
	flux_engine_collect(eng, ctx, root);
	FluxRect settled = {0};
	( void ) flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &settled);
	EXPECT(settled.w <= 0.0f && settled.h <= 0.0f, "deep collapse is marked dirty only once");
	flux_engine_finish_frame(eng, 0u);

	xent_setcollapsed(ctx, row, false);
	return 0;
}

/* Resetting the transform and collapsing in the same frame must still cover the pixels
   the previous frame painted under that transform. */
static int
test_collapse_after_transform_reset(FluxEngine *eng, XentCtx *ctx, FluxStore *store, XentNodeId root, XentNodeId row) {
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	FluxNodeData *nd = flux_store_find(store, row);
	EXPECT(nd, "row node data");
	nd->render_scale = 2.0f;
	settle_frame(eng, ctx, root); /* paints while scaled */

	nd->render_scale = 1.0f;      /* reset, then collapse in the same frame */
	xent_setcollapsed(ctx, row, true);
	flux_engine_collect(eng, ctx, root);
	FluxRect hidden = {0};
	EXPECT(
	  !flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &hidden),
	  "collapse after a transform reset still forces a full repaint"
	);
	flux_engine_finish_frame(eng, 0u);

	xent_setcollapsed(ctx, row, false);
	settle_frame(eng, ctx, root);
	return 0;
}

/* A subtree under a transformed ancestor paints through that ancestor's projection, so
   its own layout bounds do not cover its pixels either. */
static int test_collapse_under_transformed_ancestor(
  FluxEngine *eng, XentCtx *ctx, FluxStore *store, XentNodeId root, XentNodeId row
) {
	settle_frame(eng, ctx, root);
	settle_frame(eng, ctx, root);

	FluxNodeData *parent = flux_store_find(store, row);
	EXPECT(parent, "row node data");
	parent->render_scale = 2.0f;
	settle_frame(eng, ctx, root);

	/* Collapse a child while the ancestor keeps its transform. */
	XentNodeId child = xent_node_first(ctx, row);
	EXPECT(child != XENT_NODE_INVALID, "row has a child");
	xent_setcollapsed(ctx, child, true);

	flux_engine_collect(eng, ctx, root);
	FluxRect hidden = {0};
	EXPECT(
	  !flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &hidden),
	  "collapsing under a transformed ancestor forces a full repaint"
	);
	flux_engine_finish_frame(eng, 0u);

	xent_setcollapsed(ctx, child, false);
	parent = flux_store_find(store, row);
	if (parent) parent->render_scale = 1.0f;
	settle_frame(eng, ctx, root);
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
	if (test_sub_epsilon_steps_accumulate(eng, ctx, store, root, row)) return 1;
	if (test_sub_epsilon_steps_with_other_dirt(eng, ctx, store, root, row)) return 1;
	if (test_collapse_dirties_whole_subtree(eng, ctx, root, row)) return 1;
	if (test_collapse_transformed_subtree(eng, ctx, store, root, row)) return 1;
	if (test_collapse_after_transform_reset(eng, ctx, store, root, row)) return 1;
	if (test_collapse_under_transformed_ancestor(eng, ctx, store, root, row)) return 1;
	if (test_collapse_deep_subtree(eng, ctx, host, root, row)) return 1;
	if (test_tween_blocks_reuse(store, row)) return 1;

	flux_engine_destroy(eng);
	flux_host_destroy(host);
	printf("PASS: dirty rects follow transforms, collapse the whole subtree and honour tweens\n");
	return 0;
}
