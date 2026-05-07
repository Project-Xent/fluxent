#include "controls/draw/flux_control_draw.h"
#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include <string.h>

#define NB_SPIN_BTN_MIN_W 32.0f

#define NB_UP_MARGIN      4.0f
#define NB_DN_MARGIN_L    0.0f
#define NB_DN_MARGIN_T    4.0f
#define NB_DN_MARGIN_R    4.0f
#define NB_DN_MARGIN_B    4.0f

#define NB_COL_UP_W       (NB_SPIN_BTN_MIN_W + NB_UP_MARGIN + NB_UP_MARGIN)
#define NB_COL_DN_W       (NB_SPIN_BTN_MIN_W + NB_DN_MARGIN_L + NB_DN_MARGIN_R)
#define NB_SPIN_TOTAL     (NB_COL_UP_W + NB_COL_DN_W)

#define NB_DEL_BTN_W      40.0f

#define NB_INNER_MARGIN_L 0.0f
#define NB_INNER_MARGIN_T 4.0f
#define NB_INNER_MARGIN_R 4.0f
#define NB_INNER_MARGIN_B 4.0f

#define ZONE_NONE         (-1)
#define ZONE_TEXT         0
#define ZONE_SPIN_UP      1
#define ZONE_SPIN_DN      2
#define ZONE_SPIN_GAP     3
#define ZONE_DELETE       4

typedef struct NbButtonMargins {
	float l;
	float t;
	float r;
	float b;
} NbButtonMargins;

typedef struct NbButtonSpec {
	FluxRect const *outer;
	NbButtonMargins margin;
	float           radius;
	bool            hovered;
	bool            pressed;
} NbButtonSpec;

typedef struct NbIconSpec {
	FluxRect const *outer;
	NbButtonMargins margin;
	char const     *icon_utf8;
	float           font_size;
	FluxColor       color;
} NbIconSpec;

typedef struct NbDrawContext {
	FluxRenderContext const  *rc;
	FluxRenderSnapshot const *snap;
	FluxRect const           *bounds;
	FluxControlState const   *state;
	float                     radius;
	float                     text_col_w;
	int                       zone;
} NbDrawContext;

static NbButtonMargins const NB_UP_MARGINS = {NB_UP_MARGIN, NB_UP_MARGIN, NB_UP_MARGIN, NB_UP_MARGIN};
static NbButtonMargins const NB_DN_MARGINS = {NB_DN_MARGIN_L, NB_DN_MARGIN_T, NB_DN_MARGIN_R, NB_DN_MARGIN_B};
static NbButtonMargins const NB_INNER_MARGINS
  = {NB_INNER_MARGIN_L, NB_INNER_MARGIN_T, NB_INNER_MARGIN_R, NB_INNER_MARGIN_B};

static int nb_hover_zone(float lx, float bounds_w, bool show_delete, bool spin_visible) {
	if (lx < 0.0f) return ZONE_NONE;

	float right_edge = bounds_w;

	float spin_start = right_edge;
	if (spin_visible) spin_start = right_edge - NB_SPIN_TOTAL;

	float del_start = spin_start;
	if (show_delete) del_start = spin_start - NB_DEL_BTN_W;

	if (lx < del_start) return ZONE_TEXT;

	if (show_delete && lx < spin_start) {
		float inner_x0 = del_start + NB_INNER_MARGIN_L;
		float inner_x1 = spin_start - NB_INNER_MARGIN_R;
		if (lx >= inner_x0 && lx < inner_x1) return ZONE_DELETE;
		return ZONE_DELETE;
	}

	if (spin_visible && lx >= spin_start) {
		float col_up_start = spin_start;
		float up_x0        = col_up_start + NB_UP_MARGIN;
		float up_x1        = up_x0 + NB_SPIN_BTN_MIN_W;

		float col_dn_start = spin_start + NB_COL_UP_W;
		float dn_x0        = col_dn_start + NB_DN_MARGIN_L;
		float dn_x1        = dn_x0 + NB_SPIN_BTN_MIN_W;

		if (lx >= up_x0 && lx < up_x1) return ZONE_SPIN_UP;
		if (lx >= dn_x0 && lx < dn_x1) return ZONE_SPIN_DN;
		return ZONE_SPIN_GAP;
	}

	return ZONE_TEXT;
}

static FluxRect nb_inner_rect(FluxRect const *outer, NbButtonMargins margin) {
	return (FluxRect) {
	  outer->x + margin.l, outer->y + margin.t, outer->w - margin.l - margin.r, outer->h - margin.t - margin.b};
}

