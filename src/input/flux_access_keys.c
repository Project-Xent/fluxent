#include "input/flux_access_keys.h"
#include "input/flux_input_internal.h"
#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_object.h"
#include "controls/draw/flux_control_record_internal.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/flux_module.h"
#include "popup/flux_menu_internal.h"
#include "render/flux_dl_replay.h"
#include "render/flux_render_internal.h"

#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static void keys_hide(FluxAccessKeys *keys) {
	keys->visible = false;
	keys->length  = 0;
}

static void keys_alt(FluxAccessKeys *keys, bool down, bool repeated) {
	if (down) {
		if (!repeated) keys->alt_pending = !flux_key_held(FLUX_KEY_CONTROL) && !flux_key_held(FLUX_KEY_SHIFT);
		return;
	}
	if (keys->alt_pending) {
		keys->visible = !keys->visible;
		keys->length  = 0;
	}
	keys->alt_pending = false;
}

static bool keys_printable_key(FluxKey key) {
	static bool const translated [FLUX_KEY_COUNT] = {
	  [FLUX_KEY_A]        = true,
	  [FLUX_KEY_C]        = true,
	  [FLUX_KEY_V]        = true,
	  [FLUX_KEY_X]        = true,
	  [FLUX_KEY_Y]        = true,
	  [FLUX_KEY_Z]        = true,
	  [FLUX_KEY_ADD]      = true,
	  [FLUX_KEY_SUBTRACT] = true,
	  [FLUX_KEY_SHIFT]    = true,
	};
	return ( unsigned int ) key < FLUX_KEY_COUNT && translated [key];
}

bool flux_access_keys_key(FluxAccessKeys *keys, FluxKey key, bool down, bool repeated) {
	if (key == FLUX_KEY_ALT) {
		keys_alt(keys, down, repeated);
		return true;
	}
	if (!down) return false;
	keys->alt_pending = false;
	if (!keys->visible) return false;
	if (keys_printable_key(key)) return key != FLUX_KEY_SHIFT;
	keys_hide(keys);
	return key == FLUX_KEY_ESCAPE;
}

static XentNodeId keys_next(XentCtx const *ctx, XentNodeId root, XentNodeId node) {
	if (!xent_collapsed(ctx, node)) {
		XentNodeId child = xent_node_first(ctx, node);
		if (child != XENT_NODE_INVALID) return child;
	}
	for (; node != root; node = xent_node_parent(ctx, node)) {
		XentNodeId sibling = xent_node_next(ctx, node);
		if (sibling != XENT_NODE_INVALID) return sibling;
	}
	return XENT_NODE_INVALID;
}

static bool        keys_character(wchar_t ch) { return ch >= 0x21 && ch != 0x7f && ch != 0x200b; }

static char const *keys_text(FluxStore *store, XentNodeId node) {
	char const *value = flux_menubar_item_access_key(store, node);
	if (!value) value = flux_menu_row_access_key(store, node);
	if (value) return value;
	XentNodeId source = flux_cmdbar_access_source(store, node);
	return flux_uia_apptext(store, source != XENT_NODE_INVALID ? source : node, FLUX_A11Y_ACCESS_KEY);
}

static int keys_parse(FluxStore *store, XentNodeId node, wchar_t *key) {
	char const *value = keys_text(store, node);
	if (!value) return 0;
	if (_strnicmp(value, "Alt+", 4) == 0) value += 4;
	int n = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, value, -1, key, FLUX_ACCESS_KEY_CAP) - 1;
	for (int i = 0; i < n; i++)
		if (!keys_character(key [i])) return 0;
	return n > 0 ? n : 0;
}

static bool keys_match(wchar_t const *key, wchar_t const *prefix, int n) {
	return n == 0 || CompareStringOrdinal(key, n, prefix, n, TRUE) == CSTR_EQUAL;
}

static bool keys_eligible(FluxUiaContext const *info, XentNodeId node, XentRect *seen) {
	if (!input_focus_available(info->ctx, node)) return false;
	if (!xent_get_focusable(info->ctx, node) && flux_cmdbar_access_source(info->store, node) == XENT_NODE_INVALID)
		return false;
	XentRect visual;
	return flux_uia_place(info, node, -1, &visual, seen) && seen->w > 0.0f && seen->h > 0.0f;
}

static XentNodeId keys_root(FluxUiaContext const *info, FluxInput const *input) {
	return input->modal_root != XENT_NODE_INVALID && input_within(info->ctx, input->modal_root, info->root)
	       ? input->modal_root
	       : info->root;
}

