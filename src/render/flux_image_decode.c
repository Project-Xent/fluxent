#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "render/flux_image_decode.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

enum
{
	DISPOSE_NONE,
	DISPOSE_BACKGROUND,
	DISPOSE_PREVIOUS,
};

typedef struct Canvas {
	IWICImagingFactory *wic;
	uint32_t            w;
	uint32_t            h;
	uint8_t            *px;
	uint8_t            *saved;
	FluxImageFrame     *frames;
	int                 nframe;
	int                 cap;
} Canvas;

typedef struct Patch {
	uint32_t       x, y, w, h;
	uint8_t const *px;
	bool           over;
	int            dispose;
	float          delay;
} Patch;

static bool is_svg(wchar_t const *path) {
	size_t n = wcslen(path);
	return n > 4 && _wcsicmp(path + n - 4, L".svg") == 0;
}

static uint8_t *read_file(wchar_t const *path, uint32_t *len) {
	FILE *f = _wfopen(path, L"rb");
	if (!f) return NULL;
	fseek(f, 0, SEEK_END);
	long     size = ftell(f);
	uint8_t *data = size > 0 ? ( uint8_t * ) malloc(( size_t ) size + 1u) : NULL;
	fseek(f, 0, SEEK_SET);
	if (data && fread(data, 1, ( size_t ) size, f) != ( size_t ) size) {
		free(data);
		data = NULL;
	}
	fclose(f);
	if (data) data [size] = 0;
	*len = data ? ( uint32_t ) size : 0u;
	return data;
}

static float svg_number(char const *tag, char const *name) {
	char const *at = strstr(tag, name);
	if (!at) return 0.0f;
	at += strlen(name);
	while (*at == ' ' || *at == '=' || *at == '"' || *at == '\'') at++;
	char *end = NULL;
	float v   = strtof(at, &end);
	return end && *end != '%' ? v : 0.0f;
}

static bool svg_size(uint8_t const *data, float *w, float *h) {
	char const *tag = strstr(( char const * ) data, "<svg");
	if (!tag) return false;
	char const *close = strchr(tag, '>');
	char        buf [1024];
	size_t      n = close ? ( size_t ) (close - tag) : strlen(tag);
	if (n >= sizeof(buf)) n = sizeof(buf) - 1u;
	memcpy(buf, tag, n);
	buf [n]             = 0;
	*w                  = svg_number(buf, " width");
	*h                  = svg_number(buf, " height");
	float       box [4] = {0};
	char const *vb      = strstr(buf, "viewBox");
	if (vb && (*w <= 0.0f || *h <= 0.0f))
		sscanf(vb + 7, " = \"%f%*[ ,]%f%*[ ,]%f%*[ ,]%f", &box [0], &box [1], &box [2], &box [3]);
	if (*w <= 0.0f) *w = box [2] > 0.0f ? box [2] : 300.0f;
	if (*h <= 0.0f) *h = box [3] > 0.0f ? box [3] : 150.0f;
	return true;
}

static uint32_t variant_u32(PROPVARIANT const *v, uint32_t fallback) {
	switch (v->vt) {
	case VT_UI1 : return v->bVal;
	case VT_UI2 : return v->uiVal;
	case VT_UI4 : return v->ulVal;
	default     : return fallback;
	}
}

static uint32_t meta_u32(IWICMetadataQueryReader *q, wchar_t const *name, uint32_t fallback) {
	PROPVARIANT v;
	PropVariantInit(&v);
	uint32_t out
	  = q && SUCCEEDED(IWICMetadataQueryReader_GetMetadataByName(q, name, &v)) ? variant_u32(&v, fallback) : fallback;
	PropVariantClear(&v);
	return out;
}

static IWICBitmapDecoder *open_decoder(IWICImagingFactory *wic, wchar_t const *path) {
	IWICBitmapDecoder *d = NULL;
	if (FAILED(
		  IWICImagingFactory_CreateDecoderFromFilename(wic, path, NULL, GENERIC_READ, WICDecodeMetadataCacheOnLoad, &d)
		))
		return NULL;
	return d;
}

static bool decoder_is(IWICBitmapDecoder *d, GUID const *format) {
	GUID g;
	return SUCCEEDED(IWICBitmapDecoder_GetContainerFormat(d, &g)) && IsEqualGUID(&g, format);
}

