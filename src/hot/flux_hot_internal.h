#ifndef FLUX_HOT_INTERNAL_H
#define FLUX_HOT_INTERNAL_H

#include <fluxent/flux_hot.h>

#include <windows.h>

enum
{
	HOT_PATH            = MAX_PATH * 2,
	HOT_DEBOUNCE_MODULE = 200,
	HOT_DEBOUNCE_SOURCE = 500,
	HOT_MAX_LOADED      = 256,
};

typedef struct HotLib {
	HMODULE lib;
	wchar_t copy [HOT_PATH];
	bool    loaded;
} HotLib;

struct FluxHotLoader {
	FluxApp       *app;
	FluxHotCfg     cfg;
	FluxHotHooks   hooks;
	bool           watching;
	wchar_t        module [HOT_PATH];
	wchar_t        module_dir [HOT_PATH];
	wchar_t const *module_name;
	wchar_t        copy_dir [HOT_PATH];
	HotLib        *libs;
	uint32_t       lib_count;
	uint32_t       lib_cap;
	uint32_t       copy_seq;
	uint32_t       generation;
	uint64_t       hash;
	uint64_t       rejected;
	HANDLE         thread;
	HANDLE         stop;
	volatile LONG  reload_queued;
	volatile LONG  built;
};

void *flux_hot_loader_current(FluxHotLoader const *loader);

void  hot_queue_reload(FluxHotLoader *hot);
void  hot_report_built(FluxHotLoader *hot);
void  hot_post_status(FluxHotLoader *hot, FluxHotEvent event, uint32_t exit_code);
bool  hot_watch_start(FluxHotLoader *hot);
void  hot_watch_stop(FluxHotLoader *hot);

#endif
