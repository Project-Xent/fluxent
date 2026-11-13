#ifndef FLUX_TEXTBOX_DATA_H
#define FLUX_TEXTBOX_DATA_H

#include <fluxent/flux_types.h>

#include <stdbool.h>
#include <stdint.h>
#include <wchar.h>

typedef struct FluxTextBoxData {
	char const    *content;
	char const    *placeholder;
	char const    *font_family;
	float          font_size;
	FluxColor      text_color;
	FluxColor      placeholder_color;
	FluxColor      selection_color;
	uint32_t       cursor_position;
	uint32_t       selection_start;
	uint32_t       selection_end;
	float          scroll_offset_x;
	float          scroll_offset_y;
	uint32_t       max_length;
	bool           enabled;
	bool           readonly;
	bool           multiline;
	bool           combo_editor;

	wchar_t const *composition_text;
	uint32_t       composition_length;
	uint32_t       composition_cursor;

	void           (*on_change)(void *ctx, char const *text);
	void          *on_change_ctx;
	void           (*on_submit)(void *ctx);
	void          *on_submit_ctx;
} FluxTextBoxData;

typedef struct FluxPasswordBoxData {
	char const    *content;
	char const    *placeholder;
	char const    *font_family;
	float          font_size;
	FluxColor      text_color;
	FluxColor      placeholder_color;
	FluxColor      selection_color;
	uint32_t       cursor_position;
	uint32_t       selection_start;
	uint32_t       selection_end;
	float          scroll_offset_x;
	uint32_t       max_length;
	bool           enabled;
	bool           readonly;
	wchar_t        mask_char;
	bool           show_plain;

	wchar_t const *composition_text;
	uint32_t       composition_length;
	uint32_t       composition_cursor;

	void           (*on_change)(void *ctx, char const *text);
	void          *on_change_ctx;
	void           (*on_submit)(void *ctx);
	void          *on_submit_ctx;
} FluxPasswordBoxData;

typedef enum FluxNBSpinPlacement
{
	FLUX_NB_SPIN_HIDDEN  = 0,
	FLUX_NB_SPIN_COMPACT = 1,
	FLUX_NB_SPIN_INLINE  = 2,
} FluxNBSpinPlacement;

typedef enum FluxNBValidation
{
	FLUX_NB_VALIDATE_OVERWRITE = 0,
	FLUX_NB_VALIDATE_DISABLED  = 1,
} FluxNBValidation;

typedef struct FluxNumberBoxData {
	char const    *content;
	char const    *placeholder;
	char const    *font_family;
	float          font_size;
	FluxColor      text_color;
	FluxColor      placeholder_color;
	FluxColor      selection_color;
	uint32_t       cursor_position;
	uint32_t       selection_start;
	uint32_t       selection_end;
	float          scroll_offset_x;
	bool           enabled;
	bool           readonly;

	double         min_value;
	double         max_value;
	double         step;
	double         current_value;
	bool           spin_enabled;

	wchar_t const *composition_text;
	uint32_t       composition_length;
	uint32_t       composition_cursor;

	void           (*on_change)(void *ctx, char const *text);
	void          *on_change_ctx;
	void           (*on_value_change)(void *ctx, double value);
	void          *on_value_change_ctx;
	void           (*on_submit)(void *ctx);
	void          *on_submit_ctx;
} FluxNumberBoxData;

#endif
