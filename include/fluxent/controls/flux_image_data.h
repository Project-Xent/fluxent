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

/** @brief How an image is scaled to fit its bounds (matches WinUI Stretch). */
typedef enum FluxImageStretch
{
	FLUX_IMAGE_NONE = 0,        /**< Natural pixel size, centered. */
	FLUX_IMAGE_FILL,            /**< Stretch to fill bounds (ignores aspect ratio). */
	FLUX_IMAGE_UNIFORM,         /**< Scale to fit, preserving aspect ratio (default). */
	FLUX_IMAGE_UNIFORM_TO_FILL, /**< Scale to cover, preserving aspect, clipped. */
} FluxImageStretch;

/**
 * @brief Configuration for an image display control.
 *
 * The source is a path or URI to the image. natural_w/h are filled
 * in after the image loads.
 */
typedef struct FluxImageData {
	char const      *source;    /**< Image source path or URI */
	FluxImageStretch stretch;   /**< Scaling mode (default UNIFORM) */
	float            natural_w; /**< Natural width (after load) */
	float            natural_h; /**< Natural height (after load) */
} FluxImageData;

#ifdef __cplusplus
}
#endif

#endif
