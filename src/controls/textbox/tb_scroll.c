#include "tb_internal.h"
#include "bridge/flux_internal.h"
#include "tb_metrics.h"
#include "controls/factory/flux_init_textbox.h"
#include "store/flux_node_data.h"
#include "render/flux_fluent.h"
#include <stdlib.h>
#include <string.h>

#define TB_IME_STACK_BYTES     2048
#define TB_DISPLAY_STACK_BYTES 2048

typedef struct TbImeCaret {
	float x;
	float y;
	float h;
} TbImeCaret;

typedef struct TbImeComposeBuffers {
	wchar_t  wtmp_stack [TB_IME_STACK_BYTES / sizeof(wchar_t)];
	wchar_t  composed_stack [TB_IME_STACK_BYTES / sizeof(wchar_t)];
	char     u8tmp_stack [TB_IME_STACK_BYTES];
	wchar_t *wtmp;
	wchar_t *composed;
	char    *u8tmp;
	bool     wtmp_heap;
	bool     composed_heap;
	bool     u8tmp_heap;
} TbImeComposeBuffers;

typedef struct TbDisplayText {
	char const *text;
	uint32_t    len;
	uint32_t    cursor;
	char        stack [TB_DISPLAY_STACK_BYTES];
	char       *heap;
} TbDisplayText;

static bool tb_textbox_show_delete(FluxTextBoxInputData const *tb, FluxControlType ct, FluxNodeData const *nd) {
	if (ct != FLUX_CONTROL_TEXT_INPUT && ct != FLUX_CONTROL_NUMBER_BOX) return false;
	return tb->buf_len > 0 && nd && nd->state.focused && !tb->base.readonly && tb->base.enabled;
}

static bool tb_password_show_reveal(FluxTextBoxInputData const *tb, FluxControlType ct, FluxNodeData const *nd) {
	return ct == FLUX_CONTROL_PASSWORD_BOX && tb->buf_len > 0 && nd && nd->state.focused && tb->base.enabled;
}

static float tb_text_reserved_width(FluxTextBoxInputData const *tb, FluxControlType ct, FluxNodeData const *nd) {
	if (tb->base.combo_editor) return FLUX_COMBO_EDITOR_RESERVE;
	if (ct == FLUX_CONTROL_NUMBER_BOX)
		return (tb->nb && tb->nb->spin_placement == FLUX_NB_SPIN_INLINE ? FLUX_NUMBER_BOX_SPIN_W : 0.0f)
		     + (tb_textbox_show_delete(tb, ct, nd) ? FLUX_NUMBER_BOX_DELETE_BTN_W : 0.0f);
	if (tb_textbox_show_delete(tb, ct, nd)) return FLUX_TEXTBOX_ACTION_BUTTON_W;
	if (tb_password_show_reveal(tb, ct, nd)) return FLUX_PASSWORD_REVEAL_BTN_W;
	return 0.0f;
}

static float tb_visible_text_width(FluxTextBoxInputData *tb, XentRect const *rect) {
	FluxControlType ct       = flux_node_type(tb->store, tb->node);
	FluxNodeData   *nd       = flux_store_find(tb->store, tb->node);
	float           reserved = tb_text_reserved_width(tb, ct, nd);
	return rect->w - reserved - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R;
}

static void tb_apply_parent_scroll_offsets(FluxTextBoxInputData *tb, XentRect *rect) {
	XentNodeId parent = xent_node_parent(tb->ctx, tb->node);

	while (parent != XENT_NODE_INVALID) {
		FluxNodeData *nd
		  = flux_node_type(tb->store, parent) == FLUX_CONTROL_SCROLL ? flux_store_find(tb->store, parent) : NULL;
		FluxScrollData *sd = nd ? ( FluxScrollData * ) nd->component_data : NULL;
		if (sd) {
			rect->x -= sd->scroll_x;
			rect->y -= sd->scroll_y;
		}
		parent = xent_node_parent(tb->ctx, parent);
	}
}

static void *tb_scratch_alloc(size_t bytes, void *stack, size_t stack_bytes, bool *heap) {
	*heap = false;
	if (bytes <= stack_bytes) return stack;
	*heap = true;
	return malloc(bytes);
}

static bool tb_uses_password_mask(FluxTextBoxInputData *tb) {
	return flux_node_type(tb->store, tb->node) == FLUX_CONTROL_PASSWORD_BOX
	    && tb->buf_len > 0
	    && !xent_sem_checked(tb->ctx, tb->node);
}

