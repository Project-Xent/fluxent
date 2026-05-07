#include "render/flux_fluent.h"
#include <stdlib.h>
#include <string.h>

#define TB_DELETE_BTN_W 30.0f

typedef struct TextboxDrawStyles {
	FluxTextStyle text;
	FluxTextStyle hit;
	float         font_size;
} TextboxDrawStyles;

typedef struct TextboxComposedContent {
	char const *content;
	char        stack [512];
	char       *heap;
	int         composition_start_u16;
	bool        has_composition;
	bool        has_text;
} TextboxComposedContent;

typedef struct TextboxContentDrawContext {
	FluxRenderContext const      *rc;
	FluxRenderSnapshot const     *snap;
	FluxRect const               *text_area;
	FluxControlState const       *state;
	TextboxComposedContent const *content;
	TextboxDrawStyles const      *styles;
} TextboxContentDrawContext;

typedef struct TextboxDeleteButtonContext {
	FluxRenderContext const  *rc;
	FluxRenderSnapshot const *snap;
	FluxRect const           *bounds;
	FluxControlState const   *state;
	float                     radius;
	float                     col0_w;
	float                     col1_w;
} TextboxDeleteButtonContext;

typedef struct TextboxDeleteButtonState {
	FluxRect rect;
	bool     hovered;
	bool     pressed;
} TextboxDeleteButtonState;

static uint32_t utf8_bytes_to_utf16_count(char const *s, uint32_t byte_count) {
	if (!s || byte_count == 0) return 0;
	return ( uint32_t ) MultiByteToWideChar(CP_UTF8, 0, s, ( int ) byte_count, NULL, 0);
}

static D2D1_BRUSH_PROPERTIES textbox_brush_props(float opacity) {
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = opacity;
	bp.transform._11 = 1;
	bp.transform._12 = 0;
	bp.transform._21 = 0;
	bp.transform._22 = 1;
	bp.transform._31 = 0;
	bp.transform._32 = 0;
	return bp;
}

void textbox_draw_elevation_border(FluxRenderContext const *rc, FluxRect const *bounds, float radius, bool is_focused) {
	FluxThemeColors const *t = rc->theme;

	FluxColor              bottom_color, top_color;
	if (is_focused) {
		bottom_color = t ? t->accent_default : flux_color_rgb(0, 120, 212);
		top_color    = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f);
	}
	else {
		bottom_color = t ? t->ctrl_strong_stroke_default : flux_color_rgba(0, 0, 0, 0x9c);
		top_color    = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f);
	}

	D2D1_GRADIENT_STOP stops [2];
	if (is_focused) {
		stops [0].position = 1.0f;
		stops [0].color    = flux_d2d_color(bottom_color);
		stops [1].position = 1.0f;
		stops [1].color    = flux_d2d_color(top_color);
	}
	else {
		stops [0].position = 0.5f;
		stops [0].color    = flux_d2d_color(bottom_color);
		stops [1].position = 1.0f;
		stops [1].color    = flux_d2d_color(top_color);
	}

	ID2D1GradientStopCollection *collection = NULL;
	ID2D1RenderTarget_CreateGradientStopCollection(
	  FLUX_RT(rc), stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &collection
	);
	if (!collection) return;

	D2D1_LINEAR_GRADIENT_BRUSH_PROPERTIES gp;
	gp.startPoint                  = flux_point(0.0f, bounds->y + bounds->h);
	gp.endPoint                    = flux_point(0.0f, bounds->y + bounds->h - 2.0f);

	D2D1_BRUSH_PROPERTIES     bp   = textbox_brush_props(1.0f);
	ID2D1LinearGradientBrush *grad = NULL;
	ID2D1RenderTarget_CreateLinearGradientBrush(FLUX_RT(rc), &gp, &bp, collection, &grad);
	ID2D1GradientStopCollection_Release(collection);
	if (!grad) return;

	D2D1_ROUNDED_RECT rr = flux_rounded_rect(bounds, radius);
	ID2D1RenderTarget_DrawRoundedRectangle(FLUX_RT(rc), &rr, ( ID2D1Brush * ) grad, 1.0f, NULL);

	ID2D1LinearGradientBrush_Release(grad);
}

