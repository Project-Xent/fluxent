#include "fluxent/flux_text.h"

#include <stdlib.h>
#include <string.h>

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <d2d1.h>
#include <dwrite.h>

/* ── Cache constants ─────────────────────────────────────────────── */

#define FLUX_FORMAT_CACHE_SIZE 16
#define FLUX_LAYOUT_CACHE_SIZE 64

/* ── Cache key / entry types ─────────────────────────────────────── */

typedef struct FluxFormatCacheKey {
    uint32_t family_hash;
    uint32_t size_bits;
    int      font_weight;
    int      text_align;
    int      vert_align;
    int      word_wrap;
} FluxFormatCacheKey;

typedef struct FluxFormatCacheEntry {
    FluxFormatCacheKey  key;
    IDWriteTextFormat  *format;
    uint64_t            last_used;
    bool                occupied;
} FluxFormatCacheEntry;

typedef struct FluxLayoutCacheKey {
    uint32_t text_hash;
    uint32_t text_len;
    uint32_t format_hash;
    uint32_t max_w_bits;
    uint32_t max_h_bits;
} FluxLayoutCacheKey;

typedef struct FluxLayoutCacheEntry {
    FluxLayoutCacheKey   key;
    IDWriteTextLayout   *layout;
    uint64_t             last_used;
    bool                 occupied;
} FluxLayoutCacheEntry;

/* ── Renderer struct ─────────────────────────────────────────────── */

struct FluxTextRenderer {
    IDWriteFactory   *factory;
    IDWriteTextFormat *default_format;
    wchar_t          *default_font;
    float             default_size;

    /* Format cache */
    FluxFormatCacheEntry format_cache[FLUX_FORMAT_CACHE_SIZE];
    uint64_t             cache_tick;

    /* Layout cache */
    FluxLayoutCacheEntry layout_cache[FLUX_LAYOUT_CACHE_SIZE];

    /* Shared brush for text drawing */
    ID2D1SolidColorBrush *shared_brush;
};

/* ── Hash helpers (FNV-1a) ───────────────────────────────────────── */

static uint32_t fnv1a_str(const char *s) {
    uint32_t h = 0x811c9dc5u;
    if (!s) return h;
    while (*s) {
        h ^= (uint8_t)*s++;
        h *= 0x01000193u;
    }
    return h;
}

static uint32_t fnv1a_bytes(const void *data, size_t len) {
    const uint8_t *p = (const uint8_t *)data;
    uint32_t h = 0x811c9dc5u;
    for (size_t i = 0; i < len; i++) {
        h ^= p[i];
        h *= 0x01000193u;
    }
    return h;
}

static uint32_t float_to_bits(float f) {
    uint32_t bits;
    memcpy(&bits, &f, sizeof(bits));
    return bits;
}

/* ── UTF-8 helpers ───────────────────────────────────────────────── */

static int flux_utf8_to_wchar(const char *utf8, wchar_t *out, int out_cap) {
    if (!utf8 || !utf8[0]) {
        if (out && out_cap > 0) out[0] = L'\0';
        return 0;
    }
    int needed = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    if (needed <= 0) return 0;
    if (!out) return needed;
    if (needed > out_cap) needed = out_cap;
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out, needed);
    return needed;
}

static wchar_t *utf8_to_wbuf(const char *utf8, wchar_t *stack, int stack_cap, int *out_len) {
    int n = flux_utf8_to_wchar(utf8, NULL, 0);
    if (n <= 0) { *out_len = 0; return NULL; }
    wchar_t *buf = (n <= stack_cap) ? stack : (wchar_t *)malloc(sizeof(wchar_t) * n);
    flux_utf8_to_wchar(utf8, buf, n);
    *out_len = n > 0 ? n - 1 : 0;
    return buf;
}

static void free_wbuf(wchar_t *buf, wchar_t *stack) {
    if (buf && buf != stack) free(buf);
}

/* ── DWrite enum converters ──────────────────────────────────────── */

