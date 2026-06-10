/* Decodes images with WIC and uploads them to Direct2D bitmaps. Uses cd2d.h for
 * the Direct2D side (same COM type system as the renderer) and <wincodec.h> for
 * decoding. The public interface speaks void* so flux_image_cache.h stays free of
 * platform headers. */
#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <windows.h>
#include <cd2d.h>
#include <wincodec.h>

#include "render/flux_image_cache.h"

#include <stdlib.h>
#include <string.h>

#define FLUX_IMAGE_CACHE_CAP 64

typedef struct FluxImageEntry {
	char         *source;
	ID2D1Bitmap1 *bitmap;
	float         w;
	float         h;
} FluxImageEntry;

static IWICImagingFactory *g_wic;
static void               *g_device_context;
static FluxImageEntry      g_entries [FLUX_IMAGE_CACHE_CAP];
static int                 g_count;

static IWICImagingFactory *image_wic_factory(void) {
	if (g_wic) return g_wic;
	HRESULT hr = CoCreateInstance(
	  &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, ( void ** ) &g_wic
	);
	if (hr == CO_E_NOTINITIALIZED) {
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
		hr = CoCreateInstance(
		  &CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, ( void ** ) &g_wic
		);
	}
	return SUCCEEDED(hr) ? g_wic : NULL;
}

static ID2D1Bitmap1 *
image_bitmap_from_converter(ID2D1DeviceContext *dc, IWICFormatConverter *conv, float *out_w, float *out_h) {
	UINT uw = 0, uh = 0;
	IWICFormatConverter_GetSize(conv, &uw, &uh);
	D2D1_BITMAP_PROPERTIES1 props = {
	  {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
      96.0f, 96.0f, D2D1_BITMAP_OPTIONS_NONE, NULL
    };
	ID2D1Bitmap1 *bitmap = NULL;
	if (FAILED(ID2D1DeviceContext_CreateBitmapFromWicBitmap1(dc, ( IWICBitmapSource * ) conv, &props, &bitmap)))
		return NULL;
	if (out_w) *out_w = ( float ) uw;
	if (out_h) *out_h = ( float ) uh;
	return bitmap;
}

static ID2D1Bitmap1 *image_decode(ID2D1DeviceContext *dc, char const *source, float *out_w, float *out_h) {
	IWICImagingFactory *wic = image_wic_factory();
	if (!wic) return NULL;

	wchar_t wpath [MAX_PATH];
	if (MultiByteToWideChar(CP_UTF8, 0, source, -1, wpath, MAX_PATH) == 0) return NULL;

	IWICBitmapDecoder *decoder = NULL;
	if (FAILED(IWICImagingFactory_CreateDecoderFromFilename(
		  wic, wpath, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &decoder
		)))
		return NULL;

	IWICBitmapFrameDecode *frame  = NULL;
	IWICFormatConverter   *conv   = NULL;
	ID2D1Bitmap1          *bitmap = NULL;
	if (SUCCEEDED(IWICBitmapDecoder_GetFrame(decoder, 0, &frame))
		&& SUCCEEDED(IWICImagingFactory_CreateFormatConverter(wic, &conv))
		&& SUCCEEDED(IWICFormatConverter_Initialize(
		  conv, ( IWICBitmapSource * ) frame, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0,
		  WICBitmapPaletteTypeMedianCut
		)))
		bitmap = image_bitmap_from_converter(dc, conv, out_w, out_h);

	if (conv) IWICFormatConverter_Release(conv);
	if (frame) IWICBitmapFrameDecode_Release(frame);
	IWICBitmapDecoder_Release(decoder);
	return bitmap;
}

static FluxImageEntry *image_find(char const *source) {
	for (int i = 0; i < g_count; i++)
		if (strcmp(g_entries [i].source, source) == 0) return &g_entries [i];
	return NULL;
}

void flux_image_cache_release_all(void) {
	for (int i = 0; i < g_count; i++) {
		if (g_entries [i].bitmap) ID2D1Bitmap1_Release(g_entries [i].bitmap);
		free(g_entries [i].source);
	}
	g_count          = 0;
	g_device_context = NULL;
}

void *flux_image_cache_acquire(void *device_context, char const *source, float *out_w, float *out_h) {
	if (!device_context || !source || !source [0]) return NULL;

	if (device_context != g_device_context) {
		flux_image_cache_release_all();
		g_device_context = device_context;
	}

	FluxImageEntry *hit = image_find(source);
	if (hit) {
		if (out_w) *out_w = hit->w;
		if (out_h) *out_h = hit->h;
		return hit->bitmap;
	}
	if (g_count >= FLUX_IMAGE_CACHE_CAP) return NULL;

	float         w = 0.0f, h = 0.0f;
	ID2D1Bitmap1 *bmp = image_decode(( ID2D1DeviceContext * ) device_context, source, &w, &h);
	if (!bmp) return NULL;

	FluxImageEntry *e = &g_entries [g_count++];
	e->source         = _strdup(source);
	e->bitmap         = bmp;
	e->w              = w;
	e->h              = h;
	if (out_w) *out_w = w;
	if (out_h) *out_h = h;
	return bmp;
}