void textbox_draw_focus_accent(FluxRenderContext const *rc, FluxRect const *bounds, float radius) {
	FluxThemeColors const *t       = rc->theme;

	ID2D1Factory          *factory = NULL;
	ID2D1RenderTarget_GetFactory(FLUX_RT(rc), &factory);
	if (!factory) return;

	D2D1_ROUNDED_RECT              rr        = flux_rounded_rect(bounds, radius);
	ID2D1RoundedRectangleGeometry *clip_geom = NULL;
	ID2D1Factory_CreateRoundedRectangleGeometry(factory, &rr, &clip_geom);
	ID2D1Factory_Release(factory);
	if (!clip_geom) return;

	FluxColor accent = t ? t->accent_default : flux_color_rgb(0, 120, 212);
	flux_set_brush(rc, accent);

	D2D1_LAYER_PARAMETERS lp;
	lp.contentBounds.left   = -1e6f;
	lp.contentBounds.top    = -1e6f;
	lp.contentBounds.right  = 1e6f;
	lp.contentBounds.bottom = 1e6f;
	lp.geometricMask        = ( ID2D1Geometry * ) clip_geom;
	lp.maskAntialiasMode    = D2D1_ANTIALIAS_MODE_PER_PRIMITIVE;
	lp.maskTransform._11    = 1;
	lp.maskTransform._12    = 0;
	lp.maskTransform._21    = 0;
	lp.maskTransform._22    = 1;
	lp.maskTransform._31    = 0;
	lp.maskTransform._32    = 0;
	lp.opacity              = 1.0f;
	lp.opacityBrush         = NULL;
	lp.layerOptions         = D2D1_LAYER_OPTIONS_NONE;

	ID2D1RenderTarget_PushLayer(FLUX_RT(rc), &lp, NULL);

	float line_y = bounds->y + bounds->h - 1.0f;
	ID2D1RenderTarget_DrawLine(
	  FLUX_RT(rc), flux_point(bounds->x, line_y), flux_point(bounds->x + bounds->w, line_y), ( ID2D1Brush * ) rc->brush,
	  2.0f, NULL
	);

	ID2D1RenderTarget_PopLayer(FLUX_RT(rc));
	ID2D1RoundedRectangleGeometry_Release(clip_geom);
}

static TextboxDrawStyles
textbox_make_styles(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state) {
	FluxThemeColors const *t     = rc->theme;
	float                  size  = snap->font_size > 0.0f ? snap->font_size : FLUX_FONT_SIZE_DEFAULT;
	FluxColor              color = (flux_color_af(snap->text_color) > 0.0f)
	                               ? snap->text_color
	                               : (state->enabled ? (t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xe4))
													 : (t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c)));

	TextboxDrawStyles      styles;
	memset(&styles, 0, sizeof(styles));
	styles.font_size        = size;
	styles.text.font_family = snap->font_family;
	styles.text.font_size   = size;
	styles.text.font_weight = FLUX_FONT_REGULAR;
	styles.text.text_align  = FLUX_TEXT_LEFT;
	styles.text.vert_align  = FLUX_TEXT_VCENTER;
	styles.text.color       = color;
	styles.text.word_wrap   = false;
	styles.hit              = styles.text;
	styles.hit.vert_align   = FLUX_TEXT_TOP;
	return styles;
}

static char *textbox_composed_buffer(TextboxComposedContent *out, int u8len) {
	if (u8len + 1 <= ( int ) sizeof(out->stack)) return out->stack;
	out->heap = ( char * ) malloc(( size_t ) u8len + 1);
	return out->heap;
}

static bool textbox_set_utf8_from_wide(TextboxComposedContent *out, wchar_t const *text, int len) {
	int u8len = WideCharToMultiByte(CP_UTF8, 0, text, len, NULL, 0, NULL, NULL);
	if (u8len <= 0) return false;

	char *dst = textbox_composed_buffer(out, u8len);
	if (!dst) return false;
	WideCharToMultiByte(CP_UTF8, 0, text, len, dst, u8len, NULL, NULL);
	dst [u8len]  = 0;
	out->content = dst;
	return true;
}

