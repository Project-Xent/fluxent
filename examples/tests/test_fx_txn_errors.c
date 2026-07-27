/**
 * @file test_fx_txn_errors.c
 * @brief Commit-time error granularity: injected allocation failure must surface
 * as XTK_BATCH_ERR_OOM, a tree that diverged from preflight as XTK_BATCH_ERR_VALIDATION.
 */
#include <fluxent/flux_host.h>
#include <xtk/xtk_batch.h>
#include <xtk/xtk_catalog.h>

#include "bridge/flux_internal.h"

#include "xtk_alloc_internal.h" /* kit fault-injection hooks; tests may reach into kit's src */

#include <stdio.h>

#define EXPECT(cond, msg)              \
	do {                               \
		if (!(cond)) {                 \
			printf("FAIL: %s\n", msg); \
			return 1;                  \
		}                              \
	}                                  \
	while (0)

static XtkBatch *three_node_batch(XtkHost *host) {
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	if (!batch) return NULL;
	XtkValue text = {
	  .kind = XTK_VALUE_STRING, .u.string = {.data = "x", .len = 1u}
    };
	XtkPropPatch props [] = {
	  {.id = XTK_PROP_TEXT, .value = text, .change = {0}}
    };
	XtkLocalId row = xtk_batch_create(batch, XTK_CTRL_CONTAINER, xtk_ref_none(), xtk_ref_none(), NULL, 0u);
	XtkLocalId a   = xtk_batch_create(batch, XTK_CTRL_TEXT, xtk_ref_none(), xtk_ref_none(), props, 1u);
	XtkLocalId b   = xtk_batch_create(batch, XTK_CTRL_TEXT, xtk_ref_none(), xtk_ref_none(), props, 1u);
	if (row == XTK_LOCAL_INVALID || a == XTK_LOCAL_INVALID || b == XTK_LOCAL_INVALID) {
		xtk_batch_free(batch);
		return NULL;
	}
	return batch;
}

static XtkBatch *allocation_batch(XtkHost *host) {
	XtkBatch *batch = xtk_batch_alloc(host->vt->revision(host));
	if (!batch) return NULL;
	XtkValue text = {
	  .kind = XTK_VALUE_STRING, .u.string = {.data = "x", .len = 1u}
    };
	XtkPropPatch props [] = {
	  {.id = XTK_PROP_TEXT, .value = text, .change = {0}}
    };
	for (uint32_t i = 0; i < 12u; i++)
		if (xtk_batch_create(batch, XTK_CTRL_TEXT, xtk_ref_none(), xtk_ref_none(), props, 1u) == XTK_LOCAL_INVALID) {
			xtk_batch_free(batch);
			return NULL;
		}
	return batch;
}

/* Sweep every allocation point from host creation through commit: any failure,
 * wherever it lands, must be reported as OOM — never VALIDATION — and the host
 * must stay destructible with balanced xtk allocation stats. */
static int test_commit_oom_sweep(void) {
	bool saw_failure = false;
	bool saw_success = false;
	for (uint64_t fail_at = 1; fail_at <= 256; fail_at++) {
		xtk_alloc_test_reset();
		xtk_alloc_fail_at(fail_at);

		XtkHost  *host  = flux_host_create();
		XtkBatch *batch = host ? allocation_batch(host) : NULL;
		if (!host || !batch) {
			xtk_alloc_fail_at(0);
			if (batch) xtk_batch_free(batch);
			if (host) flux_host_destroy(host);
			XtkAllocStats stats = xtk_alloc_test_stats();
			EXPECT(stats.failures == 1u, "construction fails only at injected allocation");
			EXPECT(stats.live_bytes == 0u, "construction failure leaks nothing");
			saw_failure = true;
			continue;
		}

		XtkPrepare   *prepare = NULL;
		XtkBatchDiag  d;
		XtkBatchError perr = host->vt->prepare(host, batch, &prepare, &d);
		if (perr != XTK_BATCH_OK || !prepare) {
			xtk_alloc_fail_at(0);
			EXPECT(perr == XTK_BATCH_ERR_OOM, "prepare-time allocation failure reports OOM");
			xtk_batch_free(batch);
			flux_host_destroy(host);
			XtkAllocStats stats = xtk_alloc_test_stats();
			EXPECT(stats.failures == 1u, "prepare fails only at injected allocation");
			EXPECT(stats.live_bytes == 0u, "prepare failure leaks nothing");
			saw_failure = true;
			continue;
		}

		XtkBatchResult *result = host->vt->commit(host, prepare, &d);
		xtk_alloc_fail_at(0);
		if (!result) {
			EXPECT(d.error == XTK_BATCH_ERR_OOM, "commit-time allocation failure reports OOM");
			saw_failure = true;
		}
		else {
			xtk_batch_result_destroy(result);
			saw_success = true;
		}
		xtk_batch_free(batch);
		flux_host_destroy(host);

		XtkAllocStats s = xtk_alloc_test_stats();
		EXPECT(s.failures == (result ? 0u : 1u), "commit succeeds only after the injection range");
		EXPECT(s.live_bytes == 0u, "failed commit leaks nothing");
		if (saw_success) break;
	}
	EXPECT(saw_failure && saw_success, "sweep reaches every commit allocation");
	return 0;
}

