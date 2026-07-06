#include "render/flux_fluent.h"
#include "render/flux_icon.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/draw/flux_button_internal.h"
#include "fluxent/controls/flux_nav_view_data.h"

#include <string.h>

/* WinUI NavigationView, left nav (NavigationView.xaml / _themeresources.xaml).
 *
 * The pane node draws its own background + the 1px right separator, and paints
 * the moving accent selection indicator in its overlay pass (after the items)
 * so the pill sits above the item fills. Items are leaf nodes that draw their
 * own subtle-fill chrome + icon glyph + (in Expanded) label, mirroring
 * NavigationViewItemPresenter's CommonStates. */

static FluxColor nav_with_opacity(FluxColor c, float op) {
	float a = flux_color_af(c) * flux_clamp01(op);
	return flux_color_rgba(
	  ( int ) (flux_color_rf(c) * 255.0f + 0.5f), ( int ) (flux_color_gf(c) * 255.0f + 0.5f),
	  ( int ) (flux_color_bf(c) * 255.0f + 0.5f), ( int ) (a * 255.0f + 0.5f)
	);
}

/* Content-side spread of the Minimal overlay pane's drop shadow (WinUI ShadowCaster
 * ThemeShadow approximation: a soft right-edge falloff over the content). */
#define FLUX_NAV_PANE_SHADOW_W 9.0f

/* WinUI ContentGrid corner radius (OverlayCornerRadius). */
#define FLUX_NAV_CONTENT_RADIUS 8.0f

/* The content region's layer: LayerFillColorDefault over the window backdrop
 * with the top-left corner rounded (both top corners for the Top bar) and a
 * 1px CardStroke border along the rounded edges only — WinUI's ContentGrid
 * with BorderThickness 1,1,0,0. Clipping to the layer rect and painting a
 * rounded rect that extends past the square corners renders exactly that:
 * extended edges (and their stroke) fall outside the clip. */
