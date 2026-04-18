/**
 * @file tb_internal.h
 * @brief Internal definitions for TextBox/PasswordBox/NumberBox controls.
 *
 * This header is private to the textbox implementation and should not be
 * included by external code. It defines:
 * - FluxTextBoxInputData: extended runtime state for text input controls
 * - TbEditHistory: undo/redo entry structure
 * - Internal helper functions for buffer, undo, clipboard, IME, etc.
 */
#ifndef FLUX_TB_INTERNAL_H
#define FLUX_TB_INTERNAL_H

#include "fluxent/fluxent.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_window.h"
#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

/* ═══════════════════════════════════════════════════════════════════════
   Constants
   ═══════════════════════════════════════════════════════════════════════ */

#define FLUX_TEXTBOX_INITIAL_CAP 128
#define TB_UNDO_MAX              50
#define TB_TYPING_MERGE_MS       1000

/* ═══════════════════════════════════════════════════════════════════════
   Types
   ═══════════════════════════════════════════════════════════════════════ */

/**
 * @brief Undo/redo history entry.
 */
typedef struct TbEditHistory {
	char    *text;
	uint32_t len;
	uint32_t sel_start;
	uint32_t sel_end;
} TbEditHistory;

/* Forward declaration */
struct FluxApp;

/**
 * @brief Extended runtime data for TextBox/PasswordBox/NumberBox.
 *
 * The `base` field must be first so that renderers can cast to FluxTextBoxData*.
 * This structure holds all mutable state for text editing, including:
 * - Text buffer and capacity
 * - Undo/redo stacks
 * - IME composition state
 * - NumberBox-specific fields
 */
typedef struct FluxTextBoxInputData {
	FluxTextBoxData base; /* must be first — renderer casts to FluxTextBoxData* */
	char           *buffer;
	uint32_t        buf_cap;
	uint32_t        buf_len;
	XentContext    *ctx;
	XentNodeId      node;
	FluxNodeStore  *store;
	struct FluxApp *app; /* used to get text renderer and window */

	/* Undo/redo */
	TbEditHistory   undo_stack [TB_UNDO_MAX];
	int             undo_top; /* index of next free slot, -1 means empty */
	TbEditHistory   redo_stack [TB_UNDO_MAX];
	int             redo_top;

	/* Typing merge for undo grouping */
	ULONGLONG       last_typing_time;
	bool            last_op_was_typing;

	/* IME composition (owned wchar_t buffer) */
	wchar_t        *ime_buf;
	uint32_t        ime_buf_cap;

	/* Drag selection state */
	bool            dragging;
	uint32_t        drag_anchor; /* byte offset of anchor for shift-drag */

	/* Surrogate pair buffering for U+10000+ characters (emoji, CJK ext, etc.) */
	wchar_t         high_surrogate;

	/* ---- NumberBox extension (valid when control_type == NUMBER_BOX) ---- */
	double          nb_value;           /* current numeric Value (NaN = empty) */
	double          nb_minimum;         /* default: -1e308 */
	double          nb_maximum;         /* default: 1e308 */
	double          nb_small_change;    /* default: 1.0 (arrows, spin buttons) */
	double          nb_large_change;    /* default: 10.0 (PageUp/PageDown) */
	bool            nb_is_wrap_enabled; /* default: false */
	uint8_t         nb_spin_placement;  /* FluxNBSpinPlacement: 0=Hidden, 2=Inline */
	uint8_t         nb_validation;      /* FluxNBValidation: 0=Overwrite, 1=Disabled */
	bool            nb_value_updating;  /* re-entrancy guard */
	bool            nb_text_updating;   /* re-entrancy guard */
	void            (*nb_on_value_change)(void *ctx, double value);
	void           *nb_on_value_change_ctx;
	bool            nb_stepping;         /* true after keyboard step — suppress caret/selection */
	bool            nb_focus_from_click; /* set by pointer_down so on_focus knows not to enter stepping */
} FluxTextBoxInputData;

