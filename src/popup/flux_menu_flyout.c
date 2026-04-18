/* ══════════════════════════════════════════════════════════════════════════
 *  flux_menu_flyout.c — WinUI 3 MenuFlyout, 1:1 replica in C+D2D
 *  ------------------------------------------------------------------------
 *  Source of truth:
 *      fluxent-legacy/dev/CommonStyles/MenuFlyout_themeresources.xaml
 *      fluxent-legacy/dev/RadioMenuFlyoutItem/RadioMenuFlyoutItem_themeresources.xaml
 *
 *  Control templates modelled:
 *      MenuFlyoutPresenter  (outer rounded card, border, padding 0,2,0,2)
 *      MenuFlyoutSeparator  (1-px rectangle with padding -4,1,-4,1)
 *      MenuFlyoutItem       (icon + text + accel)
 *      ToggleMenuFlyoutItem (checkmark column + icon + text + accel)
 *      MenuFlyoutSubItem    (icon + text + chevron U+E76C, SubMenuOpened state)
 *      RadioMenuFlyoutItem  (filled dot column + icon + text + accel)
 *
 *  Resources replicated literally:
 *      MenuFlyoutPresenterThemePadding   = 0,2,0,2
 *      MenuFlyoutItemMargin              = 4,2,4,2
 *      MenuFlyoutItemThemePadding        = 11,8,11,9
 *      MenuFlyoutItemChevronMargin       = 24,0,0,-1
 *      MenuFlyoutSeparatorThemePadding   = -4,1,-4,1
 *      MenuFlyoutSeparatorHeight         = 1
 *      MenuFlyoutItemPlaceholderThickness       = 28,0,0,0
 *      MenuFlyoutItemDoublePlaceholderThickness = 56,0,0,0
 *      MenuFlyoutThemeMinHeight          = 32
 *      FlyoutThemeMinWidth               = 96
 *      OverlayCornerRadius               = 8
 *      ControlCornerRadius               = 4
 *      ControlContentThemeFontSize       = 14
 *      CaptionTextBlockStyle FontSize    = 12
 *
 *  Brush tokens (resolved through FluxThemeColors + Acrylic fallback):
 *      MenuFlyoutPresenterBackground     = DesktopAcrylicTransparent (fallback
 *                                          = layer colour, opaque)
 *      MenuFlyoutPresenterBorderBrush    = SurfaceStrokeColorFlyout
 *      MenuFlyoutSeparatorBackground     = DividerStrokeColorDefault
 *      MenuFlyoutItemBackgroundPointerOver = SubtleFillColorSecondary
 *      MenuFlyoutItemBackgroundPressed     = SubtleFillColorTertiary
 *      MenuFlyoutSubItemBackgroundSubMenuOpened
 *                                          = SubtleFillColorSecondary
 *      MenuFlyoutItemForeground          = TextFillColorPrimary
 *      MenuFlyoutItemForegroundDisabled  = TextFillColorDisabled
 *      KeyboardAcceleratorTextForeground = TextFillColorSecondary
 *      MenuFlyoutSubItemChevron          = TextFillColorSecondary
 *      MenuFlyoutSubItemChevronPressed   = TextFillColorTertiary
 *
 *  Keyboard (MenuFlyout routed via flux_menu_flyout_on_key):
 *      Up / Down        — move keyboard focus, wrap, skip separators/disabled
 *      Home / End       — jump to first / last enabled item
 *      Enter / Space    — activate focused item (toggles / radios / subs)
 *      Right            — open submenu of focused item, focus its first item
 *      Left             — close submenu, return focus to parent item
 *      Escape           — dismiss entire menu chain
 * ══════════════════════════════════════════════════════════════════════════ */

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include "fluxent/flux_menu_flyout.h"
#include "fluxent/flux_graphics.h"
#include "flux_render_internal.h"

#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <windows.h>

/* ══════════════════════════════════════════════════════════════════════════
 *  Segoe Fluent Icons glyphs (UTF-8)
 * ══════════════════════════════════════════════════════════════════════════ */
/* U+E73E  — "CheckMark"      (ToggleMenuFlyoutItem CheckGlyph, FontSize=12) */
static char const GLYPH_CHECKMARK [] = "\xEE\x9C\xBE";
/* U+E974  — "ChevronRightMed" (MenuFlyoutSubItem SubItemChevron, FontSize=12)
 *                              — note: WinUI default uses E974, not E76C.   */
static char const GLYPH_CHEVRON_R [] = "\xEE\xA5\xB4";
/* U+E915  — "Completed"      (RadioMenuFlyoutItem CheckGlyph — filled dot)  */
static char const GLYPH_RADIO_DOT [] = "\xEE\xA4\x95";

/* ══════════════════════════════════════════════════════════════════════════
 *  Internal item record
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct StoredItem {
	FluxMenuItemType type;
	char            *label;
	char            *icon_glyph;
	char            *accelerator_text;
	char            *radio_group;
	bool             enabled;
	bool             checked;
	void             (*on_click)(void *);
	void            *on_click_ctx;
	FluxMenuFlyout  *submenu; /* not owned */

	/* Layout cache — filled by menu_measure() */
	float            y; /* top of slot incl. margin, within content */
	float            h; /* slot height incl. margin */
} StoredItem;

/* ══════════════════════════════════════════════════════════════════════════
 *  Global active-leaf pointer for keyboard routing from the owner's loop
 * ══════════════════════════════════════════════════════════════════════════ */

static FluxMenuFlyout *g_active_leaf = NULL;

/* ══════════════════════════════════════════════════════════════════════════
 *  FluxMenuFlyout
 * ══════════════════════════════════════════════════════════════════════════ */

struct FluxMenuFlyout {
	FluxPopup               *popup;
	FluxWindow              *owner;

	FluxTextRenderer        *text;  /* not owned */
	FluxThemeColors const   *theme; /* not owned */
	bool                     is_dark;

	ID2D1SolidColorBrush    *brush; /* owned */

	StoredItem               items [FLUX_MENU_MAX_ITEMS];
	int                      item_count;

	/* Layout flags driven by items */
	bool                     has_icons;
	bool                     has_toggles; /* toggle OR radio */
	bool                     has_accels;
	bool                     has_submenus;

	/* Column widths resolved during measure */
	float                    col_placeholder; /* 0, 28, or 56 */
	float                    col_label_w;
	float                    col_accel_w;

	/* Measured presenter outer size (incl. border) */
	float                    total_w;
	float                    total_h;

	/* Active padding (chosen at show time per input modality) */
	float                    pad_top;
	float                    pad_bot;

	/* Vertical scroll offset when the content exceeds the popup card.
	 * 0 = scrolled to top. A positive value means the content has been
	 * pushed up by that many DIPs.                                       */
	float                    scroll_y;
	float                    scroll_max; /* computed in menu_measure */

	/* Transient state */
	int                      hovered_index;
	int                      pressed_index;
	int                      keyboard_index;
	int                      open_submenu_index;

	/* Hierarchy */
	struct FluxMenuFlyout   *parent;
	struct FluxMenuFlyout   *open_submenu;
	int                      parent_item_index;

	/* Dismiss */
	FluxPopupDismissCallback dismiss_cb;
	void                    *dismiss_ctx;
};

/* ══════════════════════════════════════════════════════════════════════════
 *  Small helpers
 * ══════════════════════════════════════════════════════════════════════════ */

