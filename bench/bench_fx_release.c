#include <fluxent/flux_app.h>
#include <fluxent/flux_host.h>
#include <xtk/xtk_batch.h>
#include <xtk/xtk_catalog.h>

#include "render/flux_engine.h"
#include "xtk_alloc_internal.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

enum
{
	NODE_COUNT         = 512,
	WARMUP_ITERATIONS  = 16,
	MEASURE_ITERATIONS = 100,
};

typedef struct Fixture {
	FluxApp    *app;
	XtkHost    *host;
	FluxEngine *engine;
	XtkCtrl     tree;
	XtkCtrl    *nodes;
} Fixture;

typedef struct Measure {
	uint64_t      elapsed;
	uint64_t      visits;
	uint64_t      render;
	XtkAllocStats alloc;
} Measure;

static void     fixture_dispose(Fixture *fixture);

static uint64_t clock_ns(void) {
	struct timespec value;
	if (timespec_get(&value, TIME_UTC) != TIME_UTC) return 0u;
	return ( uint64_t ) value.tv_sec * UINT64_C(1000000000) + ( uint64_t ) value.tv_nsec;
}

static XtkBatchResult *commit_batch(XtkHost *host, XtkBatch *batch) {
	XtkPrepare  *prepare = NULL;
	XtkBatchDiag diagnostic;
	if (host->vt->prepare(host, batch, &prepare, &diagnostic) != XTK_BATCH_OK) return NULL;
	return host->vt->commit(host, prepare, &diagnostic);
}

static XtkCtrl result_control(XtkBatchResult const *result, XtkLocalId local) {
	for (uint32_t i = 0; i < result->control_count; i++)
		if (result->controls [i].local == local) return result->controls [i].control;
	return XTK_CTRL_INVALID;
}

static bool append_text_nodes(XtkBatch *batch, XtkLocalId root, XtkLocalId *locals) {
	XtkPropPatch text_props [] = {
	  {.id = XTK_PROP_TEXT,   .value = {.kind = XTK_VALUE_STRING, .u.string = {.data = "row", .len = 3u}}},
	  {.id = XTK_PROP_HEIGHT, .value = {.kind = XTK_VALUE_F32, .u.f32 = 20.0f}                           },
	};
	for (uint32_t i = 0; i < NODE_COUNT; i++) {
		locals [i] = xtk_batch_create(batch, XTK_CTRL_TEXT, xtk_ref_local(root), xtk_ref_none(), text_props, 2u);
		if (locals [i] == XTK_LOCAL_INVALID) return false;
	}
	return true;
}

static bool map_text_nodes(Fixture *fixture, XtkBatchResult const *result, XtkLocalId const *locals) {
	fixture->nodes = malloc(sizeof(*fixture->nodes) * NODE_COUNT);
	if (!fixture->nodes) return false;
	for (uint32_t i = 0; i < NODE_COUNT; i++) {
		fixture->nodes [i] = result_control(result, locals [i]);
		if (fixture->nodes [i] == XTK_CTRL_INVALID) return false;
	}
	return true;
}

static bool fixture_mount(Fixture *fixture) {
	XtkBatch *batch = xtk_batch_alloc(fixture->host->vt->revision(fixture->host));
	if (!batch) return false;
	XtkPropPatch root_props [] = {
	  {.id = XTK_PROP_WIDTH,       .value = {.kind = XTK_VALUE_F32, .u.f32 = 800.0f}},
	  {.id = XTK_PROP_HEIGHT,      .value = {.kind = XTK_VALUE_F32, .u.f32 = 600.0f}},
	  {.id = XTK_PROP_ORIENTATION, .value = {.kind = XTK_VALUE_ENUM, .u.u32 = 0u}   },
	};
	XtkLocalId root = xtk_batch_create(batch, XTK_CTRL_CONTAINER, xtk_ref_none(), xtk_ref_none(), root_props, 3u);
	XtkLocalId locals [NODE_COUNT];
	if (root == XTK_LOCAL_INVALID || !append_text_nodes(batch, root, locals)) {
		xtk_batch_free(batch);
		return false;
	}
	XtkBatchResult *result = commit_batch(fixture->host, batch);
	if (!result) {
		xtk_batch_free(batch);
		return false;
	}
	fixture->tree = result_control(result, root);
	bool mapped   = fixture->tree != XTK_CTRL_INVALID && map_text_nodes(fixture, result, locals);
	xtk_batch_result_destroy(result);
	xtk_batch_free(batch);
	return mapped;
}

