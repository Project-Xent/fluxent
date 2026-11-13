#include "controls/behavior/flux_repeat_timer.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "runtime/flux_anim_driver.h"
#include "store/flux_node_data.h"
#include "render/flux_anim.h"
#include "render/flux_looping_geom.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_window.h"
#include "bridge/flux_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

#define LOOP_REPEAT_DELAY_MS    500u
#define LOOP_REPEAT_INTERVAL_MS 33u
#define LOOP_SCROLL_ANIM_MS     250.0f
#define LOOP_WHEEL_IDLE_MS      120u

#define LOOP_SNAP_EPSILON       0.5f

typedef struct FluxLoopingRuntime {
	FluxLoopingData base;

	float           last_offset;
	float           wheel_remainder;
	bool            wheel_free;
	DWORD           wheel_last;
	float           anim_from;
	float           anim_to;
	DWORD           anim_start;

	FluxRepeatTimer repeat;
	int             repeat_step;
} FluxLoopingRuntime;

static FluxLoopingRuntime *looping_runtime(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_LOOPING_SELECTOR || !nd->component_data) return NULL;
	return ( FluxLoopingRuntime * ) nd->component_data;
}

static FluxLoopingItemData *looping_cell(FluxStore *store, XentNodeId id) {
	FluxNodeData *nd = flux_store_find(store, id);
	if (!nd || nd->component_type != FLUX_CONTROL_LOOPING_ITEM || !nd->component_data) return NULL;
	return ( FluxLoopingItemData * ) nd->component_data;
}

static FluxLoopingRuntime *looping_owner(FluxLoopingData *ld) { return ( FluxLoopingRuntime * ) ld; }

static FluxScrollData     *looping_scroll_data(FluxLoopingData const *ld) {
	FluxNodeData *nd = flux_store_find(ld->store, ld->scroll);
	return nd ? ( FluxScrollData * ) nd->component_data : NULL;
}

static FluxLoopingMetrics looping_metrics(FluxLoopingData const *ld) {
	int slots = ld->should_loop ? FLUX_LOOPING_PANEL_SLOTS : (ld->item_count > 0 ? ld->item_count : 1);
	return (FluxLoopingMetrics) {ld->item_height, ld->viewport_h, slots};
}

static void looping_free_items(FluxLoopingData *ld) {
	for (int i = 0; i < ld->item_count; i++) free(ld->items [i]);
	free(ld->items);
	ld->items      = NULL;
	ld->item_count = 0;
}

static char *looping_dup(char const *text) {
	size_t n   = strlen(text) + 1u;
	char  *out = ( char * ) malloc(n);
	if (out) memcpy(out, text, n);
	return out;
}

static void looping_free_partial(char **copy, int filled) {
	while (filled-- > 0) free(copy [filled]);
	free(copy);
}

static char **looping_dup_items(char const *const *items, int count) {
	char **copy = ( char ** ) calloc(( size_t ) count, sizeof(*copy));
	if (!copy) return NULL;
	for (int i = 0; i < count; i++) {
		copy [i] = looping_dup(items [i] ? items [i] : "");
		if (copy [i]) continue;
		looping_free_partial(copy, i);
		return NULL;
	}
	return copy;
}

static void looping_apply_extent(FluxLoopingData *ld) {
	FluxScrollData *sd = looping_scroll_data(ld);
	if (!sd) return;
	FluxLoopingMetrics m = looping_metrics(ld);
	sd->content_manual   = 1;
	sd->virtualized      = 1;
	sd->content_w        = 0.0f;
	sd->content_h        = flux_looping_content_h(&m);
	sd->snap_y           = ld->item_height;
}

static void looping_drop_cells(FluxLoopingData *ld) {
	for (int i = 0; i < ld->cell_count; i++)
		if (ld->cells [i] != XENT_NODE_INVALID) xent_node_destroy(ld->ctx, ld->cells [i]);
	free(ld->cells);
	ld->cells      = NULL;
	ld->cell_count = 0;
}

