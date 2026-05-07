/**
 * @file tb_internal.h
 * @brief Internal definitions for TextBox/PasswordBox/NumberBox controls.
 *
 * Private to the textbox implementation — do not include from external code.
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

#define FLUX_TEXTBOX_INITIAL_CAP 128
#define TB_UNDO_MAX              50
#define TB_TYPING_MERGE_MS       1000

/** @brief Undo/redo history entry. */
typedef struct TbEditHistory {
	char    *deleted;
	char    *inserted;
	uint32_t pos;
	uint32_t deleted_len;
	uint32_t inserted_len;
	uint32_t before_sel_start;
	uint32_t before_sel_end;
	uint32_t after_sel_start;
	uint32_t after_sel_end;
} TbEditHistory;

/** @brief Transient pre-edit snapshot used only to derive compact undo deltas. */
typedef struct TbPendingEdit {
	char    *text;
	uint32_t len;
	uint32_t sel_start;
	uint32_t sel_end;
	bool     active;
} TbPendingEdit;

/** @brief NumberBox-specific runtime state. Allocated only for XENT_CONTROL_NUMBER_BOX nodes. */
typedef struct FluxNBExt {
	double  value;
	double  minimum;
	double  maximum;
	double  small_change;     /**< Step per arrow key or spin button. */
	double  large_change;     /**< Step per PageUp/PageDown. */
	bool    is_wrap_enabled;
	uint8_t spin_placement;   /**< FluxNBSpinPlacement: 0=Hidden, 2=Inline. */
	uint8_t validation;       /**< FluxNBValidation: 0=Overwrite, 1=Disabled. */
	bool    value_updating;   /**< Re-entrancy guard for nb_set_value. */
	bool    text_updating;    /**< Set while syncing text to avoid spurious re-entry. */
	bool    stepping;         /**< True after keyboard step — suppresses caret and selection. */
	bool    focus_from_click; /**< Set by pointer_down so on_focus skips stepping mode. */
	void    (*on_value_change)(void *ctx, double value);
	void   *on_value_change_ctx;
} FluxNBExt;

struct FluxApp;

/** @brief Extended runtime data for TextBox/PasswordBox/NumberBox. */
typedef struct FluxTextBoxInputData {
	FluxTextBoxData base; /* must be first — renderer casts to FluxTextBoxData* */
	char           *buffer;
	uint32_t        buf_cap;
	uint32_t        buf_len;
	XentContext    *ctx;
	XentNodeId      node;
	FluxNodeStore  *store;
	struct FluxApp *app;

	TbEditHistory   undo_stack [TB_UNDO_MAX];
	int             undo_top; /* -1 = empty */
	int             undo_start;
	TbEditHistory   redo_stack [TB_UNDO_MAX];
	int             redo_top; /* -1 = empty */
	int             redo_start;
	TbPendingEdit   pending_edit;
	bool            applying_history;

	ULONGLONG       last_typing_time;
	bool            last_op_was_typing;

	wchar_t        *ime_buf;
	uint32_t        ime_buf_cap;

	bool            dragging;
	uint32_t        drag_anchor;    /* byte offset of anchor for shift-drag */

	wchar_t         high_surrogate; /**< High-surrogate half awaiting a low surrogate for U+10000+ codepoints. */
	bool            password_show_plain;
	FluxNBExt      *nb;             /**< NumberBox extension; NULL for TextBox and PasswordBox. */
} FluxTextBoxInputData;

/** @brief Grows @p tb->buffer to at least @p needed bytes. */
bool          tb_ensure_cap(FluxTextBoxInputData *tb, uint32_t needed);
/** @brief Returns the byte length of the UTF-8 code unit starting at @p pos. */
uint32_t      tb_utf8_char_len(char const *s, uint32_t len, uint32_t pos);
/** @brief Returns the byte offset of the previous UTF-8 codepoint before @p pos. */
uint32_t      tb_utf8_prev(char const *s, uint32_t len, uint32_t pos);
/** @brief Returns the byte offset of the next UTF-8 codepoint after @p pos. */
uint32_t      tb_utf8_next(char const *s, uint32_t len, uint32_t pos);
/** @brief Converts a UTF-16 unit index to a UTF-8 byte offset in @p s. */
uint32_t      tb_utf16_to_byte_offset(char const *s, uint32_t len, uint32_t utf16_pos);
/** @brief Converts a UTF-8 byte offset to a UTF-16 unit index in @p s. */
uint32_t      tb_byte_to_utf16_offset(char const *s, uint32_t len, uint32_t byte_pos);
/** @brief Deletes the byte range [@p from, @p to) from the buffer. */
void          tb_delete_range(FluxTextBoxInputData *tb, uint32_t from, uint32_t to);
/** @brief Inserts @p slen UTF-8 bytes from @p s at byte offset @p pos. */
void          tb_insert_utf8(FluxTextBoxInputData *tb, uint32_t pos, char const *s, uint32_t slen);
/** @brief Replaces the entire text buffer with @p text. */
bool          tb_replace_text(FluxTextBoxInputData *tb, char const *text);
/** @brief Deletes the currently selected byte range from the buffer. */
void          tb_delete_selection(FluxTextBoxInputData *tb);
/** @brief Returns true if selection_start != selection_end. */
bool          tb_has_selection(FluxTextBoxInputData const *tb);
/** @brief Returns the byte offset of the start of the word containing @p pos. */
uint32_t      tb_word_start(char const *s, uint32_t len, uint32_t pos);
/** @brief Returns the byte offset of the end of the word containing @p pos. */
uint32_t      tb_word_end(char const *s, uint32_t len, uint32_t pos);