static bool decoder_size(IWICBitmapDecoder *d, float *w, float *h) {
	IWICMetadataQueryReader *q = NULL;
	if (decoder_is(d, &GUID_ContainerFormatGif) && SUCCEEDED(IWICBitmapDecoder_GetMetadataQueryReader(d, &q))) {
		*w = ( float ) meta_u32(q, L"/logscrdesc/Width", 0);
		*h = ( float ) meta_u32(q, L"/logscrdesc/Height", 0);
		IWICMetadataQueryReader_Release(q);
		if (*w > 0.0f && *h > 0.0f) return true;
	}
	IWICBitmapFrameDecode *f  = NULL;
	UINT                   uw = 0, uh = 0;
	if (FAILED(IWICBitmapDecoder_GetFrame(d, 0, &f))) return false;
	IWICBitmapFrameDecode_GetSize(f, &uw, &uh);
	IWICBitmapFrameDecode_Release(f);
	*w = ( float ) uw;
	*h = ( float ) uh;
	return uw > 0 && uh > 0;
}

bool flux_image_probe(IWICImagingFactory *wic, wchar_t const *path, float *w, float *h) {
	if (is_svg(path)) {
		uint32_t len  = 0;
		uint8_t *data = read_file(path, &len);
		bool     ok   = data && svg_size(data, w, h);
		free(data);
		return ok;
	}
	IWICBitmapDecoder *d = open_decoder(wic, path);
	if (!d) return false;
	bool ok = decoder_size(d, w, h);
	IWICBitmapDecoder_Release(d);
	return ok;
}

static uint8_t *source_pixels(IWICImagingFactory *wic, IWICBitmapSource *src, UINT *w, UINT *h) {
	IWICFormatConverter *conv = NULL;
	uint8_t             *px   = NULL;
	if (FAILED(IWICImagingFactory_CreateFormatConverter(wic, &conv))) return NULL;
	if (SUCCEEDED(IWICFormatConverter_Initialize(
		  conv, src, &GUID_WICPixelFormat32bppPBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom
		))
	    && SUCCEEDED(IWICFormatConverter_GetSize(conv, w, h)))
	{
		size_t size = ( size_t ) *w * *h * 4u;
		px          = ( uint8_t * ) malloc(size ? size : 1u);
		if (px && FAILED(IWICFormatConverter_CopyPixels(conv, NULL, *w * 4u, ( UINT ) size, px))) {
			free(px);
			px = NULL;
		}
	}
	IWICFormatConverter_Release(conv);
	return px;
}

static bool canvas_push(Canvas *c) {
	if (c->nframe == c->cap) {
		int             cap = c->cap ? c->cap * 2 : 8;
		FluxImageFrame *f   = ( FluxImageFrame * ) realloc(c->frames, sizeof(*f) * ( size_t ) cap);
		if (!f) return false;
		c->frames = f;
		c->cap    = cap;
	}
	IWICBitmap *bmp = NULL;
	if (FAILED(IWICImagingFactory_CreateBitmapFromMemory(
		  c->wic, c->w, c->h, &GUID_WICPixelFormat32bppPBGRA, c->w * 4u, c->w * c->h * 4u, c->px, &bmp
		)))
		return false;
	c->frames [c->nframe++] = (FluxImageFrame) {bmp, 0.0f};
	return true;
}

static void blend_row(uint8_t *dst, uint8_t const *src, uint32_t n, bool over) {
	if (!over) {
		memcpy(dst, src, n * 4u);
		return;
	}
	for (uint32_t i = 0; i < n * 4u; i += 4u) {
		uint32_t inv = 255u - src [i + 3];
		for (int k = 0; k < 4; k++) dst [i + k] = ( uint8_t ) (src [i + k] + (dst [i + k] * inv + 127u) / 255u);
	}
}

static void patch_rows(Canvas *c, Patch const *p, bool clear) {
	uint32_t w = p->x < c->w ? (p->x + p->w <= c->w ? p->w : c->w - p->x) : 0u;
	for (uint32_t y = 0; y < p->h && p->y + y < c->h && w; y++) {
		uint8_t *dst = c->px + (( size_t ) (p->y + y) * c->w + p->x) * 4u;
		if (clear) memset(dst, 0, w * 4u);
		else blend_row(dst, p->px + ( size_t ) y * p->w * 4u, w, p->over);
	}
}