static bool looping_build_cells(FluxLoopingData *ld, int want) {
	looping_drop_cells(ld);
	if (want <= 0) return false;

	ld->cells = ( XentNodeId * ) calloc(( size_t ) want, sizeof(*ld->cells));
	if (!ld->cells) return false;
	for (int i = 0; i < want; i++) {
		ld->cells [i] = flux_looping_item_init(&(FluxLoopingItemDesc) {ld->ctx, ld->store, ld->host});
		if (ld->cells [i] == XENT_NODE_INVALID) {
			ld->cell_count = i;
			looping_drop_cells(ld);
			return false;
		}
	}
	ld->cell_count = want;
	return true;
}

static void looping_place_cells(FluxLoopingData *ld, float offset) {
	if (ld->cell_count <= 0 || ld->item_count <= 0) return;

	FluxLoopingMetrics m      = looping_metrics(ld);
	int                first  = flux_looping_first_visible(&m, offset);
	float              origin = flux_looping_item_y(&m, first);
	float              width  = ld->item_width;

	ld->window_first          = first;
	xent_seth(ld->ctx, ld->host, ( float ) ld->cell_count * ld->item_height);

	FluxScrollData *sd = looping_scroll_data(ld);
	if (sd) sd->origin_y = origin;

	for (int i = 0; i < ld->cell_count; i++) {
		int   visual   = first + i;
		bool  in_panel = visual >= 0 && visual < m.slots;
		float band_y   = (ld->viewport_h - ld->item_height) * 0.5f - (flux_looping_item_y(&m, visual) - offset);
		flux_looping_item_place(
		  ld->store, ld->cells [i], in_panel ? visual : -1, ( float ) i * ld->item_height, width,
		  !ld->scrolling && visual == ld->midpoint, band_y
		);
	}
}

static void looping_report(FluxLoopingData *ld, int index) {
	if (index == ld->selected) return;
	ld->selected = index;
	if (ld->on_select) ld->on_select(ld->on_select_ctx, index);
	else
		flux_input_route_event(
		  flux_store_host(ld->store), ld->root, XTK_EVENT_SELECT,
		  (XtkEventPayload) {.kind = XTK_EVENT_PAYLOAD_I32, .u.i32 = index}, XTK_EVENT_CAUSE_USER
		);
}

static void looping_settle(FluxLoopingData *ld, float offset) {
	FluxLoopingMetrics m = looping_metrics(ld);
	ld->midpoint         = flux_looping_visual_at(&m, offset);
	ld->scrolling        = false;
	if (ld->item_count > 0) looping_report(ld, flux_looping_mod(ld->midpoint, ld->item_count));
}

static void looping_normalize(FluxLoopingData *ld, FluxScrollData *sd) {
	if (!ld->should_loop || ld->item_count <= 0) return;

	FluxLoopingMetrics m      = looping_metrics(ld);
	int                middle = m.slots / 2;
	int                visual = flux_looping_visual_at(&m, sd->scroll_y);
	int                laps   = ( int ) lroundf(( float ) (middle - visual) / ( float ) ld->item_count);
	if (laps == 0) return;

	sd->scroll_y += ( float ) (laps * ld->item_count) * ld->item_height;
}

static float looping_offset_limit(FluxLoopingData const *ld) {
	FluxLoopingMetrics m = looping_metrics(ld);
	return ( float ) (m.slots - 1) * ld->item_height;
}

static void looping_jump_to(FluxLoopingRuntime *rt, float offset) {
	FluxScrollData *sd = looping_scroll_data(&rt->base);
	if (!sd) return;
	float limit     = looping_offset_limit(&rt->base);
	rt->anim_start  = 0;
	sd->scroll_y    = offset < 0.0f ? 0.0f : (offset > limit ? limit : offset);
	rt->last_offset = sd->scroll_y;
	looping_settle(&rt->base, sd->scroll_y);
	looping_place_cells(&rt->base, sd->scroll_y);
}