static DWRITE_WORD_WRAPPING policy_to_dwrite_wrapping(XentLineBreakPolicy policy,
                                                       XentMeasureMode mode) {
    if (mode == XENT_MEASURE_UNDEFINED)
        return DWRITE_WORD_WRAPPING_NO_WRAP;
    switch (policy) {
    case XENT_LINE_BREAK_NO_WRAP:   return DWRITE_WORD_WRAPPING_NO_WRAP;
    case XENT_LINE_BREAK_WORD_WRAP: return DWRITE_WORD_WRAPPING_WRAP;
    case XENT_LINE_BREAK_CHAR_WRAP: return DWRITE_WORD_WRAPPING_CHARACTER;
    default:                        return DWRITE_WORD_WRAPPING_NO_WRAP;
    }
}

static float constraint_for_mode(float width_constraint, XentMeasureMode mode) {
    switch (mode) {
    case XENT_MEASURE_AT_MOST:
    case XENT_MEASURE_EXACTLY:
        return width_constraint;
    default:
        return 100000.0f;
    }
}

static DWRITE_TEXT_ALIGNMENT to_dw_align(FluxTextAlign a) {
    switch (a) {
    case FLUX_TEXT_CENTER:  return DWRITE_TEXT_ALIGNMENT_CENTER;
    case FLUX_TEXT_RIGHT:   return DWRITE_TEXT_ALIGNMENT_TRAILING;
    case FLUX_TEXT_JUSTIFY: return DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
    default:               return DWRITE_TEXT_ALIGNMENT_LEADING;
    }
}

static DWRITE_PARAGRAPH_ALIGNMENT to_dw_para(FluxTextVAlign a) {
    switch (a) {
    case FLUX_TEXT_VCENTER: return DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    case FLUX_TEXT_BOTTOM:  return DWRITE_PARAGRAPH_ALIGNMENT_FAR;
    default:               return DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    }
}

/* ── Format creation (uncached, used by cache miss + dwrite_measure) */

static IDWriteTextFormat *create_styled_format(FluxTextRenderer *tr, const FluxTextStyle *s) {
    const wchar_t *family = L"Segoe UI Variable";
    wchar_t fam_buf[128];
    if (s->font_family && s->font_family[0]) {
        int n = flux_utf8_to_wchar(s->font_family, fam_buf, 128);
        if (n > 0) family = fam_buf;
    }
    float size = s->font_size > 0.0f ? s->font_size : 14.0f;
    DWRITE_FONT_WEIGHT weight = (DWRITE_FONT_WEIGHT)(s->font_weight > 0 ? s->font_weight : 400);

    IDWriteTextFormat *fmt = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(tr->factory, family, NULL,
        weight, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
        size, L"", &fmt);
    if (FAILED(hr)) return NULL;

    IDWriteTextFormat_SetTextAlignment(fmt, to_dw_align(s->text_align));
    IDWriteTextFormat_SetParagraphAlignment(fmt, to_dw_para(s->vert_align));
    if (!s->word_wrap)
        IDWriteTextFormat_SetWordWrapping(fmt, DWRITE_WORD_WRAPPING_NO_WRAP);

    return fmt;
}

/* ── Format cache ────────────────────────────────────────────────── */

static FluxFormatCacheKey make_format_key(const FluxTextStyle *s) {
    FluxFormatCacheKey k;
    k.family_hash = fnv1a_str(s->font_family);
    k.size_bits   = float_to_bits(s->font_size > 0.0f ? s->font_size : 14.0f);
    k.font_weight = s->font_weight > 0 ? s->font_weight : 400;
    k.text_align  = (int)s->text_align;
    k.vert_align  = (int)s->vert_align;
    k.word_wrap   = s->word_wrap ? 1 : 0;
    return k;
}