/* The batch passes preflight with live refs, but `before` is not a child of
 * `parent`, so apply fails structurally: VALIDATION with the op index, and the
 * rollback must leave the tree untouched. */
static int test_commit_diverged_tree_reports_validation(void) {
	XtkHost *host = flux_host_create();
	EXPECT(host, "host");

	XtkBatch *batch = three_node_batch(host);
	EXPECT(batch, "batch");
	XtkPrepare  *prepare = NULL;
	XtkBatchDiag d;
	EXPECT(host->vt->prepare(host, batch, &prepare, &d) == XTK_BATCH_OK && prepare, "mount prepare");
	XtkBatchResult *result = host->vt->commit(host, prepare, &d);
	EXPECT(result, "mount commit");
	xtk_batch_free(batch);
	EXPECT(result->control_count == 3u, "three mappings");
	XtkCtrl row = result->controls [0].control;
	XtkCtrl a   = result->controls [1].control;
	XtkCtrl b   = result->controls [2].control;
	xtk_batch_result_destroy(result);
	uint32_t mounted = flux_host_take_invalidations(host);
	EXPECT(
	  (mounted & (XTK_INVALIDATE_LAYOUT | XTK_INVALIDATE_PAINT)) == (XTK_INVALIDATE_LAYOUT | XTK_INVALIDATE_PAINT),
	  "successful commit publishes invalidations"
	);

	XtkCtrl   a_parent_before = host->vt->parent(host, a);

	XtkBatch *bad             = xtk_batch_alloc(host->vt->revision(host));
	EXPECT(bad, "diverged batch");
	XtkValue replacement = {
	  .kind = XTK_VALUE_STRING, .u.string = {.data = "replacement", .len = 11u}
    };
	EXPECT(
	  xtk_batch_set(bad, xtk_ref_existing(a), XTK_PROP_TEXT, replacement, (XtkChangeStamp) {0}), "staged property"
	);
	EXPECT(xtk_batch_insert(bad, xtk_ref_existing(row), xtk_ref_existing(a), xtk_ref_existing(b)), "insert op");
	prepare = NULL;
	EXPECT(host->vt->prepare(host, bad, &prepare, &d) == XTK_BATCH_OK && prepare, "diverged batch passes preflight");
	result = host->vt->commit(host, prepare, &d);
	EXPECT(!result, "diverged commit fails");
	EXPECT(d.error == XTK_BATCH_ERR_VALIDATION, "structural apply failure reports VALIDATION");
	EXPECT(d.failed_op == 1u, "diagnostic names the failing op");
	EXPECT(host->vt->parent(host, a) == a_parent_before, "rollback leaves the tree untouched");
	XtkValueView   value;
	XtkChangeStamp change;
	EXPECT(host->vt->prop(host, a, XTK_PROP_TEXT, &value, &change), "rolled-back property readable");
	EXPECT(value.kind == XTK_VALUE_STRING && value.u.string.len == 1u, "rolled-back property preserved");
	EXPECT(flux_host_take_invalidations(host) == 0u, "failed commit publishes no invalidations");
	xtk_batch_free(bad);

	flux_host_destroy(host);
	return 0;
}

int main(void) {
	if (test_commit_oom_sweep()) return 1;
	if (test_commit_diverged_tree_reports_validation()) return 1;
	printf("PASS: commit error granularity\n");
	return 0;
}
