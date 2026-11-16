#ifndef FLUX_IMAGE_H
#define FLUX_IMAGE_H

#include <fluxent/flux_types.h>
#include <xent/xent.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxImage FluxImage;

FluxImage               *flux_image_get(char const *source);
bool                     flux_image_size(FluxImage const *image, float *w, float *h);
bool                     flux_image_ready(FluxImage const *image);
bool                     flux_image_animated(FluxImage const *image);
bool                     flux_image_failed(FluxImage const *image);
bool                     flux_image_playing(FluxImage const *image, double seconds);
uint32_t                 flux_image_frame(FluxImage const *image, double seconds);
void                     flux_image_onwake(void (*fn)(void *ctx), void *ctx);

bool                     flux_image_draw(
  void *device_context, char const *source, uint32_t frame, FluxRect bounds, XentImageFit fit, XentInsets nine
);
void *flux_image_bitmap(void *device_context, char const *source, float *w, float *h);

#endif