static bool canvas_apply(Canvas *c, Patch const *p) {
	size_t size = ( size_t ) c->w * c->h * 4u;
	if (p->dispose == DISPOSE_PREVIOUS) memcpy(c->saved, c->px, size);
	patch_rows(c, p, false);
	if (!canvas_push(c)) return false;
	c->frames [c->nframe - 1].delay = p->delay > 0.011f ? p->delay : 0.1f;
	if (p->dispose == DISPOSE_BACKGROUND) patch_rows(c, p, true);
	if (p->dispose == DISPOSE_PREVIOUS) memcpy(c->px, c->saved, size);
	return true;
}

static bool canvas_init(Canvas *c, IWICImagingFactory *wic, float w, float h) {
	*c          = (Canvas) {.wic = wic, .w = ( uint32_t ) w, .h = ( uint32_t ) h};
	size_t size = ( size_t ) c->w * c->h * 4u;
	c->px       = ( uint8_t * ) calloc(size ? size : 1u, 1u);
	c->saved    = ( uint8_t * ) calloc(size ? size : 1u, 1u);
	return c->px && c->saved && size;
}

static void canvas_finish(Canvas *c, FluxImagePixels *out, bool ok) {
	free(c->px);
	free(c->saved);
	out->frames = c->frames;
	out->nframe = c->nframe;
	if (ok && c->nframe) return;
	flux_image_pixels_free(out);
}

static bool gif_frame(Canvas *c, IWICBitmapDecoder *d, UINT i) {
	IWICBitmapFrameDecode   *f = NULL;
	IWICMetadataQueryReader *q = NULL;
	UINT                     w = 0, h = 0;
	if (FAILED(IWICBitmapDecoder_GetFrame(d, i, &f))) return false;
	IWICBitmapFrameDecode_GetMetadataQueryReader(f, &q);
	uint8_t *px       = source_pixels(c->wic, ( IWICBitmapSource * ) f, &w, &h);
	uint32_t disposal = meta_u32(q, L"/grctlext/Disposal", 0);
	Patch    p        = {
	  .x       = meta_u32(q, L"/imgdesc/Left", 0),
	  .y       = meta_u32(q, L"/imgdesc/Top", 0),
	  .w       = w,
	  .h       = h,
	  .px      = px,
	  .over    = true,
	  .dispose = disposal == 2u ? DISPOSE_BACKGROUND
	           : disposal == 3u ? DISPOSE_PREVIOUS
	                            : DISPOSE_NONE,
	  .delay   = ( float ) meta_u32(q, L"/grctlext/Delay", 0) / 100.0f,
	};
	bool ok = px && canvas_apply(c, &p);
	free(px);
	if (q) IWICMetadataQueryReader_Release(q);
	IWICBitmapFrameDecode_Release(f);
	return ok;
}

static uint32_t gif_plays(IWICBitmapDecoder *d) {
	IWICMetadataQueryReader *q = NULL;
	PROPVARIANT              v;
	uint32_t                 plays = 1u;
	PropVariantInit(&v);
	if (FAILED(IWICBitmapDecoder_GetMetadataQueryReader(d, &q))) return plays;
	if (SUCCEEDED(IWICMetadataQueryReader_GetMetadataByName(q, L"/appext/Data", &v))
	    && v.vt == (VT_VECTOR | VT_UI1)
	    && v.caub.cElems >= 4
	    && v.caub.pElems [1] == 1u)
	{
		uint32_t loops = v.caub.pElems [2] | (( uint32_t ) v.caub.pElems [3] << 8);
		plays          = loops ? loops + 1u : 0u;
	}
	PropVariantClear(&v);
	IWICMetadataQueryReader_Release(q);
	return plays;
}

static bool gif_decode(IWICImagingFactory *wic, IWICBitmapDecoder *d, FluxImagePixels *out) {
	UINT   n = 0;
	Canvas c;
	bool   ok = canvas_init(&c, wic, out->w, out->h) && SUCCEEDED(IWICBitmapDecoder_GetFrameCount(d, &n));
	for (UINT i = 0; i < n && ok; i++) ok = gif_frame(&c, d, i);
	out->plays = gif_plays(d);
	canvas_finish(&c, out, ok);
	return ok && out->nframe;
}