static IDWriteTextFormat *get_or_create_format(FluxTextRenderer *tr, const FluxTextStyle *s) {
    FluxFormatCacheKey key = make_format_key(s);

    /* Search cache */
    for (int i = 0; i < FLUX_FORMAT_CACHE_SIZE; i++) {
        FluxFormatCacheEntry *e = &tr->format_cache[i];
        if (e->occupied && memcmp(&e->key, &key, sizeof(key)) == 0) {
            e->last_used = tr->cache_tick;
            return e->format; /* cache hit — caller must NOT Release */
        }
    }

    /* Cache miss — create */
    IDWriteTextFormat *fmt = create_styled_format(tr, s);
    if (!fmt) return NULL;

    /* Find slot: first empty, or evict LRU */
    int slot = -1;
    uint64_t oldest = UINT64_MAX;
    for (int i = 0; i < FLUX_FORMAT_CACHE_SIZE; i++) {
        if (!tr->format_cache[i].occupied) { slot = i; break; }
        if (tr->format_cache[i].last_used < oldest) {
            oldest = tr->format_cache[i].last_used;
            slot = i;
        }
    }

    /* Evict if needed */
    if (tr->format_cache[slot].occupied && tr->format_cache[slot].format)
        IDWriteTextFormat_Release(tr->format_cache[slot].format);

    tr->format_cache[slot].key       = key;
    tr->format_cache[slot].format    = fmt;
    tr->format_cache[slot].last_used = tr->cache_tick;
    tr->format_cache[slot].occupied  = true;

    return fmt; /* caller must NOT Release — cache owns it */
}

/* ── Layout cache ────────────────────────────────────────────────── */

static IDWriteTextLayout *get_or_create_layout(FluxTextRenderer *tr,
                                                const wchar_t *wtext, int wlen,
                                                const char *utf8_text,
                                                const FluxTextStyle *s,
                                                float max_w, float max_h) {
    FluxFormatCacheKey fk = make_format_key(s);

    FluxLayoutCacheKey key;
    key.text_hash   = fnv1a_str(utf8_text);
    key.text_len    = (uint32_t)(utf8_text ? strlen(utf8_text) : 0);
    key.format_hash = fnv1a_bytes(&fk, sizeof(fk));
    key.max_w_bits  = float_to_bits(max_w);
    key.max_h_bits  = float_to_bits(max_h);

    /* Search cache */
    for (int i = 0; i < FLUX_LAYOUT_CACHE_SIZE; i++) {
        FluxLayoutCacheEntry *e = &tr->layout_cache[i];
        if (e->occupied && memcmp(&e->key, &key, sizeof(key)) == 0) {
            e->last_used = tr->cache_tick;
            return e->layout; /* cache hit — caller must NOT Release */
        }
    }

    /* Cache miss — need format first (from format cache) */
    IDWriteTextFormat *fmt = get_or_create_format(tr, s);
    if (!fmt) return NULL;

    IDWriteTextLayout *layout = NULL;
    HRESULT hr = IDWriteFactory_CreateTextLayout(tr->factory, wtext, (UINT32)wlen,
        fmt, max_w, max_h, &layout);
    /* Do NOT release fmt — format cache owns it */
    if (FAILED(hr)) return NULL;

    /* Find slot: first empty, or evict LRU */
    int slot = -1;
    uint64_t oldest = UINT64_MAX;
    for (int i = 0; i < FLUX_LAYOUT_CACHE_SIZE; i++) {
        if (!tr->layout_cache[i].occupied) { slot = i; break; }
        if (tr->layout_cache[i].last_used < oldest) {
            oldest = tr->layout_cache[i].last_used;
            slot = i;
        }
    }

    /* Evict if needed */
    if (tr->layout_cache[slot].occupied && tr->layout_cache[slot].layout)
        IDWriteTextLayout_Release(tr->layout_cache[slot].layout);

    tr->layout_cache[slot].key       = key;
    tr->layout_cache[slot].layout    = layout;
    tr->layout_cache[slot].last_used = tr->cache_tick;
    tr->layout_cache[slot].occupied  = true;

    return layout; /* caller must NOT Release — cache owns it */
}

/* ── xent-core text backend (uncached — only called during layout) ─ */