static int
keys_candidate(FluxUiaContext const *info, FluxAccessKeys const *keys, XentNodeId node, wchar_t *key, XentRect *seen) {
	int n = keys_parse(info->store, node, key);
	if (!n || n < keys->length || !keys_match(key, keys->prefix, keys->length)) return 0;
	return keys_eligible(info, node, seen) ? n : 0;
}

static XentNodeId keys_find(FluxAccessKeys const *keys, FluxUiaContext const *info, XentNodeId root, bool *partial) {
	for (XentNodeId node = root; node != XENT_NODE_INVALID; node = keys_next(info->ctx, root, node)) {
		wchar_t  key [FLUX_ACCESS_KEY_CAP];
		XentRect seen;
		int      n = keys_candidate(info, keys, node, key, &seen);
		if (n == keys->length) return node;
		*partial |= n > 0;
	}
	return XENT_NODE_INVALID;
}

void flux_access_keys_show(FluxAccessKeys *keys, FluxUiaContext const *info, FluxInput const *input) {
	keys_hide(keys);
	keys->surface   = info->hwnd;
	XentNodeId root = keys_root(info, input);
	for (XentNodeId node = root; node != XENT_NODE_INVALID; node = keys_next(info->ctx, root, node)) {
		wchar_t  key [FLUX_ACCESS_KEY_CAP];
		XentRect seen;
		if (!keys_candidate(info, keys, node, key, &seen)) continue;
		keys->visible = true;
		return;
	}
}

static void keys_invoke(FluxInput *input, XentNodeId node) {
	if (flux_menu_row_access_invoke(input->store, node)) return;
	if (flux_cmdbar_access_source(input->store, node) != XENT_NODE_INVALID) {
		FluxNodeData *row = flux_store_find(input->store, node);
		row->behavior.on_click(row->behavior.on_click_ctx);
		return;
	}
	flux_input_setfocus(input, node, false);
	FluxControlType type = flux_node_type(input->store, node);
	if (type == FLUX_CONTROL_MENU_BAR_ITEM && flux_input_keydown(input, FLUX_KEY_ENTER)) return;
	FluxNodeData       *nd    = flux_store_find(input->store, node);
	FluxUiaHooks const *hooks = flux_module_uia(input->store, node);
	if (nd && (nd->behavior.on_click || (hooks && hooks->invoked))) flux_input_activate(input);
}

bool flux_access_keys_char(FluxAccessKeys *keys, FluxUiaContext const *info, FluxInput *input, wchar_t ch, bool alt) {
	if (alt) keys->alt_pending = false;
	if ((!alt && !keys->visible) || !keys_character(ch)) return false;
	if (alt) keys_hide(keys);
	if (keys->length >= FLUX_ACCESS_KEY_CAP - 1) return !alt;
	keys->prefix [keys->length++] = ch;
	bool       partial            = false;
	XentNodeId node               = keys_find(keys, info, keys_root(info, input), &partial);
	if (node != XENT_NODE_INVALID) {
		keys_hide(keys);
		keys_invoke(input, node);
		return true;
	}
	if (alt || !partial) keys->length--;
	return !alt;
}

static bool tip_first(FluxUiaContext const *info, XentNodeId root, XentNodeId node, wchar_t const *key, int length) {
	for (XentNodeId earlier = root; earlier != node; earlier = keys_next(info->ctx, root, earlier)) {
		wchar_t  previous [FLUX_ACCESS_KEY_CAP];
		XentRect seen;
		if (keys_parse(info->store, earlier, previous) != length) continue;
		if (keys_match(previous, key, length) && keys_eligible(info, earlier, &seen)) return false;
	}
	return true;
}

typedef enum TipPlacement
{
	TIP_HIDDEN,
	TIP_BOTTOM,
	TIP_TOP,
	TIP_LEFT,
	TIP_RIGHT,
	TIP_CENTER,
	TIP_PLACEMENT_COUNT,
} TipPlacement;

typedef struct AccessTip {
	FluxRect     anchor;
	FluxRect     tip;
	FluxSize     size;
	TipPlacement placement;
	char         label [3 * FLUX_ACCESS_KEY_CAP];
	bool         rtl;
} AccessTip;

typedef struct AccessTips {
	AccessTip *items;
	size_t     count;
	size_t     capacity;
} AccessTips;

