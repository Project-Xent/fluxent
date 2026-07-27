/**
 * @file test_fx_engine_degraded.c
 * @brief Degraded-frame reporting under kit allocation fault injection.
 *
 * flux_engine_collect allocates its walk stack and command buffer through
 * kit's xtk_alloc family, so xtk_alloc_fail_at reaches them. A failed
 * push must mark the frame degraded and fire the report exactly once at
 * frame end — never crash, never stay silent. The test always registers the
 * degraded callback, which is what suppresses the debug-build assert.
 */
#include "render/flux_engine.h"
#include <fluxent/flux_node_store.h>

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

#define WIDE_CHILDREN 3u
#define WIDE_NODES    (1u + WIDE_CHILDREN)
/* Bare container nodes emit one MAIN + one OVERLAY command each (no scroll,
 * clip, or transform commands), so a fresh frame holds 2 * WIDE_NODES. */
#define WIDE_COMMANDS (2u * WIDE_NODES)

/* Per-frame collect allocations on a fresh engine: 1 = walk stack,
 * 2 = command buffer's first grow, 3 = walk stack grow (deep scene only). */
#define DEEP_DEPTH    70u /* stack cap starts at 64; depth 64's push grows it */
#define DEEP_COMMANDS (2u * 64u)

typedef struct Scene {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId root;
} Scene;

typedef struct Pass {
	uint32_t      commands;
	int           reports;
	XtkAllocStats stats;
} Pass;

static void on_degraded(void *ctx) {
	int *reports = ( int * ) ctx;
	(*reports)++;
}

static void scene_wide(Scene *s) {
	s->ctx   = xent_ctx_create(NULL);
	s->store = flux_store_create(64);
	s->root  = XENT_NODE_INVALID;
	if (!s->ctx || !s->store) return;
	flux_store_bind(s->store, s->ctx);
	s->root = xent_node_create(s->ctx);
	for (uint32_t i = 0; i < WIDE_CHILDREN; i++) xent_node_append(s->ctx, s->root, xent_node_create(s->ctx));
}

static void scene_deep(Scene *s) {
	s->ctx   = xent_ctx_create(NULL);
	s->store = flux_store_create(64);
	s->root  = XENT_NODE_INVALID;
	if (!s->ctx || !s->store) return;
	flux_store_bind(s->store, s->ctx);
	s->root           = xent_node_create(s->ctx);
	XentNodeId parent = s->root;
	for (uint32_t i = 1; i < DEEP_DEPTH; i++) {
		XentNodeId child = xent_node_create(s->ctx);
		xent_node_append(s->ctx, parent, child);
		parent = child;
	}
}

static void scene_end(Scene *s) {
	if (s->store) flux_store_destroy(s->store);
	if (s->ctx) xent_ctx_destroy(s->ctx);
}

/* One collect on a fresh engine with the Nth post-reset xtk allocation
 * failing (0 = no injection); the engine is destroyed before return. */
static Pass run_collect(Scene *s, uint64_t fail_at) {
	Pass        p   = {0};
	FluxEngine *eng = flux_engine_create(s->store);
	if (!eng) return p;
	flux_engine_ondegraded(eng, on_degraded, &p.reports);
	xtk_alloc_test_reset();
	xtk_alloc_fail_at(fail_at);
	flux_engine_collect(eng, s->ctx, s->root);
	xtk_alloc_fail_at(0); /* disarm: teardown must not see injection */
	p.commands = flux_engine_ncmd(eng);
	flux_engine_destroy(eng);
	p.stats = xtk_alloc_test_stats();
	return p;
}

static int test_clean_frame_stays_silent(void) {
	Scene s;
	scene_wide(&s);
	EXPECT(s.root != XENT_NODE_INVALID, "scene");
	Pass p = run_collect(&s, 0);
	EXPECT(p.reports == 0, "no report without injection");
	EXPECT(p.commands == WIDE_COMMANDS, "all nodes collected");
	EXPECT(p.stats.live_bytes == 0u, "clean pass frees every xtk allocation");
	scene_end(&s);
	return 0;
}