static bool textbox_compose_with_existing(TextboxComposedContent *out, FluxRenderSnapshot const *snap) {
	int wlen = MultiByteToWideChar(CP_UTF8, 0, snap->text_content, -1, NULL, 0);
	if (wlen <= 0) return false;

	wchar_t *wtmp = ( wchar_t * ) malloc(( size_t ) wlen * sizeof(wchar_t));
	if (!wtmp) return false;
	MultiByteToWideChar(CP_UTF8, 0, snap->text_content, -1, wtmp, wlen);

	int orig_wlen = wlen - 1;
	int start     = ( int ) utf8_bytes_to_utf16_count(snap->text_content, snap->cursor_position);
	if (start > orig_wlen) start = orig_wlen;

	int      result_wlen = orig_wlen + ( int ) snap->composition_length;
	wchar_t *wresult     = ( wchar_t * ) malloc((( size_t ) result_wlen + 1) * sizeof(wchar_t));
	if (!wresult) {
		free(wtmp);
		return false;
	}

	memcpy(wresult, wtmp, ( size_t ) start * sizeof(wchar_t));
	memcpy(wresult + start, snap->composition_text, ( size_t ) snap->composition_length * sizeof(wchar_t));
	memcpy(wresult + start + snap->composition_length, wtmp + start, ( size_t ) (orig_wlen - start) * sizeof(wchar_t));
	wresult [result_wlen]      = 0;

	bool ok                    = textbox_set_utf8_from_wide(out, wresult, result_wlen);
	out->composition_start_u16 = ok ? start : 0;
	free(wresult);
	free(wtmp);
	return ok;
}

static bool textbox_compose_from_composition(TextboxComposedContent *out, FluxRenderSnapshot const *snap) {
	out->composition_start_u16 = 0;
	return textbox_set_utf8_from_wide(out, snap->composition_text, ( int ) snap->composition_length);
}

static void textbox_prepare_content(TextboxComposedContent *out, FluxRenderSnapshot const *snap) {
	memset(out, 0, sizeof(*out));
	out->content         = snap->text_content;
	out->has_composition = snap->composition_text && snap->composition_length > 0;
	if (!out->has_composition) {
		out->has_text = out->content && out->content [0];
		return;
	}

	bool ok = (snap->text_content && snap->text_content [0]) ? textbox_compose_with_existing(out, snap)
	                                                         : textbox_compose_from_composition(out, snap);
	if (!ok) out->content = snap->text_content;
	out->has_text = out->content && out->content [0];
}

static void textbox_free_content(TextboxComposedContent *content) { free(content->heap); }

static void textbox_draw_placeholder(TextboxContentDrawContext const *dc) {
	if (dc->content->has_text || !dc->snap->placeholder || !dc->snap->placeholder [0]) return;

	FluxThemeColors const *t  = dc->rc->theme;
	FluxTextStyle          ph = dc->styles->text;
	if (!dc->state->enabled) ph.color = t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c);
	else ph.color = t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9e);
	flux_text_draw(dc->rc->text, FLUX_RT(dc->rc), dc->snap->placeholder, dc->text_area, &ph);
}

static void textbox_draw_selection(TextboxContentDrawContext const *dc) {
	if (!dc->content->has_text
		|| !dc->state->focused
		|| dc->snap->readonly
		|| dc->snap->selection_start == dc->snap->selection_end)
		return;

	uint32_t sel_s_byte
	  = dc->snap->selection_start < dc->snap->selection_end ? dc->snap->selection_start : dc->snap->selection_end;
	uint32_t sel_e_byte
	  = dc->snap->selection_start > dc->snap->selection_end ? dc->snap->selection_start : dc->snap->selection_end;
	int                    sel_s = ( int ) utf8_bytes_to_utf16_count(dc->snap->text_content, sel_s_byte);
	int                    sel_e = ( int ) utf8_bytes_to_utf16_count(dc->snap->text_content, sel_e_byte);

	FluxRect               sel_rects [16];
	FluxTextSelectionQuery query = {
	  {dc->rc->text, dc->content->content, &dc->styles->hit, dc->text_area->w},
      sel_s, sel_e, sel_rects, 16
    };
	uint32_t               n         = flux_text_selection_rects(&query);

	FluxThemeColors const *t         = dc->rc->theme;
	FluxColor              sel_base  = (flux_color_af(dc->snap->selection_color) > 0.0f)
	                                   ? dc->snap->selection_color
	                                   : (t ? t->accent_default : flux_color_rgb(0, 120, 212));
	D2D1_COLOR_F           sel_d2d   = flux_d2d_color(sel_base);
	D2D1_BRUSH_PROPERTIES  sbp       = textbox_brush_props(0.4f);
	ID2D1SolidColorBrush  *sel_brush = NULL;
	ID2D1RenderTarget_CreateSolidColorBrush(FLUX_RT(dc->rc), &sel_d2d, &sbp, &sel_brush);
	if (!sel_brush) return;

	for (uint32_t i = 0; i < n; i++) {
		FluxRect sr = {
		  dc->text_area->x + sel_rects [i].x - dc->snap->scroll_offset_x, dc->text_area->y + sel_rects [i].y,
		  sel_rects [i].w, sel_rects [i].h};
		D2D1_RECT_F srd = flux_d2d_rect(&sr);
		ID2D1RenderTarget_FillRectangle(FLUX_RT(dc->rc), &srd, ( ID2D1Brush * ) sel_brush);
	}
	ID2D1SolidColorBrush_Release(sel_brush);
}