static bool tb_display_text_init(FluxTextBoxInputData *tb, TbDisplayText *display) {
	memset(display, 0, sizeof(*display));

	char const *content = tb_sync_content(tb);
	display->text       = content;
	display->len        = tb->buf_len;
	display->cursor     = tb->base.cursor_position;
	if (!tb_uses_password_mask(tb)) return true;

	if (tb->buf_len > (UINT32_MAX - 1u) / 3u) return false;
	size_t bytes = ( size_t ) tb->buf_len * 3u + 1u;
	char  *dst   = bytes <= sizeof(display->stack) ? display->stack : ( char * ) malloc(bytes);
	if (!dst) return false;

	display->len    = pb_build_mask(content, tb->buf_len, dst, ( uint32_t ) bytes);
	display->cursor = pb_original_offset_to_mask(content, tb->buf_len, tb->base.cursor_position);
	display->text   = dst;
	if (dst != display->stack) display->heap = dst;
	return true;
}

static void tb_display_text_free(TbDisplayText *display) { free(display->heap); }

static void tb_ime_compose_free(TbImeComposeBuffers *buf) {
	if (buf->u8tmp_heap) free(buf->u8tmp);
	if (buf->composed_heap) free(buf->composed);
	if (buf->wtmp_heap) free(buf->wtmp);
}

static bool tb_ime_prepare_wide(TbImeComposeBuffers *buf, char const *text, int orig_wlen, int total) {
	buf->wtmp = ( wchar_t * ) tb_scratch_alloc(
	  ( size_t ) total * sizeof(wchar_t), buf->wtmp_stack, sizeof(buf->wtmp_stack), &buf->wtmp_heap
	);
	if (!buf->wtmp) return false;
	MultiByteToWideChar(CP_UTF8, 0, text, -1, buf->wtmp, orig_wlen + 1);
	return true;
}

static bool
tb_ime_compose_wide(TbImeComposeBuffers *buf, FluxTextBoxInputData *tb, int orig_wlen, int comp_len, int insert_at) {
	buf->composed = ( wchar_t * ) tb_scratch_alloc(
	  ( size_t ) (orig_wlen + comp_len + 1) * sizeof(wchar_t), buf->composed_stack, sizeof(buf->composed_stack),
	  &buf->composed_heap
	);
	if (!buf->composed) return false;

	memcpy(buf->composed, buf->wtmp, ( size_t ) insert_at * sizeof(wchar_t));
	memcpy(buf->composed + insert_at, tb->base.composition_text, ( size_t ) comp_len * sizeof(wchar_t));
	memcpy(
	  buf->composed + insert_at + comp_len, buf->wtmp + insert_at, ( size_t ) (orig_wlen - insert_at) * sizeof(wchar_t)
	);
	buf->composed [orig_wlen + comp_len] = 0;
	return true;
}

static bool tb_ime_compose_utf8(TbImeComposeBuffers *buf, int composed_wlen) {
	int u8len = WideCharToMultiByte(CP_UTF8, 0, buf->composed, composed_wlen, NULL, 0, NULL, NULL);
	if (u8len <= 0) return false;

	buf->u8tmp
	  = ( char * ) tb_scratch_alloc(( size_t ) u8len + 1, buf->u8tmp_stack, sizeof(buf->u8tmp_stack), &buf->u8tmp_heap);
	if (!buf->u8tmp) return false;

	WideCharToMultiByte(CP_UTF8, 0, buf->composed, composed_wlen, buf->u8tmp, u8len, NULL, NULL);
	buf->u8tmp [u8len] = 0;
	return true;
}

static bool tb_ime_build_composed_utf8(FluxTextBoxInputData *tb, TbImeComposeBuffers *buffers, int *out_ins) {
	TbDisplayText display;
	if (!tb_display_text_init(tb, &display)) return false;

	int  orig_wlen = MultiByteToWideChar(CP_UTF8, 0, display.text, -1, NULL, 0) - 1;
	int  comp_len  = ( int ) tb->base.composition_length;
	int  total     = orig_wlen + comp_len + 1;
	bool ok        = orig_wlen >= 0 && comp_len > 0 && total > 0 && tb->base.composition_text;
	if (ok) ok = tb_ime_prepare_wide(buffers, display.text, orig_wlen, total);

	int ins = 0;
	if (ok) {
		ins = ( int ) MultiByteToWideChar(CP_UTF8, 0, display.text, ( int ) display.cursor, NULL, 0);
		if (ins > orig_wlen) ins = orig_wlen;
		if (out_ins) *out_ins = ins;
		ok = tb_ime_compose_wide(buffers, tb, orig_wlen, comp_len, ins)
		  && tb_ime_compose_utf8(buffers, orig_wlen + comp_len);
	}

	tb_display_text_free(&display);
	return ok;
}

