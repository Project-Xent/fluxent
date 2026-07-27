/**
 * @file test_subtree.c
 * @brief Headless test for flux_tree_drop and the node-removed listener.
 *
 * Builds a controls subtree via Host CREATE + SET_PROP batches, destroys it,
 * and asserts the store drained and the listener saw every node.
 */
#include <fluxent/flux_host.h>
#include "controls/data/flux_component_data.h"
#include "store/flux_node_data.h"
#include <fluxent/flux_input.h>
#include <fluxent/flux_node_store.h>
#include <xtk/xtk_batch.h>
#include <xtk/xtk_catalog.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int  g_removed_count;

static void count_removed(void *userdata, XentNodeId id) {
	FluxInput *input = ( FluxInput * ) userdata;
	flux_input_dropnode(input, id);
	g_removed_count++;
}

static bool value_string(XtkBatch *batch, XtkNodeRef target, XtkPropertyId id, char const *text) {
	if (!text) return true;
	XtkValue v = {
	  .kind = XTK_VALUE_STRING, .u.string = {.data = ( char * ) text, .len = ( uint32_t ) strlen(text)}
    };
	return xtk_batch_set(batch, target, id, v, (XtkChangeStamp) {0});
}

static XtkBatchResult *commit_batch(XtkHost *host, XtkBatch *batch) {
	XtkPrepare  *prepare = NULL;
	XtkBatchDiag diagnostic;
	if (host->vt->prepare(host, batch, &prepare, &diagnostic) != XTK_BATCH_OK || !prepare) return NULL;
	return host->vt->commit(host, prepare, &diagnostic);
}

static XentNodeId as_node(XtkCtrl c) { return ( XentNodeId ) c; }

static XentNodeId build_page(XtkHost *host, XentNodeId root) {
	XtkBatch  *batch  = xtk_batch_alloc(host->vt->revision(host));
	XtkNodeRef parent = xtk_ref_existing(( XtkCtrl ) root);

	XtkLocalId scroll = xtk_batch_create(batch, XTK_CTRL_SCROLL, parent, xtk_ref_none(), NULL, 0);
	XtkNodeRef page   = xtk_ref_local(scroll);

	XtkLocalId title  = xtk_batch_create(batch, XTK_CTRL_TEXT, page, xtk_ref_none(), NULL, 0);
	( void ) value_string(batch, xtk_ref_local(title), XTK_PROP_TEXT, "Title");
	{
		XtkValue sz = {.kind = XTK_VALUE_F32, .u.f32 = 24.0f};
		( void ) xtk_batch_set(batch, xtk_ref_local(title), XTK_PROP_FONT_SIZE, sz, (XtkChangeStamp) {0});
	}

	XtkLocalId button = xtk_batch_create(batch, XTK_CTRL_BUTTON, page, xtk_ref_none(), NULL, 0);
	( void ) value_string(batch, xtk_ref_local(button), XTK_PROP_TEXT, "Button");

	XtkLocalId check = xtk_batch_create(batch, XTK_CTRL_CHECKBOX, page, xtk_ref_none(), NULL, 0);
	( void ) value_string(batch, xtk_ref_local(check), XTK_PROP_TEXT, "Check");

	XtkLocalId slider = xtk_batch_create(batch, XTK_CTRL_SLIDER, page, xtk_ref_none(), NULL, 0);
	{
		XtkValue v = {.kind = XTK_VALUE_F32, .u.f32 = 0.5f};
		( void ) xtk_batch_set(batch, xtk_ref_local(slider), XTK_PROP_VALUE_F32, v, (XtkChangeStamp) {0});
	}

	( void ) xtk_batch_create(batch, XTK_CTRL_TEXTBOX, page, xtk_ref_none(), NULL, 0);
	( void ) xtk_batch_create(batch, XTK_CTRL_PROGRESS, page, xtk_ref_none(), NULL, 0);

	XtkLocalId card = xtk_batch_create(batch, XTK_CTRL_CARD, page, xtk_ref_none(), NULL, 0);
	XtkLocalId nest = xtk_batch_create(batch, XTK_CTRL_TEXT, xtk_ref_local(card), xtk_ref_none(), NULL, 0);
	( void ) value_string(batch, xtk_ref_local(nest), XTK_PROP_TEXT, "Nested");
	XtkLocalId nest_btn = xtk_batch_create(batch, XTK_CTRL_BUTTON, xtk_ref_local(card), xtk_ref_none(), NULL, 0);
	( void ) value_string(batch, xtk_ref_local(nest_btn), XTK_PROP_TEXT, "Nested button");

	XtkBatchResult *result  = commit_batch(host, batch);
	XentNodeId      page_id = XENT_NODE_INVALID;
	if (result && result->control_count > 0) page_id = as_node(result->controls [0].control);
	if (result) xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return page_id;
}

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