/** @brief Pushes the current buffer and selection onto the undo stack. */
void          tb_push_undo(FluxTextBoxInputData *tb);
/** @brief Commits a pending undo capture after an edit has changed the buffer. */
void          tb_commit_pending_undo(FluxTextBoxInputData *tb);
/** @brief Reverts to the previous undo state. */
void          tb_undo(FluxTextBoxInputData *tb);
/** @brief Reapplies the next redo state. */
void          tb_redo(FluxTextBoxInputData *tb);
/** @brief Frees all heap-allocated entries in the undo and redo stacks. */
void          tb_free_undo_redo(FluxTextBoxInputData *tb);
/** @brief Releases TextBox/PasswordBox/NumberBox component data. */
void          tb_destroy(void *component_data);

/** @brief Copies the current selection to the system clipboard. */
void          tb_perform_copy(FluxTextBoxInputData *tb);
/** @brief Pastes clipboard text at the current cursor position. */
void          tb_perform_paste(FluxTextBoxInputData *tb);

/** @brief Builds a FluxTextStyle from the textbox font properties. */
FluxTextStyle tb_make_style(FluxTextBoxInputData const *tb);
/** @brief Adjusts scroll_offset_x to keep the caret within the visible width. */
void          tb_update_scroll(FluxTextBoxInputData *tb);
/** @brief Repositions the IME candidate window to follow the composition cursor. */
void          tb_update_ime_position(FluxTextBoxInputData *tb);
/** @brief Fires the on_change callback with the current buffer contents. */
void          tb_notify_change(FluxTextBoxInputData *tb);

/** @brief Writes a bullet-masked copy of @p src (● per codepoint) into @p dst.
 *  @return Number of bytes written (excluding null terminator). */
uint32_t      pb_build_mask(char const *src, uint32_t src_len, char *dst, uint32_t dst_cap);
/** @brief Converts a byte offset in the masked string back to a byte offset in the original text.
 *  @return Byte offset in @p original corresponding to @p mask_byte_offset. */
uint32_t      pb_mask_offset_to_original(char const *original, uint32_t original_len, uint32_t mask_byte_offset);
/** @brief Converts an original text byte offset to the corresponding byte offset in the masked string. */
uint32_t      pb_original_offset_to_mask(char const *original, uint32_t original_len, uint32_t original_byte_offset);

/** @brief Returns true if @p tb has NumberBox extension data attached (i.e. is an XENT_CONTROL_NUMBER_BOX node). */
bool          nb_is_number_box(FluxTextBoxInputData *tb);
/** @brief Formats nb_value to text and refreshes the buffer and display. */
void          nb_update_text_to_value(FluxTextBoxInputData *tb);
/** @brief Parses the current text buffer and updates nb_value; reverts display on invalid input. */
void          nb_validate_input(FluxTextBoxInputData *tb);
/** @brief Increments nb_value by @p change, with optional min/max wrapping. */
void          nb_step_value(FluxTextBoxInputData *tb, double change);
/** @brief Sets nb_value with coercion, change detection, and on_value_change event firing. */
void          nb_set_value(FluxTextBoxInputData *tb, double new_value);
/** @brief Clamps nb_value to [nb_minimum, nb_maximum] when validation mode is Overwrite. */
void          nb_coerce_value(FluxTextBoxInputData *tb);
/** @brief Writes nb_value, nb_minimum, nb_maximum, and spin placement to the node's semantic properties. */
void          nb_sync_semantics(FluxTextBoxInputData *tb);

/** @brief Handles a keyboard key press or release event. */
void          tb_on_key(void *ctx, unsigned int vk, bool down);
/** @brief Handles a character input event (WM_CHAR). */
void          tb_on_char(void *ctx, wchar_t ch);
/** @brief Handles pointer move for hit-testing and drag selection. */
void          tb_on_pointer_move(void *ctx, float local_x, float local_y);
/** @brief Handles pointer press for cursor placement, drag start, and button hit-testing. */
void          tb_on_pointer_down(void *ctx, float local_x, float local_y, int click_count);
/** @brief Handles an IME composition update with inline text and cursor position. */
void          tb_on_ime_composition(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor);
/** @brief Handles a context menu request at the given window-local coordinates. */
void          tb_on_context_menu(void *ctx, float x, float y);
/** @brief Handles focus acquisition; enters stepping mode for NumberBox if appropriate. */
void          tb_on_focus(void *ctx);
/** @brief Handles focus loss; clears selection, IME state, and validates NumberBox input. */
void          tb_on_blur(void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* FLUX_TB_INTERNAL_H */