static bool dwrite_measure(const XentTextBackend *backend,
                           const char *text,
                           float font_size,
                           float width_constraint,
                           XentLineBreakPolicy policy,
                           XentMeasureMode mode,
                           XentTextMetrics *out) {
    FluxTextRenderer *tr = (FluxTextRenderer *)backend->userdata;
    if (!tr || !out) return false;

    wchar_t stack_buf[256];
    int wlen = flux_utf8_to_wchar(text, NULL, 0);
    wchar_t *wtext = (wlen <= 256) ? stack_buf : (wchar_t *)malloc(sizeof(wchar_t) * wlen);
    if (!wtext) return false;
    flux_utf8_to_wchar(text, wtext, wlen);

    uint32_t text_len = (wlen > 0) ? (uint32_t)(wlen - 1) : 0;
    float layout_width = constraint_for_mode(width_constraint, mode);

    IDWriteTextFormat *fmt = NULL;
    HRESULT hr = IDWriteFactory_CreateTextFormat(
        tr->factory,
        tr->default_font ? tr->default_font : L"Segoe UI",
        NULL,
        DWRITE_FONT_WEIGHT_REGULAR,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        font_size,
        L"",
        &fmt);

    if (FAILED(hr)) {
        if (wtext != stack_buf) free(wtext);
        return false;
    }

    IDWriteTextFormat_SetWordWrapping(fmt, policy_to_dwrite_wrapping(policy, mode));

    IDWriteTextLayout *layout = NULL;
    hr = IDWriteFactory_CreateTextLayout(
        tr->factory,
        wtext, text_len,
        fmt,
        layout_width, 100000.0f,
        &layout);

    if (wtext != stack_buf) free(wtext);

    if (FAILED(hr)) {
        IDWriteTextFormat_Release(fmt);
        return false;
    }

    DWRITE_TEXT_METRICS metrics;
    hr = IDWriteTextLayout_GetMetrics(layout, &metrics);

    IDWriteTextLayout_Release(layout);
    IDWriteTextFormat_Release(fmt);

    if (FAILED(hr)) return false;

    out->width = metrics.widthIncludingTrailingWhitespace;
    out->height = metrics.height;
    out->line_count = metrics.lineCount;

    if (mode == XENT_MEASURE_EXACTLY) {
        out->width = width_constraint;
    }

    return true;
}

static bool dwrite_shape(const XentTextBackend *backend,
                         const char *text,
                         float font_size,
                         float width_constraint,
                         XentLineBreakPolicy policy,
                         XentMeasureMode mode,
                         XentShapedGlyph *out_glyphs,
                         uint32_t glyph_capacity,
                         XentShapedRun *out_runs,
                         uint32_t run_capacity,
                         XentShapedLine *out_lines,
                         uint32_t line_capacity,
                         XentShapingResult *out_result) {
    if (!out_result) return false;

    XentTextMetrics m;
    if (!dwrite_measure(backend, text, font_size, width_constraint, policy, mode, &m))
        return false;

    memset(out_result, 0, sizeof(*out_result));
    out_result->metrics = m;
    out_result->glyph_count = 0;
    out_result->run_count = 0;
    out_result->line_count = m.line_count;
    out_result->truncated = false;
    return true;
}

static const XentTextBackend kDWriteBackend = {
    .name    = "fluxent-dwrite",
    .measure = dwrite_measure,
    .shape   = dwrite_shape,
    .userdata = NULL,
};

/* ── Lifecycle ───────────────────────────────────────────────────── */

FluxTextRenderer *flux_text_renderer_create(void) {
    FluxTextRenderer *tr = (FluxTextRenderer *)calloc(1, sizeof(*tr));
    if (!tr) return NULL;

    HRESULT hr = DWriteCreateFactory(
        DWRITE_FACTORY_TYPE_SHARED,
        &IID_IDWriteFactory,
        (IUnknown **)&tr->factory);

    if (FAILED(hr)) {
        free(tr);
        return NULL;
    }

    tr->default_size = 14.0f;
    return tr;
}

void flux_text_renderer_destroy(FluxTextRenderer *tr) {
    if (!tr) return;

    /* Release all cached formats */
    for (int i = 0; i < FLUX_FORMAT_CACHE_SIZE; i++) {
        if (tr->format_cache[i].occupied && tr->format_cache[i].format)
            IDWriteTextFormat_Release(tr->format_cache[i].format);
    }

    /* Release all cached layouts */
    for (int i = 0; i < FLUX_LAYOUT_CACHE_SIZE; i++) {
        if (tr->layout_cache[i].occupied && tr->layout_cache[i].layout)
            IDWriteTextLayout_Release(tr->layout_cache[i].layout);
    }

    /* Release shared brush */
    if (tr->shared_brush)
        ID2D1SolidColorBrush_Release(tr->shared_brush);

    if (tr->factory) IDWriteFactory_Release(tr->factory);
    free(tr->default_font);
    free(tr);
}

