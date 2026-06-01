/**
 * @file flux_shape.h
 * @brief Vector chrome primitive: an animatable rounded-rectangle ShapeVisual.
 *
 * The building block for control chrome in the retained tree (ADR 0001 §4 #3).
 * Unlike a rasterized surface, a `CompositionShape` is vector — the compositor
 * re-rasterizes it at the final scale, so it stays crisp under scale animation,
 * and its fill color animates off-thread (pair with flux_compose_anim on the
 * fill brush's `L"Color"`).
 *
 * NOTE: compile-verified; visual fidelity (corner radii, stroke alignment) is a
 * runtime judgment on a GPU session.
 */
#ifndef FLUX_COMPOSE_SHAPE_H
#define FLUX_COMPOSE_SHAPE_H

#include "compose/flux_compose.h"
#include "fluxent/flux_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxShapeRect FluxShapeRect;

/**
 * @brief Create a rounded-rectangle shape visual.
 * @return New shape, or NULL on failure.
 */
FluxShapeRect               *flux_shape_rect_create(FluxCompositor *c, float w, float h, float radius);

/** @brief Destroy the shape visual and its resources. NULL is safe. */
void                         flux_shape_rect_destroy(FluxShapeRect *s);

/** @brief Set the rectangle extent (DIPs). */
void                         flux_shape_rect_set_size(FluxShapeRect *s, float w, float h);

/** @brief Set the corner radius (DIPs). */
void                         flux_shape_rect_set_corner_radius(FluxShapeRect *s, float radius);

/** @brief Set the fill color (use the fill brush directly to animate it). */
void                         flux_shape_rect_set_fill(FluxShapeRect *s, FluxColor fill);

/**
 * @brief Fill the shape with an arbitrary composition brush.
 *
 * Replaces the solid color fill with any `CompositionBrush` — e.g. the
 * backdrop-blur effect brush from flux_effect, turning the rounded rect into a
 * corner-clipped acrylic material layer. The shape takes its own reference; the
 * caller may release theirs after this returns.
 */
void                         flux_shape_rect_set_fill_brush(FluxShapeRect *s, WUC_Brush *brush);

/** @brief Set (or update) the stroke color and thickness; thickness 0 clears it visually. */
void                         flux_shape_rect_set_stroke(FluxShapeRect *s, FluxColor stroke, float thickness);

/** @brief The visual to parent into the tree (not owned). */
WUC_Visual                  *flux_shape_rect_visual(FluxShapeRect *s);

/** @brief The fill color brush (not owned) — animate its `L"Color"` property. */
WUC_CompositionColorBrush   *flux_shape_rect_fill_brush(FluxShapeRect *s);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_COMPOSE_SHAPE_H */
