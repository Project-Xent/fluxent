/**
 * @file flux_image_data.h
 * @brief Data structure for FluxImage control.
 */
#ifndef FLUX_IMAGE_DATA_H
#define FLUX_IMAGE_DATA_H

#include <xtk/xtk_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef XtkImageStretch FluxImageStretch;

#define FLUX_IMAGE_NONE            XTK_IMAGE_NONE
#define FLUX_IMAGE_FILL            XTK_IMAGE_FILL
#define FLUX_IMAGE_UNIFORM         XTK_IMAGE_UNIFORM
#define FLUX_IMAGE_UNIFORM_TO_FILL XTK_IMAGE_UNIFORM_TO_FILL

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
