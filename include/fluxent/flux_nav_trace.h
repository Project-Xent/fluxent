#ifndef FLUX_NAV_TRACE_H
#define FLUX_NAV_TRACE_H

#include <stdint.h>

#include <xent/xent.h>

typedef struct FluxStore FluxStore;

void                     flux_nav_trace_reset(void);
uint64_t                 flux_nav_trace_begin(char const *format, ...);
void                     flux_nav_trace_adopt(uint64_t trace_id);
void                     flux_nav_trace_clear(void);
uint64_t                 flux_nav_trace_current(void);
void                     flux_nav_trace_log(char const *stage, char const *format, ...);
void                     flux_nav_trace_snapshot(XentCtx *ctx, FluxStore *store, XentNodeId root, char const *stage);

#endif
