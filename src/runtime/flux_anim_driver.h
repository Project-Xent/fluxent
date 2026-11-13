#ifndef FLUX_ANIM_DRIVER_H
#define FLUX_ANIM_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

typedef bool  (*FluxAnimStep)(void *ctx, unsigned long now_ms);

void          flux_anim_register(void *ctx, FluxAnimStep step);

void          flux_anim_unregister(void *ctx);

bool          flux_anim_frame(void);

uint64_t      flux_anim_ms(void);

unsigned long flux_anim_now(void);

void          flux_anim_presented(void);

bool          flux_anim_take_presented(void);

#endif