static bool fixture_init(Fixture *fixture) {
	fixture->app = flux_app_create(NULL);
	if (!fixture->app) return false;
	fixture->host
	  = flux_host_adopt(flux_app_ctx(fixture->app), flux_app_store(fixture->app), flux_app_root(fixture->app));
	fixture->engine = flux_app_engine(fixture->app);
	bool valid      = fixture->host
	               && fixture->engine
	               && fixture_mount(fixture)
	               && xent_layout(flux_app_ctx(fixture->app), flux_app_root(fixture->app), 800.0f, 600.0f);
	if (!valid) fixture_dispose(fixture);
	return valid;
}

static bool patch_one(Fixture *fixture, uint32_t revision) {
	XtkBatch *batch = xtk_batch_alloc(fixture->host->vt->revision(fixture->host));
	if (!batch) return false;
	char text [32];
	int  len = snprintf(text, sizeof(text), "row-%u", revision);
	if (len < 0) {
		xtk_batch_free(batch);
		return false;
	}
	uint32_t index  = revision % NODE_COUNT;
	bool     pushed = xtk_batch_set(
	  batch, xtk_ref_existing(fixture->nodes [index]), XTK_PROP_TEXT,
	  (XtkValue) {
		.kind = XTK_VALUE_STRING, .u.string = {.data = text, .len = ( uint32_t ) len}
    },
	  (XtkChangeStamp) {.source = 1u, .revision = revision + 1u}
	);
	XtkBatchResult *result = pushed ? commit_batch(fixture->host, batch) : NULL;
	xtk_batch_free(batch);
	if (!result) return false;
	xtk_batch_result_destroy(result);
	return true;
}

static uint64_t layout_visits(XentProfStats const *profile) {
	return profile->swiftstack_layout_calls + profile->flex_layout_calls + profile->grid_layout_calls;
}

static void fixture_dispose(Fixture *fixture) {
	if (fixture->host && fixture->tree != XTK_CTRL_INVALID) {
		XtkBatch *batch = xtk_batch_alloc(fixture->host->vt->revision(fixture->host));
		if (batch && xtk_batch_drop(batch, xtk_ref_existing(fixture->tree))) {
			XtkBatchResult *result = commit_batch(fixture->host, batch);
			xtk_batch_result_destroy(result);
		}
		xtk_batch_free(batch);
	}
	free(fixture->nodes);
	flux_host_destroy(fixture->host);
	flux_app_destroy(fixture->app);
}

static bool warmup(Fixture *fixture) {
	for (uint32_t i = 0; i < WARMUP_ITERATIONS; i++) {
		if (!patch_one(fixture, i)) return false;
		if (!xent_layout(flux_app_ctx(fixture->app), flux_app_root(fixture->app), 800.0f, 600.0f)) return false;
		flux_engine_collect(fixture->engine, flux_app_ctx(fixture->app), flux_app_root(fixture->app));
	}
	return true;
}

static bool measure(Fixture *fixture, Measure *measure) {
	xtk_alloc_test_reset();
	uint64_t begin = clock_ns();
	for (uint32_t i = 0; i < MEASURE_ITERATIONS; i++) {
		xent_profile_reset(flux_app_ctx(fixture->app));
		if (!patch_one(fixture, i + WARMUP_ITERATIONS)) return false;
		if (!xent_layout(flux_app_ctx(fixture->app), flux_app_root(fixture->app), 800.0f, 600.0f)) return false;
		XentProfStats profile  = xent_profile_get(flux_app_ctx(fixture->app));
		measure->visits       += layout_visits(&profile);
		flux_engine_collect(fixture->engine, flux_app_ctx(fixture->app), flux_app_root(fixture->app));
		measure->render += flux_engine_ncmd(fixture->engine);
	}
	measure->elapsed = clock_ns() - begin;
	measure->alloc   = xtk_alloc_test_stats();
	return measure->visits > 0u && measure->render >= NODE_COUNT * MEASURE_ITERATIONS;
}

static void report(Measure const *measure) {
	printf(
	  "workload=flux_single_property nodes=%d iterations=%d elapsed_ns=%" PRIu64 " ns_per_iteration=%" PRIu64
	  " property_patches=%d layout_visits=%" PRIu64 " render_work=%" PRIu64 " allocation_attempts=%" PRIu64
	  " allocation_peak_bytes=%zu\n",
	  NODE_COUNT, MEASURE_ITERATIONS, measure->elapsed, measure->elapsed / MEASURE_ITERATIONS, MEASURE_ITERATIONS,
	  measure->visits, measure->render, measure->alloc.attempts, measure->alloc.peak_bytes
	);
}

int main(void) {
	setvbuf(stdout, NULL, _IONBF, 0);
	Fixture fixture = {0};
	if (!fixture_init(&fixture)) return 1;
	Measure measure_result = {0};
	bool    valid          = warmup(&fixture) && measure(&fixture, &measure_result);
	if (valid) report(&measure_result);
	fixture_dispose(&fixture);
	return valid ? 0 : 1;
}