static void textbox_draw_composition_underline(TextboxContentDrawContext const *dc) {
	if (!dc->content->has_composition || !dc->content->has_text) return;

	int                    comp_end_u16 = dc->content->composition_start_u16 + ( int ) dc->snap->composition_length;
	FluxRect               comp_rects [16];
	FluxTextSelectionQuery query = {
	  {dc->rc->text, dc->content->content, &dc->styles->hit, dc->text_area->w},
      dc->content->composition_start_u16,
	  comp_end_u16, comp_rects, 16
    };
	uint32_t               cn       = flux_text_selection_rects(&query);
	FluxThemeColors const *t        = dc->rc->theme;
	FluxColor              ul_color = dc->state->enabled ? (t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xe4))
	                                                     : (t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c));
	for (uint32_t i = 0; i < cn; i++) {
		float lx0 = dc->text_area->x + comp_rects [i].x - dc->snap->scroll_offset_x;
		float ly  = dc->text_area->y + comp_rects [i].y + comp_rects [i].h;
		float lx1 = lx0 + comp_rects [i].w;
		flux_draw_line(dc->rc, &(FluxLineSpec) {lx0, ly, lx1, ly}, ul_color, 1.0f);
	}
}

static void textbox_draw_content_text(TextboxContentDrawContext const *dc) {
	if (!dc->content->has_text) return;

	FluxRect draw_bounds = {
	  dc->text_area->x - dc->snap->scroll_offset_x, dc->text_area->y, dc->text_area->w + dc->snap->scroll_offset_x,
	  dc->text_area->h};
	flux_text_draw(dc->rc->text, FLUX_RT(dc->rc), dc->content->content, &draw_bounds, &dc->styles->text);
}

static bool textbox_caret_visible(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, int caret_u16) {
	FluxCacheEntry *ce = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	if (!ce) return true;

	float cur_pos_f = ( float ) caret_u16;
	if (!ce->focus_anim.initialized || ce->focus_anim.start_val != cur_pos_f) {
		ce->focus_anim.start_time  = rc->now;
		ce->focus_anim.start_val   = cur_pos_f;
		ce->focus_anim.initialized = true;
	}
	double elapsed = rc->now - ce->focus_anim.start_time;
	int    phase   = ( int ) (elapsed / 0.5);
	if (rc->animations_active) *rc->animations_active = true;
	return (phase % 2) == 0;
}

static bool textbox_should_draw_caret(TextboxContentDrawContext const *dc) {
	if (!dc->state->focused) return false;
	if (dc->rc->animations_active) *dc->rc->animations_active = true;
	if (dc->snap->readonly) return false;
	return dc->content->has_composition || dc->snap->selection_start == dc->snap->selection_end;
}

static int textbox_caret_index(TextboxContentDrawContext const *dc) {
	if (dc->content->has_composition) return dc->content->composition_start_u16 + ( int ) dc->snap->composition_cursor;
	return ( int ) utf8_bytes_to_utf16_count(dc->snap->text_content, dc->snap->cursor_position);
}