static bool looping_anim_step(void *ctx, unsigned long now) {
	FluxLoopingRuntime *rt = ( FluxLoopingRuntime * ) ctx;
	FluxScrollData     *sd = looping_scroll_data(&rt->base);
	if (!rt->anim_start || !sd) {
		rt->anim_start = 0;
		return false;
	}
	float t = ( float ) (( DWORD ) now - rt->anim_start) / LOOP_SCROLL_ANIM_MS;
	if (t >= 1.0f) {
		rt->anim_start = 0;
		sd->scroll_y   = rt->anim_to;
		return false;
	}
	sd->scroll_y = rt->anim_from + (rt->anim_to - rt->anim_from) * flux_ease_out_cubic(t);
	return true;
}

static void looping_scroll_to(FluxLoopingRuntime *rt, float offset, bool animate) {
	FluxScrollData *sd = looping_scroll_data(&rt->base);
	if (!sd) return;
	rt->wheel_free = false;
	float item     = rt->base.item_height;
	offset         = floorf(offset / item + 0.5f) * item;
	float limit    = looping_offset_limit(&rt->base);
	float target   = offset < 0.0f ? 0.0f : (offset > limit ? limit : offset);
	if (!animate) {
		looping_jump_to(rt, target);
		return;
	}
	if (target == sd->scroll_y) return;
	rt->anim_from  = sd->scroll_y;
	rt->anim_to    = target;
	rt->anim_start = flux_anim_now();
	flux_anim_register(rt, looping_anim_step);
}

static void looping_step(FluxLoopingRuntime *rt, int delta) {
	FluxLoopingData *ld = &rt->base;
	if (ld->item_count <= 0) return;
	int index = ld->selected + delta;
	if (!ld->should_loop && (index < 0 || index >= ld->item_count)) return;

	FluxScrollData *sd = looping_scroll_data(ld);
	if (!sd) return;
	float from = rt->anim_start ? rt->anim_to : sd->scroll_y;
	looping_scroll_to(rt, from + ( float ) delta * ld->item_height, true);
}

static bool looping_wheel_step(void *ctx, unsigned long now) {
	FluxLoopingRuntime *rt = ( FluxLoopingRuntime * ) ctx;
	FluxScrollData     *sd = looping_scroll_data(&rt->base);
	if (!sd || !rt->wheel_free) return false;
	if (( DWORD ) now - rt->wheel_last < LOOP_WHEEL_IDLE_MS) return true;
	float item     = rt->base.item_height;
	float limit    = looping_offset_limit(&rt->base);
	float target   = floorf(sd->scroll_y / item + 0.5f) * item;
	rt->wheel_free = false;
	rt->anim_from  = sd->scroll_y;
	rt->anim_to    = target < 0.0f ? 0.0f : (target > limit ? limit : target);
	rt->anim_start = now ? ( DWORD ) now : 1u;
	flux_anim_register(rt, looping_anim_step);
	return true;
}

static void looping_wheel_pan(FluxLoopingRuntime *rt, FluxScrollData *sd, float delta) {
	float limit         = looping_offset_limit(&rt->base);
	float next          = sd->scroll_y + delta * rt->base.item_height;
	sd->scroll_y        = next < 0.0f ? 0.0f : (next > limit ? limit : next);
	rt->anim_start      = 0;
	rt->wheel_remainder = 0.0f;
	rt->wheel_free      = true;
	rt->wheel_last      = flux_anim_now();
	flux_anim_register(rt, looping_wheel_step);
}

bool flux_looping_handle_wheel(FluxStore *store, XentNodeId id, float delta) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt || !rt->base.enabled || rt->base.item_count <= 0 || delta == 0.0f) return false;

	FluxScrollData *sd = looping_scroll_data(&rt->base);
	if (!sd) return false;
	if (rt->wheel_free || fabsf(delta - roundf(delta)) > 0.001f) {
		looping_wheel_pan(rt, sd, delta);
		return true;
	}
	rt->wheel_remainder += delta;
	float steps          = truncf(rt->wheel_remainder);
	rt->wheel_remainder -= steps;
	if (steps != 0.0f) {
		float from = rt->anim_start ? rt->anim_to : sd->scroll_y;
		looping_scroll_to(rt, from + steps * rt->base.item_height, true);
	}
	return true;
}

