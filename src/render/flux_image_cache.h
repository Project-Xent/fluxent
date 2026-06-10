/**
 * @file flux_image_cache.h
 * @brief WIC-backed decode + Direct2D bitmap cache for the Image control.
 *
 * Isolated from cd2d.h: the implementation includes the real Windows
 * Direct2D/WIC headers, so this interface speaks only in void* COM pointers.
 * The returned bitmap is a Direct2D bitmap (ID2D1Bitmap1) usable anywhere an
 * ID2D1Bitmap* is expected.
 */
#ifndef FLUX_IMAGE_CACHE_H
#define FLUX_IMAGE_CACHE_H

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * @brief Get (decoding and caching on first use) the Direct2D bitmap for a source.
 *
 * @param device_context  The active ID2D1DeviceContext (passed as void*).
 * @param source          UTF-8 file path / URI; NULL or empty returns NULL.
 * @param out_w           Receives natural pixel width (may be NULL).
 * @param out_h           Receives natural pixel height (may be NULL).
 * @return Borrowed ID2D1Bitmap1* (owned by the cache), or NULL on failure.
 */
void *flux_image_cache_acquire(void *device_context, char const *source, float *out_w, float *out_h);

/** @brief Release every cached bitmap and the WIC factory (call on shutdown / device loss). */
void  flux_image_cache_release_all(void);

#ifdef __cplusplus
}
#endif

#endif
