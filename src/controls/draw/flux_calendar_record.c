#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_anim.h"
#include "render/flux_calendar_geom.h"
#include "runtime/flux_date.h"

#include <stdio.h>

#define CAL_HEADER_FADE_S 0.167
#define CAL_VIEW_OUT_S    0.15
#define CAL_VIEW_END_S    0.5

typedef struct CalItemState {
	bool today, selected, hovered, pressed, out_of_scope, enabled;
} CalItemState;

static FluxColor cal_clear(void) { return flux_color_rgba(0, 0, 0, 0); }

static FluxColor cal_item_fill(FluxThemeColors const *t, CalItemState const *s) {
	if (!s->enabled) return s->today ? t->accent_disabled : cal_clear();
	if (s->today) return s->pressed ? t->accent_tertiary : (s->hovered ? t->accent_secondary : t->accent_default);
	if (s->pressed) return t->subtle_fill_tertiary;
	return s->hovered ? t->subtle_fill_secondary : cal_clear();
}

static FluxColor cal_item_border(FluxThemeColors const *t, CalItemState const *s) {
	if (s->today) return s->enabled ? cal_clear() : t->accent_disabled;
	if (s->selected && !s->enabled) return t->accent_disabled;
	if (s->selected)
		return s->pressed ? t->subtle_fill_tertiary : (s->hovered ? t->accent_secondary : t->accent_default);
	if (s->pressed) return t->subtle_fill_tertiary;
	return s->hovered ? t->subtle_fill_secondary : cal_clear();
}

static FluxColor cal_item_text_enabled(FluxThemeColors const *t, CalItemState const *s) {
	if (s->today) return t->text_on_accent_primary;
	if (s->selected) return s->pressed ? t->accent_text_tertiary : t->accent_text_primary;
	if (s->pressed) return s->out_of_scope ? t->text_tertiary : t->text_secondary;
	if (s->out_of_scope) return s->hovered ? t->text_primary : t->text_secondary;
	return t->text_primary;
}

static FluxColor cal_item_text(FluxThemeColors const *t, CalItemState const *s) {
	if (s->enabled) return cal_item_text_enabled(t, s);
	if (s->today) return t->text_on_accent_primary;
	return s->selected ? t->accent_text_disabled : t->text_disabled;
}

static void cal_item_strings(int view, int date, bool label, char *main, char *sub, size_t size) {
	sub [0]   = '\0';
	bool lead = view == FLUX_CAL_MONTH ? flux_date_day(date) == 1 : flux_date_month(date) == 1;
	if (view == FLUX_CAL_MONTH) snprintf(main, size, "%d", flux_date_day(date));
	else if (view == FLUX_CAL_YEAR) flux_locale_formatdate(date, L"MMM", main, size);
	else snprintf(main, size, "%d", flux_date_year(date));
	if (!label || !lead || view == FLUX_CAL_DECADE) return;
	if (view == FLUX_CAL_MONTH) flux_locale_formatdate(date, L"MMM", sub, size);
	else snprintf(sub, size, "%d", flux_date_year(date));
}

static bool cal_item_texts(XentDlBuilder *b, FluxRect const *item, int view, int date, bool label, FluxColor color) {
	char main [32], sub [32];
	cal_item_strings(view, date, label, main, sub, sizeof(main));
	bool     day = view == FLUX_CAL_MONTH;
	FluxRect text
	  = {item->x + 1.0f, item->y + 1.0f + (day ? 3.0f : 0.0f), item->w - 2.0f, item->h - 2.0f - (day ? 7.0f : 0.0f)};
	XentTextStyle style = {.font_size = 14.0f, .font_weight = 400u, .h_align = 1u, .v_align = 1u};
	if (!flux_rec_text_ex(b, &text, main, &style, color)) return false;
	if (!sub [0]) return true;
	FluxRect      top = {item->x + 1.0f, item->y + 1.0f + (day ? 2.0f : 9.0f), item->w - 2.0f, item->h - 2.0f};
	XentTextStyle lab = {.font_size = 8.0f, .font_weight = 400u, .h_align = 1u, .v_align = 0u};
	return flux_rec_text_ex(b, &top, sub, &lab, color);
}