static bool still_decode(IWICImagingFactory *wic, IWICBitmapDecoder *d, FluxImagePixels *out) {
	IWICBitmapFrameDecode *f = NULL;
	UINT                   w = 0, h = 0;
	IWICBitmap            *bmp = NULL;
	if (FAILED(IWICBitmapDecoder_GetFrame(d, 0, &f))) return false;
	uint8_t *px = source_pixels(wic, ( IWICBitmapSource * ) f, &w, &h);
	IWICBitmapFrameDecode_Release(f);
	if (px)
		IWICImagingFactory_CreateBitmapFromMemory(
		  wic, w, h, &GUID_WICPixelFormat32bppPBGRA, w * 4u, w * h * 4u, px, &bmp
		);
	free(px);
	out->frames = bmp ? ( FluxImageFrame * ) malloc(sizeof(FluxImageFrame)) : NULL;
	if (!out->frames) {
		if (bmp) IWICBitmap_Release(bmp);
		return false;
	}
	out->frames [0] = (FluxImageFrame) {bmp, 0.0f};
	out->nframe     = 1;
	return true;
}

static uint32_t be32(uint8_t const *p) {
	return (( uint32_t ) p [0] << 24) | (( uint32_t ) p [1] << 16) | (( uint32_t ) p [2] << 8) | p [3];
}

static void put32(uint8_t *p, uint32_t v) {
	p [0] = ( uint8_t ) (v >> 24);
	p [1] = ( uint8_t ) (v >> 16);
	p [2] = ( uint8_t ) (v >> 8);
	p [3] = ( uint8_t ) v;
}

static uint32_t crc32(uint8_t const *p, size_t n) {
	uint32_t c = 0xffffffffu;
	for (size_t i = 0; i < n; i++) {
		c ^= p [i];
		for (int k = 0; k < 8; k++) c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
	}
	return c ^ 0xffffffffu;
}

typedef struct Buf {
	uint8_t *data;
	size_t   len;
	size_t   cap;
} Buf;

static bool buf_put(Buf *b, void const *data, size_t n) {
	if (b->len + n > b->cap) {
		size_t   cap  = (b->len + n) * 2u;
		uint8_t *next = ( uint8_t * ) realloc(b->data, cap);
		if (!next) return false;
		b->data = next;
		b->cap  = cap;
	}
	memcpy(b->data + b->len, data, n);
	b->len += n;
	return true;
}

static bool buf_chunk(Buf *b, char const *type, uint8_t const *data, uint32_t n) {
	uint8_t head [8];
	put32(head, n);
	memcpy(head + 4, type, 4);
	size_t at = b->len;
	if (!buf_put(b, head, 8) || (n && !buf_put(b, data, n))) return false;
	uint8_t crc [4];
	put32(crc, crc32(b->data + at + 4, n + 4u));
	return buf_put(b, crc, 4);
}

typedef struct Apng {
	uint8_t const *file;
	uint32_t       len;
	uint8_t        ihdr [13];
	Buf            head;
	Buf            data;
	uint8_t const *fctl;
	uint32_t       plays;
	bool           seen_idat;
} Apng;

