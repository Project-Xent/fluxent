#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <windows.h>
#include <cd2d.h>
#include <wincodec.h>

#include "render/flux_image.h"
#include "render/flux_image_decode.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

enum
{
	IMAGE_PENDING,
	IMAGE_SIZED,
	IMAGE_READY,
	IMAGE_FAILED,
};

struct FluxImage {
	char *source;
	LONG volatile state;
	float             w;
	float             h;
	FluxImagePixels   px;
	double            duration;
	void             *device;
	ID2D1Bitmap1    **bitmaps;
	ID2D1SvgDocument *svg;
};

static SRWLOCK     g_lock = SRWLOCK_INIT;
static FluxImage **g_images;
static int         g_count;
static int         g_cap;
static SRWLOCK     g_wake_lock = SRWLOCK_INIT;
static void        (*g_wake)(void *ctx);
static void       *g_wake_ctx;

static LONG        image_state(FluxImage const *im) {
	return InterlockedCompareExchange(( LONG volatile * ) &im->state, 0, 0);
}

static void image_wake(void) {
	AcquireSRWLockShared(&g_wake_lock);
	if (g_wake) g_wake(g_wake_ctx);
	ReleaseSRWLockShared(&g_wake_lock);
}

void flux_image_onwake(void (*fn)(void *ctx), void *ctx) {
	AcquireSRWLockExclusive(&g_wake_lock);
	g_wake     = fn;
	g_wake_ctx = ctx;
	ReleaseSRWLockExclusive(&g_wake_lock);
}

static void image_load(FluxImage *im, IWICImagingFactory *wic, wchar_t const *path) {
	if (flux_image_probe(wic, path, &im->w, &im->h)) {
		InterlockedExchange(&im->state, IMAGE_SIZED);
		image_wake();
	}
	FluxImagePixels px;
	bool            ok = flux_image_decode(wic, path, &px);
	if (ok) {
		im->px = px;
		im->w  = px.w;
		im->h  = px.h;
		for (int i = 0; i < px.nframe; i++) im->duration += px.frames [i].delay;
	}
	InterlockedExchange(&im->state, ok ? IMAGE_READY : IMAGE_FAILED);
	image_wake();
}

static void CALLBACK image_work(PTP_CALLBACK_INSTANCE instance, void *ctx) {
	( void ) instance;
	FluxImage          *im  = ( FluxImage * ) ctx;
	HRESULT             co  = CoInitializeEx(NULL, COINIT_MULTITHREADED);
	IWICImagingFactory *wic = NULL;
	wchar_t             path [MAX_PATH * 2];
	bool                ok
	  = MultiByteToWideChar(CP_UTF8, 0, im->source, -1, path, MAX_PATH * 2) > 0
	 && SUCCEEDED(
	   CoCreateInstance(&CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, &IID_IWICImagingFactory, ( void ** ) &wic)
	 );
	if (ok) image_load(im, wic, path);
	else {
		InterlockedExchange(&im->state, IMAGE_FAILED);
		image_wake();
	}
	if (wic) IWICImagingFactory_Release(wic);
	if (SUCCEEDED(co)) CoUninitialize();
}

static FluxImage *image_add(char const *source) {
	if (g_count == g_cap) {
		int         cap  = g_cap ? g_cap * 2 : 32;
		FluxImage **next = ( FluxImage ** ) realloc(g_images, sizeof(*next) * ( size_t ) cap);
		if (!next) return NULL;
		g_images = next;
		g_cap    = cap;
	}
	FluxImage *im = ( FluxImage * ) calloc(1, sizeof(*im));
	if (!im || !(im->source = _strdup(source))) {
		free(im);
		return NULL;
	}
	if (!TrySubmitThreadpoolCallback(image_work, im, NULL)) im->state = IMAGE_FAILED;
	g_images [g_count++] = im;
	return im;
}

FluxImage *flux_image_get(char const *source) {
	if (!source || !source [0]) return NULL;
	FluxImage *found = NULL;
	AcquireSRWLockExclusive(&g_lock);
	for (int i = 0; i < g_count && !found; i++)
		if (strcmp(g_images [i]->source, source) == 0) found = g_images [i];
	if (!found) found = image_add(source);
	ReleaseSRWLockExclusive(&g_lock);
	return found;
}

bool flux_image_size(FluxImage const *im, float *w, float *h) {
	LONG state = im ? image_state(im) : IMAGE_FAILED;
	if (state != IMAGE_SIZED && state != IMAGE_READY) return false;
	*w = im->w;
	*h = im->h;
	return true;
}