static void looping_page(FluxLoopingRuntime *rt, int sign) {
	FluxScrollData *sd = looping_scroll_data(&rt->base);
	if (!sd) return;
	float from = rt->anim_start ? rt->anim_to : sd->scroll_y;
	looping_scroll_to(rt, from + ( float ) sign * rt->base.viewport_h * 0.5f, true);
}

static void looping_go_to_index(FluxLoopingRuntime *rt, int index, bool animate) {
	FluxLoopingData *ld = &rt->base;
	if (ld->item_count <= 0) return;
	FluxLoopingMetrics m      = looping_metrics(ld);
	int                middle = ld->should_loop ? m.slots / 2 : 0;
	int                visual = ld->should_loop ? middle - flux_looping_mod(middle - index, ld->item_count) : index;
	looping_scroll_to(rt, ( float ) visual * ld->item_height, animate);
}

static void looping_repeat_tick(void *ctx) {
	FluxLoopingRuntime *rt = ( FluxLoopingRuntime * ) ctx;
	looping_step(rt, rt->repeat_step);
}

static void looping_release(FluxLoopingRuntime *rt) {
	flux_repeat_timer_stop(&rt->repeat);
	rt->base.up_pressed   = false;
	rt->base.down_pressed = false;
}

static bool looping_bounds(FluxLoopingData const *ld, FluxRect *out) {
	XentRect r = {0};
	if (!xent_layout_rect(ld->ctx, ld->root, &r)) return false;
	*out = (FluxRect) {0.0f, 0.0f, r.w, r.h};
	return true;
}

static bool looping_buttons_shown(FluxLoopingData const *ld) {
	return ld->enabled && ld->pointer_inside && !ld->pointer_touch;
}

static bool looping_chrome_hit(void *ctx, float x, float y) {
	FluxLoopingRuntime *rt  = ( FluxLoopingRuntime * ) ctx;
	FluxRect            bar = {0};
	if (!looping_buttons_shown(&rt->base) || !looping_bounds(&rt->base, &bar)) return false;
	FluxRect up = flux_looping_up_rect(&bar), down = flux_looping_down_rect(&bar);
	return flux_rect_contains(&up, x, y) || flux_rect_contains(&down, x, y);
}

static void looping_track_pointer(FluxLoopingData *ld, XentNodeId node, bool inside) {
	FluxNodeData *nd   = flux_store_find(ld->store, node);
	ld->pointer_inside = inside;
	ld->pointer_touch  = nd && nd->state.pointer_type == FLUX_POINTER_TOUCH;
	if (!inside) looping_release(looping_owner(ld));
}

static void looping_on_hover(void *ctx, bool hovered) {
	FluxLoopingRuntime *rt = ( FluxLoopingRuntime * ) ctx;
	looping_track_pointer(&rt->base, rt->base.root, hovered);
}

static void looping_cell_hover(void *ctx, bool hovered) {
	FluxLoopingItemData *cell = ( FluxLoopingItemData * ) ctx;
	if (cell && cell->owner) looping_track_pointer(cell->owner, cell->node, hovered);
}

static void looping_on_pointer_down(void *ctx, float x, float y, int click_count) {
	( void ) click_count;
	FluxLoopingRuntime *rt  = ( FluxLoopingRuntime * ) ctx;
	FluxRect            bar = {0};
	if (!rt->base.enabled || !looping_bounds(&rt->base, &bar)) return;

	FluxRect up = flux_looping_up_rect(&bar), down = flux_looping_down_rect(&bar);
	if (flux_rect_contains(&up, x, y)) rt->base.up_pressed = true;
	else if (flux_rect_contains(&down, x, y)) rt->base.down_pressed = true;
	else return;

	rt->repeat_step = rt->base.up_pressed ? -1 : 1;
	looping_step(rt, rt->repeat_step);
	flux_repeat_timer_start(&rt->repeat, LOOP_REPEAT_DELAY_MS, LOOP_REPEAT_INTERVAL_MS, looping_repeat_tick, rt);
}

