#ifndef FLUX_IMAGE_CACHE_H
#define FLUX_IMAGE_CACHE_H

void *flux_image_cache_acquire(void *device_context, char const *source, float *out_w, float *out_h);

void  flux_image_cache_clear(void);

#endif