static bool apng_emit(Apng *a, Canvas *c) {
	if (!a->fctl || !a->data.len) return true;
	static uint8_t const sig [8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
	uint8_t              ihdr [13];
	Buf                  png = {0};
	memcpy(ihdr, a->ihdr, 13);
	memcpy(ihdr, a->fctl + 4, 8);
	bool               ok     = buf_put(&png, sig, 8)
	                         && buf_chunk(&png, "IHDR", ihdr, 13)
	                         && buf_put(&png, a->head.data, a->head.len)
	                         && buf_chunk(&png, "IDAT", a->data.data, ( uint32_t ) a->data.len)
	                         && buf_chunk(&png, "IEND", NULL, 0);
	IWICStream        *stream = NULL;
	IWICBitmapDecoder *d      = NULL;
	if (ok)
		ok = SUCCEEDED(IWICImagingFactory_CreateStream(c->wic, &stream))
		  && SUCCEEDED(IWICStream_InitializeFromMemory(stream, png.data, ( DWORD ) png.len))
		  && SUCCEEDED(IWICImagingFactory_CreateDecoderFromStream(
			c->wic, ( IStream * ) stream, NULL, WICDecodeMetadataCacheOnLoad, &d
		  ));
	IWICBitmapFrameDecode *f = NULL;
	UINT                   w = 0, h = 0;
	uint8_t               *px  = ok && SUCCEEDED(IWICBitmapDecoder_GetFrame(d, 0, &f))
	                             ? source_pixels(c->wic, ( IWICBitmapSource * ) f, &w, &h)
	                             : NULL;
	uint8_t const         *fc  = a->fctl;
	uint32_t               den = (( uint32_t ) fc [22] << 8) | fc [23];
	Patch                  p   = {
	  .x       = be32(fc + 12),
	  .y       = be32(fc + 16),
	  .w       = w,
	  .h       = h,
	  .px      = px,
	  .over    = fc [25] == 1,
	  .dispose = fc [24],
	  .delay   = ( float ) ((( uint32_t ) fc [20] << 8) | fc [21]) / ( float ) (den ? den : 100u),
	};
	if (c->nframe == 0 && p.dispose == DISPOSE_PREVIOUS) p.dispose = DISPOSE_BACKGROUND;
	ok = px && canvas_apply(c, &p);
	free(px);
	if (f) IWICBitmapFrameDecode_Release(f);
	if (d) IWICBitmapDecoder_Release(d);
	if (stream) IWICStream_Release(stream);
	free(png.data);
	a->data.len = 0;
	return ok;
}

static bool apng_chunk(Apng *a, Canvas *c, uint8_t const *type, uint8_t const *body, uint32_t n) {
	if (!memcmp(type, "IHDR", 4)) {
		if (n == 13) memcpy(a->ihdr, body, 13);
		return true;
	}
	if (!memcmp(type, "fcTL", 4)) {
		if (n < 26) return true;
		bool ok = apng_emit(a, c);
		a->fctl = body;
		return ok;
	}
	if (!memcmp(type, "IDAT", 4)) {
		a->seen_idat = true;
		return !a->fctl || buf_put(&a->data, body, n);
	}
	if (!memcmp(type, "fdAT", 4)) return n <= 4 || buf_put(&a->data, body + 4, n - 4u);
	if (!memcmp(type, "acTL", 4) && n >= 8) a->plays = be32(body + 4);
	if (a->seen_idat || !memcmp(type, "acTL", 4) || !memcmp(type, "IEND", 4)) return true;
	return buf_put(&a->head, type - 4, ( size_t ) n + 12u);
}

static bool apng_animated(uint8_t const *f, uint32_t len) {
	for (uint32_t at = 8; at + 8 <= len; at += be32(f + at) + 12u) {
		if (!memcmp(f + at + 4, "acTL", 4)) return true;
		if (!memcmp(f + at + 4, "IDAT", 4)) return false;
	}
	return false;
}

static bool apng_decode(IWICImagingFactory *wic, uint8_t const *f, uint32_t len, FluxImagePixels *out) {
	Apng   a = {.file = f, .len = len};
	Canvas c;
	bool   ok = canvas_init(&c, wic, out->w, out->h);
	for (uint32_t at = 8; ok && at + 12 <= len; at += be32(f + at) + 12u) {
		uint32_t n = be32(f + at);
		if (at + 12u + n > len) break;
		ok = apng_chunk(&a, &c, f + at + 4, f + at + 8, n);
	}
	ok         = ok && apng_emit(&a, &c);
	out->plays = a.plays;
	free(a.head.data);
	free(a.data.data);
	canvas_finish(&c, out, ok);
	return ok && out->nframe;
}

static bool raster_decode(IWICImagingFactory *wic, wchar_t const *path, FluxImagePixels *out) {
	IWICBitmapDecoder *d = open_decoder(wic, path);
	if (!d) return false;
	bool ok = decoder_size(d, &out->w, &out->h);
	if (ok && decoder_is(d, &GUID_ContainerFormatPng)) {
		uint32_t len  = 0;
		uint8_t *file = read_file(path, &len);
		if (file && len > 8 && apng_animated(file, len)) {
			ok = apng_decode(wic, file, len, out);
			free(file);
			IWICBitmapDecoder_Release(d);
			return ok;
		}
		free(file);
	}
	if (ok) ok = decoder_is(d, &GUID_ContainerFormatGif) ? gif_decode(wic, d, out) : still_decode(wic, d, out);
	IWICBitmapDecoder_Release(d);
	return ok;
}

bool flux_image_decode(IWICImagingFactory *wic, wchar_t const *path, FluxImagePixels *out) {
	*out = (FluxImagePixels) {0};
	if (!is_svg(path)) return raster_decode(wic, path, out);
	out->svg = read_file(path, &out->svg_len);
	return out->svg && svg_size(out->svg, &out->w, &out->h);
}

void flux_image_pixels_free(FluxImagePixels *px) {
	for (int i = 0; i < px->nframe; i++)
		if (px->frames [i].bitmap) IWICBitmap_Release(px->frames [i].bitmap);
	free(px->frames);
	free(px->svg);
	px->frames = NULL;
	px->nframe = 0;
	px->svg    = NULL;
}