static void looping_on_click(void *ctx) { looping_release(( FluxLoopingRuntime * ) ctx); }

static void looping_on_cancel(void *ctx) { looping_release(( FluxLoopingRuntime * ) ctx); }

static bool looping_on_key(void *ctx, FluxKey key, bool down) {
	FluxLoopingRuntime *rt = ( FluxLoopingRuntime * ) ctx;
	if (!down || !rt->base.enabled || flux_key_held(FLUX_KEY_ALT)) return false;
	switch (key) {
	case FLUX_KEY_UP        : looping_step(rt, -1); return true;
	case FLUX_KEY_DOWN      : looping_step(rt, 1); return true;
	case FLUX_KEY_PAGE_UP   : looping_page(rt, -1); return true;
	case FLUX_KEY_PAGE_DOWN : looping_page(rt, 1); return true;
	case FLUX_KEY_HOME      : looping_go_to_index(rt, 0, false); return true;
	case FLUX_KEY_END       : looping_go_to_index(rt, rt->base.item_count - 1, false); return true;
	default                 : return false;
	}
}

static void looping_item_click(void *ctx) {
	FluxLoopingItemData *cell = ( FluxLoopingItemData * ) ctx;
	if (!cell || !cell->owner || cell->visual < 0) return;
	FluxLoopingRuntime *rt = looping_owner(cell->owner);
	if (rt->base.scrolling || cell->visual == rt->base.midpoint) return;
	looping_scroll_to(rt, ( float ) cell->visual * rt->base.item_height, true);
}

static bool looping_resize(FluxLoopingData *ld, float viewport_h) {
	FluxLoopingMetrics probe = {ld->item_height, viewport_h, 1};
	int                want  = flux_looping_window_slots(&probe);
	ld->viewport_h           = viewport_h;
	looping_apply_extent(ld);
	if (want == ld->cell_count) return true;
	return looping_build_cells(ld, want);
}

static bool looping_off_grid(float offset, float item) {
	if (item <= 0.0f) return false;
	float rest = offset / item;
	return fabsf(rest - floorf(rest + 0.5f)) * item > LOOP_SNAP_EPSILON;
}

static bool looping_sync_scroll(FluxLoopingRuntime *rt, FluxScrollData *sd) {
	FluxLoopingData *ld     = &rt->base;

	bool             redraw = rt->anim_start || sd->scroll_y != rt->last_offset;
	ld->scrolling           = redraw || looping_off_grid(sd->scroll_y, ld->item_height);
	if (!ld->scrolling) {
		looping_normalize(ld, sd);
		looping_settle(ld, sd->scroll_y);
	}
	looping_place_cells(ld, sd->scroll_y);
	rt->last_offset = sd->scroll_y;
	return redraw;
}

bool flux_looping_sync(XentNodeId node, FluxNodeData *nd) {
	FluxLoopingRuntime *rt = nd ? ( FluxLoopingRuntime * ) nd->component_data : NULL;
	FluxLoopingData    *ld = rt ? &rt->base : NULL;
	if (!ld || ld->root != node) return false;

	XentRect vp = {0};
	if (!xent_layout_rect(ld->ctx, ld->scroll, &vp) || vp.h <= 0.0f) return false;
	if (vp.h != ld->viewport_h && !looping_resize(ld, vp.h)) return false;

	FluxScrollData *sd = looping_scroll_data(ld);
	return sd && looping_sync_scroll(rt, sd);
}

static void looping_destroy(void *component_data) {
	FluxLoopingRuntime *rt = ( FluxLoopingRuntime * ) component_data;
	if (!rt) return;
	flux_repeat_timer_stop(&rt->repeat);
	flux_anim_unregister(rt);
	looping_free_items(&rt->base);
	free(rt->base.cells);
	free(rt);
}

