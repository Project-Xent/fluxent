#ifndef FLUX_TYPES_H
#define FLUX_TYPES_H

#include <stdint.h>
#include <stdbool.h>

typedef struct FluxColor { uint32_t rgba; } FluxColor;
typedef struct FluxRect  { float x, y, w, h; } FluxRect;
typedef struct FluxPoint { float x, y; }       FluxPoint;
typedef struct FluxSize  { float w, h; }       FluxSize;

typedef struct FluxDpiInfo {
    float dpi_x;
    float dpi_y;
} FluxDpiInfo;

static inline FluxColor flux_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    FluxColor c = { ((uint32_t)r << 24) | ((uint32_t)g << 16) | ((uint32_t)b << 8) | a };
    return c;
}

static inline FluxColor flux_color_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return flux_color_rgba(r, g, b, 255);
}

static inline FluxColor flux_color_hex(uint32_t hex) {
    uint8_t r = (hex >> 16) & 0xFF;
    uint8_t g = (hex >> 8) & 0xFF;
    uint8_t b = hex & 0xFF;
    return flux_color_rgb(r, g, b);
}

static inline float flux_color_rf(FluxColor c) { return ((c.rgba >> 24) & 0xFF) / 255.0f; }
static inline float flux_color_gf(FluxColor c) { return ((c.rgba >> 16) & 0xFF) / 255.0f; }
static inline float flux_color_bf(FluxColor c) { return ((c.rgba >> 8)  & 0xFF) / 255.0f; }
static inline float flux_color_af(FluxColor c) { return  (c.rgba        & 0xFF) / 255.0f; }

#if defined(_D2D1_H) || defined(__d2d1_h__)
static inline D2D1_COLOR_F flux_to_d2d_color(FluxColor c) {
    D2D1_COLOR_F d;
    d.r = flux_color_rf(c);
    d.g = flux_color_gf(c);
    d.b = flux_color_bf(c);
    d.a = flux_color_af(c);
    return d;
}
#endif

#define FLUX_RELEASE(p) do { if (p) { (p)->lpVtbl->Release(p); (p) = NULL; } } while (0)

#define FLUX_CHECK(hr) do { HRESULT _hr = (hr); if (FAILED(_hr)) return _hr; } while (0)

#endif