static bool tip_append(AccessTips *tips, AccessTip const *item) {
	if (tips->count == tips->capacity) {
		size_t capacity = tips->capacity ? tips->capacity * 2u : 32u;
		if (capacity > SIZE_MAX / sizeof(*tips->items)) return false;
		AccessTip *items = realloc(tips->items, capacity * sizeof(*items));
		if (!items) return false;
		tips->items    = items;
		tips->capacity = capacity;
	}
	tips->items [tips->count++] = *item;
	return true;
}

static void
tip_label(AccessTip *item, FluxAccessKeys const *keys, FluxUiaContext const *info, XentNodeId root, XentNodeId node) {
	wchar_t key [FLUX_ACCESS_KEY_CAP];
	int     n = keys_parse(info->store, node, key);
	if (!n || n < keys->length || !keys_match(key, keys->prefix, keys->length)) return;
	if (tip_first(info, root, node, key, n))
		WideCharToMultiByte(CP_UTF8, 0, key, -1, item->label, sizeof(item->label), NULL, NULL);
}

static bool tip_collect(AccessTips *tips, FluxAccessKeys const *keys, FluxUiaContext const *info, XentNodeId root) {
	for (XentNodeId node = root; node != XENT_NODE_INVALID; node = keys_next(info->ctx, root, node)) {
		XentRect seen;
		if (!keys_eligible(info, node, &seen)) continue;
		AccessTip item = {
		  .anchor = {seen.x, seen.y, seen.w, seen.h},
		  .rtl    = xent_resolved_dir(info->ctx, node) == XENT_DIRECTION_RTL,
		};
		tip_label(&item, keys, info, root, node);
		if (!tip_append(tips, &item)) return false;
	}
	return true;
}

static bool tip_intersects(FluxRect a, FluxRect b) {
	return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h;
}

static int tip_cost(AccessTips const *tips, size_t current, FluxRect box) {
	int cost = 0;
	for (size_t i = 0; i < tips->count; i++) {
		AccessTip const *item = &tips->items [i];
		if (i != current && item->tip.w > 0.0f && tip_intersects(box, item->tip)) return INT_MAX;
		if (!tip_intersects(box, item->anchor)) continue;
		cost += i == current ? 3 : item->label [0] ? 10 : 1;
	}
	return cost;
}

static float tip_axis_gap(float a, float aw, float b, float bw) { return fmaxf(a - b - bw, b - a - aw); }

static bool  tip_neighbor_counts(
  AccessTips const *tips, size_t current, int horizontal [TIP_PLACEMENT_COUNT], int vertical [TIP_PLACEMENT_COUNT]
) {
	FluxRect anchor          = tips->items [current].anchor;
	bool     adjacent_column = false;
	for (size_t i = 0; i < tips->count; i++) {
		AccessTip const *other = &tips->items [i];
		if (i == current || other->placement == TIP_HIDDEN) continue;
		float x = tip_axis_gap(anchor.x, anchor.w, other->anchor.x, other->anchor.w);
		float y = tip_axis_gap(anchor.y, anchor.h, other->anchor.y, other->anchor.h);
		if (x < 50.0f && y < 0.0f) horizontal [other->placement]++;
		if (y < 50.0f && x < 0.0f) {
			vertical [other->placement]++;
			adjacent_column = true;
		}
	}
	return adjacent_column;
}

static bool tip_side(TipPlacement p) { return p == TIP_LEFT || p == TIP_RIGHT; }

static bool tip_prefer(AccessTips const *tips, size_t index, TipPlacement before, TipPlacement next) {
	int  horizontal [TIP_PLACEMENT_COUNT] = {0}, vertical [TIP_PLACEMENT_COUNT] = {0};
	bool adjacent_column = tip_neighbor_counts(tips, index, horizontal, vertical);
	if (horizontal [before] || horizontal [next]) return horizontal [next] > horizontal [before];
	if (vertical [before] || vertical [next]) return vertical [next] > vertical [before];
	return adjacent_column && tip_side(next) && !tip_side(before);
}