typedef struct CalPaint {
	FluxRenderCtx const    *rc;
	FluxThemeColors const  *t;
	FluxCalendarData const *d;
	FluxControlState const *state;
	double                  now;
} CalPaint;

static CalItemState cal_item_state(CalPaint const *p, int view, int index, int date) {
	bool live = view == p->d->view;
	return (CalItemState) {
	  .today        = p->d->today_highlighted && flux_calendar_is_today(view, date),
	  .selected     = view == FLUX_CAL_MONTH && flux_calendar_is_selected(p->d, date),
	  .hovered      = live && p->d->hover == index,
	  .pressed      = live && p->d->pressed == index && p->d->hover == index,
	  .out_of_scope = flux_calendar_out_of_scope(p->d, view, index, p->now),
	  .enabled      = p->state->enabled != 0};
}

static bool cal_record_item(XentDlBuilder *b, CalPaint const *p, int view, int index, FluxRect const *cell) {
	int          date   = flux_calendar_date_at(p->d, view, index);
	CalItemState s      = cal_item_state(p, view, index, date);
	FluxRect     item   = flux_calendar_item(cell, view);
	float        radius = flux_calendar_radius(&item);
	bool         ok     = flux_rec_fill_rounded(b, &item, radius, cal_item_fill(p->t, &s));
	ok                  = ok && flux_rec_stroke_rounded(b, &item, radius - 0.5f, cal_item_border(p->t, &s), 1.0f);
	if (ok && s.today && s.selected) {
		FluxRect inner = {item.x + 1.0f, item.y + 1.0f, item.w - 2.0f, item.h - 2.0f};
		ok             = flux_rec_stroke_rounded(b, &inner, radius - 1.5f, p->t->text_on_accent_primary, 1.0f);
	}
	ok = ok && cal_item_texts(b, &item, view, date, p->d->group_label, cal_item_text(p->t, &s));
	if (!ok || view != p->d->view || p->d->focus != index || !p->d->focus_visual || !p->state->focused) return ok;
	FluxRect ring = {item.x - 1.0f, item.y - 1.0f, item.w + 2.0f, item.h + 2.0f};
	return flux_rec_focus_rect(b, p->t, &ring, radius + 1.0f);
}

static bool cal_record_panel(XentDlBuilder *b, CalPaint const *p, int view, FluxCalendarGeom const *g) {
	float rowpos = flux_calendar_rowpos(p->d, view, p->now);
	int   start = flux_calendar_start(p->d, view), size = flux_calendar_size(p->d, view);
	int   first = ( int ) floorf(rowpos);
	bool  ok    = xent_dl_pushcliprect(b, flux_rec_xrect(&g->viewport));
	for (int slot = first * g->cols; ok && slot < (first + g->rows + 1) * g->cols; ++slot) {
		int index = slot - start;
		if (index < 0 || index >= size) continue;
		FluxRect cell = flux_calendar_cell(g, slot, rowpos);
		ok            = cal_record_item(b, p, view, index, &cell);
	}
	return xent_dl_popclip(b) && ok;
}

static bool cal_record_weekdays(XentDlBuilder *b, CalPaint const *p, FluxRect const *row) {
	FluxColor     color = p->state->enabled ? p->t->text_primary : p->t->text_disabled;
	XentTextStyle style = {.font_size = 12.0f, .font_weight = 600u, .h_align = 1u, .v_align = 1u};
	float         w     = row->w / 7.0f;
	bool          ok    = true;
	for (int i = 0; ok && i < 7; ++i) {
		FluxRect col = {row->x + ( float ) i * w, row->y, w, row->h};
		ok           = flux_rec_text_ex(b, &col, p->d->weekday [(p->d->first_day + i) % 7], &style, color);
	}
	return ok;
}

