/**
 * @file flux_image_data.h
 * @brief Data structure for FluxImage control.
 */
#ifndef FLUX_IMAGE_DATA_H
#define FLUX_IMAGE_DATA_H

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════════
   FluxImageData — Control-specific data for FluxImage
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Configuration for an image display control.
 *
 * The source is a path or URI to the image. natural_w/h are filled
 * in after the image loads.
 */
typedef struct FluxImageData {
	char const *source;    /**< Image source path or URI */
	float       natural_w; /**< Natural width (after load) */
	float       natural_h; /**< Natural height (after load) */
} FluxImageData;

#ifdef __cplusplus
}
#endif

#endif /* FLUX_IMAGE_DATA_H */