static char *dup_cstr(char const *s) {
	if (!s) return NULL;
	size_t n = strlen(s);
	char  *c = ( char * ) malloc(n + 1);
	if (c) memcpy(c, s, n + 1);
	return c;
}

static void invalidate(FluxMenuFlyout *m) {
	if (!m) return;
	HWND hwnd = flux_popup_get_hwnd(m->popup);
	if (hwnd) InvalidateRect(hwnd, NULL, FALSE);
}

static FluxMenuFlyout *root_of(FluxMenuFlyout *m) {
	while (m && m->parent) m = m->parent;
	return m;
}

static void refresh_active_leaf(FluxMenuFlyout *root) {
	FluxMenuFlyout *leaf = root;
	while (leaf && leaf->open_submenu && flux_popup_is_visible(leaf->open_submenu->popup)) leaf = leaf->open_submenu;
	g_active_leaf = leaf;
}

static int first_enabled(FluxMenuFlyout const *m) {
	for (int i = 0; i < m->item_count; i++)
		if (m->items [i].type != FLUX_MENU_ITEM_SEPARATOR && m->items [i].enabled) return i;
	return -1;
}

static int last_enabled(FluxMenuFlyout const *m) {
	for (int i = m->item_count - 1; i >= 0; i--)
		if (m->items [i].type != FLUX_MENU_ITEM_SEPARATOR && m->items [i].enabled) return i;
	return -1;
}

