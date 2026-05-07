#include "tb_internal.h"
#include "fluxent/fluxent.h"
#include <stdlib.h>
#include <string.h>

#define TB_IME_STACK_BYTES 2048

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

static void *tb_scratch_alloc(size_t bytes, void *stack, size_t stack_bytes, bool *heap) {
	*heap = false;
	if (bytes <= stack_bytes) return stack;
	*heap = true;
	return malloc(bytes);
}

static void tb_ime_compose_free(TbImeComposeBuffers *buf) {
	if (buf->u8tmp_heap) free(buf->u8tmp);
	if (buf->composed_heap) free(buf->composed);
	if (buf->wtmp_heap) free(buf->wtmp);
}

static bool tb_ime_prepare_wide(TbImeComposeBuffers *buf, FluxTextBoxInputData *tb, int orig_wlen, int total) {
	buf->wtmp = ( wchar_t * ) tb_scratch_alloc(
	  ( size_t ) total * sizeof(wchar_t), buf->wtmp_stack, sizeof(buf->wtmp_stack), &buf->wtmp_heap
	);
	if (!buf->wtmp) return false;
	MultiByteToWideChar(CP_UTF8, 0, tb->buffer, -1, buf->wtmp, orig_wlen + 1);
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

static bool tb_measure_composed_ime_caret(
  FluxTextBoxInputData *tb, FluxTextRenderer *tr, FluxTextStyle const *ts, float width, TbImeCaret *out
) {
	int orig_wlen = MultiByteToWideChar(CP_UTF8, 0, tb->buffer, -1, NULL, 0) - 1;
	int comp_len  = ( int ) tb->base.composition_length;
	int total     = orig_wlen + comp_len + 1;
	if (orig_wlen < 0 || comp_len <= 0 || total <= 0) return false;

	TbImeComposeBuffers buffers = {0};
	if (!tb_ime_prepare_wide(&buffers, tb, orig_wlen, total)) return false;

	int ins = ( int ) MultiByteToWideChar(CP_UTF8, 0, tb->buffer, ( int ) tb->base.cursor_position, NULL, 0);
	if (ins > orig_wlen) ins = orig_wlen;

	bool ok = tb_ime_compose_wide(&buffers, tb, orig_wlen, comp_len, ins)
	       && tb_ime_compose_utf8(&buffers, orig_wlen + comp_len);
	if (!ok) {
		tb_ime_compose_free(&buffers);
		return false;
	}

	int                hit_u16 = ins + ( int ) tb->base.composition_cursor;
	FluxTextCaretQuery query   = {
	  {tr, buffers.u8tmp, ts, width},
      hit_u16
    };
	FluxRect caret = flux_text_caret_rect(&query);
	out->x         = caret.x;
	out->y         = caret.y;
	out->h         = caret.h;

	tb_ime_compose_free(&buffers);
	return true;
}

FluxTextStyle tb_make_style(FluxTextBoxInputData const *tb) {
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = tb->base.font_family;
	ts.font_size   = tb->base.font_size > 0.0f ? tb->base.font_size : 14.0f;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.word_wrap   = false;
	return ts;
}

void tb_update_scroll(FluxTextBoxInputData *tb) {
	FluxTextRenderer *tr = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!tr) return;

	XentRect rect = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);
	float visible_w = rect.width - 10.0f - 6.0f;
	if (visible_w <= 0.0f) return;

	FluxTextStyle      ts         = tb_make_style(tb);
	uint32_t           u16_cursor = tb_byte_to_utf16_offset(tb->buffer, tb->buf_len, tb->base.cursor_position);
	FluxTextCaretQuery query      = {
	  {tr, tb->buffer, &ts, visible_w + tb->base.scroll_offset_x},
      ( int ) u16_cursor
    };
	FluxRect caret = flux_text_caret_rect(&query);
	float    cx    = caret.x;

	if (cx - tb->base.scroll_offset_x < 0.0f) tb->base.scroll_offset_x = cx;
	else if (cx - tb->base.scroll_offset_x > visible_w) tb->base.scroll_offset_x = cx - visible_w;
	if (tb->base.scroll_offset_x < 0.0f) tb->base.scroll_offset_x = 0.0f;
}

void tb_notify_change(FluxTextBoxInputData *tb) {
	if (!tb->last_op_was_typing) tb_commit_pending_undo(tb);
	if (tb->base.on_change) tb->base.on_change(tb->base.on_change_ctx, tb->buffer);
}

void tb_update_ime_position(FluxTextBoxInputData *tb) {
	FluxWindow       *window = tb->app ? flux_app_get_window(tb->app) : NULL;
	FluxTextRenderer *tr     = tb->app ? flux_app_get_text_renderer(tb->app) : NULL;
	if (!window || !tr) return;

	XentRect rect = {0};
	xent_get_layout_rect(tb->ctx, tb->node, &rect);

	FluxTextStyle ts     = tb_make_style(tb);
	ts.vert_align        = FLUX_TEXT_TOP;
	float      visible_w = rect.width - 10.0f - 6.0f;

	TbImeCaret measured  = {0.0f, 0.0f, 0.0f};
	bool       composed  = tb->base.composition_text && tb->base.composition_length > 0 && tb->buffer [0];
	bool ok = composed && tb_measure_composed_ime_caret(tb, tr, &ts, visible_w + tb->base.scroll_offset_x, &measured);
	if (!ok) {
		uint32_t           u16_cursor = tb_byte_to_utf16_offset(tb->buffer, tb->buf_len, tb->base.cursor_position);
		FluxTextCaretQuery query      = {
		  {tr, tb->buffer, &ts, visible_w + tb->base.scroll_offset_x},
          ( int ) u16_cursor
        };
		FluxRect caret = flux_text_caret_rect(&query);
		measured.x     = caret.x;
		measured.y     = caret.y;
		measured.h     = caret.h;
	}

	float       cx    = rect.x + 10.0f + measured.x - tb->base.scroll_offset_x;
	float       cy    = rect.y + 5.0f + measured.y;
	float       ch    = measured.h > 0.0f ? measured.h : (ts.font_size > 0.0f ? ts.font_size : 14.0f) * 1.2f;
	FluxDpiInfo dpi   = flux_window_dpi(window);
	float       scale = dpi.dpi_x / 96.0f;

	flux_window_set_ime_position(window, ( int ) (cx * scale), ( int ) (cy * scale), ( int ) (ch * scale));
}
