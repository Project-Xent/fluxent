#include "runtime/flux_anim_driver.h"

#include "runtime/flux_time.h"

#include <windows.h>
#include <stdlib.h>

typedef struct FluxAnimEntry {
	void        *ctx;
	FluxAnimStep step;
} FluxAnimEntry;

static FluxAnimEntry *g_entries;
static int            g_count;
static int            g_cap;
static UINT_PTR       g_timer;
static bool           g_presented;

uint64_t              flux_anim_ms(void) {
	int64_t freq = flux_perf_freq();
	return freq > 0 ? ( uint64_t ) (flux_perf_now() / (freq / 1000)) : GetTickCount64();
}

unsigned long flux_anim_now(void) { return ( unsigned long ) flux_anim_ms(); }

void          flux_anim_presented(void) { g_presented = true; }

bool          flux_anim_take_presented(void) {
	bool was    = g_presented;
	g_presented = false;
	return was;
}

bool flux_anim_frame(void) {
	unsigned long now = flux_anim_now();
	for (int i = g_count - 1; i >= 0; i--) {
		if (i >= g_count) continue;
		FluxAnimEntry e = g_entries [i];
		if (!e.step(e.ctx, now)) flux_anim_unregister(e.ctx);
	}
	return g_count > 0;
}

static void CALLBACK anim_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD systime) {
	( void ) hwnd;
	( void ) msg;
	( void ) id;
	( void ) systime;
	flux_anim_frame();
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
		int            ncap  = g_cap ? g_cap * 2 : 8;
		FluxAnimEntry *grown = ( FluxAnimEntry * ) realloc(g_entries, ( size_t ) ncap * sizeof(*grown));
		if (!grown) return;
		g_entries = grown;
		g_cap     = ncap;
	}

	g_entries [g_count].ctx  = ctx;
	g_entries [g_count].step = step;
	g_count++;

	if (!g_timer) g_timer = SetTimer(NULL, 0, 16, anim_timer_proc);
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
