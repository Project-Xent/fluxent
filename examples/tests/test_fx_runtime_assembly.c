/**
 * @file test_fx_runtime_assembly.c
 * @brief Entry-path assembly under kit allocation fault injection.
 *
 * flux_run blocks on the Win32 message loop, so this drives its headless
 * equivalent (the test_fx_reconcile harness): ctx/store/root stand in for the
 * app's scene, then host adopt -> runtime create -> frame -> destroy. Each
 * injected failure must be reported (the wiring step returns NULL), teardown
 * must still complete in reverse-dependency order, and xtk allocation stats
 * must return to the allocation baseline after every pass.
 */
#include "flux_internal.h"

#include <fluxent/flux_host.h>

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

static void noop_update(void *model, XtkMsg msg) {
	( void ) model;
	( void ) msg;
}

static XtkEl *one_label_view(XtkUi *ui, void *model) {
	( void ) model;
	return xtk_text(ui, "hello", (XtkTextDesc) {0});
}

typedef struct Assembly {
	XentCtx       *ctx;
	FluxStore     *store;
	XentNodeId     root;
	FluxBackendCtx bctx;
} Assembly;

/* Scene stand-in for flux_app_create: an owned ctx/store plus a bare root. */
static void assembly_begin(Assembly *a) {
	a->ctx   = xent_ctx_create(NULL);
	a->store = flux_store_create(64);
	a->root  = XENT_NODE_INVALID;
	if (a->ctx && a->store) {
		flux_store_bind(a->store, a->ctx);
		a->root = xent_node_create(a->ctx);
	}
	a->bctx = (FluxBackendCtx) {.ctx = a->ctx, .store = a->store};
}

/* The entry path's wiring steps minus the window: adopt the host over the
 * scene, then create the runtime on it. */
static void assembly_wire(Assembly *a) {
	a->bctx.host = flux_host_adopt(a->ctx, a->store, a->root);
	if (!a->bctx.host) return;
	XtkRtCfg cfg    = flux_xtk_runtime_config(&a->bctx);
	a->bctx.runtime = xtk_rt_create(a->bctx.host, &cfg, a->root, NULL, noop_update, one_label_view);
}

/* flux_teardown with store/ctx standing in for the app: the runtime uses the
 * host, the host borrows the scene. Returns xtk_rt_destroy's result. */
static bool assembly_teardown(Assembly *a) {
	bool ok = true;
	if (a->bctx.runtime) ok = xtk_rt_destroy(a->bctx.runtime);
	if (a->bctx.host) flux_host_destroy(a->bctx.host);
	if (a->store) flux_store_destroy(a->store);
	if (a->ctx) xent_ctx_destroy(a->ctx);
	return ok;
}

static int test_success_pass_balances(void) {
	Assembly a;
	assembly_begin(&a);
	EXPECT(a.root != XENT_NODE_INVALID, "scene");
	xtk_alloc_test_reset();
	assembly_wire(&a);
	EXPECT(a.bctx.host && a.bctx.runtime, "assembly without injection");
	EXPECT(xtk_rt_frame(a.bctx.runtime).status == XTK_FRAME_COMMITTED, "initial mount commits");
	EXPECT(assembly_teardown(&a), "runtime destroy succeeds");
	XtkAllocStats s = xtk_alloc_test_stats();
	EXPECT(s.live_bytes == 0u, "success pass frees every xtk allocation");
	return 0;
}

static int test_assembly_fault_sweep(void) {
	bool saw_failure = false;
	bool saw_success = false;
	for (uint64_t fail_at = 1; fail_at <= 64; fail_at++) {
		Assembly a;
		assembly_begin(&a);
		EXPECT(a.root != XENT_NODE_INVALID, "scene");
		xtk_alloc_test_reset();
		xtk_alloc_fail_at(fail_at);
		assembly_wire(&a);
		xtk_alloc_fail_at(0); /* disarm: teardown must not see injection */
		bool assembled = a.bctx.runtime != NULL;
		EXPECT(assembly_teardown(&a), "teardown completes after wiring failure");
		XtkAllocStats s = xtk_alloc_test_stats();
		EXPECT(s.live_bytes == 0u, "failed assembly leaks nothing");
		if (assembled) {
			saw_success = true;
			break;
		}
		saw_failure = true;
		EXPECT(s.failures == 1u, "injected failure reported exactly once");
	}
	EXPECT(saw_failure && saw_success, "sweep reaches every assembly allocation");
	return 0;
}

static int test_destroy_fault_sweep(void) {
	bool saw_failure = false;
	bool saw_success = false;
	for (uint64_t fail_at = 1; fail_at <= 64; fail_at++) {
		Assembly a;
		assembly_begin(&a);
		EXPECT(a.root != XENT_NODE_INVALID, "scene");
		xtk_alloc_test_reset();
		assembly_wire(&a);
		EXPECT(a.bctx.host && a.bctx.runtime, "assembly without injection");
		EXPECT(xtk_rt_frame(a.bctx.runtime).status == XTK_FRAME_COMMITTED, "mount commits");

		/* fail_at counts attempts since reset; skip past the assembly's. */
		xtk_alloc_fail_at(xtk_alloc_test_stats().attempts + fail_at);
		bool destroyed = xtk_rt_destroy(a.bctx.runtime);
		xtk_alloc_fail_at(0);
		if (destroyed) saw_success = true;
		else {
			saw_failure = true;
			EXPECT(xtk_rt_destroy(a.bctx.runtime), "destroy retry completes");
		}
		a.bctx.runtime = NULL;
		( void ) assembly_teardown(&a);
		XtkAllocStats s = xtk_alloc_test_stats();
		EXPECT(s.live_bytes == 0u, "destroy pass leaks nothing");
		if (destroyed) break;
	}
	EXPECT(saw_failure && saw_success, "sweep reaches every destroy allocation");
	return 0;
}

int main(void) {
	if (test_success_pass_balances()) return 1;
	if (test_assembly_fault_sweep()) return 1;
	if (test_destroy_fault_sweep()) return 1;
	printf("PASS: entry assembly survives allocation fault injection\n");
	return 0;
}
