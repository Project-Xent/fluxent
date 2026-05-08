#include "flux_time.h"

#include <windows.h>

#define FLUX_DT_FALLBACK 0.016f
#define FLUX_DT_MAX      0.1f

int64_t flux_perf_freq(void) {
	static int64_t freq = 0;
	if (freq == 0) {
		LARGE_INTEGER f;
		QueryPerformanceFrequency(&f);
		freq = f.QuadPart;
	}
	return freq;
}

int64_t flux_perf_now(void) {
	LARGE_INTEGER t;
	QueryPerformanceCounter(&t);
	return t.QuadPart;
}

double flux_perf_seconds(int64_t ticks) {
	int64_t freq = flux_perf_freq();
	if (freq == 0) return 0.0;
	return ( double ) ticks / ( double ) freq;
}

float flux_compute_dt(int64_t prev, int64_t now) {
	if (prev == 0) return 0.0f;
	int64_t freq = flux_perf_freq();
	if (freq == 0) return FLUX_DT_FALLBACK;
	float dt = ( float ) (now - prev) / ( float ) freq;
	if (dt < 0.0f) dt = 0.0f;
	if (dt > FLUX_DT_MAX) dt = FLUX_DT_MAX;
	return dt;
}
