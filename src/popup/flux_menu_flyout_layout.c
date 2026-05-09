#include "flux_menu_flyout_internal.h"

#include <string.h>
#include <windows.h>

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
		return flux_text_measure(m->text, s, &ts, 10000.0f);
	}

	sz.w = ( float ) strlen(s) * 7.0f;
	sz.h = font_size * 1.35f;
	return sz;
}

static void
menu_measure_item_text(FluxMenuFlyout *m, StoredItem const *it, float *max_label, float *max_accel, float *max_text_h) {
	if (it->label) {
		FluxSize sz = measure_text(m, it->label, FLUX_MENU_ITEM_FONT_SIZE);
		if (sz.w > *max_label) *max_label = sz.w;
		if (sz.h > *max_text_h) *max_text_h = sz.h;
	}
	if (it->accelerator_text) {
		FluxSize sz = measure_text(m, it->accelerator_text, FLUX_MENU_ITEM_ACCEL_FONT_SIZE);
		if (sz.w > *max_accel) *max_accel = sz.w;
	}
}

static void menu_scan_item_kinds(FluxMenuFlyout *m) {
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
}

static float menu_placeholder_width(FluxMenuFlyout const *m) {
	if (m->has_toggles && m->has_icons) return 56.0f;
	if (m->has_toggles || m->has_icons) return 28.0f;
	return 0.0f;
}

static void menu_measure_columns(FluxMenuFlyout *m, float *max_label, float *max_accel, float *max_text_h) {
	*max_label  = 0.0f;
	*max_accel  = 0.0f;
	*max_text_h = 0.0f;

	for (int i = 0; i < m->item_count; i++) {
		StoredItem *it = &m->items [i];
		if (it->type == FLUX_MENU_ITEM_SEPARATOR) continue;
		menu_measure_item_text(m, it, max_label, max_accel, max_text_h);
	}
}

static float menu_item_slot_height(FluxMenuFlyout const *m, float max_text_h) {
	float text_h = max_text_h > FLUX_MENU_ITEM_ICON_SIZE ? max_text_h : FLUX_MENU_ITEM_ICON_SIZE;
	float item_h = m->pad_top + text_h + m->pad_bot;
	return item_h + 2.0f * FLUX_MENU_ITEM_MARGIN_V;
}

static float menu_assign_item_layouts(FluxMenuFlyout *m, float item_slot_h) {
	float cumulative_y = 0.0f;

	for (int i = 0; i < m->item_count; i++) {
		StoredItem *it  = &m->items [i];
		it->h           = it->type == FLUX_MENU_ITEM_SEPARATOR
		                  ? FLUX_MENU_SEP_PAD_TOP + FLUX_MENU_SEP_HEIGHT + FLUX_MENU_SEP_PAD_BOTTOM
		                  : item_slot_h;
		it->y           = cumulative_y;
		cumulative_y   += it->h;
	}

	return cumulative_y;
}

static float menu_owner_max_height(FluxMenuFlyout *m) {
	if (!m->owner) return 0.0f;

	HWND owner_hwnd = flux_window_hwnd(m->owner);
	if (!owner_hwnd) return 0.0f;

	HMONITOR mon = MonitorFromWindow(owner_hwnd, MONITOR_DEFAULTTONEAREST);
	if (!mon) return 0.0f;

	MONITORINFO mi = {.cbSize = sizeof(mi)};
	if (!GetMonitorInfoW(mon, &mi)) return 0.0f;

	FluxDpiInfo dpi   = flux_window_dpi(m->owner);
	float       scale = dpi.dpi_x / 96.0f;
	if (scale < 0.1f) scale = 1.0f;
	return ( float ) (mi.rcWork.bottom - mi.rcWork.top) / scale - 16.0f;
}

static void menu_clamp_scroll(FluxMenuFlyout *m, float intrinsic_h, float total_h) {
	m->scroll_max = intrinsic_h > total_h ? intrinsic_h - total_h : 0.0f;
	if (m->scroll_y > m->scroll_max) m->scroll_y = m->scroll_max;
	if (m->scroll_y < 0.0f) m->scroll_y = 0.0f;
}

