#include "runtime/flux_anim_driver.h"

#include <windows.h>
#include <stdlib.h>

/* Single shared registry of active animators. Behaviour matches the per-control
 * timers it replaces (one WM_TIMER at ~16ms, GetTickCount sampled once per tick,
 * swap-remove, timer killed when empty) — only the fixed cap is gone. */
typedef struct FluxAnimEntry {
	void        *ctx;
	FluxAnimStep step;
} FluxAnimEntry;

static FluxAnimEntry *g_entries;
static int            g_count;
static int            g_cap;
static UINT_PTR       g_timer;

static void CALLBACK flux_anim_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD systime) {
	(void) hwnd;
	(void) msg;
	(void) id;
	(void) systime;

	unsigned long now = GetTickCount();
	/* Iterate downward so a step that unregisters itself (swap-from-end) is safe;
	 * a step that registers a new animator appends past the current index. */
	for (int i = g_count - 1; i >= 0; i--) {
		FluxAnimEntry e = g_entries [i];
		if (!e.step(e.ctx, now)) flux_anim_unregister(e.ctx);
	}
}

void flux_anim_register(void *ctx, FluxAnimStep step) {
	if (!ctx || !step) return;

	for (int i = 0; i < g_count; i++) {
		if (g_entries [i].ctx == ctx) {
			g_entries [i].step = step;
			return;
		}
	}

	if (g_count == g_cap) {
		int            ncap = g_cap ? g_cap * 2 : 8;
		FluxAnimEntry *grown = ( FluxAnimEntry * ) realloc(g_entries, ( size_t ) ncap * sizeof(*grown));
		if (!grown) return; /* drop the registration rather than corrupt the live list */
		g_entries = grown;
		g_cap     = ncap;
	}

	g_entries [g_count].ctx  = ctx;
	g_entries [g_count].step = step;
	g_count++;

	if (!g_timer) g_timer = SetTimer(NULL, 0, 16, flux_anim_timer_proc);
}

void flux_anim_unregister(void *ctx) {
	for (int i = 0; i < g_count; i++) {
		if (g_entries [i].ctx != ctx) continue;
		g_entries [i] = g_entries [--g_count];
		break;
	}

	if (g_count == 0 && g_timer) {
		KillTimer(NULL, g_timer);
		g_timer = 0;
	}
}
