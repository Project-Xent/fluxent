#ifndef FLUX_TYPES_H
#define FLUX_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <xtk/xtk_types.h>
#include <xent/xent_types.h>

typedef XtkColor FluxColor;

typedef struct FluxRect {
	float x, y, w, h;
} FluxRect;

typedef struct FluxPoint {
	float x, y;
} FluxPoint;

typedef struct FluxSize {
	float w, h;
} FluxSize;

typedef struct FluxDpi {
	float dpi_x;
	float dpi_y;
} FluxDpi;

#define FLUX_DPI_BASE 96.0f

typedef enum FluxKey
{
	FLUX_KEY_NONE = 0,
	FLUX_KEY_BACKSPACE,
	FLUX_KEY_TAB,
	FLUX_KEY_ENTER,
	FLUX_KEY_SHIFT,
	FLUX_KEY_CONTROL,
	FLUX_KEY_ALT,
	FLUX_KEY_ESCAPE,
	FLUX_KEY_SPACE,
	FLUX_KEY_PAGE_UP,
	FLUX_KEY_PAGE_DOWN,
	FLUX_KEY_END,
	FLUX_KEY_HOME,
	FLUX_KEY_LEFT,
	FLUX_KEY_UP,
	FLUX_KEY_RIGHT,
	FLUX_KEY_DOWN,
	FLUX_KEY_DELETE,
	FLUX_KEY_A,
	FLUX_KEY_C,
	FLUX_KEY_V,
	FLUX_KEY_X,
	FLUX_KEY_Y,
	FLUX_KEY_Z,
	FLUX_KEY_F4,
	FLUX_KEY_F6,
	FLUX_KEY_ADD,
	FLUX_KEY_SUBTRACT,
	FLUX_KEY_COUNT,
} FluxKey;

bool flux_key_held(FluxKey key);

typedef enum FluxPointerType
{
	FLUX_POINTER_MOUSE = 0,
	FLUX_POINTER_TOUCH = 1,
	FLUX_POINTER_PEN   = 2,
} FluxPointerType;

typedef enum FluxPointerEventKind
{
	FLUX_POINTER_DOWN,
	FLUX_POINTER_MOVE,
	FLUX_POINTER_UP,
	FLUX_POINTER_CANCEL,
	FLUX_POINTER_WHEEL,
	FLUX_POINTER_HOLD,
	FLUX_POINTER_CONTEXT_MENU,
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
	FluxPointerType      device;
	uint32_t             pointer_id;

	float                x, y;

	uint32_t             buttons;
	uint32_t             changed_button;

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
static inline D2D1_COLOR_F flux_color_tod2d(FluxColor c) {
	D2D1_COLOR_F d;
	d.r = flux_color_rf(c);
	d.g = flux_color_gf(c);
	d.b = flux_color_bf(c);
	d.a = flux_color_af(c);
	return d;
}
#endif

#ifdef COBJMACROS
  #define FLUX_RELEASE(p)                           \
	  do {                                          \
		  if (p) {                                  \
			  IUnknown_Release(( IUnknown * ) (p)); \
			  (p) = NULL;                           \
		  }                                         \
	  }                                             \
	  while (0)
#else
  #define FLUX_RELEASE(p)                                                \
	  do {                                                               \
		  if (p) {                                                       \
			  (( IUnknown * ) (p))->lpVtbl->Release(( IUnknown * ) (p)); \
			  (p) = NULL;                                                \
		  }                                                              \
	  }                                                                  \
	  while (0)
#endif

#define FLUX_CHECK(hr)               \
	do {                             \
		HRESULT _hr = (hr);          \
		if (FAILED(_hr)) return _hr; \
	}                                \
	while (0)

#endif
