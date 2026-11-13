#ifndef FLUX_TB_INTERNAL_H
#define FLUX_TB_INTERNAL_H

#include "controls/factory/flux_init_textbox.h"
#include "store/flux_node_data.h"
#include "text/flux_utf.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_window.h"
#include <windows.h>

#define FLUX_TEXTBOX_INITIAL_CAP 128
#define TB_BUFFER_GROWTH_FACTOR  2u
#define TB_UNDO_MAX              50
#define TB_TYPING_MERGE_MS       1000

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

typedef struct TbPendingEdit {
	char    *text;
	uint32_t len;
	uint32_t sel_start;
	uint32_t sel_end;
	bool     active;
} TbPendingEdit;

typedef struct FluxNBExt {
	double  value;
	double  minimum;
	double  maximum;
	double  small_change;
	double  large_change;
	bool    is_wrap_enabled;
	uint8_t spin_placement;
	uint8_t validation;
	bool    value_updating;
	bool    text_updating;
	bool    stepping;
	bool    focus_from_click;
	void    (*on_value_change)(void *ctx, double value);
	void   *on_value_change_ctx;
} FluxNBExt;

struct FluxApp;

typedef struct FluxTextBoxInputData {
	FluxTextBoxData base;
	char           *buffer;
	char           *flat_buffer;
	uint32_t        buf_cap;
	uint32_t        flat_cap;
	uint32_t        buf_len;
	uint32_t        gap_start;
	uint32_t        gap_end;
	bool            flat_dirty;
	XentCtx        *ctx;
	XentNodeId      node;
	FluxStore      *store;
	struct FluxApp *app;

	TbEditHistory   undo_stack [TB_UNDO_MAX];
	int             undo_top;
	int             undo_start;
	TbEditHistory   redo_stack [TB_UNDO_MAX];
	int             redo_top;
	int             redo_start;
	TbPendingEdit   pending_edit;
	bool            applying_history;

	ULONGLONG       last_typing_time;
	bool            last_op_was_typing;

	wchar_t        *ime_buf;
	uint32_t        ime_buf_cap;

	bool            dragging;
	uint32_t        drag_anchor;

	float           caret_column_x;
	bool            caret_column_held;

	wchar_t         high_surrogate;
	bool            password_show_plain;
	FluxNBExt      *nb;
} FluxTextBoxInputData;

bool          tb_ensure_cap(FluxTextBoxInputData *tb, uint32_t needed);
char const   *tb_sync_content(FluxTextBoxInputData *tb);
uint32_t      tb_buffer_utf8_prev(FluxTextBoxInputData const *tb, uint32_t pos);
uint32_t      tb_buffer_utf8_next(FluxTextBoxInputData const *tb, uint32_t pos);
uint32_t      tb_grapheme_next(FluxTextBoxInputData *tb, uint32_t pos);
uint32_t      tb_grapheme_prev(FluxTextBoxInputData *tb, uint32_t pos);
void          tb_delete_range(FluxTextBoxInputData *tb, uint32_t from, uint32_t to);
void          tb_insert_utf8(FluxTextBoxInputData *tb, uint32_t pos, char const *s, uint32_t slen);
bool          tb_replace_text(FluxTextBoxInputData *tb, char const *text);
void          tb_delete_selection(FluxTextBoxInputData *tb);
bool          tb_has_selection(FluxTextBoxInputData const *tb);
uint32_t      tb_buffer_word_start(FluxTextBoxInputData const *tb, uint32_t pos);
uint32_t      tb_buffer_word_end(FluxTextBoxInputData const *tb, uint32_t pos);

void          tb_push_undo(FluxTextBoxInputData *tb);
void          tb_commit_pending_undo(FluxTextBoxInputData *tb);
void          tb_undo(FluxTextBoxInputData *tb);
void          tb_redo(FluxTextBoxInputData *tb);
void          tb_free_undo_redo(FluxTextBoxInputData *tb);
void          tb_destroy(void *component_data);

void          tb_perform_copy(FluxTextBoxInputData *tb);
void          tb_perform_paste(FluxTextBoxInputData *tb);

FluxTextStyle tb_make_style(FluxTextBoxInputData const *tb);
float         tb_visible_text_height(FluxTextBoxInputData const *tb, XentRect const *rect);
float         tb_layout_width(FluxTextBoxInputData const *tb, float visible_w);
void          tb_update_scroll(FluxTextBoxInputData *tb);
void          tb_clamp_scroll(FluxTextBoxInputData *tb);
void          tb_update_ime_position(FluxTextBoxInputData *tb);
void          tb_notify_change(FluxTextBoxInputData *tb);

uint32_t      pb_build_mask(char const *src, uint32_t src_len, char *dst, uint32_t dst_cap);
uint32_t      pb_mask_offset_to_original(char const *original, uint32_t original_len, uint32_t mask_byte_offset);
uint32_t      pb_original_offset_to_mask(char const *original, uint32_t original_len, uint32_t original_byte_offset);

bool          nb_is_number_box(FluxTextBoxInputData *tb);
void          nb_update_text_to_value(FluxTextBoxInputData *tb);
void          nb_validate_input(FluxTextBoxInputData *tb);
void          nb_step_value(FluxTextBoxInputData *tb, double change);
void          nb_project_value(FluxTextBoxInputData *tb, double new_value);
void          nb_coerce_value(FluxTextBoxInputData *tb);
void          nb_sync_semantics(FluxTextBoxInputData *tb);

bool          tb_on_key(void *ctx, FluxKey key, bool down);
void          tb_on_char(void *ctx, wchar_t ch);
void          tb_on_pointer_move(void *ctx, float local_x, float local_y);
void          tb_on_pointer_down(void *ctx, float local_x, float local_y, int click_count);
void          tb_on_pointer_finish(void *ctx);
void          tb_on_ime_composition(void *ctx, wchar_t const *text, uint32_t len, uint32_t cursor);
void          tb_on_context_menu(void *ctx, float x, float y);
void          tb_on_focus(void *ctx);
void          tb_on_blur(void *ctx);

#endif