static bool tb_measure_composed_ime_caret(
  FluxTextBoxInputData *tb, FluxText *tr, FluxTextStyle const *ts, float width, TbImeCaret *out
) {
	TbImeComposeBuffers buffers = {0};
	int                 ins     = 0;
	bool                ok      = tb_ime_build_composed_utf8(tb, &buffers, &ins);
	if (!ok) {
		tb_ime_compose_free(&buffers);
		return false;
	}

	int                hit_u16 = ins + ( int ) tb->base.composition_cursor;
	FluxTextCaretQuery query   = {
	  {tr, buffers.u8tmp, ts, width},
      hit_u16
    };
	FluxRect caret = flux_text_caret(&query);
	out->x         = caret.x;
	out->y         = caret.y;
	out->h         = caret.h;

	tb_ime_compose_free(&buffers);
	return true;
}

static bool
tb_measure_active_caret(FluxTextBoxInputData *tb, FluxText *tr, FluxTextStyle const *ts, float width, TbImeCaret *out) {
	if (tb->base.composition_text && tb->base.composition_length > 0)
		return tb_measure_composed_ime_caret(tb, tr, ts, width, out);

	TbDisplayText display;
	if (!tb_display_text_init(tb, &display)) return false;

	uint32_t           u16_cursor = flux_utf8_to_utf16_offset(display.text, display.len, display.cursor);
	FluxTextCaretQuery query      = {
	  {tr, display.text, ts, width},
      ( int ) u16_cursor
    };
	FluxRect caret = flux_text_caret(&query);
	out->x         = caret.x;
	out->y         = caret.y;
	out->h         = caret.h;
	tb_display_text_free(&display);
	return true;
}

static float tb_measure_active_text_width(FluxTextBoxInputData *tb, FluxText *tr, FluxTextStyle const *ts) {
	TbImeComposeBuffers buffers = {0};
	if (tb_ime_build_composed_utf8(tb, &buffers, NULL)) {
		FluxSize size = flux_text_measure(tr, buffers.u8tmp, ts, 0.0f);
		tb_ime_compose_free(&buffers);
		return size.w;
	}

	TbDisplayText display;
	if (!tb_display_text_init(tb, &display)) return 0.0f;

	FluxSize size = flux_text_measure(tr, display.text, ts, 0.0f);
	tb_display_text_free(&display);
	return size.w;
}

FluxTextStyle tb_make_style(FluxTextBoxInputData const *tb) {
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = tb->base.font_family;
	ts.font_size   = tb->base.font_size > 0.0f ? tb->base.font_size : 14.0f;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = tb->base.multiline ? FLUX_TEXT_TOP : FLUX_TEXT_VCENTER;
	ts.word_wrap   = tb->base.multiline;
	return ts;
}

float tb_visible_text_height(FluxTextBoxInputData const *tb, XentRect const *rect) {
	( void ) tb;
	return rect->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B;
}

float tb_layout_width(FluxTextBoxInputData const *tb, float visible_w) {
	return tb->base.multiline ? visible_w : visible_w + tb->base.scroll_offset_x;
}

static float tb_measure_active_text_height(FluxTextBoxInputData *tb, FluxText *tr, FluxTextStyle const *ts, float w) {
	TbDisplayText display;
	if (!tb_display_text_init(tb, &display)) return 0.0f;

	FluxSize size = flux_text_measure(tr, display.text, ts, w);
	tb_display_text_free(&display);
	return size.h;
}

static float tb_clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

static void  tb_scroll_axis_into_view(float caret_low, float caret_high, float visible, float *offset) {
	if (caret_low - *offset < 0.0f) *offset = caret_low;
	else if (caret_high - *offset > visible) *offset = caret_high - visible;
	if (*offset < 0.0f) *offset = 0.0f;
}

void tb_update_scroll(FluxTextBoxInputData *tb) {
	FluxText *tr = tb->app ? flux_app_text(tb->app) : NULL;
	if (!tr) return;

	XentRect rect = {0};
	xent_layout_rect(tb->ctx, tb->node, &rect);
	float visible_w = tb_visible_text_width(tb, &rect);
	if (visible_w <= 0.0f) return;

	FluxTextStyle ts    = tb_make_style(tb);
	TbImeCaret    caret = {0.0f, 0.0f, 0.0f};
	if (!tb_measure_active_caret(tb, tr, &ts, tb_layout_width(tb, visible_w), &caret)) return;

	if (!tb->base.multiline) tb_scroll_axis_into_view(caret.x, caret.x, visible_w, &tb->base.scroll_offset_x);
	else {
		float visible_h = tb_visible_text_height(tb, &rect);
		if (visible_h > 0.0f)
			tb_scroll_axis_into_view(caret.y, caret.y + caret.h, visible_h, &tb->base.scroll_offset_y);
	}
	tb_clamp_scroll(tb);
}

