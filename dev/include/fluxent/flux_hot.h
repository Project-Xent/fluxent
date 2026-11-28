#ifndef FLUX_HOT_H
#define FLUX_HOT_H

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

#include <fluxent/flux_app.h>

#include <xtk/xtk_scene.h>

typedef struct FluxHot FluxHot;

typedef enum FluxHotEvent
{
	FLUX_HOT_LOADED = 0,
	FLUX_HOT_RELOADED,
	FLUX_HOT_BUILDING,
	FLUX_HOT_BUILD_FAILED,
	FLUX_HOT_LOAD_FAILED,
	FLUX_HOT_FRAME_FAILED,
	FLUX_HOT_ABI_CHANGED,
	FLUX_HOT_MODEL_CHANGED,
	FLUX_HOT_LIMIT,
	FLUX_HOT_UNCHANGED,
} FluxHotEvent;

typedef struct FluxHotStatus {
	FluxHotEvent event;
	uint32_t     generation;
	uint32_t     loaded;
	uint32_t     exit_code;
} FluxHotStatus;

typedef struct FluxHotCfg {
	wchar_t const *module;
	wchar_t const *build;
	wchar_t const *watch;
	wchar_t const *build_dir;
	uint32_t       debounce_ms;
	uint32_t       max_loaded;
	bool           manual;
	void           (*on_status)(void *user, FluxHotStatus const *status);
	void          *user;
} FluxHotCfg;

typedef struct FluxHotLoader FluxHotLoader;

typedef struct FluxHotHooks {
	/** Window thread. `prev` is NULL on the first load. Returning FLUX_HOT_RELOADED keeps `next`; any other event rejects it and is reported. */
	FluxHotEvent (*swap)(void *ctx, void *prev, void *next);
	/** Optional. True when `prev` may be freed now that `cur` runs; without it no module is freed before close. */
	bool         (*retire)(void *ctx, void *prev, void *cur);
	/** Optional. Runs before a kept module is freed. */
	void         (*unload)(void *ctx, void *lib);
	void        *ctx;
} FluxHotHooks;

/** Window thread. Loads the module through `hooks.swap`, then watches as `cfg` describes. `module` is resolved against the executable's directory when relative. With `build`, `watch` and `build_dir` all set, `watch` is watched and `build` runs through cmd.exe in `build_dir`; otherwise the module file itself is watched. Only one loader runs per process. */
FluxHotLoader *flux_hot_loader_open(FluxApp *app, FluxHotCfg const *cfg, FluxHotHooks const *hooks);
/** Stops watching; call before the app is destroyed. */
void           flux_hot_loader_stop(FluxHotLoader *loader);
/** Stops watching and frees every kept module. */
void           flux_hot_loader_close(FluxHotLoader *loader);
void          *flux_hot_symbol(void *lib, char const *name);
/** Writes UTF-8 text to the hot reload console, if there is one. */
void           flux_hot_print(char const *text);

int            flux_run_hot(FluxAppCfg const *app, FluxHotCfg const *hot, void *model);
/** Window thread. Reloads replace the controls the module creates directly under `slot`; the module must not subscribe outside what it creates. Only one hot session runs per process. */
FluxHot       *flux_hot_page_open(FluxApp *app, XtkScene *scene, XtkCtrl slot, FluxHotCfg const *cfg, void *model);
bool           flux_hot_page_rebuild(FluxHot *hot);
void           flux_hot_page_close(FluxHot *hot);
/** Any thread. Queues a reload of the running hot session; false when none is running. */
bool           flux_hot_reload(void);

#endif