/* ═══════════════════════════════════════════════════════════════════════
   Buffer helpers (tb_buffer.c)
   ═══════════════════════════════════════════════════════════════════════ */

void          tb_ensure_cap(FluxTextBoxInputData *tb, uint32_t needed);
uint32_t      tb_utf8_char_len(char const *s, uint32_t pos);
uint32_t      tb_utf8_prev(char const *s, uint32_t pos);
uint32_t      tb_utf8_next(char const *s, uint32_t pos);
uint32_t      tb_utf16_to_byte_offset(char const *s, uint32_t utf16_pos);
uint32_t      tb_byte_to_utf16_offset(char const *s, uint32_t byte_pos);
void          tb_delete_range(FluxTextBoxInputData *tb, uint32_t from, uint32_t to);
void          tb_insert_utf8(FluxTextBoxInputData *tb, uint32_t pos, char const *s, uint32_t slen);
void          tb_delete_selection(FluxTextBoxInputData *tb);
bool          tb_has_selection(FluxTextBoxInputData const *tb);
uint32_t      tb_word_start(char const *s, uint32_t pos);
uint32_t      tb_word_end(char const *s, uint32_t pos);

/* ═══════════════════════════════════════════════════════════════════════
   Undo/redo (tb_undo.c)
   ═══════════════════════════════════════════════════════════════════════ */

void          tb_push_undo(FluxTextBoxInputData *tb);
void          tb_undo(FluxTextBoxInputData *tb);
void          tb_redo(FluxTextBoxInputData *tb);
void          tb_free_undo_redo(FluxTextBoxInputData *tb);

/* ═══════════════════════════════════════════════════════════════════════
   Clipboard (tb_clipboard.c)
   ═══════════════════════════════════════════════════════════════════════ */

void          tb_perform_copy(FluxTextBoxInputData *tb);
void          tb_perform_paste(FluxTextBoxInputData *tb);

/* ═══════════════════════════════════════════════════════════════════════
   Scroll and IME position (tb_scroll.c)
   ═══════════════════════════════════════════════════════════════════════ */

FluxTextStyle tb_make_style(FluxTextBoxInputData const *tb);
void          tb_update_scroll(FluxTextBoxInputData *tb);
void          tb_update_ime_position(FluxTextBoxInputData *tb);
void          tb_notify_change(FluxTextBoxInputData *tb);

/* ═══════════════════════════════════════════════════════════════════════
   PasswordBox mask (pb_mask.c)
   ═══════════════════════════════════════════════════════════════════════ */

uint32_t      pb_build_mask(char const *src, uint32_t src_len, char *dst, uint32_t dst_cap);
uint32_t      pb_mask_offset_to_original(char const *original, uint32_t mask_byte_offset);

/* ═══════════════════════════════════════════════════════════════════════
   NumberBox (nb_validate.c)
   ═══════════════════════════════════════════════════════════════════════ */

bool          nb_is_number_box(FluxTextBoxInputData *tb);
void          nb_update_text_to_value(FluxTextBoxInputData *tb);
void          nb_validate_input(FluxTextBoxInputData *tb);
void          nb_step_value(FluxTextBoxInputData *tb, double change);
void          nb_set_value(FluxTextBoxInputData *tb, double new_value);
void          nb_coerce_value(FluxTextBoxInputData *tb);
void          nb_sync_semantics(FluxTextBoxInputData *tb);

/* ═══════════════════════════════════════════════════════════════════════
   Event handlers (tb_input.c)
   ═══════════════════════════════════════════════════════════════════════ */

void          tb_on_key(void *ctx, unsigned int vk, bool down);
void          tb_on_char(void *ctx, wchar_t ch);
void          tb_on_pointer_move(void *ctx, float local_x, float local_y);
void          tb_on_pointer_down(void *ctx, float local_x, float local_y, int click_count);
void          tb_on_ime_composition(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor);
void          tb_on_context_menu(void *ctx, float x, float y);
void          tb_on_focus(void *ctx);
void          tb_on_blur(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_TB_INTERNAL_H */