static bool looping_build_spine(FluxLoopingData *ld, XentNodeId root) {
	ld->scroll = flux_scroll_init(&(FluxContainerDesc) {ld->ctx, ld->store, root});
	if (ld->scroll == XENT_NODE_INVALID) return false;
	ld->host = flux_factory_create_node(ld->ctx, ld->store, ld->scroll, FLUX_CONTROL_CONTAINER);
	if (ld->host == XENT_NODE_INVALID) return false;

	xent_setproto(ld->ctx, root, XENT_PROTOCOL_FLEX);
	xent_setflexdir(ld->ctx, root, XENT_FLEX_COLUMN);
	xent_setitems(ld->ctx, root, XENT_FLEX_ALIGN_STRETCH);
	xent_setgrow(ld->ctx, ld->scroll, 1.0f);
	xent_setbasis(ld->ctx, ld->scroll, 0.0f);
	xent_setproto(ld->ctx, ld->host, XENT_PROTOCOL_ABSOLUTE);
	xent_setwpct(ld->ctx, ld->host, 1.0f);
	xent_seth(ld->ctx, ld->host, 0.0f);
	xent_setshrink(ld->ctx, ld->host, 0.0f);

	FluxScrollData *sd = looping_scroll_data(ld);
	if (sd) {
		sd->v_vis = FLUX_SCROLL_NEVER;
		sd->h_vis = FLUX_SCROLL_NEVER;
	}
	return true;
}

XentNodeId flux_looping_init(FluxLoopingDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId root = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_LOOPING_SELECTOR);
	if (root == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData       *nd = flux_store_find(info->store, root);
	FluxLoopingRuntime *rt = nd ? ( FluxLoopingRuntime * ) calloc(1, sizeof(FluxLoopingRuntime)) : NULL;
	if (!nd || !rt) {
		free(rt);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	FluxLoopingData *ld = &rt->base;
	ld->ctx             = info->ctx;
	ld->store           = info->store;
	ld->input           = info->input;
	ld->root            = root;
	ld->item_height     = info->item_height > 0.0f ? info->item_height : FLUX_LOOPING_ITEM_HEIGHT;
	ld->item_width      = info->item_width;
	ld->should_loop     = info->should_loop;
	ld->accent          = info->accent;
	ld->left            = info->left;
	ld->enabled         = true;
	ld->selected        = -1;
	ld->midpoint        = -1;
	ld->on_select       = info->on_select;
	ld->on_select_ctx   = info->userdata;

	if (!looping_build_spine(ld, root)) {
		free(rt);
		return flux_factory_fail_node(info->ctx, info->store, root);
	}

	nd->component_data                = rt;
	nd->destroy_component_data        = looping_destroy;
	nd->behavior.on_pointer_down      = looping_on_pointer_down;
	nd->behavior.on_pointer_down_ctx  = rt;
	nd->behavior.on_click             = looping_on_click;
	nd->behavior.on_click_ctx         = rt;
	nd->behavior.on_cancel            = looping_on_cancel;
	nd->behavior.on_cancel_ctx        = rt;
	nd->behavior.on_key               = looping_on_key;
	nd->behavior.on_key_ctx           = rt;
	nd->behavior.on_hover_changed     = looping_on_hover;
	nd->behavior.on_hover_changed_ctx = rt;
	nd->behavior.chrome_hit           = looping_chrome_hit;
	nd->behavior.chrome_hit_ctx       = rt;

	xent_set_focusable(info->ctx, root, true);
	xent_sem_setrole(info->ctx, root, XENT_SEM_ROLE_CONTAINER);

	if (info->items && info->item_count > 0) flux_looping_setitems(info->store, root, info->items, info->item_count);
	if (info->selected >= 0) flux_looping_setselected(info->store, root, info->selected);
	return root;
}

void flux_looping_retarget(XentCtx *ctx, FluxStore *store, XentNodeId id) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt) return;
	rt->base.ctx   = ctx;
	rt->base.store = store;
	rt->base.root  = id;
}