bool flux_text_renderer_register(FluxTextRenderer *tr, XentContext *ctx) {
    if (!tr || !ctx) return false;

    XentTextBackend backend = kDWriteBackend;
    backend.userdata = tr;

    return xent_set_text_backend(ctx, &backend);
}

/* ── Public functions (cached) ───────────────────────────────────── */

#define FLUX_TEXT_UNBOUNDED 100000.0f

void flux_text_draw(FluxTextRenderer *tr, ID2D1RenderTarget *rt,
                    const char *text, const FluxRect *bounds,
                    const FluxTextStyle *style) {
    if (!tr || !rt || !text || !text[0] || !bounds || !style) return;

    tr->cache_tick++;

    wchar_t stack[256];
    int wlen;
    wchar_t *wtext = utf8_to_wbuf(text, stack, 256, &wlen);
    if (!wtext) return;

    IDWriteTextLayout *layout = get_or_create_layout(tr, wtext, wlen, text, style,
                                                      bounds->w, bounds->h);
    if (!layout) { free_wbuf(wtext, stack); return; }

    D2D1_COLOR_F cf;
    cf.r = flux_color_rf(style->color);
    cf.g = flux_color_gf(style->color);
    cf.b = flux_color_bf(style->color);
    cf.a = flux_color_af(style->color);

    /* Reuse shared brush — create once, then just SetColor */
    if (!tr->shared_brush) {
        D2D1_BRUSH_PROPERTIES bp;
        memset(&bp, 0, sizeof(bp));
        bp.opacity = 1.0f;
        bp.transform._11 = 1.0f; bp.transform._22 = 1.0f;
        HRESULT hr = ID2D1RenderTarget_CreateSolidColorBrush(rt, &cf, &bp, &tr->shared_brush);
        if (FAILED(hr)) { free_wbuf(wtext, stack); return; }
    } else {
        ID2D1SolidColorBrush_SetColor(tr->shared_brush, &cf);
    }

    D2D1_POINT_2F origin;
    origin.x = bounds->x;
    origin.y = bounds->y;
    ID2D1RenderTarget_DrawTextLayout(rt, origin, layout,
        (ID2D1Brush *)tr->shared_brush, D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);

    /* Do NOT release layout — cache owns it */
    free_wbuf(wtext, stack);
}

FluxSize flux_text_measure(FluxTextRenderer *tr, const char *text,
                           const FluxTextStyle *style, float max_width) {
    FluxSize result = {0, 0};
    if (!tr || !text || !text[0] || !style) return result;

    tr->cache_tick++;

    wchar_t stack[256];
    int wlen;
    wchar_t *wtext = utf8_to_wbuf(text, stack, 256, &wlen);
    if (!wtext) return result;

    float mw = max_width > 0.0f ? max_width : FLUX_TEXT_UNBOUNDED;
    IDWriteTextLayout *layout = get_or_create_layout(tr, wtext, wlen, text, style,
                                                      mw, FLUX_TEXT_UNBOUNDED);
    if (!layout) { free_wbuf(wtext, stack); return result; }

    DWRITE_TEXT_METRICS metrics;
    if (SUCCEEDED(IDWriteTextLayout_GetMetrics(layout, &metrics))) {
        result.w = metrics.widthIncludingTrailingWhitespace;
        result.h = metrics.height;
    }

    /* Do NOT release layout — cache owns it */
    free_wbuf(wtext, stack);
    return result;
}

int flux_text_hit_test(FluxTextRenderer *tr, const char *text,
                       const FluxTextStyle *style, float max_width,
                       float x, float y) {
    if (!tr || !text || !text[0] || !style) return 0;

    tr->cache_tick++;

    wchar_t stack[256];
    int wlen;
    wchar_t *wtext = utf8_to_wbuf(text, stack, 256, &wlen);
    if (!wtext) return 0;

    float mw = max_width > 0.0f ? max_width : FLUX_TEXT_UNBOUNDED;
    IDWriteTextLayout *layout = get_or_create_layout(tr, wtext, wlen, text, style,
                                                      mw, FLUX_TEXT_UNBOUNDED);
    if (!layout) { free_wbuf(wtext, stack); return 0; }

    BOOL is_trailing, is_inside;
    DWRITE_HIT_TEST_METRICS htm;
    int pos = 0;
    if (SUCCEEDED(IDWriteTextLayout_HitTestPoint(layout, x, y, &is_trailing, &is_inside, &htm)))
        pos = is_trailing ? (int)htm.textPosition + 1 : (int)htm.textPosition;

    /* Do NOT release layout — cache owns it */
    free_wbuf(wtext, stack);
    return pos;
}

