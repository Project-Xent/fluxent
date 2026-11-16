#ifndef FLUX_IMAGE_DECODE_H
#define FLUX_IMAGE_DECODE_H

#include <windows.h>
#include <wincodec.h>

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxImageFrame {
	IWICBitmap *bitmap;
	float       delay;
} FluxImageFrame;

typedef struct FluxImagePixels {
	float           w;
	float           h;
	FluxImageFrame *frames;
	int             nframe;
	uint32_t        plays;
	uint8_t        *svg;
	uint32_t        svg_len;
} FluxImagePixels;

bool flux_image_probe(IWICImagingFactory *wic, wchar_t const *path, float *w, float *h);
bool flux_image_decode(IWICImagingFactory *wic, wchar_t const *path, FluxImagePixels *out);
void flux_image_pixels_free(FluxImagePixels *px);

#endif
