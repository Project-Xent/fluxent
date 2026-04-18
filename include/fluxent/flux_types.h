/**
 * @file flux_types.h
 * @brief Core type definitions for Fluxent (FluxColor, FluxRect, FluxPoint, FluxSize).
 *
 * This header defines the fundamental value types used throughout the Fluxent
 * framework, including color representation, geometry, and pointer input events.
 */

#ifndef FLUX_TYPES_H
#define FLUX_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#if defined(_MSC_VER) && !defined(__attribute__)
  #define __attribute__(x)
#endif

/** @brief RGBA color packed as uint32_t (0xRRGGBBAA). */
typedef struct FluxColor {
	uint32_t rgba;
} FluxColor;

/** @brief Axis-aligned rectangle (position + size). */
typedef struct FluxRect {
	float x, y, w, h;
} FluxRect;

/** @brief 2D point. */
typedef struct FluxPoint {
	float x, y;
} FluxPoint;

/** @brief 2D size. */
typedef struct FluxSize {
	float w, h;
} FluxSize;

/** @brief DPI information for a display. */
typedef struct FluxDpiInfo {
	float dpi_x;
	float dpi_y;
} FluxDpiInfo;

/* ─────────────────────────────────────────────────────────────────
 *  Pointer input model (shared by flux_window + flux_input + app)
 *
 *  One unified event struct replaces the old (x,y,button,down,type)
 *  parameter soup and the separate scroll / context-menu callbacks.
 *  Every event kind carries identical coordinate + device + id
 *  fields so downstream consumers don't branch on callback identity.
 * ───────────────────────────────────────────────────────────────── */
typedef enum FluxPointerType
{
	FLUX_POINTER_MOUSE = 0,
	FLUX_POINTER_TOUCH = 1,
	FLUX_POINTER_PEN   = 2,
} FluxPointerType;

typedef enum FluxPointerEventKind
{
	FLUX_POINTER_DOWN,         /* button pressed / finger contact / pen contact        */
	FLUX_POINTER_MOVE,         /* cursor or contact moved                               */
	FLUX_POINTER_UP,           /* button released / finger lift / pen lift              */
	FLUX_POINTER_CANCEL,       /* input stream aborted (capture lost, system gesture)   */
	FLUX_POINTER_WHEEL,        /* wheel/touchpad scroll — wheel_dx/dy carry notches     */
	FLUX_POINTER_CONTEXT_MENU, /* right-click release OR touch press-and-hold OR Menu key*/
} FluxPointerEventKind;

typedef enum FluxPointerButton
{
	FLUX_POINTER_BUTTON_NONE   = 0,
	FLUX_POINTER_BUTTON_LEFT   = 1u << 0,
	FLUX_POINTER_BUTTON_RIGHT  = 1u << 1,
	FLUX_POINTER_BUTTON_MIDDLE = 1u << 2,
} FluxPointerButton;

typedef struct FluxPointerEvent {
	FluxPointerEventKind kind;
	FluxPointerType      device;     /* MOUSE / TOUCH / PEN                          */
	uint32_t             pointer_id; /* Win32 pointer id (0 = synthesized / unknown) */

	/* Position in window-client DIPs.  Valid for every kind.  For
	 * CONTEXT_MENU from keyboard Menu key the window fills in the
	 * caret/focus rect center. */
	float                x, y;

	/* Bitmask of FluxPointerButton values held at the time of the
	 * event.  For DOWN/UP the changed_button is the single bit that
	 * just transitioned.  buttons already reflects the new state. */
	uint32_t             buttons;
	uint32_t             changed_button; /* single bit for DOWN/UP; 0 otherwise          */

	/* Wheel deltas in notches (one physical click = ±1.0).  Only
	 * populated for FLUX_POINTER_WHEEL. */
	float                wheel_dx;
	float                wheel_dy;
} FluxPointerEvent;

static FluxColor inline flux_color_rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	FluxColor c = {(( uint32_t ) r << 24) | (( uint32_t ) g << 16) | (( uint32_t ) b << 8) | a};
	return c;
}

static FluxColor inline flux_color_rgb(uint8_t r, uint8_t g, uint8_t b) { return flux_color_rgba(r, g, b, 255); }

static FluxColor inline flux_color_hex(uint32_t hex) {
	uint8_t r = (hex >> 16) & 0xff;
	uint8_t g = (hex >> 8) & 0xff;
	uint8_t b = hex & 0xff;
	return flux_color_rgb(r, g, b);
}

static float inline flux_color_rf(FluxColor c) { return ((c.rgba >> 24) & 0xff) / 255.0f; }

static float inline flux_color_gf(FluxColor c) { return ((c.rgba >> 16) & 0xff) / 255.0f; }

static float inline flux_color_bf(FluxColor c) { return ((c.rgba >> 8) & 0xff) / 255.0f; }

static float inline flux_color_af(FluxColor c) { return (c.rgba & 0xff) / 255.0f; }

#if defined(_D2D1_H_) || defined(__d2d1_h__) || defined(D2D1_APPEND_ALIGNED_ELEMENT)
static inline D2D1_COLOR_F flux_to_d2d_color(FluxColor c) {
	D2D1_COLOR_F d;
	d.r = flux_color_rf(c);
	d.g = flux_color_gf(c);
	d.b = flux_color_bf(c);
	d.a = flux_color_af(c);
	return d;
}
#endif

#ifdef COBJMACROS
  #define FLUX_RELEASE(p)                                   \
		  do {                                              \
				  if (p) {                                  \
					  IUnknown_Release(( IUnknown * ) (p)); \
					  (p) = NULL;                           \
				  }                                         \
		  }                                                 \
	  while (0)
#else
  #define FLUX_RELEASE(p)                                                        \
		  do {                                                                   \
				  if (p) {                                                       \
					  (( IUnknown * ) (p))->lpVtbl->Release(( IUnknown * ) (p)); \
					  (p) = NULL;                                                \
				  }                                                              \
		  }                                                                      \
	  while (0)
#endif

#define FLUX_CHECK(hr)                   \
		do {                             \
			HRESULT _hr = (hr);          \
			if (FAILED(_hr)) return _hr; \
		}                                \
	while (0)

#endif