FluxRect flux_text_caret_rect(FluxTextRenderer *tr, const char *text,
                              const FluxTextStyle *style, float max_width, int index) {
    FluxRect result = {0, 0, 1.0f, 0};
    if (!tr || !style) return result;

    tr->cache_tick++;

    const char *safe = (text && text[0]) ? text : " ";
    wchar_t stack[256];
    int wlen;
    wchar_t *wtext = utf8_to_wbuf(safe, stack, 256, &wlen);
    if (!wtext) return result;

    if (index < 0) index = 0;
    if (index > wlen) index = wlen;

    float mw = max_width > 0.0f ? max_width : FLUX_TEXT_UNBOUNDED;
    IDWriteTextLayout *layout = get_or_create_layout(tr, wtext, wlen, safe, style,
                                                      mw, FLUX_TEXT_UNBOUNDED);
    if (!layout) { free_wbuf(wtext, stack); return result; }

    float cx, cy;
    DWRITE_HIT_TEST_METRICS htm;
    if (SUCCEEDED(IDWriteTextLayout_HitTestTextPosition(layout, (UINT32)index, FALSE, &cx, &cy, &htm))) {
        result.x = cx;
        result.y = cy;
        result.w = 1.0f;
        result.h = htm.height;
    }

    /* Do NOT release layout — cache owns it */
    free_wbuf(wtext, stack);
    return result;
}

uint32_t flux_text_selection_rects(FluxTextRenderer *tr, const char *text,
                                   const FluxTextStyle *style, float max_width,
                                   int start, int end,
                                   FluxRect *out_rects, uint32_t max_rects) {
    if (!tr || !text || !text[0] || !style || !out_rects || max_rects == 0) return 0;
    if (start >= end) return 0;

    tr->cache_tick++;

    wchar_t stack[256];
    int wlen;
    wchar_t *wtext = utf8_to_wbuf(text, stack, 256, &wlen);
    if (!wtext) return 0;

    if (start < 0) start = 0;
    if (end > wlen) end = wlen;
    if (start >= end) { free_wbuf(wtext, stack); return 0; }

    float mw = max_width > 0.0f ? max_width : FLUX_TEXT_UNBOUNDED;
    IDWriteTextLayout *layout = get_or_create_layout(tr, wtext, wlen, text, style,
                                                      mw, FLUX_TEXT_UNBOUNDED);
    if (!layout) { free_wbuf(wtext, stack); return 0; }

    UINT32 count = 0;
    IDWriteTextLayout_HitTestTextRange(layout, (UINT32)start, (UINT32)(end - start),
                                       0, 0, NULL, 0, &count);
    if (count == 0) {
        /* Do NOT release layout — cache owns it */
        free_wbuf(wtext, stack);
        return 0;
    }

    DWRITE_HIT_TEST_METRICS *htm = (DWRITE_HIT_TEST_METRICS *)malloc(
        sizeof(DWRITE_HIT_TEST_METRICS) * count);
    if (!htm) {
        free_wbuf(wtext, stack);
        return 0;
    }

    IDWriteTextLayout_HitTestTextRange(layout, (UINT32)start, (UINT32)(end - start),
                                       0, 0, htm, count, &count);

    uint32_t n = count < max_rects ? count : max_rects;
    for (uint32_t i = 0; i < n; i++) {
        out_rects[i].x = htm[i].left;
        out_rects[i].y = htm[i].top;
        out_rects[i].w = htm[i].width;
        out_rects[i].h = htm[i].height;
    }

    free(htm);
    /* Do NOT release layout — cache owns it */
    free_wbuf(wtext, stack);
    return n;
}