static int find_next(FluxMenuFlyout const *m, int from, int dir) {
	if (m->item_count == 0) return -1;
	int idx = from;
		for (int attempt = 0; attempt < m->item_count; attempt++) {
			idx += dir;
			if (idx < 0) idx = m->item_count - 1;
			if (idx >= m->item_count) idx = 0;
			if (m->items [idx].type != FLUX_MENU_ITEM_SEPARATOR && m->items [idx].enabled) return idx;
		}
	return -1;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Text measurement
 * ══════════════════════════════════════════════════════════════════════════ */

static FluxSize measure_text(FluxMenuFlyout *m, char const *s, float font_size) {
	FluxSize sz = {0.0f, 0.0f};
	if (!s || !*s) return sz;
		if (m->text) {
			FluxTextStyle ts = {0};
			ts.font_size     = font_size;
			ts.font_weight   = FLUX_FONT_REGULAR;
			ts.text_align    = FLUX_TEXT_LEFT;
			ts.vert_align    = FLUX_TEXT_TOP;
			ts.color         = flux_color_rgba(0, 0, 0, 0xff);
			sz               = flux_text_measure(m->text, s, &ts, 10000.0f);
		}
		else {
			sz.w = ( float ) strlen(s) * 7.0f;
			sz.h = font_size * 1.35f;
		}
	return sz;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Layout / measure
 *
 *  Geometry (DIP, matches WinUI template):
 *
 *  +─ Presenter (border) ───────────────────────────────────+
 *  │                                                        │
 *  │   +─ Presenter padding 0,2,0,2 ─ Item margin 4,2,4,2 ┐ │
 *  │   │ +─ Item (CornerRadius=4) ──────────────────────┐ │ │
 *  │   │ │  CheckCol  IconCol  Label        Accel Chvr  │ │ │
 *  │   │ │   (28/0)    (28/0)  ...           (auto)     │ │ │
 *  │   │ └──────────────────────────────────────────────┘ │ │
 *  │   └──────────────────────────────────────────────────┘ │
 *  +────────────────────────────────────────────────────────+
 * ══════════════════════════════════════════════════════════════════════════ */

static void menu_measure(FluxMenuFlyout *m) {
	/* Aggregate which columns are needed (per WinUI CheckPlaceholderStates
	 * visual states:  NoPlaceholder / CheckPlaceholder / IconPlaceholder /
	 * CheckAndIconPlaceholder). The MenuFlyoutPresenter sets the same
	 * "checkbox column visible?" flag on every child, so all items share
	 * the same left inset — which is exactly what we're doing here.     */
	m->has_icons    = false;
	m->has_toggles  = false;
	m->has_accels   = false;
	m->has_submenus = false;

		for (int i = 0; i < m->item_count; i++) {
			StoredItem *it = &m->items [i];
			if (it->type == FLUX_MENU_ITEM_SEPARATOR) continue;
			if (it->icon_glyph) m->has_icons = true;
			if (it->type == FLUX_MENU_ITEM_TOGGLE || it->type == FLUX_MENU_ITEM_RADIO) m->has_toggles = true;
			if (it->accelerator_text) m->has_accels = true;
			if (it->type == FLUX_MENU_ITEM_SUBMENU) m->has_submenus = true;
		}

	/* Resolve the CheckPlaceholder column width. */
	float placeholder = 0.0f;
	if (m->has_toggles && m->has_icons) placeholder = 56.0f;
	else if (m->has_toggles || m->has_icons) placeholder = 28.0f;
	m->col_placeholder = placeholder;

	/* Widest label and widest accelerator (CaptionTextBlockStyle = 12pt). */
	float max_label = 0.0f, max_accel = 0.0f, max_text_h = 0.0f;
		for (int i = 0; i < m->item_count; i++) {
			StoredItem *it = &m->items [i];
			if (it->type == FLUX_MENU_ITEM_SEPARATOR) continue;

				if (it->label) {
					FluxSize sz = measure_text(m, it->label, FLUX_MENU_ITEM_FONT_SIZE);
					if (sz.w > max_label) max_label = sz.w;
					if (sz.h > max_text_h) max_text_h = sz.h;
				}
				if (it->accelerator_text) {
					FluxSize sz = measure_text(m, it->accelerator_text, FLUX_MENU_ITEM_ACCEL_FONT_SIZE);
					if (sz.w > max_accel) max_accel = sz.w;
				}
		}
	if (max_text_h <= 0.0f) max_text_h = FLUX_MENU_ITEM_FONT_SIZE * 1.35f;

	m->col_label_w = max_label;
	m->col_accel_w = max_accel;

	/* Trailing column extras: 24 DIP gap + column content.                */
	float trailing = 0.0f;
	if (m->has_accels) trailing += FLUX_MENU_ITEM_ACCEL_GAP + max_accel;
	if (m->has_submenus) trailing += FLUX_MENU_ITEM_CHEVRON_GAP + FLUX_MENU_ITEM_CHEVRON_SIZE;

	/* Item interior (inside the margin band) = padding L + columns + padding R */
	float       icon_band           = m->has_icons ? FLUX_MENU_ITEM_ICON_SIZE : 0.0f;
	float       item_inner_w        = FLUX_MENU_ITEM_PAD_LEFT
	                                + placeholder
	                                + /* check col or 0 */
	                                  icon_band
	                                + /* icon col or 0 */
	                                  (icon_band > 0.0f ? FLUX_MENU_ITEM_ICON_GAP : 0.0f)
	                                + max_label
	                                + trailing
	                                + FLUX_MENU_ITEM_PAD_RIGHT;

	/* Presenter content width = item_inner + 2 * item margin horizontal.  */
	float       presenter_content_w = item_inner_w + 2.0f * FLUX_MENU_ITEM_MARGIN_H;

	/* Presenter outer width = content + 2 * border.  (Presenter padding
	 * is 0 horizontally so we do not add it.)                            */
	float       total_w             = presenter_content_w + 2.0f * FLUX_MENU_PRESENTER_BORDER;

	float const MIN_W               = 96.0f; /* FlyoutThemeMinWidth */
	if (total_w < MIN_W) total_w = MIN_W;

	/* Per-item vertical size. */
	float item_content_h
	  = m->pad_top + (max_text_h > FLUX_MENU_ITEM_ICON_SIZE ? max_text_h : FLUX_MENU_ITEM_ICON_SIZE) + m->pad_bot;
	float item_slot_h  = item_content_h + 2.0f * FLUX_MENU_ITEM_MARGIN_V;

	float cumulative_y = 0.0f;
		for (int i = 0; i < m->item_count; i++) {
			StoredItem *it = &m->items [i];
				if (it->type == FLUX_MENU_ITEM_SEPARATOR) {
					/* Separator height = its own padding 1+line 1+1 = 3 DIP.      */
					it->h = FLUX_MENU_SEP_PAD_TOP + FLUX_MENU_SEP_HEIGHT + FLUX_MENU_SEP_PAD_BOTTOM;
				}
				else { it->h = item_slot_h; }
			it->y         = cumulative_y;
			cumulative_y += it->h;
		}

	float intrinsic_h
	  = FLUX_MENU_PRESENTER_PAD_TOP + cumulative_y + FLUX_MENU_PRESENTER_PAD_BOTTOM + 2.0f * FLUX_MENU_PRESENTER_BORDER;
	if (intrinsic_h < FLUX_MENU_THEME_MIN_HEIGHT) intrinsic_h = FLUX_MENU_THEME_MIN_HEIGHT;

	/* Clamp the presenter height to the owner monitor's work area so that
	 * a very long menu never overflows the screen. When clamped we enable
	 * vertical scrolling via the mouse wheel (see menu_mouse).           */
	float total_h    = intrinsic_h;
	float max_h_work = 0.0f;
		if (m->owner) {
			HWND owner_hwnd = flux_window_hwnd(m->owner);
				if (owner_hwnd) {
					HMONITOR    mon = MonitorFromWindow(owner_hwnd, MONITOR_DEFAULTTONEAREST);
					MONITORINFO mi;
					mi.cbSize = sizeof(mi);
						if (GetMonitorInfoW(mon, &mi)) {
							FluxDpiInfo dpi   = flux_window_dpi(m->owner);
							float       scale = dpi.dpi_x / 96.0f;
							if (scale < 0.1f) scale = 1.0f;
							float work_h_dip = ( float ) (mi.rcWork.bottom - mi.rcWork.top) / scale;
							max_h_work       = work_h_dip - 16.0f; /* small screen margin */
						}
				}
		}
	if (max_h_work > 64.0f && total_h > max_h_work) total_h = max_h_work;

	/* scroll_max measures how many DIPs we need to be able to scroll to
	 * see the tail of the content.                                       */
	m->scroll_max = intrinsic_h > total_h ? (intrinsic_h - total_h) : 0.0f;
	if (m->scroll_y > m->scroll_max) m->scroll_y = m->scroll_max;
	if (m->scroll_y < 0.0f) m->scroll_y = 0.0f;

	m->total_w = total_w;
	m->total_h = total_h;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Hit testing
 * ══════════════════════════════════════════════════════════════════════════ */

static int menu_hit_test(FluxMenuFlyout const *m, float px, float py) {
	if (m->item_count == 0) return -1;

	/* Reject hits outside the presenter's visible content band (points
	 * scrolled out of view or on the border/pad are not items).         */
	float band_top = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP;
	float band_bot = m->total_h - FLUX_MENU_PRESENTER_BORDER - FLUX_MENU_PRESENTER_PAD_BOTTOM;
	if (py < band_top || py >= band_bot) return -1;

	float local_x   = px - FLUX_MENU_PRESENTER_BORDER;
	/* Add the scroll offset so physical y on screen maps to the content
	 * coordinate space that matches each item's stored `y`.              */
	float local_y   = py - band_top + m->scroll_y;

	float content_w = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER;

		for (int i = 0; i < m->item_count; i++) {
			StoredItem const *it = &m->items [i];
				if (local_y >= it->y && local_y < it->y + it->h) {
					if (it->type == FLUX_MENU_ITEM_SEPARATOR) return -1;

					float ir_left  = FLUX_MENU_ITEM_MARGIN_H;
					float ir_right = content_w - FLUX_MENU_ITEM_MARGIN_H;
					float ir_top   = it->y + FLUX_MENU_ITEM_MARGIN_V;
					float ir_bot   = it->y + it->h - FLUX_MENU_ITEM_MARGIN_V;

					if (local_x >= ir_left && local_x < ir_right && local_y >= ir_top && local_y < ir_bot) return i;
					return -1;
				}
		}
	return -1;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Submenu open / close
 * ══════════════════════════════════════════════════════════════════════════ */

static void close_submenu(FluxMenuFlyout *m);

static void _dismiss_internal(FluxMenuFlyout *m, bool fire_cb) {
	if (!m) return;
	close_submenu(m);

	flux_popup_dismiss(m->popup);

	m->hovered_index      = -1;
	m->pressed_index      = -1;
	m->keyboard_index     = -1;
	m->open_submenu_index = -1;

		if (m->parent && m->parent->open_submenu == m) {
			m->parent->open_submenu       = NULL;
			m->parent->open_submenu_index = -1;
			invalidate(m->parent);
		}

	FluxMenuFlyout *parent_before = m->parent;
	m->parent                     = NULL;
	m->parent_item_index          = -1;

	if (fire_cb && m->dismiss_cb) m->dismiss_cb(m->dismiss_ctx);

	if (parent_before) refresh_active_leaf(root_of(parent_before));
	else if (g_active_leaf == m) g_active_leaf = NULL;
}

static void close_submenu(FluxMenuFlyout *m) {
	if (!m || !m->open_submenu) return;
	FluxMenuFlyout *child = m->open_submenu;
	m->open_submenu       = NULL;
	m->open_submenu_index = -1;
	_dismiss_internal(child, false);
	invalidate(m);
}

static void open_submenu(FluxMenuFlyout *m, int idx, bool focus_first) {
	if (!m || idx < 0 || idx >= m->item_count) return;
	StoredItem *it = &m->items [idx];
	if (it->type != FLUX_MENU_ITEM_SUBMENU || !it->submenu || !it->enabled) return;

		/* Already open on this same item? — just (optionally) focus first.    */
		if (m->open_submenu == it->submenu && flux_popup_is_visible(it->submenu->popup)) {
				if (focus_first) {
					it->submenu->keyboard_index = first_enabled(it->submenu);
					it->submenu->hovered_index  = -1;
					invalidate(it->submenu);
					refresh_active_leaf(root_of(m));
				}
			return;
		}

	/* Close any previously open sibling submenu. */
	close_submenu(m);

	FluxMenuFlyout *child    = it->submenu;
	child->parent            = m;
	child->parent_item_index = idx;
	child->theme             = m->theme;
	child->is_dark           = m->is_dark;
	child->text              = m->text;
	/* Cascade the parent's opening-input padding so a touch-opened root
	 * gives touch-sized children too.                                   */
	child->pad_top           = (m->pad_top > 0.0f) ? m->pad_top : FLUX_MENU_ITEM_PAD_TOP_NARROW;
	child->pad_bot           = (m->pad_bot > 0.0f) ? m->pad_bot : FLUX_MENU_ITEM_PAD_BOT_NARROW;
	child->scroll_y          = 0.0f;

	menu_measure(m);
	menu_measure(child);
	flux_popup_set_size(child->popup, child->total_w, child->total_h);
	flux_popup_enable_system_backdrop(child->popup, child->is_dark);

	/* Anchor rect: the hovered item's layout rectangle in screen space.   */
	FluxDpiInfo dpi         = flux_window_dpi(m->owner);
	float       scale       = dpi.dpi_x / 96.0f;

	HWND        parent_hwnd = flux_popup_get_hwnd(m->popup);
	RECT        wr          = {0};
	GetWindowRect(parent_hwnd, &wr);

	float ir_x_dip = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_ITEM_MARGIN_H;
	/* Account for the parent's scroll offset — the item's *screen* y is
	 * (BORDER + PAD_TOP + it->y - scroll_y) * scale above the window.   */
	float ir_y_dip
	  = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP + it->y + FLUX_MENU_ITEM_MARGIN_V - m->scroll_y;
	float    ir_h_dip = it->h - 2.0f * FLUX_MENU_ITEM_MARGIN_V;
	float    ir_w_dip = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER - 2.0f * FLUX_MENU_ITEM_MARGIN_H;

	FluxRect anchor;
	anchor.x                  = ( float ) wr.left + ir_x_dip * scale;
	anchor.y                  = ( float ) wr.top + ir_y_dip * scale;
	anchor.w                  = ir_w_dip * scale;
	anchor.h                  = ir_h_dip * scale;

	m->open_submenu           = child;
	m->open_submenu_index     = idx;

	child->hovered_index      = -1;
	child->pressed_index      = -1;
	child->open_submenu_index = -1;
	child->keyboard_index     = focus_first ? first_enabled(child) : -1;

	flux_popup_show(child->popup, anchor, FLUX_PLACEMENT_RIGHT);

	invalidate(m);
	invalidate(child);
	refresh_active_leaf(root_of(m));
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Item activation (click / Enter)
 * ══════════════════════════════════════════════════════════════════════════ */

static void uncheck_radio_group(FluxMenuFlyout *m, char const *group, int except) {
	if (!group || !*group) return;
		for (int i = 0; i < m->item_count; i++) {
			if (i == except) continue;
			StoredItem *it = &m->items [i];
			if (it->type == FLUX_MENU_ITEM_RADIO && it->radio_group && strcmp(it->radio_group, group) == 0)
				it->checked = false;
		}
}

static void activate_item(FluxMenuFlyout *m, int idx) {
	if (!m || idx < 0 || idx >= m->item_count) return;
	StoredItem *it = &m->items [idx];
	if (it->type == FLUX_MENU_ITEM_SEPARATOR || !it->enabled) return;

		if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu) {
			m->keyboard_index = idx;
			m->hovered_index  = -1;
			open_submenu(m, idx, true);
			return;
		}

		if (it->type == FLUX_MENU_ITEM_TOGGLE) { it->checked = !it->checked; }
		else if (it->type == FLUX_MENU_ITEM_RADIO) {
				if (!it->checked) {
					uncheck_radio_group(m, it->radio_group, idx);
					it->checked = true;
				}
		}

	if (it->on_click) it->on_click(it->on_click_ctx);

	flux_menu_flyout_dismiss_all(m);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Mouse handling (forwarded from the popup WM_MOUSE* messages)
 * ══════════════════════════════════════════════════════════════════════════ */

static void menu_mouse(void *ctx, FluxPopup *popup, FluxPopupMouseEvent ev, float x, float y) {
	( void ) popup;
	FluxMenuFlyout *m = ( FluxMenuFlyout * ) ctx;
	if (!m) return;

		switch (ev) {
			case FLUX_POPUP_MOUSE_MOVE : {
				int idx = menu_hit_test(m, x, y);
					if (idx != m->hovered_index) {
						m->hovered_index  = idx;
						m->keyboard_index = -1;
						invalidate(m);
					}
					if (idx >= 0) {
						StoredItem *it = &m->items [idx];
						if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu && it->enabled)
							open_submenu(m, idx, false);
						else if (m->open_submenu_index >= 0 && m->open_submenu_index != idx) close_submenu(m);
					}
					else if (m->open_submenu_index >= 0) {
						/* Do not auto-close when hovering blank area — matches WinUI
						 * behaviour: submenu stays open until you move into another
						 * item of the parent. Nothing to do here.                   */
					}
				break;
			}

		case FLUX_POPUP_MOUSE_DOWN :
			m->pressed_index = menu_hit_test(m, x, y);
			invalidate(m);
			break;

			case FLUX_POPUP_MOUSE_UP : {
				int idx          = menu_hit_test(m, x, y);
				int pressed      = m->pressed_index;
				m->pressed_index = -1;
				invalidate(m);
				if (idx >= 0 && idx == pressed) activate_item(m, idx);
				break;
			}

		case FLUX_POPUP_MOUSE_LEAVE :
			m->hovered_index = -1;
			m->pressed_index = -1;
			invalidate(m);
			break;

			case FLUX_POPUP_MOUSE_WHEEL : {
				/* y carries the requested scroll delta in DIPs (positive = up). */
				if (m->scroll_max <= 0.0f) break;

				float new_y = m->scroll_y - y; /* wheel up → show earlier items */
				if (new_y < 0.0f) new_y = 0.0f;
				if (new_y > m->scroll_max) new_y = m->scroll_max;
					if (new_y != m->scroll_y) {
						m->scroll_y      = new_y;
						m->hovered_index = -1;
						/* Any open submenu was anchored at an item that has now moved
						 * on screen — close it so we don't leave an orphan popup.    */
						if (m->open_submenu_index >= 0) close_submenu(m);
						invalidate(m);
					}
				break;
			}
		}
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Brush helper
 * ══════════════════════════════════════════════════════════════════════════ */

static void ensure_brush(FluxMenuFlyout *m, ID2D1DeviceContext *d2d) {
	if (m->brush || !d2d) return;
	D2D1_COLOR_F          black = {0, 0, 0, 1};
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = 1.0f;
	bp.transform._11 = 1;
	bp.transform._12 = 0;
	bp.transform._21 = 0;
	bp.transform._22 = 1;
	bp.transform._31 = 0;
	bp.transform._32 = 0;
	ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &bp, &m->brush);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Colour resolution (maps WinUI brush tokens → FluxThemeColors + fallback)
 * ══════════════════════════════════════════════════════════════════════════ */

typedef struct ResolvedColors {
	/* Presenter */
	FluxColor presenter_bg;
	FluxColor presenter_border;
	FluxColor separator;

	/* Text */
	FluxColor fg_primary;   /* TextFillColorPrimary */
	FluxColor fg_secondary; /* TextFillColorSecondary (accel, chevron) */
	FluxColor fg_tertiary;  /* TextFillColorTertiary  (accel/chevron pressed) */
	FluxColor fg_disabled;  /* TextFillColorDisabled */

	/* Item backgrounds */
	FluxColor bg_hover;   /* SubtleFillColorSecondary */
	FluxColor bg_pressed; /* SubtleFillColorTertiary  */

	/* Focus ring */
	FluxColor focus_outer; /* FocusStrokeColorOuter */
	FluxColor focus_inner; /* FocusStrokeColorInner */
} ResolvedColors;

static ResolvedColors resolve_colors(FluxMenuFlyout const *m) {
	ResolvedColors         r;
	FluxThemeColors const *t             = m->theme;
	bool const             dark          = m->is_dark;

	/* MenuFlyoutPresenterBackground = DesktopAcrylicTransparentBrush,
	 * defined in AcrylicBrush_themeresources.xaml as:
	 *     <SolidColorBrush x:Key="DesktopAcrylicTransparentBrush"
	 *                      Color="#00000000" />
	 * i.e. fully transparent. The apparent colour comes from the
	 * MenuFlyoutPresenter's SystemBackdrop = DesktopAcrylicBackdrop,
	 * which is the Windows 11 composition-layer acrylic.
	 *
	 * When fluxent successfully enabled the DWM transient backdrop on
	 * the popup HWND, we leave the card fully transparent — matching
	 * WinUI exactly. Otherwise we fall back to a reasonable solid
	 * colour (AcrylicInAppFillColorDefault's FallbackColor: #F9F9F9 /
	 * #2C2C2C) so the menu is still legible on downlevel systems.     */
	bool                   have_backdrop = m->popup && flux_popup_has_system_backdrop(m->popup);
		if (have_backdrop) {
			/* DWM's DWMSBT_TRANSIENTWINDOW material is visibly *lighter* than
			 * WinUI 3's DesktopAcrylicBackdrop (the composition-API path uses a
			 * heavier LuminosityOpacity). We cannot reach the latter from C
			 * without pulling in Microsoft.UI.Composition.SystemBackdrops.
			 *
			 * To close the gap, overlay a translucent tint sampled from
			 * AcrylicBrush_themeresources.xaml's `AcrylicBackgroundFillColorDefault`:
			 *     dark  : TintColor=#2C2C2C  FallbackColor=#2C2C2C
			 *     light : TintColor=#FCFCFC  FallbackColor=#F9F9F9
			 * Paint it over the transparent presenter so the DWM acrylic still
			 * shows through, but with the extra darkening/lightening WinUI does. */
			r.presenter_bg = dark ? flux_color_rgba(0x2c, 0x2c, 0x2c, 0x7a) : flux_color_rgba(0xfc, 0xfc, 0xfc, 0x66);
		}
		else { r.presenter_bg = dark ? flux_color_rgb(0x2c, 0x2c, 0x2c) : flux_color_rgb(0xf9, 0xf9, 0xf9); }

	/* SurfaceStrokeColorFlyoutBrush (not ControlStrokeColorDefault):
	 *   Dark  : #33000000   Light : #0F000000                             */
	r.presenter_border = dark ? flux_color_rgba(0, 0, 0, 0x33) : flux_color_rgba(0, 0, 0, 0x0f);

	/* DividerStrokeColorDefaultBrush (Common_themeresources_any.xaml):
	 *   Dark  : #15FFFFFF   Light : #0F000000                             */
	r.separator        = t ? t->divider_stroke_default
	                       : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x15) : flux_color_rgba(0, 0, 0, 0x0f));

	/* TextFillColor* (Common_themeresources_any.xaml):
	 *   Primary   Dark: #FFFFFF (opaque white)  Light: #E4000000
	 *   Secondary Dark: #C5FFFFFF               Light: #9E000000
	 *   Tertiary  Dark: #87FFFFFF               Light: #72000000
	 *   Disabled  Dark: #5DFFFFFF               Light: #5C000000          */
	r.fg_primary
	  = t ? t->text_primary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0xff) : flux_color_rgba(0, 0, 0, 0xe4));
	r.fg_secondary
	  = t ? t->text_secondary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0xc5) : flux_color_rgba(0, 0, 0, 0x9e));
	r.fg_tertiary
	  = t ? t->text_tertiary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x87) : flux_color_rgba(0, 0, 0, 0x72));
	r.fg_disabled
	  = t ? t->text_disabled : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x5d) : flux_color_rgba(0, 0, 0, 0x5c));

	/* SubtleFillColor* (Common_themeresources_any.xaml):
	 *   Secondary Dark: #0FFFFFFF  Light: #09000000
	 *   Tertiary  Dark: #0AFFFFFF  Light: #06000000                       */
	r.bg_hover = t ? t->subtle_fill_secondary
	               : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x0f) : flux_color_rgba(0, 0, 0, 0x09));
	r.bg_pressed
	  = t ? t->subtle_fill_tertiary : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0x0a) : flux_color_rgba(0, 0, 0, 0x06));

	/* FocusStrokeColor* (Common_themeresources_any.xaml):
	 *   Outer Dark: #FFFFFF (opaque)  Light: #E4000000
	 *   Inner Dark: #B3000000         Light: #B3FFFFFF                    */
	r.focus_outer
	  = t ? t->focus_stroke_outer : (dark ? flux_color_rgba(0xff, 0xff, 0xff, 0xff) : flux_color_rgba(0, 0, 0, 0xe4));
	r.focus_inner
	  = t ? t->focus_stroke_inner : (dark ? flux_color_rgba(0, 0, 0, 0xb3) : flux_color_rgba(0xff, 0xff, 0xff, 0xb3));
	return r;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Rendering
 * ══════════════════════════════════════════════════════════════════════════ */