int main(void) {
	XtkHost *host = flux_host_create();
	EXPECT(host, "host creation");
	XentCtx   *ctx   = flux_host_context(host);
	FluxStore *store = flux_host_store(host);
	EXPECT(ctx && store, "context/store");

	FluxInput *input = flux_input_create(ctx, store);
	EXPECT(input, "input creation");
	flux_store_ondel(store, count_removed, input);

	XentNodeId root = as_node(host->vt->root(host));
	EXPECT(root != XENT_NODE_INVALID, "scene root");
	uint32_t   baseline = flux_store_n(store);
	XentNodeId page     = build_page(host, root);
	EXPECT(page != XENT_NODE_INVALID, "page created");
	uint32_t built = flux_store_n(store);
	EXPECT(built > baseline, "store grew while building");

	XentNodeId inner_button = xent_node_first(ctx, page);
	flux_input_setfocus(input, inner_button);

	g_removed_count = 0;
	flux_tree_drop(store, page);

	EXPECT(flux_store_n(store) == baseline, "store drained back to baseline");
	EXPECT(!xent_node_valid(ctx, page), "page node freed in xent");
	EXPECT(( uint32_t ) g_removed_count == built - baseline, "listener saw every node");
	EXPECT(flux_input_focus(input) == XENT_NODE_INVALID, "focus reference dropped");
	EXPECT(xent_node_nchild(ctx, root) == 0, "root detached from destroyed child");

	for (int i = 0; i < 1000; i++) {
		XentNodeId p = build_page(host, root);
		EXPECT(p != XENT_NODE_INVALID, "cycle page");
		flux_tree_drop(store, p);
	}
	EXPECT(flux_store_n(store) == baseline, "1000 cycles leave store clean");

	char volatile_label [32];
	snprintf(volatile_label, sizeof(volatile_label), "Transient %d", 42);
	XtkBatch  *batch = xtk_batch_alloc(host->vt->revision(host));
	XtkLocalId btn_local
	  = xtk_batch_create(batch, XTK_CTRL_BUTTON, xtk_ref_existing(( XtkCtrl ) root), xtk_ref_none(), NULL, 0);
	( void ) value_string(batch, xtk_ref_local(btn_local), XTK_PROP_TEXT, volatile_label);
	XtkBatchResult *result = commit_batch(host, batch);
	EXPECT(result && result->control_count == 1u, "transient button");
	XentNodeId btn = as_node(result->controls [0].control);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	memset(volatile_label, 'X', sizeof(volatile_label) - 1);

	FluxNodeData   *bnd = flux_store_find(store, btn);
	FluxButtonData *bd  = bnd ? ( FluxButtonData * ) bnd->component_data : NULL;
	EXPECT(bd && bd->label && strcmp(bd->label, "Transient 42") == 0, "control owns a copy of its label");
	flux_tree_drop(store, btn);

	flux_input_destroy(input);
	flux_host_destroy(host);
	printf("PASS: subtree destroy (%d total node removals across 1001 cycles)\n", g_removed_count);
	return 0;
}