static FluxRect textbox_caret_local_rect(TextboxContentDrawContext const *dc, int caret_u16, float *caret_h) {
	*caret_h = dc->styles->font_size * 1.2f;
	if (!dc->content->has_text) return (FluxRect) {0};

	FluxTextCaretQuery query = {
	  {dc->rc->text, dc->content->content, &dc->styles->hit, dc->text_area->w},
      caret_u16
    };
	FluxRect caret = flux_text_caret_rect(&query);
	if (caret.h > 0.0f) *caret_h = caret.h;
	return caret;
}

static void textbox_draw_caret(TextboxContentDrawContext const *dc) {
	if (!textbox_should_draw_caret(dc)) return;

	int caret_u16 = textbox_caret_index(dc);
	if (!textbox_caret_visible(dc->rc, dc->snap, caret_u16)) return;

	float    caret_h;
	FluxRect caret = textbox_caret_local_rect(dc, caret_u16, &caret_h);
	float    caret_y
	  = dc->content->has_text ? dc->text_area->y + caret.y : dc->text_area->y + (dc->text_area->h - caret_h) * 0.5f;
	FluxRect               caret_abs = {dc->text_area->x + caret.x - dc->snap->scroll_offset_x, caret_y, 1.0f, caret_h};

	FluxThemeColors const *t         = dc->rc->theme;
	FluxColor              caret_color = t ? t->text_primary : flux_color_rgba(0, 0, 0, 0xe4);
	flux_fill_rect(dc->rc, &caret_abs, caret_color);
}

static void textbox_reset_caret_animation(FluxRenderContext const *rc, FluxRenderSnapshot const *snap) {
	if (!rc->cache) return;
	FluxCacheEntry *ce = flux_render_cache_get(rc->cache, snap->id);
	if (ce) ce->focus_anim.initialized = false;
}

void flux_draw_textbox_content(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *text_area, FluxControlState const *state
) {
	D2D1_RECT_F clip = flux_d2d_rect(text_area);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

	if (!rc->text) {
		ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
		return;
	}

	TextboxDrawStyles      styles = textbox_make_styles(rc, snap, state);
	TextboxComposedContent content;
	textbox_prepare_content(&content, snap);

	TextboxContentDrawContext dc = {rc, snap, text_area, state, &content, &styles};
	textbox_draw_placeholder(&dc);
	textbox_draw_selection(&dc);
	textbox_draw_composition_underline(&dc);
	textbox_draw_content_text(&dc);
	textbox_draw_caret(&dc);
	if (!state->focused) textbox_reset_caret_animation(rc, snap);

	textbox_free_content(&content);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static FluxColor
textbox_fill_color(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxControlState const *state) {
	FluxThemeColors const *t = rc->theme;
	if (!state->enabled) return t ? t->ctrl_fill_disabled : flux_color_rgba(249, 249, 249, 0x4d);
	if (state->focused) return t ? t->ctrl_fill_input_active : flux_color_rgba(255, 255, 255, 0xff);
	if (state->hovered) return t ? t->ctrl_fill_secondary : flux_color_rgba(249, 249, 249, 0x80);
	( void ) snap;
	return t ? t->ctrl_fill_default : flux_color_rgba(255, 255, 255, 0xb3);
}

static void textbox_draw_fill(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state,
  float radius
) {
	FluxColor       fill    = textbox_fill_color(rc, snap, state);
	FluxCacheEntry *ce_fill = rc->cache ? flux_render_cache_get_or_create(rc->cache, snap->id) : NULL;
	if (ce_fill) {
		FluxColor tweened;
		bool active = flux_color_tween_update(&ce_fill->color_anim, fill, FLUX_ANIM_DURATION_FAST, rc->now, &tweened);
		fill        = tweened;
		if (active && rc->animations_active) *rc->animations_active = true;
	}
	flux_fill_rounded_rect(rc, bounds, radius, fill);
}

static void
textbox_draw_frame(FluxRenderContext const *rc, FluxRect const *bounds, FluxControlState const *state, float radius) {
	FluxThemeColors const *t = rc->theme;
	if (!state->enabled) {
		FluxColor dis_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f);
		flux_draw_rounded_rect(rc, bounds, radius, dis_stroke, 1.0f);
		return;
	}

	textbox_draw_elevation_border(rc, bounds, radius, state->focused);
	if (state->focused) textbox_draw_focus_accent(rc, bounds, radius);
}