static void draw_glyph(
  FluxMenuFlyout *m, ID2D1RenderTarget *rt, char const *glyph, FluxRect const *box, float size, FluxColor color,
  FluxTextAlign ha, FluxTextVAlign va
) {
	if (!m->text || !glyph || !*glyph) return;
	FluxTextStyle ts = {0};
	/* Segoe Fluent Icons is the Windows 11 icon font used by WinUI 3.
	 * DirectWrite's built-in font-fallback chain will fall back to
	 * "Segoe MDL2 Assets" on Windows 10 automatically when a glyph is
	 * missing, so we don't need to try two font names.                 */
	ts.font_family   = "Segoe Fluent Icons";
	ts.font_size     = size;
	ts.font_weight   = FLUX_FONT_REGULAR;
	ts.text_align    = ha;
	ts.vert_align    = va;
	ts.color         = color;
	flux_text_draw(m->text, rt, glyph, box, &ts);
}

static void draw_label(
  FluxMenuFlyout *m, ID2D1RenderTarget *rt, char const *text, FluxRect const *box, float size, FluxColor color,
  FluxTextAlign ha
) {
	if (!m->text || !text) return;
	FluxTextStyle ts = {0};
	ts.font_size     = size;
	ts.font_weight   = FLUX_FONT_REGULAR;
	ts.text_align    = ha;
	ts.vert_align    = FLUX_TEXT_VCENTER;
	ts.color         = color;
	flux_text_draw(m->text, rt, text, box, &ts);
}