static void nb_draw_inner_button(FluxRenderContext const *rc, NbButtonSpec const *button, FluxThemeColors const *t) {
	FluxRect r = nb_inner_rect(button->outer, button->margin);
	float    w = r.w;
	float    h = r.h;
	if (w <= 0.0f || h <= 0.0f) return;

	if (button->pressed) {
		FluxColor fill = t ? t->ctrl_alt_fill_tertiary : flux_color_rgba(0, 0, 0, 0x0f);
		flux_fill_rounded_rect(rc, &r, button->radius, fill);
	}
	else if (button->hovered) {
		FluxColor fill = t ? t->ctrl_alt_fill_secondary : flux_color_rgba(0, 0, 0, 0x06);
		flux_fill_rounded_rect(rc, &r, button->radius, fill);
	}
}

static void nb_draw_icon(FluxRenderContext const *rc, NbIconSpec const *icon) {
	FluxRect r = nb_inner_rect(icon->outer, icon->margin);
	if (r.w <= 0.0f || r.h <= 0.0f) return;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_family = "Segoe Fluent Icons";
	ts.font_size   = icon->font_size;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_CENTER;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = icon->color;
	ts.word_wrap   = false;
	flux_text_draw(rc->text, FLUX_RT(rc), icon->icon_utf8, &r, &ts);
}

static FluxColor nb_fill_color(FluxControlState const *state, bool chrome_hovered, FluxThemeColors const *t) {
	if (!state->enabled) return t ? t->ctrl_fill_disabled : flux_color_rgba(249, 249, 249, 0x4d);
	if (state->focused) return t ? t->ctrl_fill_input_active : flux_color_rgba(255, 255, 255, 0xff);
	if (chrome_hovered) return t ? t->ctrl_fill_secondary : flux_color_rgba(249, 249, 249, 0x80);
	return t ? t->ctrl_fill_default : flux_color_rgba(255, 255, 255, 0xb3);
}

static void nb_draw_chrome(NbDrawContext const *dc, bool chrome_hovered) {
	FluxThemeColors const *t    = dc->rc->theme;
	FluxColor              fill = nb_fill_color(dc->state, chrome_hovered, t);
	FluxCacheEntry        *ce   = dc->rc->cache ? flux_render_cache_get_or_create(dc->rc->cache, dc->snap->id) : NULL;
	if (ce) {
		FluxColor tweened;
		bool      a = flux_color_tween_update(&ce->color_anim, fill, FLUX_ANIM_DURATION_FAST, dc->rc->now, &tweened);
		fill        = tweened;
		if (a && dc->rc->animations_active) *dc->rc->animations_active = true;
	}

	flux_fill_rounded_rect(dc->rc, dc->bounds, dc->radius, fill);
	if (!dc->state->enabled) {
		FluxColor dis_stroke = t ? t->ctrl_stroke_default : flux_color_rgba(0, 0, 0, 0x0f);
		flux_draw_rounded_rect(dc->rc, dc->bounds, dc->radius, dis_stroke, 1.0f);
		return;
	}

	textbox_draw_elevation_border(dc->rc, dc->bounds, dc->radius, dc->state->focused);
	if (dc->state->focused) textbox_draw_focus_accent(dc->rc, dc->bounds, dc->radius);
}

static void nb_draw_text_area(NbDrawContext const *dc) {
	FluxRect text_area = {
	  dc->bounds->x + FLUX_TEXTBOX_PAD_L, dc->bounds->y + FLUX_TEXTBOX_PAD_T,
	  flux_maxf(0.0f, dc->text_col_w - FLUX_TEXTBOX_PAD_L - FLUX_TEXTBOX_PAD_R),
	  flux_maxf(0.0f, dc->bounds->h - FLUX_TEXTBOX_PAD_T - FLUX_TEXTBOX_PAD_B)};

	FluxControlState text_state = *dc->state;
	if (dc->zone != ZONE_TEXT && !dc->state->focused) text_state.hovered = 0;
	flux_draw_textbox_content(dc->rc, dc->snap, &text_area, &text_state);
}

static FluxColor nb_action_icon_color(bool enabled, bool pressed, FluxThemeColors const *t) {
	if (!enabled) return t ? t->text_disabled : flux_color_rgba(0, 0, 0, 0x5c);
	if (pressed) return t ? t->text_tertiary : flux_color_rgba(0, 0, 0, 0x72);
	return t ? t->text_secondary : flux_color_rgba(0, 0, 0, 0x9e);
}