static void nav_draw_content_layer(
  FluxRenderContext const *rc, FluxRect const *root, FluxRenderSnapshot const *snap, FluxThemeColors const *t
) {
	float    cx    = snap->u.nav.nav_content_x;
	float    cy    = snap->u.nav.nav_content_y;
	FluxRect layer = {root->x + cx, root->y + cy, root->w - cx, root->h - cy};
	if (layer.w < 1.0f || layer.h < 1.0f) return;

	float       r    = FLUX_NAV_CONTENT_RADIUS;
	FluxRect    ext  = {layer.x, layer.y, layer.w + (snap->u.nav.nav_top ? 0.0f : r), layer.h + r};
	D2D1_RECT_F clip = flux_d2d_rect(&layer);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
	flux_fill_rounded_rect(rc, &ext, r, t->layer_default);
	FluxRect stroke = {ext.x + 0.5f, ext.y + 0.5f, ext.w - 1.0f, ext.h - 1.0f};
	flux_draw_rounded_rect(rc, &stroke, r, t->card_stroke_default, FLUX_STROKE_WIDTH);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

void flux_draw_nav_view(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	( void ) state;
	FluxThemeColors const *t  = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxRect               pb = flux_snap_bounds(bounds, 1.0f, 1.0f);

	if (!snap->u.nav.nav_is_pane) {
		/* Root: only the content layer — the pane region stays unpainted so the
		 * DWM backdrop (Mica) shows through, matching WinUI's transparent
		 * NavigationViewDefaultPaneBackground. */
		nav_draw_content_layer(rc, &pb, snap, t);
		return;
	}

	/* Drop shadow cast onto the content as the overlay pane slides in (Minimal). */
	if (snap->u.nav.nav_shadow_opacity > 0.01f) {
		float    a  = 0.22f * snap->u.nav.nav_shadow_opacity;
		FluxRect sh = {pb.x + pb.w, pb.y, FLUX_NAV_PANE_SHADOW_W, pb.h};
		flux_fill_h_gradient(
		  rc, &sh, flux_color_rgba(0, 0, 0, ( int ) (a * 255.0f + 0.5f)), flux_color_rgba(0, 0, 0, 0)
		);
	}

	/* Inline panes are transparent over the backdrop and separated from content
	 * by the layer's own border; the Minimal overlay pane floats above content,
	 * so it keeps an opaque background (ExpandedPaneBackground) + edge line. */
	if (!snap->u.nav.nav_pane_solid) return;
	flux_fill_rect(rc, &pb, t->solid_background);
	if (snap->u.nav.nav_top) {
		/* Top bar: 1px separator along the bottom edge instead of the right. */
		float ey = pb.y + pb.h - FLUX_STROKE_WIDTH * 0.5f;
		flux_draw_line(rc, &(FluxLineSpec) {pb.x, ey, pb.x + pb.w, ey}, t->divider_stroke_default, FLUX_STROKE_WIDTH);
		return;
	}
	/* Right-edge separator between pane and content (NavigationViewBorderThickness 1). */
	float ex = pb.x + pb.w - FLUX_STROKE_WIDTH * 0.5f;
	flux_draw_line(rc, &(FluxLineSpec) {ex, pb.y, ex, pb.y + pb.h}, t->divider_stroke_default, FLUX_STROKE_WIDTH);
}

void flux_draw_nav_view_overlay(FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds) {
	if (snap->u.nav.nav_ind_opacity <= 0.01f) return;
	FluxThemeColors const *t      = rc->theme ? rc->theme : flux_theme_default_colors();
	float                  lead   = snap->u.nav.nav_ind_top;
	float                  extent = snap->u.nav.nav_ind_bottom - snap->u.nav.nav_ind_top;
	if (extent <= 0.0f) return;

	FluxColor fill = nav_with_opacity(t->accent_default, snap->u.nav.nav_ind_opacity);
	if (snap->u.nav.nav_top) {
		/* Horizontal pill at the bottom of the bar (16x3, animated along X). */
		FluxRect pill = {bounds->x + lead, bounds->y + bounds->h - 4.0f - FLUX_NAV_TOP_IND_H, extent, FLUX_NAV_TOP_IND_H};
		flux_fill_rounded_rect(rc, &pill, FLUX_NAV_INDICATOR_R, fill);
		return;
	}
	/* The pill indents with the item's depth (nested items) and is clipped to the
	 * menu-item viewport so it never bleeds over the pinned footer. */
	float    px   = bounds->x + FLUX_NAV_ITEM_MARGIN_H - 1.0f + snap->u.nav.nav_ind_indent;
	FluxRect pill = {px, bounds->y + lead, FLUX_NAV_INDICATOR_W, extent};

	float    ct   = bounds->y + snap->u.nav.nav_ind_clip_top;
	float    cb   = bounds->y + snap->u.nav.nav_ind_clip_bottom;
	FluxRect clip = {bounds->x, ct, bounds->w, cb - ct};
	D2D1_RECT_F dc = flux_d2d_rect(&clip);
	ID2D1RenderTarget_PushAxisAlignedClip(FLUX_RT(rc), &dc, D2D1_ANTIALIAS_MODE_ALIASED);
	flux_fill_rounded_rect(rc, &pill, FLUX_NAV_INDICATOR_R, fill);
	ID2D1RenderTarget_PopAxisAlignedClip(FLUX_RT(rc));
}

static FluxColor nav_item_fill(FluxControlState const *state, bool selected, FluxThemeColors const *t) {
	if (state->enabled && state->pressed) return t->subtle_fill_tertiary;
	if (selected && state->enabled && state->hovered) return t->subtle_fill_tertiary;
	if (state->enabled && state->hovered) return t->subtle_fill_secondary;
	if (selected) return t->subtle_fill_secondary;
	return flux_color_rgba(0, 0, 0, 0);
}

/* Icon column reserved before the label: the full icon box when the item has an
 * icon, or WinUI's collapsed 8px when it does not (IconColumn.Width in the
 * IconCollapsed state) — so an iconless nested item's depth indent lands its
 * text aligned with the parent's, instead of reserving a full empty icon slot. */
#define FLUX_NAV_ICON_COLLAPSED 8.0f

static void nav_item_draw_label(
  FluxRenderContext const *rc, FluxRect const *ib, char const *label, FluxColor fg, float icon_w
) {
	if (!label || !rc->text) return;
	FluxRect tr = {ib->x + icon_w, ib->y, ib->w - icon_w - 8.0f, ib->h};
	if (tr.w <= 0.0f) return;

	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_NAV_LABEL_FONT;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = fg;
	flux_text_draw(rc->text, FLUX_RT(rc), label, &tr, &ts);
}

/* NavigationViewItemSeparator: 1px rule with 0,3,0,4 margins (vertical 24px
 * line with 3,0,4,0 margins on the Top bar). */
static void nav_item_draw_separator(FluxRenderContext const *rc, FluxRect const *ib, FluxRenderSnapshot const *snap,
                                    FluxThemeColors const *t) {
	if (snap->u.nav.nav_top) {
		float x  = ib->x + 3.0f + 0.5f;
		float y0 = ib->y + (ib->h - FLUX_NAV_SEP_TOP_LINE_H) * 0.5f;
		flux_draw_line(rc, &(FluxLineSpec) {x, y0, x, y0 + FLUX_NAV_SEP_TOP_LINE_H}, t->divider_stroke_default, 1.0f);
		return;
	}
	float y = ib->y + 3.0f + 0.5f;
	flux_draw_line(rc, &(FluxLineSpec) {ib->x, y, ib->x + ib->w, y}, t->divider_stroke_default, 1.0f);
}

/* NavigationViewItemHeader: caption text, secondary foreground, collapsed with
 * the labels (Compact / closed pane). */
static void nav_item_draw_header(FluxRenderContext const *rc, FluxRect const *ib, FluxRenderSnapshot const *snap,
                                 FluxThemeColors const *t) {
	if (!snap->u.nav.label || !rc->text) return;
	float         pad = snap->u.nav.nav_top ? 12.0f : FLUX_NAV_HEADER_PAD;
	FluxTextStyle ts;
	memset(&ts, 0, sizeof(ts));
	ts.font_size   = FLUX_NAV_HEADER_FONT;
	ts.font_weight = FLUX_FONT_REGULAR;
	ts.text_align  = FLUX_TEXT_LEFT;
	ts.vert_align  = FLUX_TEXT_VCENTER;
	ts.color       = t->text_secondary;
	FluxRect tr    = {ib->x + pad, ib->y, ib->w - pad * 2.0f, ib->h};
	if (tr.w > 0.0f) flux_text_draw(rc->text, FLUX_RT(rc), snap->u.nav.label, &tr, &ts);
}

void flux_draw_nav_view_item(
  FluxRenderContext const *rc, FluxRenderSnapshot const *snap, FluxRect const *bounds, FluxControlState const *state
) {
	FluxThemeColors const *t        = rc->theme ? rc->theme : flux_theme_default_colors();
	FluxRect               ib       = flux_snap_bounds(bounds, 1.0f, 1.0f);
	if (ib.w < 1.0f || ib.h < 1.0f) return; /* hidden (e.g. the toggle in Top mode) */
	if (snap->u.nav.nav_item_kind == FLUX_NAV_ITEM_SEPARATOR) {
		nav_item_draw_separator(rc, &ib, snap, t);
		return;
	}
	if (snap->u.nav.nav_item_kind == FLUX_NAV_ITEM_HEADER) {
		nav_item_draw_header(rc, &ib, snap, t);
		return;
	}
	bool                   selected = snap->u.nav.is_checked;

	FluxColor              fill     = nav_item_fill(state, selected, t);
	if (flux_color_af(fill) > 0.0f) flux_fill_rounded_rect(rc, &ib, FLUX_CORNER_RADIUS, fill);

	FluxColor      fg    = !state->enabled ? t->text_disabled : (state->pressed ? t->text_secondary : t->text_primary);

	/* Hierarchical children indent their content by 31px per depth
	 * (NavigationViewItemBase::c_itemIndentation); the chrome stays full-width. */
	FluxRect ci  = ib;
	ci.x        += ( float ) snap->u.nav.nav_depth * 31.0f;
	ci.w        -= ( float ) snap->u.nav.nav_depth * 31.0f;

	wchar_t const *glyph   = flux_icon_lookup(snap->u.nav.icon_name);
	float          icon_w  = glyph ? FLUX_NAV_ICON_BOX : FLUX_NAV_ICON_COLLAPSED;
	if (glyph) {
		FluxRect icon = {ci.x, ci.y, FLUX_NAV_ICON_BOX, ci.h};
		flux_button_draw_chevron(rc, &icon, glyph, FLUX_NAV_ICON_GLYPH, fg);
	}

	nav_item_draw_label(rc, &ci, snap->u.nav.label, fg, icon_w);

	/* Expand chevron at the trailing edge while children are not flyout-only. */
	if (snap->u.nav.nav_has_children && (snap->u.nav.label || snap->u.nav.nav_top)) {
		FluxRect cb = {ib.x + ib.w - 28.0f, ib.y, 16.0f, ib.h};
		flux_button_draw_chevron(rc, &cb, snap->u.nav.nav_expanded ? L"\xE70E" : L"\xE70D", 12.0f, fg);
	}
}