void menu_measure(FluxMenuFlyout *m) {
	menu_scan_item_kinds(m);

	float max_label    = 0.0f;
	float max_accel    = 0.0f;
	float max_text_h   = 0.0f;
	m->col_placeholder = menu_placeholder_width(m);
	menu_measure_columns(m, &max_label, &max_accel, &max_text_h);
	if (max_text_h <= 0.0f) max_text_h = FLUX_MENU_ITEM_FONT_SIZE * 1.35f;

	m->col_label_w = max_label;
	m->col_accel_w = max_accel;

	float trailing = 0.0f;
	if (m->has_accels) trailing += FLUX_MENU_ITEM_ACCEL_GAP + max_accel;
	if (m->has_submenus) trailing += FLUX_MENU_ITEM_CHEVRON_GAP + FLUX_MENU_ITEM_CHEVRON_SIZE;

	float icon_band           = m->has_icons ? FLUX_MENU_ITEM_ICON_SIZE : 0.0f;
	float icon_gap            = icon_band > 0.0f ? FLUX_MENU_ITEM_ICON_GAP : 0.0f;
	float item_inner_w        = FLUX_MENU_ITEM_PAD_LEFT
	                          + m->col_placeholder
	                          + icon_band
	                          + icon_gap
	                          + max_label
	                          + trailing
	                          + FLUX_MENU_ITEM_PAD_RIGHT;

	float presenter_content_w = item_inner_w + 2.0f * FLUX_MENU_ITEM_MARGIN_H;
	float total_w             = presenter_content_w + 2.0f * FLUX_MENU_PRESENTER_BORDER;
	if (total_w < 96.0f) total_w = 96.0f;

	float item_slot_h  = menu_item_slot_height(m, max_text_h);
	float cumulative_y = menu_assign_item_layouts(m, item_slot_h);

	float intrinsic_h
	  = FLUX_MENU_PRESENTER_PAD_TOP + cumulative_y + FLUX_MENU_PRESENTER_PAD_BOTTOM + 2.0f * FLUX_MENU_PRESENTER_BORDER;
	if (intrinsic_h < FLUX_MENU_THEME_MIN_HEIGHT) intrinsic_h = FLUX_MENU_THEME_MIN_HEIGHT;

	float total_h    = intrinsic_h;
	float max_h_work = menu_owner_max_height(m);
	if (max_h_work > 64.0f && total_h > max_h_work) total_h = max_h_work;

	menu_clamp_scroll(m, intrinsic_h, total_h);
	m->total_w = total_w;
	m->total_h = total_h;
}

int menu_hit_test(FluxMenuFlyout const *m, float px, float py) {
	if (m->item_count == 0) return -1;

	float band_top = FLUX_MENU_PRESENTER_BORDER + FLUX_MENU_PRESENTER_PAD_TOP;
	float band_bot = m->total_h - FLUX_MENU_PRESENTER_BORDER - FLUX_MENU_PRESENTER_PAD_BOTTOM;
	if (py < band_top || py >= band_bot) return -1;

	float local_x   = px - FLUX_MENU_PRESENTER_BORDER;
	float local_y   = py - band_top + m->scroll_y;
	float content_w = m->total_w - 2.0f * FLUX_MENU_PRESENTER_BORDER;

	for (int i = 0; i < m->item_count; i++) {
		StoredItem const *it = &m->items [i];
		if (local_y < it->y || local_y >= it->y + it->h) continue;
		if (it->type == FLUX_MENU_ITEM_SEPARATOR) return -1;
		float ir_left  = FLUX_MENU_ITEM_MARGIN_H;
		float ir_right = content_w - FLUX_MENU_ITEM_MARGIN_H;
		float ir_top   = it->y + FLUX_MENU_ITEM_MARGIN_V;
		float ir_bot   = it->y + it->h - FLUX_MENU_ITEM_MARGIN_V;
		if (local_x >= ir_left && local_x < ir_right && local_y >= ir_top && local_y < ir_bot) return i;
		return -1;
	}
	return -1;
}