bool flux_image_ready(FluxImage const *im) { return im && image_state(im) == IMAGE_READY; }

bool flux_image_animated(FluxImage const *im) { return flux_image_ready(im) && im->px.nframe > 1; }

bool flux_image_failed(FluxImage const *im) { return !im || image_state(im) == IMAGE_FAILED; }

bool flux_image_playing(FluxImage const *im, double seconds) {
	return flux_image_animated(im) && im->duration > 0.0 && (!im->px.plays || seconds < im->duration * im->px.plays);
}

uint32_t flux_image_frame(FluxImage const *im, double seconds) {
	if (!flux_image_animated(im) || im->duration <= 0.0) return 0u;
	if (!flux_image_playing(im, seconds)) return ( uint32_t ) (im->px.nframe - 1);
	double at = fmod(seconds, im->duration);
	for (int i = 0; i < im->px.nframe; i++) {
		at -= im->px.frames [i].delay;
		if (at < 0.0) return ( uint32_t ) i;
	}
	return ( uint32_t ) (im->px.nframe - 1);
}

static void image_bind(FluxImage *im, void *dc) {
	if (im->device == dc) return;
	for (int i = 0; im->bitmaps && i < im->px.nframe; i++)
		if (im->bitmaps [i]) ID2D1Bitmap1_Release(im->bitmaps [i]);
	free(im->bitmaps);
	if (im->svg) ID2D1SvgDocument_Release(im->svg);
	im->bitmaps = NULL;
	im->svg     = NULL;
	im->device  = dc;
}

static ID2D1Bitmap1 *image_bitmap(FluxImage *im, ID2D1DeviceContext *dc, uint32_t frame) {
	if (im->px.nframe <= 0) return NULL;
	if (!im->bitmaps) im->bitmaps = ( ID2D1Bitmap1 ** ) calloc(( size_t ) im->px.nframe, sizeof(*im->bitmaps));
	if (!im->bitmaps) return NULL;
	if (frame >= ( uint32_t ) im->px.nframe) frame = 0u;
	if (im->bitmaps [frame]) return im->bitmaps [frame];
	D2D1_BITMAP_PROPERTIES1 props = {
	  {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED},
      96.0f, 96.0f, D2D1_BITMAP_OPTIONS_NONE, NULL
    };
	ID2D1DeviceContext_CreateBitmapFromWicBitmap1(
	  dc, ( IWICBitmapSource * ) im->px.frames [frame].bitmap, &props, &im->bitmaps [frame]
	);
	return im->bitmaps [frame];
}

static FluxRect image_dest(FluxRect b, float w, float h, XentImageFit fit) {
	if (w <= 0.0f || h <= 0.0f || fit == XENT_IMAGE_FIT_FILL) return b;
	float sx = b.w / w, sy = b.h / h, s = 1.0f;
	if (fit == XENT_IMAGE_FIT_UNIFORM) s = fminf(sx, sy);
	else if (fit == XENT_IMAGE_FIT_UNIFORM_TO_FILL) s = fmaxf(sx, sy);
	return (FluxRect) {b.x + (b.w - w * s) * 0.5f, b.y + (b.h - h * s) * 0.5f, w * s, h * s};
}

static void image_snap(ID2D1DeviceContext *dc, float *dx, float *dy) {
	D2D_MATRIX_3X2_F m;
	float            dpi_x = 96.0f, dpi_y = 96.0f;
	ID2D1DeviceContext_GetTransform(dc, &m);
	ID2D1DeviceContext_GetDpi(dc, &dpi_x, &dpi_y);
	float sx = m._11 * dpi_x / 96.0f, ox = m._31 * dpi_x / 96.0f;
	float sy = m._22 * dpi_y / 96.0f, oy = m._32 * dpi_y / 96.0f;
	for (int i = 0; i < 4 && sx != 0.0f && sy != 0.0f; i++) {
		dx [i] = (roundf(dx [i] * sx + ox) - ox) / sx;
		dy [i] = (roundf(dy [i] * sy + oy) - oy) / sy;
	}
}