static FluxRect textbox_content_rect(FluxRect const *bounds, float col0_w) {
	FluxRect tb = {
	  bounds->x + FLUX_TEXTBOX_PAD_L, bounds->y + FLUX_TEXTBOX_PAD_T,
	  flux_maxf(0.0f, col0_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
	  flux_maxf(0.0f, bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)};
	return tb;
}

static FluxRect textbox_delete_button_rect(FluxRect const *bounds, float col0_w, float col1_w) {
	float    btn_margin_l = 0.0f;
	float    btn_margin_t = 4.0f;
	float    btn_margin_r = 4.0f;
	float    btn_margin_b = 4.0f;
	FluxRect btn          = {
	  bounds->x + col0_w + btn_margin_l, bounds->y + btn_margin_t, col1_w - btn_margin_l - btn_margin_r,
	  bounds->h - btn_margin_t - btn_margin_b};
	if (btn.w < 0.0f) btn.w = 0.0f;
	if (btn.h < 0.0f) btn.h = 0.0f;
	return btn;
}

static TextboxDeleteButtonState textbox_delete_button_state(TextboxDeleteButtonContext const *dc) {
	TextboxDeleteButtonState s = {0};
	s.rect                     = textbox_delete_button_rect(dc->bounds, dc->col0_w, dc->col1_w);
	s.hovered                  = dc->state->hovered && dc->snap->hover_local_x >= dc->col0_w;
	s.pressed                  = s.hovered && dc->state->pressed;
	return s;
}

static void
textbox_draw_delete_button_fill(TextboxDeleteButtonContext const *dc, TextboxDeleteButtonState const *button) {
	FluxThemeColors const *t = dc->rc->theme;
	if (button->pressed) {
		FluxColor pressed_fill = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0f);
		flux_fill_rounded_rect(dc->rc, &button->rect, dc->radius, pressed_fill);
		return;
	}

	if (button->hovered) {
		FluxColor hover_fill = t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06);
		flux_fill_rounded_rect(dc->rc, &button->rect, dc->radius, hover_fill);
	}
}

static FluxTextStyle textbox_delete_icon_style(FluxThemeColors const *t, bool pressed) {
	FluxColor     icon_color = pressed ? (t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 0x72))
	                                   : (t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9e));

	FluxTextStyle xs;
	memset(&xs, 0, sizeof(xs));
	xs.font_family = "Segoe Fluent Icons";
	xs.font_size   = 12.0f;
	xs.font_weight = FLUX_FONT_REGULAR;
	xs.text_align  = FLUX_TEXT_CENTER;
	xs.vert_align  = FLUX_TEXT_VCENTER;
	xs.color       = icon_color;
	xs.word_wrap   = false;
	return xs;
}

static void textbox_draw_delete_button(TextboxDeleteButtonContext const *dc) {
	if (!dc->rc->text) return;

	TextboxDeleteButtonState button = textbox_delete_button_state(dc);
	textbox_draw_delete_button_fill(dc, &button);

	char          x_utf8 [4] = {( char ) 0xee, ( char ) 0xa2, ( char ) 0x94, '\0'};
	FluxTextStyle xs         = textbox_delete_icon_style(dc->rc->theme, button.pressed);
	flux_text_draw(dc->rc->text, FLUX_RT(dc->rc), x_utf8, &button.rect, &xs);
}

void flux_draw_textbox(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	float radius   = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool  has_text = snap->text_content && snap->text_content [0];
	bool  show_delete
	  = snap->type == XENT_CONTROL_TEXT_INPUT && has_text && state->focused && !snap->readonly && state->enabled;
	float col1_w = show_delete ? TB_DELETE_BTN_W : 0.0f;
	float col0_w = flux_maxf(0.0f, bounds->w - col1_w);

	textbox_draw_fill(rc, snap, bounds, state, radius);
	textbox_draw_frame(rc, bounds, state, radius);

	FluxRect tb = textbox_content_rect(bounds, col0_w);
	flux_draw_textbox_content(rc, snap, &tb, state);

	if (show_delete) {
		TextboxDeleteButtonContext dc = {rc, snap, bounds, state, radius, col0_w, col1_w};
		textbox_draw_delete_button(&dc);
	}
}
