#ifndef FLUX_BUTTON_DATA_H
#define FLUX_BUTTON_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>

typedef XtkButtonStyle FluxButtonStyle;

#define FLUX_BUTTON_STANDARD XTK_BUTTON_STANDARD
#define FLUX_BUTTON_SUBTLE   XTK_BUTTON_SUBTLE
#define FLUX_BUTTON_TEXT     XTK_BUTTON_TEXT
#define FLUX_BUTTON_ACCENT   XTK_BUTTON_ACCENT

typedef struct FluxButtonData {
	char const     *label;
	char const     *icon_name;
	FluxColor       label_color;
	FluxColor       hover_color;
	FluxColor       pressed_color;
	float           font_size;
	FluxButtonStyle style;
	bool            is_checked;
	bool            pressed_primary;
	void            (*on_click)(void *ctx);
	void           *on_click_ctx;
	void            (*on_toggle)(void *ctx, bool checked);
	void           *on_toggle_ctx;
} FluxButtonData;

typedef struct FluxRepeatButtonData {
	char const     *label;
	char const     *icon_name;
	FluxColor       label_color;
	float           font_size;
	FluxButtonStyle style;
	uint32_t        repeat_delay_ms;
	uint32_t        repeat_interval_ms;
	void            (*on_click)(void *ctx);
	void           *on_click_ctx;
} FluxRepeatButtonData;

typedef struct FluxHyperlinkData {
	char const *label;
	char const *url;
	char const *icon_name;
	FluxColor   label_color;
	float       font_size;
	bool        visited;
	void        (*on_click)(void *ctx);
	void       *on_click_ctx;
} FluxHyperlinkData;

#endif