static bool tip_place(AccessTips *tips, size_t index, FluxSize view) {
	AccessTip *item = &tips->items [index];
	FluxSize   size = item->size;
	FluxRect   a    = item->anchor;
	bool       rtl  = item->rtl;
	float      cx = a.x + (a.w - size.w) * 0.5f, cy = a.y + (a.h - size.h) * 0.5f;
	float      left = a.x - size.w, right = a.x + a.w;
	FluxPoint  positions [TIP_PLACEMENT_COUNT] = {
	  [TIP_BOTTOM] = {cx,                 a.y + a.h   },
	  [TIP_TOP]    = {cx,                 a.y - size.h},
	  [TIP_LEFT]   = {rtl ? right : left, cy          },
	  [TIP_RIGHT]  = {rtl ? left : right, cy          },
	  [TIP_CENTER] = {cx,                 cy          },
	};
	FluxRect     best      = {0};
	int          best_cost = INT_MAX;
	TipPlacement placement = TIP_HIDDEN;
	for (TipPlacement i = TIP_BOTTOM; i < TIP_PLACEMENT_COUNT; i++) {
		FluxRect box = {
		  fmaxf(0.0f, fminf(positions [i].x, view.w - size.w)),
		  fmaxf(0.0f, fminf(positions [i].y, view.h - size.h)),
		  size.w,
		  size.h,
		};
		int cost = tip_cost(tips, index, box);
		if (cost == INT_MAX || cost > best_cost) continue;
		if (cost == best_cost && !tip_prefer(tips, index, placement, i)) continue;
		best      = box;
		best_cost = cost;
		placement = i;
	}
	bool changed    = item->placement != placement;
	item->tip       = best;
	item->placement = placement;
	return changed;
}

static bool tip_record(XentDlBuilder *b, AccessTip const *item) {
	FluxRect      tip   = item->tip;
	FluxRect      text  = {tip.x + 5.0f, tip.y + 5.0f, tip.w - 10.0f, tip.h - 10.0f};
	XentTextStyle style = {.font_size = 12.0f, .font_weight = FLUX_FONT_REGULAR};
	return flux_rec_fill_rect(b, &tip, (FluxColor) {0x767676ff})
	    && flux_rec_text_ex(b, &text, item->label, &style, (FluxColor) {0xffffffff});
}

static void tips_measure(AccessTips *tips, FluxRenderCtx const *rc, FluxSize viewport) {
	FluxTextStyle style = {.font_size = 12.0f, .font_weight = FLUX_FONT_REGULAR};
	for (size_t i = 0; i < tips->count; i++) {
		AccessTip *item = &tips->items [i];
		if (!item->label [0]) continue;
		FluxSize text = flux_text_measure(rc->text, item->label, &style, viewport.w);
		FluxSize size = {ceilf(text.w) + 10.0f, ceilf(text.h) + 10.0f};
		if (size.w <= viewport.w && size.h <= viewport.h) item->size = size;
	}
}

static bool tips_place_pass(AccessTips *tips, FluxSize viewport) {
	bool changed = false;
	for (size_t i = 0; i < tips->count; i++)
		if (tips->items [i].size.w > 0.0f) changed |= tip_place(tips, i, viewport);
	return changed;
}

static bool tips_place(XentDlBuilder *b, AccessTips *tips, FluxRenderCtx const *rc, FluxSize viewport) {
	tips_measure(tips, rc, viewport);
	for (int pass = 0; pass < 15 && tips_place_pass(tips, viewport); pass++) { }
	for (size_t i = 0; i < tips->count; i++)
		if (tips->items [i].tip.w > 0.0f && !tip_record(b, &tips->items [i])) return false;
	return true;
}

static bool tips_record(
  XentDlBuilder *b, FluxAccessKeys const *keys, FluxUiaContext const *info, XentNodeId root, FluxRenderCtx const *rc
) {
	RECT client = {0};
	if (!GetClientRect(info->hwnd, &client)) return true;
	float      scale    = ( float ) GetDpiForWindow(info->hwnd) / 96.0f;
	FluxSize   viewport = {( float ) client.right / scale, ( float ) client.bottom / scale};
	AccessTips tips     = {0};
	bool       ok       = tip_collect(&tips, keys, info, root) && tips_place(b, &tips, rc, viewport);
	free(tips.items);
	return ok;
}

void flux_access_keys_paint(
  FluxAccessKeys const *keys, FluxUiaContext const *info, FluxInput const *input, FluxRenderCtx const *rc
) {
	if (!keys->visible || keys->surface != info->hwnd || !rc->text) return;
	XentDlBuilder *b = xent_dl_begin();
	if (!b) return;
	XentDl *list = flux_rec_finish(b, tips_record(b, keys, info, keys_root(info, input), rc));
	if (!list) return;
	( void ) flux_xtk_replay_rc(rc, list);
	xent_dl_free(list);
}
