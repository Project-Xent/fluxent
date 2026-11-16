#include "shell/flux_data_internal.h"

#include <stdlib.h>
#include <string.h>

static BITMAPV5HEADER dibv5_header(uint32_t width, uint32_t height) {
	return (BITMAPV5HEADER) {
	  .bV5Size        = sizeof(BITMAPV5HEADER),
	  .bV5Width       = ( LONG ) width,
	  .bV5Height      = ( LONG ) height,
	  .bV5Planes      = 1,
	  .bV5BitCount    = 32,
	  .bV5Compression = BI_BITFIELDS,
	  .bV5SizeImage   = ( DWORD ) (( size_t ) width * 4u * height),
	  .bV5RedMask     = 0x00ff0000u,
	  .bV5GreenMask   = 0x0000ff00u,
	  .bV5BlueMask    = 0x000000ffu,
	  .bV5AlphaMask   = 0xff000000u,
	  .bV5CSType      = LCS_sRGB,
	  .bV5Intent      = LCS_GM_IMAGES,
	};
}

static HGLOBAL dibv5_global(FluxBitmap const *bitmap) {
	size_t  row = ( size_t ) bitmap->width * 4u;
	HGLOBAL h   = GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPV5HEADER) + row * bitmap->height);
	char   *p   = h ? ( char * ) GlobalLock(h) : NULL;
	if (!p) {
		if (h) GlobalFree(h);
		return NULL;
	}
	*( BITMAPV5HEADER * ) p  = dibv5_header(bitmap->width, bitmap->height);
	p                       += sizeof(BITMAPV5HEADER);
	for (uint32_t y = 0; y < bitmap->height; y++)
		memcpy(p + row * (bitmap->height - 1u - y), bitmap->pixels + ( size_t ) bitmap->stride * y, row);
	GlobalUnlock(h);
	return h;
}

bool flux_data_setbitmap(FluxData *data, FluxBitmap const *bitmap) {
	if (!data || !bitmap || !bitmap->pixels || !bitmap->width || !bitmap->height) return false;
	return flux_data_put(data, CF_DIBV5, dibv5_global(bitmap));
}

typedef struct DibView {
	BITMAPINFOHEADER const *info;
	uint8_t const          *bits;
	uint32_t                width;
	uint32_t                height;
	size_t                  stride;
	bool                    top_down;
	bool                    alpha;
} DibView;

static bool dib_alpha(BITMAPINFOHEADER const *bi) {
	return bi->biBitCount == 32
	    && bi->biSize >= sizeof(BITMAPV4HEADER)
	    && (( BITMAPV4HEADER const * ) bi)->bV4AlphaMask != 0u;
}

static bool dib_supported(BITMAPINFOHEADER const *bi, size_t size) {
	if (size < sizeof(*bi) || bi->biSize < sizeof(*bi) || bi->biWidth <= 0 || bi->biHeight == 0) return false;
	if (bi->biBitCount != 24 && bi->biBitCount != 32) return false;
	return bi->biCompression == BI_RGB || bi->biCompression == BI_BITFIELDS;
}

static bool dib_view(void const *p, size_t size, DibView *out) {
	BITMAPINFOHEADER const *bi = ( BITMAPINFOHEADER const * ) p;
	if (!dib_supported(bi, size)) return false;
	size_t masks  = bi->biCompression == BI_BITFIELDS && bi->biSize == sizeof(*bi) ? 12u : 0u;
	size_t offset = bi->biSize + masks + ( size_t ) bi->biClrUsed * 4u;
	*out          = (DibView) {
	  .info     = bi,
	  .width    = ( uint32_t ) bi->biWidth,
	  .height   = ( uint32_t ) (bi->biHeight < 0 ? -bi->biHeight : bi->biHeight),
	  .stride   = (( size_t ) bi->biWidth * bi->biBitCount + 31u) / 32u * 4u,
	  .top_down = bi->biHeight < 0,
	  .alpha    = dib_alpha(bi),
	};
	out->bits = ( uint8_t const * ) p + offset;
	return offset + out->stride * out->height <= size;
}

static void dib_row(DibView const *v, uint32_t y, uint8_t *dst) {
	uint8_t const *src = v->bits + v->stride * (v->top_down ? y : v->height - 1u - y);
	uint32_t       bpp = v->info->biBitCount / 8u;
	for (uint32_t x = 0; x < v->width; x++, src += bpp, dst += 4) {
		dst [0] = src [0];
		dst [1] = src [1];
		dst [2] = src [2];
		dst [3] = v->alpha ? src [3] : 0xffu;
	}
}

static bool dib_copy(void const *p, size_t size, FluxBitmap *out) {
	DibView v;
	if (!dib_view(p, size, &v)) return false;
	uint8_t *pixels = ( uint8_t * ) malloc(( size_t ) v.width * 4u * v.height);
	if (!pixels) return false;
	for (uint32_t y = 0; y < v.height; y++) dib_row(&v, y, pixels + ( size_t ) v.width * 4u * y);
	*out = (FluxBitmap) {v.width, v.height, v.width * 4u, pixels};
	return true;
}

static bool bitmap_from(FluxData const *data, CLIPFORMAT cf, FluxBitmap *out) {
	STGMEDIUM sm = {0};
	if (!flux_data_take(data, cf, &sm)) return false;
	void const *p  = GlobalLock(sm.hGlobal);
	bool        ok = p && dib_copy(p, GlobalSize(sm.hGlobal), out);
	if (p) GlobalUnlock(sm.hGlobal);
	ReleaseStgMedium(&sm);
	return ok;
}

bool flux_data_bitmap(FluxData const *data, FluxBitmap *out) {
	if (!data || !out) return false;
	return bitmap_from(data, CF_DIBV5, out) || bitmap_from(data, CF_DIB, out);
}