static bool cal_record_view(XentDlBuilder *b, CalPaint const *p, int view, FluxRect const *bounds) {
	FluxCalendarGeom g = flux_calendar_layout(p->d, view, bounds);
	if (view == FLUX_CAL_MONTH && !cal_record_weekdays(b, p, &g.weekdays)) return false;
	return cal_record_panel(b, p, view, &g);
}

static float cal_spline(float t) {
	return flux_cubic_bezier(t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t), 0.0f, 0.0f, 0.0f, 1.0f);
}

static bool cal_record_scaled(
  XentDlBuilder *b, CalPaint const *p, int view, FluxRect const *bounds, FluxRect const *views, float scale, float alpha
) {
	if (alpha <= 0.0f) return true;
	float       cx = views->x + views->w * 0.5f, cy = views->y + views->h * 0.5f;
	XentAffine2 m  = {scale, 0.0f, cx - cx * scale, 0.0f, scale, cy - cy * scale};
	bool        ok = xent_dl_pushopacity(b, alpha) && xent_dl_pushtransform(b, m);
	ok             = ok && cal_record_view(b, p, view, bounds);
	return xent_dl_poptransform(b) && xent_dl_popopacity(b) && ok;
}

static bool cal_record_views(XentDlBuilder *b, CalPaint const *p, FluxRect const *bounds, FluxRect const *views) {
	double t  = p->d->view_since < 0.0 ? CAL_VIEW_END_S : p->now - p->d->view_since;
	bool   ok = xent_dl_pushcliprect(b, flux_rec_xrect(views));
	if (t >= CAL_VIEW_END_S) {
		ok = ok && cal_record_view(b, p, p->d->view, bounds);
		return xent_dl_popclip(b) && ok;
	}
	flux_rec_note_animating(p->rc, true);
	bool  up    = p->d->view > p->d->old_view;
	float leave = cal_spline(( float ) (t / CAL_VIEW_OUT_S));
	float come  = cal_spline(( float ) ((t - CAL_VIEW_OUT_S) / (CAL_VIEW_END_S - CAL_VIEW_OUT_S)));
	float s_old = up ? 0.84f : 1.29f, s_new = up ? 1.29f : 0.84f;
	if (t < CAL_VIEW_OUT_S)
		ok = ok && cal_record_scaled(b, p, p->d->old_view, bounds, views, 1.0f + (s_old - 1.0f) * leave, 1.0f - leave);
	if (t >= CAL_VIEW_OUT_S)
		ok = ok && cal_record_scaled(b, p, p->d->view, bounds, views, s_new + (1.0f - s_new) * come, come);
	return xent_dl_popclip(b) && ok;
}

static bool cal_record_frame(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, float rule_y) {
	bool ok = flux_rec_fill_rounded(b, bounds, FLUX_CORNER_RADIUS, t->ctrl_fill_input_active);
	ok      = ok && flux_rec_stroke_rounded(b, bounds, FLUX_CORNER_RADIUS, t->ctrl_stroke_default, FLUX_CAL_BORDER);
	return ok
	    && flux_rec_fill_rect(
		  b, &(FluxRect) {bounds->x + 1.0f, rule_y, bounds->w - 2.0f, 1.0f}, t->ctrl_stroke_default
		);
}

typedef struct CalButton {
	int         part;
	FluxRect    rect;
	bool        enabled;
	char const *text;
} CalButton;

static FluxColor cal_button_fg(FluxThemeColors const *t, CalButton const *btn, bool pressed) {
	bool header = btn->part == FLUX_CAL_HEADER;
	if (!btn->enabled) return header ? t->text_disabled : t->ctrl_strong_fill_disabled;
	if (!header) return t->ctrl_strong_fill_default;
	return pressed ? t->text_secondary : t->text_primary;
}

