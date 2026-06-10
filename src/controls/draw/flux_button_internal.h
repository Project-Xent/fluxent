/**
 * @file flux_button_internal.h
 * @brief Shared button chrome used by Button, DropDownButton, and SplitButton.
 *
 * The button chrome (fill, elevation border, focus ring) and its state-to-color
 * mapping are identical across the button family. Renderers that add their own
 * content layout (a trailing chevron, a split secondary zone) paint the chrome
 * via flux_button_paint_chrome and then draw content into the returned bounds.
 */
#ifndef FLUX_BUTTON_INTERNAL_H
#define FLUX_BUTTON_INTERNAL_H

#include "render/flux_fluent.h"

/** @brief Resolved button chrome geometry and foreground for content drawing. */
typedef struct FluxButtonChrome {
	FluxRect  sb;         /**< Pixel-snapped bounds the chrome was painted into. */
	float     radius;     /**< Resolved corner radius. */
	FluxColor text_color; /**< Resolved foreground for label/icon/chevron. */
	FluxColor fill;       /**< Resolved (animated) background fill. */
	bool      is_accent;  /**< Chrome used the accent palette. */
} FluxButtonChrome;

/**
 * @brief Paint button fill + border + focus ring and return geometry/foreground.
 *
 * Does everything render_button_common does except draw the centered content,
 * letting callers place their own content (chevron, split zones) afterwards.
 */
FluxButtonChrome flux_button_paint_chrome(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state,
  bool force_accent
);

/**
 * @brief Draw the standard centered icon+label content, clipped to @p sb.
 * Uses standard button padding; safe to pass a sub-rect of the full button.
 */
void flux_button_draw_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *sb, FluxColor text_color
);

/** @brief Draw a Segoe Fluent Icons chevron glyph centered in @p box. */
void flux_button_draw_chevron(
  FluxRenderContext const *rc, FluxRect const *box, wchar_t const *glyph, float font_size, FluxColor color
);

/* DropDownButton/SplitButton chevron. WinUI uses an AnimatedIcon (12x12) whose
 * Lottie visual Uniform-scales to fill the box; its FontIconSource fallback
 * (U+E96E @ 8pt) only shows when the animation is unavailable and renders far
 * smaller than the visible animated chevron. With no Lottie backend we match the
 * visible 12x12 footprint instead: the standard ChevronDown (U+E70D) at 12pt,
 * consistent with the ComboBox and Expander chevrons. Shared by both controls. */
#define FLUX_CHEVRON_GLYPH L"\xE70D"
#define FLUX_CHEVRON_FONT  12.0f
#define FLUX_CHEVRON_BOX   12.0f

#endif
