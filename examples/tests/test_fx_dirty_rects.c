#include <fluxent/flux_controls.h>
#include <fluxent/flux_host.h>
#include <fluxent/flux_node_store.h>
#include <store/flux_node_data.h>
#include <render/flux_engine.h>
#include <render/flux_render_internal.h>

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

static XentNodeId mount_three_nodes(XtkHost *host) {
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	if (!batch) return XENT_NODE_INVALID;
	XtkValue text = {
	  .kind = XTK_VALUE_STRING, .u.string = {.data = "x", .len = 1u}
    };
	XtkPropPatch props [] = {
	  {.id = XTK_PROP_TEXT, .value = text, .change = {0}}
    };
	XtkLocalId row  = xtk_batch_create(batch, XTK_CTRL_CONTAINER, xtk_ref_none(), xtk_ref_none(), NULL, 0u);
	XtkLocalId a    = xtk_batch_create(batch, XTK_CTRL_TEXT, xtk_ref_local(row), xtk_ref_none(), props, 1u);
	XtkLocalId b    = xtk_batch_create(batch, XTK_CTRL_TEXT, xtk_ref_local(row), xtk_ref_none(), props, 1u);
	XentNodeId root = XENT_NODE_INVALID;
	if (row == XTK_LOCAL_INVALID || a == XTK_LOCAL_INVALID || b == XTK_LOCAL_INVALID) goto done;

	XtkPrepare  *prepare = NULL;
	XtkBatchDiag d;
	if (host->vt->prepare(host, batch, &prepare, &d) != XTK_BATCH_OK || !prepare) goto done;
	XtkBatchResult *result = host->vt->commit(host, prepare, &d);
	if (!result) goto done;
	if (result->control_count >= 1u) root = ( XentNodeId ) result->controls [0].control;
	xtk_batchresult_destroy(result);
done:
	xtk_batch_free(batch);
	return root;
}

static int
test_opacity_change_dirties(XentCtx *ctx, FluxStore *store, FluxEngine *eng, XentNodeId root, XentNodeId card) {
	flux_engine_collect(eng, ctx, root);
	FluxRect first = {0};
	/* Establish the baseline; the first frame has no previous state to compare against. */
	( void ) flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &first);
	flux_engine_finish_frame(eng, 0u);

	flux_engine_collect(eng, ctx, root);
	FluxRect calm = {0};
	EXPECT(flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &calm), "second frame stays incremental");
	EXPECT(calm.w <= 0.0f && calm.h <= 0.0f, "an unchanged frame produces no dirty area");
	flux_engine_finish_frame(eng, 0u);

	FluxNodeData *nd = flux_store_find(store, card);
	EXPECT(nd, "card node data");
	nd->render_opacity = 0.5f;

	flux_engine_collect(eng, ctx, root);
	FluxRect animated    = {0};
	/* A transform change repaints the frame in full, so the engine reports "no partial
	   rect" and the caller takes its full-present path. */
	bool     incremental = flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &animated);
	EXPECT(!incremental, "a render_opacity change leaves the partial path");
	flux_engine_finish_frame(eng, 0u);
	return 0;
}

static int
test_collapse_dirties_old_bounds(XentCtx *ctx, FluxStore *store, FluxEngine *eng, XentNodeId root, XentNodeId card) {
	flux_engine_collect(eng, ctx, root);
	FluxRect ignored = {0};
	( void ) flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &ignored);
	flux_engine_finish_frame(eng, 0u);

	flux_engine_collect(eng, ctx, root);
	FluxRect calm = {0};
	( void ) flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &calm);
	flux_engine_finish_frame(eng, 0u);

	xent_setcollapsed(ctx, card, true);

	flux_engine_collect(eng, ctx, root);
	FluxRect hidden = {0};
	EXPECT(flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &hidden), "collapse frame stays incremental");
	EXPECT(hidden.w > 0.0f && hidden.h > 0.0f, "a collapsed subtree repaints the area it used to cover");
	flux_engine_finish_frame(eng, 0u);

	/* The record was dropped, so the following frame no longer marks the same area. */
	flux_engine_collect(eng, ctx, root);
	FluxRect settled = {0};
	( void ) flux_engine_dirty_rect(eng, 0u, 1000.0f, 1000.0f, false, &settled);
	EXPECT(settled.w <= 0.0f && settled.h <= 0.0f, "a hidden subtree is marked dirty only once");
	flux_engine_finish_frame(eng, 0u);
	return 0;
}

int main(void) {
	flux_controls_all();

	XtkHost *host = flux_host_create();
	EXPECT(host, "host");

	XentNodeId root = mount_three_nodes(host);
	EXPECT(root != XENT_NODE_INVALID, "mounted root");

	XentCtx   *ctx   = flux_host_context(host);
	FluxStore *store = flux_host_store(host);
	EXPECT(ctx && store, "host context and store");

	FluxEngine *eng = flux_engine_create(store);
	EXPECT(eng, "engine");
	flux_engine_bind(eng, NULL);

	/* Move the mounted row so the collected geometry is stable and non-empty. */
	xent_setsize(ctx, root, (XentSize) {200.0f, 100.0f});
	xent_layout(ctx, root, 200.0f, 100.0f);

	XentNodeId card = xent_node_first(ctx, root);
	EXPECT(card != XENT_NODE_INVALID, "row child");

	if (test_opacity_change_dirties(ctx, store, eng, root, card)) return 1;
	if (test_collapse_dirties_old_bounds(ctx, store, eng, root, card)) return 1;

	flux_engine_destroy(eng);
	flux_host_destroy(host);
	printf("PASS: dirty rects follow render transforms and collapse\n");
	return 0;
}