void tb_clamp_scroll(FluxTextBoxInputData *tb) {
	FluxText *tr = tb->app ? flux_app_text(tb->app) : NULL;
	if (!tr) return;

	XentRect rect = {0};
	xent_layout_rect(tb->ctx, tb->node, &rect);
	float visible_w = tb_visible_text_width(tb, &rect);
	if (visible_w <= 0.0f) {
		tb->base.scroll_offset_x = 0.0f;
		tb->base.scroll_offset_y = 0.0f;
		return;
	}

	FluxTextStyle ts = tb_make_style(tb);
	if (!tb->base.multiline) {
		float max_x              = tb_measure_active_text_width(tb, tr, &ts) - visible_w;
		tb->base.scroll_offset_x = tb_clampf(tb->base.scroll_offset_x, 0.0f, max_x > 0.0f ? max_x : 0.0f);
		return;
	}

	tb->base.scroll_offset_x = 0.0f;

	float visible_h          = tb_visible_text_height(tb, &rect);
	float max_y              = tb_measure_active_text_height(tb, tr, &ts, visible_w) - visible_h;
	tb->base.scroll_offset_y = tb_clampf(tb->base.scroll_offset_y, 0.0f, max_y > 0.0f ? max_y : 0.0f);
}

float flux_textbox_wheel(FluxStore *store, XentNodeId node, float delta) {
	FluxNodeData *nd = flux_store_find(store, node);
	if (!nd || nd->component_type != FLUX_CONTROL_TEXT_INPUT || !nd->component_data) return 0.0f;
	FluxTextBoxInputData *tb = ( FluxTextBoxInputData * ) nd->component_data;
	if (!tb->base.multiline) return 0.0f;
	float before             = tb->base.scroll_offset_y;
	tb->base.scroll_offset_y = before + delta;
	tb_clamp_scroll(tb);
	return tb->base.scroll_offset_y - before;
}

void tb_notify_change(FluxTextBoxInputData *tb) {
	if (!tb->last_op_was_typing) tb_commit_pending_undo(tb);
	char const *content                         = tb_sync_content(tb);
	void        (*notify)(void *, char const *) = tb->base.on_change;
	void       *notify_ctx                      = tb->base.on_change_ctx;
	if (!nb_is_number_box(tb))
		flux_input_route_event(
		  flux_store_host(tb->store), tb->node, XTK_EVENT_CHANGE_STRING,
		  (XtkEventPayload) {
			.kind     = XTK_EVENT_PAYLOAD_STRING,
			.u.string = {content, ( uint32_t ) strlen(content)},
		},
		  XTK_EVENT_CAUSE_USER
		);
	if (notify) notify(notify_ctx, content);
}

void tb_update_ime_position(FluxTextBoxInputData *tb) {
	FluxWindow *window = tb->app ? flux_app_window(tb->app) : NULL;
	FluxText   *tr     = tb->app ? flux_app_text(tb->app) : NULL;
	if (!window || !tr) return;

	XentRect rect = {0};
	xent_layout_rect(tb->ctx, tb->node, &rect);
	tb_apply_parent_scroll_offsets(tb, &rect);

	FluxTextStyle ts = tb_make_style(tb);
	ts.vert_align    = FLUX_TEXT_TOP;
	float visible_w  = tb_visible_text_width(tb, &rect);
	if (visible_w <= 0.0f) return;
	TbImeCaret measured = {0.0f, 0.0f, 0.0f};
	bool       ok       = tb_measure_active_caret(tb, tr, &ts, tb_layout_width(tb, visible_w), &measured);
	if (!ok) return;

	float   cx      = rect.x + FLUX_TEXTBOX_PAD_L + measured.x - tb->base.scroll_offset_x;
	float   cy      = rect.y + FLUX_TEXTBOX_PAD_T + measured.y - tb->base.scroll_offset_y;
	float   ch      = measured.h > 0.0f ? measured.h : (ts.font_size > 0.0f ? ts.font_size : 14.0f) * 1.2f;
	FluxDpi dpi     = flux_window_dpi(window);
	float   x_scale = dpi.dpi_x / FLUX_DPI_BASE;
	float   y_scale = dpi.dpi_y / FLUX_DPI_BASE;

	flux_window_setimepos(window, ( int ) (cx * x_scale), ( int ) (cy * y_scale), ( int ) (ch * y_scale));
}
