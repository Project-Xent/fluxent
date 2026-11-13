#ifndef FLUX_CONTROL_RECORD_INTERNAL_H
#define FLUX_CONTROL_RECORD_INTERNAL_H

#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "render/flux_scroll_geom.h"
#include "render/flux_shape_resource.h"

#include <xent/xent_display.h>

FluxThemeColors const *flux_rec_theme(FluxRenderCtx const *rc);
XentColor              flux_rec_xcolor(FluxColor c);
XentRect               flux_rec_xrect(FluxRect const *r);
FluxRect               flux_rec_snap_bounds(FluxRenderCtx const *rc, FluxRect const *bounds);
float                  flux_rec_snapx(FluxRenderCtx const *rc, float x);
float                  flux_rec_snapy(FluxRenderCtx const *rc, float y);
XentDl                *flux_rec_finish(XentDlBuilder *b, bool ok);

bool                   flux_rec_fill_rect(XentDlBuilder *b, FluxRect const *r, FluxColor c);
bool                   flux_rec_fill_rounded(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c);
bool                   flux_rec_stroke_rounded(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c, float w);
bool                   flux_rec_fill_circle(XentDlBuilder *b, float cx, float cy, float d, FluxColor c);
bool                   flux_rec_stroke_circle(XentDlBuilder *b, float cx, float cy, float d, FluxColor c, float w);
bool                   flux_rec_line_h(XentDlBuilder *b, float x0, float x1, float y, float h, FluxColor c);
bool flux_rec_linehs(XentDlBuilder *b, FluxRenderCtx const *rc, float x0, float x1, float y, float h, FluxColor c);
bool flux_rec_strokev(XentDlBuilder *b, float x, float y0, float y1, float w, FluxColor c);
bool flux_rec_strokevs(XentDlBuilder *b, FluxRenderCtx const *rc, float x, float y0, float y1, float w, FluxColor c);
bool flux_rec_fill_shape(XentDlBuilder *b, XentResourceId shape, FluxRect const *r, FluxColor c);
bool flux_rec_text(XentDlBuilder *b, FluxRect const *r, char const *txt, float size, uint16_t weight, FluxColor c);
bool flux_rec_text_ex(XentDlBuilder *b, FluxRect const *r, char const *txt, XentTextStyle const *style, FluxColor c);
bool flux_rec_glyph_wc(XentDlBuilder *b, FluxRect const *r, wchar_t const *wc, float size, FluxColor c);
bool flux_rec_fill_triangle(XentDlBuilder *b, float x0, float y0, float x1, float y1, float x2, float y2, FluxColor c);
bool flux_rec_filltop(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c);
bool flux_rec_fillbot(XentDlBuilder *b, FluxRect const *r, float radius, FluxColor c);
bool flux_rec_fill_bg(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds);
bool flux_rec_stroke_border(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds);

typedef struct FluxRecButtonColors {
	FluxColor fill;
	FluxColor stroke;
	FluxColor text;
	bool      use_elevation;
	bool      is_accent;
} FluxRecButtonColors;

bool flux_rec_elevation_border(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds, float radius, bool is_accent
);
bool flux_rec_focus_rect(XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bounds, float radius);
bool flux_rec_fillgrad(
  XentDlBuilder *b, FluxRect const *r, float x0, float y0, float x1, float y1, float s0, float s1, FluxColor c0,
  FluxColor c1
);

typedef struct FluxRecStateAnim {
	float hover;
	float press;
	float focus;
	float check;
} FluxRecStateAnim;

void flux_rec_note_animating(FluxRenderCtx const *rc, bool active);
bool flux_rec_scrollbar_mouse(
  XentDlBuilder *b, FluxThemeColors const *t, FluxRect const *bar, FluxScrollBarModel const *model, float hover_x,
  float hover_y, float expanded
);

FluxVisualState *flux_rec_vstate(FluxRenderCtx const *rc, FluxRenderSnap const *snap, uint32_t state);

FluxRecStateAnim flux_rec_animstate(
  FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state, bool check_target
);
FluxColor flux_rec_tweenfill(FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxColor target);
bool      flux_rec_fillsink(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *r, float radius, FluxColor fill);

FluxRecButtonColors flux_rec_button_colors(
  FluxRenderSnap const *snap, FluxControlState const *state, bool force_accent, FluxThemeColors const *t
);
bool flux_rec_button_chrome(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxControlState const *state,
  FluxRect const *bounds, float radius, FluxRecButtonColors *colors
);
bool flux_rec_button_label(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds, FluxColor text_color);
bool flux_rec_build_button(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state, bool force_accent
);
bool flux_rec_checkbox_box(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *box,
  FluxControlState const *state, FluxCheckState cs
);
bool flux_rec_build_checkbox(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_switch(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_radio(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_slider(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_progress(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
);
bool flux_rec_textbox_frame(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_password(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_number(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_dropdown(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_combo(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_split(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_infobar(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_menuitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_breadcrumb(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_selectoritem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_pager(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_build_pips(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_exphead(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_expbody(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds);
bool flux_rec_dialogbody(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds);
bool flux_rec_listitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_treeitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_nav(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds);
bool flux_rec_nav_overlay(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
);
bool flux_rec_navitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_tabs(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRect const *bounds);
bool flux_rec_tabitem(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_titlebar(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_splitpane(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds);
bool flux_rec_build_rating(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds,
  FluxControlState const *state
);
bool flux_rec_badge(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds);
bool flux_rec_stroke_arc(
  XentDlBuilder *b, FluxRect const *bounds, float start_deg, float sweep_deg, FluxColor c, float w, XentStrokeCap cap
);
bool flux_rec_progressring(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
);
bool flux_rec_avatar(XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds);
bool flux_rec_build_refresh(XentDlBuilder *b, FluxRenderSnap const *snap, FluxRect const *bounds);
bool flux_rec_refresh_overlay(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *bounds
);

#endif