static int test_command_round_trip(void) {
	Scene s;
	scene_wide(&s);
	EXPECT(s.root != XENT_NODE_INVALID, "scene");
	EXPECT(xent_setw(s.ctx, s.root, 320.0f), "root width");
	EXPECT(xent_seth(s.ctx, s.root, 180.0f), "root height");
	EXPECT(xent_layout(s.ctx, s.root, 320.0f, 180.0f), "layout");

	FluxEngine *eng = flux_engine_create(s.store);
	EXPECT(eng, "engine");
	flux_engine_collect(eng, s.ctx, s.root);

	FluxRenderCommand const *cmd = flux_engine_cmd(eng, 0);
	EXPECT(cmd, "first command");
	EXPECT(cmd->snapshot.type == FLUX_CONTROL_CONTAINER, "snapshot type survives command push");
	EXPECT(cmd->phase == FLUX_PHASE_MAIN, "phase survives command push");
	EXPECT(cmd->clip_action == FLUX_CLIP_NONE, "clip action survives command push");
	EXPECT(cmd->bounds.w == 320.0f && cmd->bounds.h == 180.0f, "bounds survive command push");

	flux_engine_destroy(eng);
	scene_end(&s);
	return 0;
}

static int test_walk_stack_failure_degrades_frame(void) {
	Scene s;
	scene_wide(&s);
	EXPECT(s.root != XENT_NODE_INVALID, "scene");
	Pass p = run_collect(&s, 1);
	EXPECT(p.reports == 1, "report fires exactly once");
	EXPECT(p.commands == 0, "frame empty when the walk stack fails");
	EXPECT(p.stats.live_bytes == 0u, "degraded pass frees every xtk allocation");
	scene_end(&s);
	return 0;
}

static int test_command_buffer_failure_degrades_frame(void) {
	Scene s;
	scene_wide(&s);
	EXPECT(s.root != XENT_NODE_INVALID, "scene");
	Pass p = run_collect(&s, 2);
	EXPECT(p.reports == 1, "report fires exactly once");
	EXPECT(p.commands == WIDE_COMMANDS - 1u, "only the failed command is dropped");
	EXPECT(p.stats.live_bytes == 0u, "degraded pass frees every xtk allocation");
	scene_end(&s);
	return 0;
}

static int test_unreached_failure_leaves_clean_frame(void) {
	Scene s;
	scene_wide(&s);
	EXPECT(s.root != XENT_NODE_INVALID, "scene");
	Pass p = run_collect(&s, 3); /* a fresh wide frame allocates only twice */
	EXPECT(p.reports == 0, "no report when no allocation fails");
	EXPECT(p.commands == WIDE_COMMANDS, "all nodes collected");
	scene_end(&s);
	return 0;
}

static int test_walk_stack_grow_failure_degrades_frame(void) {
	Scene s;
	scene_deep(&s);
	EXPECT(s.root != XENT_NODE_INVALID, "scene");
	Pass p = run_collect(&s, 3);
	EXPECT(p.reports == 1, "report fires exactly once");
	EXPECT(p.commands == DEEP_COMMANDS, "subtree below the failed grow is dropped");
	EXPECT(p.stats.live_bytes == 0u, "degraded pass frees every xtk allocation");
	scene_end(&s);
	return 0;
}

static int test_report_is_once_per_frame(void) {
	Scene s;
	scene_wide(&s);
	EXPECT(s.root != XENT_NODE_INVALID, "scene");
	FluxEngine *eng = flux_engine_create(s.store);
	EXPECT(eng, "engine");
	int reports = 0;
	flux_engine_ondegraded(eng, on_degraded, &reports);

	xtk_alloc_test_reset();
	xtk_alloc_fail_at(1);
	flux_engine_collect(eng, s.ctx, s.root);
	EXPECT(reports == 1, "first degraded frame reports once");

	xtk_alloc_test_reset();
	xtk_alloc_fail_at(1);
	flux_engine_collect(eng, s.ctx, s.root);
	EXPECT(reports == 2, "second degraded frame reports again (no once-ever latch)");

	xtk_alloc_fail_at(0);
	flux_engine_collect(eng, s.ctx, s.root);
	EXPECT(reports == 2, "clean frame after a degraded one stays silent");
	EXPECT(flux_engine_ncmd(eng) == WIDE_COMMANDS, "flag reset lets the next frame collect fully");

	flux_engine_destroy(eng);
	scene_end(&s);
	return 0;
}

int main(void) {
	if (test_clean_frame_stays_silent()) return 1;
	if (test_command_round_trip()) return 1;
	if (test_walk_stack_failure_degrades_frame()) return 1;
	if (test_command_buffer_failure_degrades_frame()) return 1;
	if (test_unreached_failure_leaves_clean_frame()) return 1;
	if (test_walk_stack_grow_failure_degrades_frame()) return 1;
	if (test_report_is_once_per_frame()) return 1;
	printf("PASS: test_fx_engine_degraded\n");
	return 0;
}
