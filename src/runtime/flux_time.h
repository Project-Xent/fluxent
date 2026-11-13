#ifndef FLUX_TIME_H
#define FLUX_TIME_H

#include <stdint.h>

int64_t flux_perf_freq(void);

int64_t flux_perf_now(void);

double  flux_perf_seconds(int64_t ticks);

float   flux_compute_dt(int64_t prev, int64_t now);

#endif