static void image_draw_nine(ID2D1DeviceContext *dc, ID2D1Bitmap *bmp, FluxRect d, FluxImage const *im, XentInsets n) {
	float k      = fminf(1.0f, fminf(d.w / fmaxf(n.left + n.right, 1e-3f), d.h / fmaxf(n.top + n.bottom, 1e-3f)));
	float sx [4] = {0.0f, n.left, im->w - n.right, im->w};
	float sy [4] = {0.0f, n.top, im->h - n.bottom, im->h};
	float dx [4] = {d.x, d.x + n.left * k, d.x + d.w - n.right * k, d.x + d.w};
	float dy [4] = {d.y, d.y + n.top * k, d.y + d.h - n.bottom * k, d.y + d.h};
	image_snap(dc, dx, dy);
	for (int cell = 0; cell < 9; cell++) {
		int i = cell % 3, j = cell / 3;
		if (sx [i + 1] <= sx [i] || sy [j + 1] <= sy [j] || dx [i + 1] <= dx [i] || dy [j + 1] <= dy [j]) continue;
		D2D1_RECT_F dst = {dx [i], dy [j], dx [i + 1], dy [j + 1]};
		D2D1_RECT_F src = {sx [i], sy [j], sx [i + 1], sy [j + 1]};
		ID2D1DeviceContext_DrawBitmap(dc, bmp, &dst, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, &src);
	}
}

static ID2D1SvgDocument *image_svg(FluxImage *im, ID2D1DeviceContext5 *dc5) {
	if (im->svg) return im->svg;
	HGLOBAL  mem    = GlobalAlloc(GMEM_MOVEABLE, im->px.svg_len);
	void    *at     = mem ? GlobalLock(mem) : NULL;
	IStream *stream = NULL;
	if (!at) {
		if (mem) GlobalFree(mem);
		return NULL;
	}
	memcpy(at, im->px.svg, im->px.svg_len);
	GlobalUnlock(mem);
	if (FAILED(CreateStreamOnHGlobal(mem, TRUE, &stream))) {
		GlobalFree(mem);
		return NULL;
	}
	ID2D1DeviceContext5_CreateSvgDocument(dc5, stream, (D2D_SIZE_F) {im->w, im->h}, &im->svg);
	IStream_Release(stream);
	return im->svg;
}

static bool image_draw_svg(FluxImage *im, ID2D1DeviceContext *dc, FluxRect d) {
	ID2D1DeviceContext5 *dc5 = NULL;
	if (FAILED(ID2D1DeviceContext_QueryInterface(dc, &IID_ID2D1DeviceContext5, ( void ** ) &dc5))) return false;
	ID2D1SvgDocument *svg = image_svg(im, dc5);
	if (svg) {
		D2D_MATRIX_3X2_F o, m;
		float            sx = d.w / im->w, sy = d.h / im->h;
		ID2D1DeviceContext_GetTransform(dc, &o);
		m._11 = sx * o._11;
		m._12 = sx * o._12;
		m._21 = sy * o._21;
		m._22 = sy * o._22;
		m._31 = d.x * o._11 + d.y * o._21 + o._31;
		m._32 = d.x * o._12 + d.y * o._22 + o._32;
		ID2D1DeviceContext_SetTransform(dc, &m);
		ID2D1DeviceContext5_DrawSvgDocument(dc5, svg);
		ID2D1DeviceContext_SetTransform(dc, &o);
	}
	ID2D1DeviceContext5_Release(dc5);
	return svg != NULL;
}

bool flux_image_draw(
  void *device_context, char const *source, uint32_t frame, FluxRect bounds, XentImageFit fit, XentInsets nine
) {
	FluxImage          *im = flux_image_get(source);
	ID2D1DeviceContext *dc = ( ID2D1DeviceContext * ) device_context;
	if (!dc || !flux_image_ready(im)) return false;
	image_bind(im, dc);
	FluxRect d = image_dest(bounds, im->w, im->h, fit);
	if (im->px.svg) return image_draw_svg(im, dc, d);
	ID2D1Bitmap *bmp = ( ID2D1Bitmap * ) image_bitmap(im, dc, frame);
	if (!bmp) return false;
	if (nine.left > 0.0f || nine.top > 0.0f || nine.right > 0.0f || nine.bottom > 0.0f) {
		image_draw_nine(dc, bmp, d, im, nine);
		return true;
	}
	D2D1_RECT_F dst = {d.x, d.y, d.x + d.w, d.y + d.h};
	ID2D1DeviceContext_DrawBitmap(dc, bmp, &dst, 1.0f, D2D1_BITMAP_INTERPOLATION_MODE_LINEAR, NULL);
	return true;
}

void *flux_image_bitmap(void *device_context, char const *source, float *w, float *h) {
	FluxImage *im = flux_image_get(source);
	if (!device_context || !flux_image_ready(im) || im->px.svg) return NULL;
	image_bind(im, device_context);
	*w = im->w;
	*h = im->h;
	return image_bitmap(im, ( ID2D1DeviceContext * ) device_context, 0u);
}