void flux_looping_setitems(FluxStore *store, XentNodeId id, char const *const *items, int count) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt) return;
	char **copy = (items && count > 0) ? looping_dup_items(items, count) : NULL;
	if (count > 0 && !copy) return;

	int previous = rt->base.selected;
	looping_free_items(&rt->base);
	rt->base.items      = copy;
	rt->base.item_count = copy ? count : 0;
	rt->base.selected   = -1;
	looping_apply_extent(&rt->base);
	if (previous >= 0) flux_looping_setselected(store, id, previous < count ? previous : count - 1);
}

void flux_looping_setselected(FluxStore *store, XentNodeId id, int index) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt || rt->base.item_count <= 0) return;
	if (index < 0) index = 0;
	if (index >= rt->base.item_count) index = rt->base.item_count - 1;
	if (index == rt->base.selected) return;
	rt->base.selected = index;
	looping_go_to_index(rt, index, false);
}

void flux_looping_setitemheight(FluxStore *store, XentNodeId id, float height) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt || height <= 0.0f || height == rt->base.item_height) return;
	rt->base.item_height = height;
	rt->base.viewport_h  = 0.0f;
	looping_apply_extent(&rt->base);
}

void flux_looping_setitemwidth(FluxStore *store, XentNodeId id, float width) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt || width == rt->base.item_width) return;
	rt->base.item_width = width;
}

void flux_looping_setloop(FluxStore *store, XentNodeId id, bool should_loop) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt || should_loop == rt->base.should_loop) return;
	rt->base.should_loop = should_loop;
	looping_apply_extent(&rt->base);
	if (rt->base.selected >= 0) looping_go_to_index(rt, rt->base.selected, false);
}

void flux_looping_setenabled(FluxStore *store, XentNodeId id, bool enabled) {
	FluxLoopingRuntime *rt = looping_runtime(store, id);
	if (!rt) return;
	rt->base.enabled = enabled;
	if (!enabled) looping_release(rt);
}

XentNodeId flux_looping_item_init(FluxLoopingItemDesc const *info) {
	if (!info || !info->ctx || !info->store) return XENT_NODE_INVALID;

	XentNodeId node = flux_factory_create_node(info->ctx, info->store, info->parent, FLUX_CONTROL_LOOPING_ITEM);
	if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

	FluxNodeData        *nd   = flux_store_find(info->store, node);
	FluxLoopingItemData *cell = nd ? ( FluxLoopingItemData * ) calloc(1, sizeof(FluxLoopingItemData)) : NULL;
	if (!nd || !cell) {
		free(cell);
		return flux_factory_fail_node(info->ctx, info->store, node);
	}

	XentNodeId          scroll = xent_node_parent(info->ctx, info->parent);
	XentNodeId          root   = scroll != XENT_NODE_INVALID ? xent_node_parent(info->ctx, scroll) : XENT_NODE_INVALID;
	FluxLoopingRuntime *rt     = root != XENT_NODE_INVALID ? looping_runtime(info->store, root) : NULL;

	cell->owner                = rt ? &rt->base : NULL;
	cell->node                 = node;
	cell->visual               = -1;
	nd->component_data         = cell;
	nd->destroy_component_data = free;
	nd->behavior.on_click      = looping_item_click;
	nd->behavior.on_click_ctx  = cell;
	nd->behavior.on_hover_changed     = looping_cell_hover;
	nd->behavior.on_hover_changed_ctx = cell;

	xent_set_focusable(info->ctx, node, false);
	xent_sem_setrole(info->ctx, node, XENT_SEM_ROLE_BUTTON);
	return node;
}

void flux_looping_item_place(
  FluxStore *store, XentNodeId item, int visual, float y, float width, bool selected, float band_y
) {
	FluxLoopingItemData *cell = looping_cell(store, item);
	if (!cell || !cell->owner) return;
	FluxLoopingData *ld = cell->owner;

	cell->visual        = visual;
	cell->logical_y     = y;
	cell->band_y        = band_y;
	cell->selected      = selected;

	xent_setpos(ld->ctx, item, (XentPoint) {0.0f, y});
	xent_seth(ld->ctx, item, ld->item_height);
	if (width > 0.0f) xent_setw(ld->ctx, item, width);
	else xent_setwpct(ld->ctx, item, 1.0f);
}
