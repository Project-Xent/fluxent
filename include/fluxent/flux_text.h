#ifndef FLUX_TEXT_H
#define FLUX_TEXT_H

#include "flux_types.h"
#include "flux_component_data.h"
#include <xent/xent.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ID2D1RenderTarget ID2D1RenderTarget;

typedef struct FluxTextRenderer FluxTextRenderer;

typedef struct FluxTextStyle {
    const char    *font_family;
    float          font_size;
    FluxFontWeight font_weight;
    FluxTextAlign  text_align;
    FluxTextVAlign vert_align;
    FluxColor      color;
    bool           word_wrap;
} FluxTextStyle;

FluxTextRenderer *flux_text_renderer_create(void);
void              flux_text_renderer_destroy(FluxTextRenderer *tr);
bool              flux_text_renderer_register(FluxTextRenderer *tr, XentContext *ctx);

void              flux_text_draw(FluxTextRenderer *tr, ID2D1RenderTarget *rt,
                                 const char *text, const FluxRect *bounds,
                                 const FluxTextStyle *style);

FluxSize          flux_text_measure(FluxTextRenderer *tr,
                                    const char *text, const FluxTextStyle *style,
                                    float max_width);

int               flux_text_hit_test(FluxTextRenderer *tr,
                                     const char *text, const FluxTextStyle *style,
                                     float max_width, float x, float y);

FluxRect          flux_text_caret_rect(FluxTextRenderer *tr,
                                       const char *text, const FluxTextStyle *style,
                                       float max_width, int index);

uint32_t          flux_text_selection_rects(FluxTextRenderer *tr,
                                            const char *text, const FluxTextStyle *style,
                                            float max_width, int start, int end,
                                            FluxRect *out_rects, uint32_t max_rects);

#ifdef __cplusplus
}
#endif

#endif