static void menu_paint(void *ctx, FluxPopup *popup) {
	FluxMenuFlyout *m = ( FluxMenuFlyout * ) ctx;
	if (!m) return;

	FluxGraphics *gfx = flux_popup_get_graphics(popup);
	if (!gfx) return;
	ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
	if (!d2d) return;

	ensure_brush(m, d2d);
	if (!m->brush) return;

	ResolvedColors    c = resolve_colors(m);

	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d                    = d2d;
	rc.brush                  = m->brush;
	rc.text                   = m->text;
	rc.theme                  = m->theme;
	rc.dpi                    = flux_window_dpi(m->owner);
	rc.is_dark                = m->is_dark;

	ID2D1RenderTarget *rt     = ( ID2D1RenderTarget * ) d2d;

	/* ── Presenter card (Border with BackgroundSizing=InnerBorderEdge) ── */
	FluxRect           bounds = {0.0f, 0.0f, m->total_w, m->total_h};
	flux_fill_rounded_rect(&rc, &bounds, FLUX_MENU_PRESENTER_CORNER, c.presenter_bg);
	{
		float    half  = FLUX_MENU_PRESENTER_BORDER * 0.5f;
		FluxRect inset = {bounds.x + half, bounds.y + half, bounds.w - half * 2.0f, bounds.h - half * 2.0f};
		flux_draw_rounded_rect(&rc, &inset, FLUX_MENU_PRESENTER_CORNER, c.presenter_border, FLUX_MENU_PRESENTER_BORDER);
	}

	/* ── Items ─────────────────────────────────────────────────────── */
	float       content_x0 = FLUX_MENU_PRESENTER_BORDER;
	float       content_y0 = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP;
	float       content_w  = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER;
	float       content_y1 = m->total_h - FLUX_MENU_PRESENTER_BORDER - FLUX_MENU_PRESENTER_PAD_BOTTOM;

	/* Clip to the presenter's interior so scrolled-out items never peek
	 * past the rounded card edge; translate by -scroll_y so item `y`
	 * values (0-based from the first item) map to on-screen positions.
	 *
	 * CRITICAL: compose with the existing transform set by the popup's
	 * WM_PAINT (entrance animation translate) instead of overwriting —
	 * otherwise the slide-in animation is destroyed for menu flyouts. */
	D2D1_RECT_F clip_rect  = {content_x0, content_y0, content_x0 + content_w, content_y1};
	ID2D1RenderTarget_PushAxisAlignedClip(rt, &clip_rect, D2D1_ANTIALIAS_MODE_ALIASED);

	D2D1_MATRIX_3X2_F prev_xform;
	ID2D1RenderTarget_GetTransform(rt, &prev_xform);

	/* scroll_xform = Translate(0, -scroll_y) composed with prev.
	 * D2D matrices are affine row-vectors; composition is scroll * prev,
	 * but since scroll is a pure translate we can just offset the _31/_32
	 * of prev directly.                                                  */
	D2D1_MATRIX_3X2_F scroll_xform  = prev_xform;
	scroll_xform._32               -= m->scroll_y;
	ID2D1RenderTarget_SetTransform(rt, &scroll_xform);

		for (int i = 0; i < m->item_count; i++) {
			StoredItem *it       = &m->items [i];
			float       slot_top = content_y0 + it->y;

				/* ── Separator (Rectangle Height=1, Padding=-4,1,-4,1) ─────── */
				if (it->type == FLUX_MENU_ITEM_SEPARATOR) {
					/* Padding -4 on X means the line extends 4 DIP *past* the item
					 * margin on both sides (so it spans from the inner edge of the
					 * presenter border to the opposite inner edge).            */
					float y_mid = slot_top + FLUX_MENU_SEP_PAD_TOP + FLUX_MENU_SEP_HEIGHT * 0.5f;
					float x0    = content_x0; /* inner border edge */
					float x1    = content_x0 + content_w;
					flux_draw_line(&rc, x0, y_mid, x1, y_mid, c.separator, FLUX_MENU_SEP_HEIGHT);
					continue;
				}

			bool focused_kb = (i == m->keyboard_index);
			bool hovered    = (i == m->hovered_index);
			bool pressed    = (i == m->pressed_index);
			bool submenu_open = (it->type == FLUX_MENU_ITEM_SUBMENU &&
                             i == m->open_submenu_index &&
                             m->open_submenu &&
                             flux_popup_is_visible(m->open_submenu->popup));

			/* LayoutRoot rectangle (inside margin band). */
			float     ir_x       = content_x0 + FLUX_MENU_ITEM_MARGIN_H;
			float     ir_y       = slot_top + FLUX_MENU_ITEM_MARGIN_V;
			float     ir_w       = content_w - 2.0f * FLUX_MENU_ITEM_MARGIN_H;
			float     ir_h       = it->h - 2.0f * FLUX_MENU_ITEM_MARGIN_V;
			FluxRect  ir         = {ir_x, ir_y, ir_w, ir_h};

			/* ── Visual state → Background + Foreground ──────────────── */
			FluxColor bg_fill    = flux_color_rgba(0, 0, 0, 0);
			FluxColor fg_text    = c.fg_primary;
			FluxColor fg_accel   = c.fg_secondary;
			FluxColor fg_chevron = c.fg_secondary;

				if (!it->enabled) {
					fg_text    = c.fg_disabled;
					fg_accel   = c.fg_disabled;
					fg_chevron = c.fg_disabled;
				}
				else if (pressed) {
					bg_fill    = c.bg_pressed;
					fg_accel   = c.fg_tertiary;
					fg_chevron = c.fg_tertiary;
				}
				else if (submenu_open) { bg_fill = c.bg_hover; /* SubMenuOpened state */ }
				else if (hovered || focused_kb) { bg_fill = c.bg_hover; }

			if (flux_color_af(bg_fill) > 0.0f) flux_fill_rounded_rect(&rc, &ir, FLUX_MENU_ITEM_CORNER, bg_fill);

				/* Focus ring — WinUI UseSystemFocusVisuals=True draws a dual ring:
			     *   outer = FocusStrokeColorOuter, 2 DIP, flush to the element edge
			     *   inner = FocusStrokeColorInner, 1 DIP, inset by the outer width
			     * Both rings follow the item's CornerRadius (ControlCornerRadius=4). */
				if (focused_kb && it->enabled) {
					float const outer_w    = 2.0f;
					float const inner_w    = 1.0f;

					/* Outer ring — centred on the element edge (inset by half the
					 * stroke so the full 2 DIP lands inside the item bounds).     */
					float       oh         = outer_w * 0.5f;
					FluxRect    outer_rect = {ir.x + oh, ir.y + oh, ir.w - outer_w, ir.h - outer_w};
					flux_draw_rounded_rect(&rc, &outer_rect, FLUX_MENU_ITEM_CORNER, c.focus_outer, outer_w);

					/* Inner ring — inset by the full outer width plus half of its
					 * own, so the two rings are visually contiguous.              */
					float    ih         = inner_w * 0.5f;
					float    ins        = outer_w + ih;
					FluxRect inner_rect = {ir.x + ins, ir.y + ins, ir.w - 2.0f * ins, ir.h - 2.0f * ins};
					float    inner_r    = FLUX_MENU_ITEM_CORNER - outer_w;
					if (inner_r < 0.0f) inner_r = 0.0f;
					flux_draw_rounded_rect(&rc, &inner_rect, inner_r, c.focus_inner, inner_w);
				}

			/* ── Column walk ─────────────────────────────────────────── */
			float cx = ir.x + FLUX_MENU_ITEM_PAD_LEFT;
			float text_x;

				/* 1) Check column (28 DIP) — Toggle / Radio only (glyph when
			     *    checked).                                                  */
				if (m->has_toggles) {
					FluxRect check_box = {cx, ir.y, 28.0f - 12.0f, ir.h};
						if (it->checked && it->type == FLUX_MENU_ITEM_TOGGLE) {
							draw_glyph(
							  m, rt, GLYPH_CHECKMARK, &check_box, FLUX_MENU_ITEM_CHECK_GLYPH_SIZE, fg_text,
							  FLUX_TEXT_CENTER, FLUX_TEXT_VCENTER
							);
						}
						else if (it->checked && it->type == FLUX_MENU_ITEM_RADIO) {
							/* RadioMenuFlyoutItem uses FontIcon Glyph=U+E915 "Completed"
							 * (a filled round bullet) at FontSize=12 — exactly as the
							 * WinUI RadioMenuFlyoutItem template does.                */
							draw_glyph(
							  m, rt, GLYPH_RADIO_DOT, &check_box, FLUX_MENU_ITEM_CHECK_GLYPH_SIZE, fg_text,
							  FLUX_TEXT_CENTER, FLUX_TEXT_VCENTER
							);
						}
					cx += 28.0f;
				}

				/* 2) Icon column (16 DIP + 12 DIP gap).                        */
				if (m->has_icons) {
						if (it->icon_glyph) {
							FluxRect icon_box = {
							  cx, ir.y + (ir.h - FLUX_MENU_ITEM_ICON_SIZE) * 0.5f, FLUX_MENU_ITEM_ICON_SIZE,
							  FLUX_MENU_ITEM_ICON_SIZE};
							draw_glyph(
							  m, rt, it->icon_glyph, &icon_box, FLUX_MENU_ITEM_ICON_SIZE, fg_text, FLUX_TEXT_CENTER,
							  FLUX_TEXT_VCENTER
							);
						}
					cx += FLUX_MENU_ITEM_ICON_SIZE + FLUX_MENU_ITEM_ICON_GAP;
				}

			text_x            = cx;

			/* 3) Right column extent — work backward from the inner right
			 *    padding, subtracting chevron + accel widths.              */
			float right_edge  = ir.x + ir.w - FLUX_MENU_ITEM_PAD_RIGHT;
			float label_right = right_edge;

				/* 3a) Chevron (submenu).  */
				if (it->type == FLUX_MENU_ITEM_SUBMENU) {
					FluxRect chev_box
					  = {right_edge - FLUX_MENU_ITEM_CHEVRON_SIZE, ir.y, FLUX_MENU_ITEM_CHEVRON_SIZE, ir.h};
					draw_glyph(
					  m, rt, GLYPH_CHEVRON_R, &chev_box, FLUX_MENU_ITEM_CHEVRON_SIZE, fg_chevron, FLUX_TEXT_CENTER,
					  FLUX_TEXT_VCENTER
					);
					label_right -= FLUX_MENU_ITEM_CHEVRON_SIZE + FLUX_MENU_ITEM_CHEVRON_GAP;
				}

				/* 3b) Accelerator text (CaptionTextBlockStyle, right aligned). */
				if (it->accelerator_text) {
					float    accel_right = label_right;
					FluxRect accel_box   = {accel_right - m->col_accel_w, ir.y, m->col_accel_w, ir.h};
					draw_label(
					  m, rt, it->accelerator_text, &accel_box, FLUX_MENU_ITEM_ACCEL_FONT_SIZE, fg_accel, FLUX_TEXT_RIGHT
					);
					label_right -= m->col_accel_w + FLUX_MENU_ITEM_ACCEL_GAP;
				}

				/* 4) Label (ControlContentThemeFontSize = 14, VCenter).        */
				if (it->label) {
					float lw = label_right - text_x;
					if (lw < 0.0f) lw = 0.0f;
					FluxRect lbl_box = {text_x, ir.y, lw, ir.h};
					draw_label(m, rt, it->label, &lbl_box, FLUX_MENU_ITEM_FONT_SIZE, fg_text, FLUX_TEXT_LEFT);
				}
		}

	/* Restore the caller's transform (preserves entrance animation) and
	 * pop the item clip.                                                */
	ID2D1RenderTarget_SetTransform(rt, &prev_xform);
	ID2D1RenderTarget_PopAxisAlignedClip(rt);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Public API — lifecycle
 * ══════════════════════════════════════════════════════════════════════════ */

FluxMenuFlyout *flux_menu_flyout_create(FluxWindow *owner) {
	if (!owner) return NULL;

	FluxMenuFlyout *m = ( FluxMenuFlyout * ) calloc(1, sizeof(*m));
	if (!m) return NULL;

	m->owner = owner;
	m->popup = flux_popup_create(owner);
		if (!m->popup) {
			free(m);
			return NULL;
		}

	flux_popup_set_dismiss_on_outside(m->popup, true);
	flux_popup_set_paint_callback(m->popup, menu_paint, m);
	flux_popup_set_mouse_callback(m->popup, menu_mouse, m);

	m->hovered_index      = -1;
	m->pressed_index      = -1;
	m->keyboard_index     = -1;
	m->open_submenu_index = -1;
	m->parent_item_index  = -1;
	return m;
}

void flux_menu_flyout_destroy(FluxMenuFlyout *m) {
	if (!m) return;

		if (m->open_submenu) {
			m->open_submenu->parent            = NULL;
			m->open_submenu->parent_item_index = -1;
			m->open_submenu                    = NULL;
		}
		if (m->parent && m->parent->open_submenu == m) {
			m->parent->open_submenu       = NULL;
			m->parent->open_submenu_index = -1;
		}
	if (g_active_leaf == m) g_active_leaf = NULL;

		for (int i = 0; i < m->item_count; i++) {
			free(m->items [i].label);
			free(m->items [i].icon_glyph);
			free(m->items [i].accelerator_text);
			free(m->items [i].radio_group);
		}

		if (m->brush) {
			ID2D1SolidColorBrush_Release(m->brush);
			m->brush = NULL;
		}
	if (m->popup) flux_popup_destroy(m->popup);
	free(m);
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Public API — configuration
 * ══════════════════════════════════════════════════════════════════════════ */

void flux_menu_flyout_set_theme(FluxMenuFlyout *m, FluxThemeColors const *theme, bool is_dark) {
	if (!m) return;
	m->theme   = theme;
	m->is_dark = is_dark;
}

void flux_menu_flyout_set_text_renderer(FluxMenuFlyout *m, FluxTextRenderer *tr) {
	if (m) m->text = tr;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Public API — item management
 * ══════════════════════════════════════════════════════════════════════════ */

int flux_menu_flyout_add_item(FluxMenuFlyout *m, FluxMenuItemDef const *def) {
	if (!m || !def) return -1;
	if (m->item_count >= FLUX_MENU_MAX_ITEMS) return -1;

	int         idx = m->item_count++;
	StoredItem *it  = &m->items [idx];
	memset(it, 0, sizeof(*it));

	it->type             = def->type;
	it->label            = dup_cstr(def->label);
	it->icon_glyph       = dup_cstr(def->icon_glyph);
	it->accelerator_text = dup_cstr(def->accelerator_text);
	it->radio_group      = dup_cstr(def->radio_group);
	it->enabled          = def->enabled;
	it->checked          = def->checked;
	it->on_click         = def->on_click;
	it->on_click_ctx     = def->on_click_ctx;
	it->submenu          = def->submenu;
	return idx;
}

int flux_menu_flyout_add_separator(FluxMenuFlyout *m) {
	if (!m) return -1;
	if (m->item_count >= FLUX_MENU_MAX_ITEMS) return -1;
	int         idx = m->item_count++;
	StoredItem *it  = &m->items [idx];
	memset(it, 0, sizeof(*it));
	it->type    = FLUX_MENU_ITEM_SEPARATOR;
	it->enabled = false;
	return idx;
}

void flux_menu_flyout_clear(FluxMenuFlyout *m) {
	if (!m) return;
	close_submenu(m);
		for (int i = 0; i < m->item_count; i++) {
			free(m->items [i].label);
			free(m->items [i].icon_glyph);
			free(m->items [i].accelerator_text);
			free(m->items [i].radio_group);
			memset(&m->items [i], 0, sizeof(StoredItem));
		}
	m->item_count         = 0;
	m->hovered_index      = -1;
	m->pressed_index      = -1;
	m->keyboard_index     = -1;
	m->open_submenu_index = -1;
}

int  flux_menu_flyout_item_count(FluxMenuFlyout const *m) { return m ? m->item_count : 0; }

/* ══════════════════════════════════════════════════════════════════════════
 *  Public API — show / dismiss
 * ══════════════════════════════════════════════════════════════════════════ */

void flux_menu_flyout_show(FluxMenuFlyout *m, FluxRect anchor, FluxPlacement placement) {
	flux_menu_flyout_show_for_input(m, anchor, placement, FLUX_MENU_INPUT_MOUSE);
}

void flux_menu_flyout_show_for_input(
  FluxMenuFlyout *m, FluxRect anchor, FluxPlacement placement, FluxMenuInputKind input_kind
) {
	if (!m) return;

		/* WinUI PaddingSizeStates: Touch → DefaultPadding (11,8,11,9),
	     * everything else → NarrowPadding (11,4,11,5).                      */
		if (input_kind == FLUX_MENU_INPUT_TOUCH) {
			m->pad_top = FLUX_MENU_ITEM_PAD_TOP_TOUCH;
			m->pad_bot = FLUX_MENU_ITEM_PAD_BOT_TOUCH;
		}
		else {
			m->pad_top = FLUX_MENU_ITEM_PAD_TOP_NARROW;
			m->pad_bot = FLUX_MENU_ITEM_PAD_BOT_NARROW;
		}

	m->scroll_y = 0.0f;

	menu_measure(m);
	flux_popup_set_size(m->popup, m->total_w, m->total_h);

	/* Enable Windows 11 DWM transient-acrylic backdrop. If it succeeds,
	 * resolve_colors() overlays a translucent WinUI-tint sampled from
	 * AcrylicBackgroundFillColorDefault (#2C2C2C dark / #FCFCFC light)
	 * on top of the system material, because DWMSBT_TRANSIENTWINDOW is
	 * visibly lighter than WinUI 3's DesktopAcrylicBackdrop without an
	 * explicit tint layer. On older Windows the call is a no-op and we
	 * fall back to the opaque FallbackColor (#2C2C2C / #F9F9F9).        */
	flux_popup_enable_system_backdrop(m->popup, m->is_dark);

	m->hovered_index  = -1;
	m->keyboard_index = -1;
	m->pressed_index  = -1;
	close_submenu(m);

	flux_popup_show(m->popup, anchor, placement);
	refresh_active_leaf(root_of(m));
}

void flux_menu_flyout_dismiss(FluxMenuFlyout *m) { _dismiss_internal(m, true); }

void flux_menu_flyout_dismiss_all(FluxMenuFlyout *m) {
	FluxMenuFlyout *root = root_of(m);
	if (!root) return;

	while (root->open_submenu) close_submenu(root);

	FluxPopupDismissCallback cb = root->dismiss_cb;
	void                    *cx = root->dismiss_ctx;

	flux_popup_dismiss(root->popup);
	root->hovered_index      = -1;
	root->pressed_index      = -1;
	root->keyboard_index     = -1;
	root->open_submenu_index = -1;
	root->open_submenu       = NULL;
	root->parent             = NULL;
	root->parent_item_index  = -1;

	if (cb) cb(cx);
	g_active_leaf = NULL;
}

bool flux_menu_flyout_is_visible(FluxMenuFlyout const *m) { return m ? flux_popup_is_visible(m->popup) : false; }

void flux_menu_flyout_set_dismiss_callback(FluxMenuFlyout *m, FluxPopupDismissCallback cb, void *ctx) {
	if (!m) return;
	m->dismiss_cb  = cb;
	m->dismiss_ctx = ctx;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Public API — keyboard routing
 * ══════════════════════════════════════════════════════════════════════════ */

bool flux_menu_flyout_on_key(FluxMenuFlyout *m, unsigned int vk) {
	if (!m || !flux_popup_is_visible(m->popup)) return false;

		switch (vk) {
			case VK_DOWN : {
				int start = (m->keyboard_index >= 0) ? m->keyboard_index : -1;
				int n     = find_next(m, start, +1);
					if (n >= 0) {
						m->keyboard_index = n;
						m->hovered_index  = -1;
						close_submenu(m);
						invalidate(m);
					}
				return true;
			}
			case VK_UP : {
				int start = (m->keyboard_index >= 0) ? m->keyboard_index : m->item_count;
				int p     = find_next(m, start, -1);
					if (p >= 0) {
						m->keyboard_index = p;
						m->hovered_index  = -1;
						close_submenu(m);
						invalidate(m);
					}
				return true;
			}
			case VK_HOME : {
				int f = first_enabled(m);
					if (f >= 0) {
						m->keyboard_index = f;
						m->hovered_index  = -1;
						close_submenu(m);
						invalidate(m);
					}
				return true;
			}
			case VK_END : {
				int l = last_enabled(m);
					if (l >= 0) {
						m->keyboard_index = l;
						m->hovered_index  = -1;
						close_submenu(m);
						invalidate(m);
					}
				return true;
			}
		case VK_RETURN :
			case VK_SPACE : {
				if (m->keyboard_index >= 0) activate_item(m, m->keyboard_index);
				return true;
			}
		case VK_ESCAPE    : flux_menu_flyout_dismiss_all(m); return true;

			case VK_RIGHT : {
				int idx = m->keyboard_index;
					if (idx >= 0 && idx < m->item_count) {
						StoredItem *it = &m->items [idx];
						if (it->type == FLUX_MENU_ITEM_SUBMENU && it->submenu) open_submenu(m, idx, true);
					}
				return true;
			}
			case VK_LEFT : {
					if (m->parent) {
						FluxMenuFlyout *parent = m->parent;
						int             pidx   = m->parent_item_index;
						_dismiss_internal(m, true);
						parent->keyboard_index = pidx;
						parent->hovered_index  = -1;
						parent->pressed_index  = -1;
						invalidate(parent);
						refresh_active_leaf(root_of(parent));
					}
				return true;
			}
		}
	return false;
}

/* ══════════════════════════════════════════════════════════════════════════
 *  Public API — hierarchy accessors & global active query
 * ══════════════════════════════════════════════════════════════════════════ */

FluxMenuFlyout *flux_menu_flyout_get_open_submenu(FluxMenuFlyout *m) { return m ? m->open_submenu : NULL; }

FluxMenuFlyout *flux_menu_flyout_get_parent(FluxMenuFlyout *m) { return m ? m->parent : NULL; }

int             flux_menu_flyout_get_parent_item_index(FluxMenuFlyout *m) { return m ? m->parent_item_index : -1; }

FluxMenuFlyout *flux_menu_flyout_get_active(void) { return g_active_leaf; }