static void nb_draw_delete(NbDrawContext const *dc) {
	FluxThemeColors const *t           = dc->rc->theme;
	float                  del_x       = dc->bounds->x + dc->text_col_w;
	FluxRect               del_outer   = {del_x, dc->bounds->y, NB_DEL_BTN_W, dc->bounds->h};
	bool                   del_hovered = (dc->zone == ZONE_DELETE);
	bool                   del_pressed = (del_hovered && dc->state->pressed);

	NbButtonSpec           del_button  = {&del_outer, NB_INNER_MARGINS, dc->radius, del_hovered, del_pressed};
	nb_draw_inner_button(dc->rc, &del_button, t);

	char       x_utf8 [4] = {( char ) 0xee, ( char ) 0xa2, ( char ) 0x94, '\0'};
	NbIconSpec icon       = {&del_outer, NB_INNER_MARGINS, x_utf8, 12.0f, nb_action_icon_color(true, del_pressed, t)};
	nb_draw_icon(dc->rc, &icon);
}

static void nb_draw_spin(NbDrawContext const *dc) {
	FluxThemeColors const *t           = dc->rc->theme;
	float                  spin_base_x = dc->bounds->x + dc->bounds->w - NB_SPIN_TOTAL;
	FluxRect               up_outer    = {spin_base_x, dc->bounds->y, NB_COL_UP_W, dc->bounds->h};
	FluxRect               dn_outer    = {spin_base_x + NB_COL_UP_W, dc->bounds->y, NB_COL_DN_W, dc->bounds->h};

	bool                   up_hovered  = (dc->zone == ZONE_SPIN_UP && dc->snap->nb_up_enabled);
	bool                   dn_hovered  = (dc->zone == ZONE_SPIN_DN && dc->snap->nb_down_enabled);
	bool                   up_pressed  = (up_hovered && dc->state->pressed);
	bool                   dn_pressed  = (dn_hovered && dc->state->pressed);

	NbButtonSpec           up_button   = {&up_outer, NB_UP_MARGINS, dc->radius, up_hovered, up_pressed};
	NbButtonSpec           dn_button   = {&dn_outer, NB_DN_MARGINS, dc->radius, dn_hovered, dn_pressed};
	nb_draw_inner_button(dc->rc, &up_button, t);
	nb_draw_inner_button(dc->rc, &dn_button, t);

	float      icon_fs     = dc->snap->font_size > 0.0f ? dc->snap->font_size : 14.0f;
	char       up_utf8 [4] = {( char ) 0xee, ( char ) 0x9c, ( char ) 0x8e, '\0'};
	char       dn_utf8 [4] = {( char ) 0xee, ( char ) 0x9c, ( char ) 0x8d, '\0'};

	NbIconSpec up_icon
	  = {&up_outer, NB_UP_MARGINS, up_utf8, icon_fs, nb_action_icon_color(dc->snap->nb_up_enabled, up_pressed, t)};
	NbIconSpec dn_icon
	  = {&dn_outer, NB_DN_MARGINS, dn_utf8, icon_fs, nb_action_icon_color(dc->snap->nb_down_enabled, dn_pressed, t)};
	nb_draw_icon(dc->rc, &up_icon);
	nb_draw_icon(dc->rc, &dn_icon);
}

void flux_draw_number_box(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	float radius       = snap->corner_radius > 0.0f ? snap->corner_radius : FLUX_CORNER_RADIUS;
	bool  has_text     = snap->text_content && snap->text_content [0];
	bool  spin_visible = (snap->nb_spin_placement == 2);

	bool  show_delete  = has_text && state->focused && !snap->readonly && state->enabled;

	float spin_w       = spin_visible ? NB_SPIN_TOTAL : 0.0f;
	float delete_w     = show_delete ? NB_DEL_BTN_W : 0.0f;
	float reserved     = delete_w + spin_w;
	float text_col_w   = bounds->w - reserved;
	if (text_col_w < 0.0f) text_col_w = 0.0f;

	int zone = ZONE_NONE;
	if (state->hovered) zone = nb_hover_zone(snap->hover_local_x, bounds->w, show_delete, spin_visible);

	NbDrawContext dc             = {rc, snap, bounds, state, radius, text_col_w, zone};
	bool          chrome_hovered = (zone == ZONE_TEXT);
	nb_draw_chrome(&dc, chrome_hovered);
	nb_draw_text_area(&dc);

	if (!state->enabled || !rc->text) return;

	if (show_delete) nb_draw_delete(&dc);

	if (!spin_visible) return;
	nb_draw_spin(&dc);
}