static bool cal_record_button(XentDlBuilder *b, CalPaint const *p, CalButton const *btn, float alpha) {
	bool      hovered = btn->enabled && p->d->hover == btn->part;
	bool      pressed = hovered && p->d->pressed == btn->part;
	FluxColor fill    = pressed ? p->t->subtle_fill_tertiary : (hovered ? p->t->subtle_fill_secondary : cal_clear());
	FluxColor fg      = cal_button_fg(p->t, btn, pressed);
	bool      ok      = flux_rec_fill_rounded(b, &btn->rect, FLUX_CORNER_RADIUS, fill);
	if (btn->part == FLUX_CAL_HEADER) {
		FluxRect      text  = {btn->rect.x + 8.0f, btn->rect.y + 7.0f, btn->rect.w - 16.0f, btn->rect.h - 15.0f};
		XentTextStyle style = {.font_size = 14.0f, .font_weight = 600u, .h_align = 0u, .v_align = 1u, .max_lines = 1u};
		ok                  = ok
		                   && xent_dl_pushopacity(b, alpha)
		                   && flux_rec_text_ex(b, &text, btn->text, &style, fg)
		                   && xent_dl_popopacity(b);
	}
	else
		ok = ok
		  && flux_rec_glyph_wc(
			b, &btn->rect, btn->part == FLUX_CAL_PREV ? L"\xEDDB" : L"\xEDDC", FLUX_CAL_NAV_GLYPH, fg
		  );
	if (!ok || p->d->focus != btn->part || !p->d->focus_visual || !p->state->focused) return ok;
	FluxRect ring = {btn->rect.x + 1.0f, btn->rect.y + 1.0f, btn->rect.w - 2.0f, btn->rect.h - 2.0f};
	return flux_rec_focus_rect(b, p->t, &ring, FLUX_CORNER_RADIUS - 1.0f);
}

static bool cal_record_buttons(XentDlBuilder *b, CalPaint const *p, FluxCalendarGeom const *g) {
	char header [64];
	bool before, after;
	flux_calendar_header(p->d, p->now, header, sizeof(header));
	flux_calendar_nav(p->d, p->now, &before, &after);
	bool      enabled  = p->state->enabled != 0;
	double    fade     = p->d->header_since < 0.0 ? 1.0 : (p->now - p->d->header_since) / CAL_HEADER_FADE_S;
	float     alpha    = fade >= 1.0 ? 1.0f : ( float ) fade;
	CalButton btns [3] = {
	  {FLUX_CAL_HEADER, g->header, enabled && p->d->view != FLUX_CAL_DECADE, header},
	  {FLUX_CAL_PREV,   g->prev,   enabled && before,                        NULL  },
	  {FLUX_CAL_NEXT,   g->next,   enabled && after,                         NULL  },
	};
	flux_rec_note_animating(p->rc, alpha < 1.0f);
	bool ok = true;
	for (int i = 0; ok && i < 3; ++i) ok = cal_record_button(b, p, &btns [i], alpha);
	return ok;
}

XentDl *flux_calendar_record(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxCalendarData const *d = snap->u.calendar.data;
	XentDlBuilder          *b = xent_dl_begin();
	if (!b || !d) return flux_rec_finish(b, false);
	CalPaint p = {rc, flux_rec_theme(rc), d, state, rc ? rc->now : 0.0};
	if (d->scroll.start >= 0.0 || d->wheel_free)
		flux_rec_note_animating(rc, d->wheel_free || p.now - d->scroll.start < 0.25);
	FluxCalendarGeom g  = flux_calendar_layout(d, d->view, bounds);
	bool             ok = cal_record_frame(b, p.t, bounds, g.rule_y);
	ok                  = ok && cal_record_buttons(b, &p, &g);
	ok                  = ok && cal_record_views(b, &p, bounds, &g.views);
	return flux_rec_finish(b, ok);
}
