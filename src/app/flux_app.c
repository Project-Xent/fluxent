#include "fluxent/fluxent.h"
#include "controls/factory/flux_init_picker.h"
#include "controls/behavior/flux_color_picker.h"
#include "store/flux_node_data.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_gfx.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_text.h"
#include "fluxent/flux_host.h"
#include "input/flux_dmanip.h"
#include "render/flux_render_internal.h"
#include "fluxent/flux_theme.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_tooltip.h"
#include "app/flux_scene.h"
#include "runtime/flux_time.h"
#include "app/flux_post.h"
#include "input/flux_dmanip_sync.h"
#include "controls/behavior/flux_control_cursor.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/factory/flux_factory.h"
#include "controls/factory/flux_init_container.h"
#include "controls/factory/flux_init_items.h"
#include "controls/factory/flux_init_nav.h"
#include "controls/factory/flux_init_textbox.h"
#include "compose/flux_compose_render.h"
#include "a11y/flux_uia.h"
#include "graphics/flux_graphics_compose.h"
#include "render/flux_dl_replay.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FLUX_APP_DISABLE_DMANIP_ENV_CAP 8
#define FLUX_APP_DEFAULT_WIDTH          800
#define FLUX_APP_DEFAULT_HEIGHT         600
#define FLUX_APP_RENDER_CACHE_CAPACITY  512
#define FLUX_APP_SCENE_INITIAL_CAPACITY 512
#define FLUX_APP_TOOLTIP_ANCHOR_H       20.0f

static bool const                kFluxAppPresentUseVsync = true;

typedef struct FluxRenderBackend FluxRenderBackend;

struct FluxApp {
	FluxWindow              *window;
	FluxEngine              *engine;
	FluxControlRegistry     *registry;
	FluxInput               *input;
	FluxText                *text;
	FluxRenderCache         *cache;
	FluxTheme               *theme;
	FluxScene               *scene;

	ID2D1SolidColorBrush    *shared_brush;
	int64_t                  last_frame_ticks;
	XentNodeId               last_uia_focus;

	FluxTooltip             *tooltip;
	bool                     auto_tip_open;

	FluxDManip              *dmanip;

	FluxComposeRender       *compose;
	FluxRenderBackend const *backend;
	XtkHost                 *host;
	XtkBackend              *xtk_backend;
	FluxPostSink            *post_sink;
	XtkRt                   *post_rt;

	void                     (*frame_cb)(void *ctx);
	void                    *frame_cb_ctx;
	XentNodeId              *deferred;
	uint32_t                 deferred_count;
	uint32_t                 deferred_cap;
};

static XentCtx   *app_ctx(FluxApp const *app) { return app ? flux_scene_context(app->scene) : NULL; }

static FluxStore *app_store(FluxApp const *app) { return app ? flux_scene_store(app->scene) : NULL; }

static XentNodeId app_root(FluxApp const *app) { return app ? flux_scene_root(app->scene) : XENT_NODE_INVALID; }

static void       app_ensure_shared_brush(FluxApp *app, FluxGfx *gfx) {
	if (app->shared_brush) return;
	ID2D1DeviceContext *d2d = flux_gfx_d2d(gfx);
	if (!d2d) return;
	D2D1_COLOR_F          black = {0, 0, 0, 1};
	D2D1_BRUSH_PROPERTIES bp;
	bp.opacity       = 1.0f;
	bp.transform._11 = 1;
	bp.transform._12 = 0;
	bp.transform._21 = 0;
	bp.transform._22 = 1;
	bp.transform._31 = 0;
	bp.transform._32 = 0;
	ID2D1RenderTarget_CreateSolidColorBrush(( ID2D1RenderTarget * ) d2d, &black, &bp, &app->shared_brush);
}

static void app_try_dmanip_handoff(FluxApp *app, FluxPointerEvent const *ev) {
	if (flux_dmanip_pan_handoff(app->dmanip, app->input, app_store(app), ev->pointer_id))
		flux_window_abandonpointer(app->window, ev->pointer_id);
}

static void app_try_interaction_handoff(FluxApp *app, FluxPointerEvent const *ev) {
	if (!app->compose || !flux_input_takepan(app->input)) return;
	XentNodeId target = flux_input_pantarget(app->input);
	if (target == XENT_NODE_INVALID) return;
	if (flux_compose_redirect_scroll(app->compose, target, ev->pointer_id))
		flux_window_abandonpointer(app->window, ev->pointer_id);
}

static void app_init_dmanip(FluxApp *app) {
	char  val [FLUX_APP_DISABLE_DMANIP_ENV_CAP] = {0};
	DWORD n                                     = GetEnvironmentVariableA("FLUXENT_DISABLE_DMANIP", val, sizeof(val));
	if (n > 0 && val [0] == '1') return;
	HWND hwnd = flux_window_hwnd(app->window);
	if (hwnd) ( void ) flux_dmanip_create(hwnd, &app->dmanip);
}

static bool app_focused_accepts_command(FluxApp *app) {
	XentNodeId focused = flux_input_focus(app->input);
	if (focused == XENT_NODE_INVALID || !app_ctx(app)) return false;

	FluxControlType ct = flux_node_type(app_store(app), focused);
	return ct != FLUX_CONTROL_TEXT_INPUT && ct != FLUX_CONTROL_PASSWORD_BOX && ct != FLUX_CONTROL_NUMBER_BOX;
}

static void app_request_render(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (app) flux_app_redraw(app);
}

static float    app_dpi_scale(FluxDpi dpi) { return dpi.dpi_x / FLUX_DPI_BASE; }

static FluxSize app_client_dips(FluxApp const *app, FluxDpi dpi) {
	FluxSize px    = flux_window_size(app->window);
	float    scale = app_dpi_scale(dpi);
	return (FluxSize) {px.w / scale, px.h / scale};
}

static bool app_mode_dark(FluxThemeMode mode) {
	return mode == FLUX_THEME_DARK || (mode == FLUX_THEME_SYSTEM && flux_theme_systemdark());
}

static bool app_theme_is_dark(FluxTheme *theme) { return theme && app_mode_dark(flux_theme_mode(theme)); }

static bool app_defer(FluxApp *app, XentNodeId node) {
	if (app->deferred_count == app->deferred_cap) {
		if (app->deferred_cap > UINT32_MAX / 2u) return false;
		uint32_t    next_cap = app->deferred_cap ? app->deferred_cap * 2u : 16u;
		XentNodeId *next     = ( XentNodeId * ) realloc(app->deferred, sizeof(*next) * next_cap);
		if (!next) return false;
		app->deferred     = next;
		app->deferred_cap = next_cap;
	}
	app->deferred [app->deferred_count++] = node;
	return true;
}

static void app_sync_picker_control(FluxApp *app, XentNodeId node, FluxNodeData *nd) {
	switch (nd->component_type) {
	case FLUX_CONTROL_LOOPING_SELECTOR :
		if (flux_looping_sync(node, nd)) flux_window_redraw(app->window);
		break;
	case FLUX_CONTROL_DATE_PICKER  :
	case FLUX_CONTROL_TIME_PICKER  : flux_picker_sync(node, nd); break;
	case FLUX_CONTROL_COLOR_PICKER : flux_colorpicker_sync(node, nd); break;
	default                        : break;
	}
}

static void app_sync_layout_control(FluxApp *app, XentCtx *ctx, XentNodeId node, FluxNodeData *nd) {
	if (!nd || !nd->component_data) return;
	if (xtk_ctrl_islist(( XtkCtrlType ) nd->component_type) && flux_list_window_stale(ctx, nd))
		( void ) app_defer(app, node);
	if (nd->component_type == FLUX_CONTROL_SCROLL && flux_scroll_sync(ctx, node, nd)) ( void ) app_defer(app, node);
	switch (nd->component_type) {
	case FLUX_CONTROL_FLIP_VIEW      : flux_flip_sync(ctx, node, nd); break;
	case FLUX_CONTROL_SPLIT_VIEW     : flux_splitview_sync(ctx, node, nd); break;
	case FLUX_CONTROL_BREADCRUMB_BAR : flux_breadcrumb_sync(ctx, node, nd); break;
	case FLUX_CONTROL_TAB_VIEW       : flux_tabs_sync_layout(app_store(app), node); break;
	default                          : app_sync_picker_control(app, node, nd); break;
	}
}

static bool app_layout_stack_push(XentNodeId **stack, uint32_t *top, uint32_t *cap, XentNodeId node) {
	if (*top == *cap) {
		if (*cap > UINT32_MAX / 2u) return false;
		uint32_t    next_cap = *cap * 2u;
		XentNodeId *next     = ( XentNodeId * ) realloc(*stack, sizeof(**stack) * next_cap);
		if (!next) return false;
		*stack = next;
		*cap   = next_cap;
	}
	(*stack) [(*top)++] = node;
	return true;
}

static bool app_layout_push_children(XentCtx *ctx, XentNodeId node, XentNodeId **stack, uint32_t *top, uint32_t *cap) {
	for (XentNodeId child = xent_node_first(ctx, node); child != XENT_NODE_INVALID; child = xent_node_next(ctx, child))
		if (!app_layout_stack_push(stack, top, cap, child)) return false;
	return true;
}

static void app_sync_layout_controls(FluxApp *app, XentCtx *ctx, FluxStore *store, XentNodeId root) {
	uint32_t    cap   = 64u;
	uint32_t    top   = 0u;
	XentNodeId *stack = ( XentNodeId * ) malloc(sizeof(*stack) * cap);
	if (!stack) return;
	stack [top++] = root;

	while (top > 0u) {
		XentNodeId node = stack [--top];
		app_sync_layout_control(app, ctx, node, flux_store_find(store, node));
		if (app_layout_push_children(ctx, node, &stack, &top, &cap)) continue;
		free(stack);
		return;
	}
	free(stack);
}

static void app_prepare_layout_tree(FluxApp *app, FluxDpi dpi) {
	FluxSize   dips  = app_client_dips(app, dpi);
	XentCtx   *ctx   = app_ctx(app);
	FluxStore *store = app_store(app);
	XentNodeId root  = app_root(app);
	float      scale = app_dpi_scale(dpi);

	if (app->dmanip) flux_dmanip_tick(app->dmanip);
	xent_layout(ctx, root, dips.w, dips.h);
	app_sync_layout_controls(app, ctx, store, root);
	xent_layout(ctx, root, dips.w, dips.h);
	if (app->dmanip) flux_dmanip_sync_tree(app->dmanip, ctx, store, root, scale);
	flux_textbox_sync_ime(store, flux_input_focus(app->input));
}

static FluxRenderCtx app_render_context(FluxApp *app, FluxGfx *gfx, FluxDpi dpi, int64_t now_ticks) {
	FluxRenderCtx rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = flux_gfx_d2d(gfx);
	rc.brush   = app->shared_brush;
	rc.cache   = app->cache;
	rc.text    = app->text;
	rc.dpi     = dpi;
	rc.theme   = app->theme ? flux_theme_colors(app->theme) : NULL;
	rc.now     = flux_perf_seconds(now_ticks);
	rc.dt      = flux_compute_dt(app->last_frame_ticks, now_ticks);
	rc.is_dark = app_theme_is_dark(app->theme);
	return rc;
}

static FluxColor app_clear_color(FluxApp const *app) {
	if (app->window && flux_window_backdrop(app->window) != FLUX_BACKDROP_NONE) return flux_color_rgba(0, 0, 0, 0);
	FluxThemeColors const *t = app->theme ? flux_theme_colors(app->theme) : flux_theme_defaults();
	return t->solid_background;
}

static void app_sync_tooltip_theme(FluxApp *app, FluxRenderCtx const *rc) {
	if (!app->tooltip) return;

	flux_tooltip_settext(app->tooltip, app->text);
	flux_tooltip_settheme(app->tooltip, rc->theme, rc->is_dark);
}

static void app_sync_auto_tooltip(FluxApp *app) {
	XentNodeId    node = app->input ? flux_input_focus(app->input) : XENT_NODE_INVALID;
	FluxNodeData *nd   = node != XENT_NODE_INVALID ? flux_store_find(app_store(app), node) : NULL;
	char          text [64];
	FluxRect      local     = {0};
	FluxPlacement placement = FLUX_PLACEMENT_TOP;
	bool open               = nd
	                       && nd->behavior.auto_tooltip
	                       && nd->behavior.auto_tooltip(nd->behavior.auto_tooltip_ctx, text, sizeof(text), &local, &placement);
	if (!open) {
		if (app->auto_tip_open) flux_tooltip_dismiss(app->tooltip);
		app->auto_tip_open = false;
		return;
	}
	XentRect r = {0};
	xent_layout_rect(app_ctx(app), node, &r);
	float scale = flux_window_dpi(app->window).dpi_x / FLUX_DPI_BASE;
	POINT pt    = {( LONG ) ((r.x + local.x) * scale), ( LONG ) ((r.y + local.y) * scale)};
	ClientToScreen(flux_window_hwnd(app->window), &pt);
	FluxRect anchor = {( float ) pt.x, ( float ) pt.y, local.w * scale, local.h * scale};
	flux_tooltip_showat(app->tooltip, text, anchor, placement);
	app->auto_tip_open = true;
}

static void app_ensure_compose(FluxApp *app, FluxGfx *gfx) {
	if (app->compose) return;
	FluxCompositor                *c    = flux_graphics_compositor(gfx);
	WUC_CompositionGraphicsDevice *gdev = flux_graphics_graphics_device(gfx);
	WUC_Visual                    *root = flux_graphics_composition_root(gfx);
	if (c && gdev && root) app->compose = flux_compose_render_create(c, gdev, root, app->registry, app->xtk_backend);
}

static void app_render_composition(FluxApp *app, FluxGfx *gfx, FluxDpi dpi) {
	app_prepare_layout_tree(app, dpi);
	if (app->cache) flux_rcache_begin(app->cache);
	app_ensure_compose(app, gfx);
	if (!app->compose) return;

	int64_t       now_ticks = flux_perf_now();
	FluxRenderCtx rc        = app_render_context(app, gfx, dpi, now_ticks);
	app->last_frame_ticks   = now_ticks;

	bool anims_active       = false;
	rc.animations_active    = &anims_active;
	app_sync_tooltip_theme(app, &rc);
	if (app->tooltip) app_sync_auto_tooltip(app);

	flux_compose_render_frame(app->compose, app_ctx(app), app_store(app), app_root(app), &rc);

	if (anims_active) flux_app_redraw(app);
}

static void app_render_classic(FluxApp *app, FluxGfx *gfx, FluxDpi dpi) {
	app_prepare_layout_tree(app, dpi);
	if (app->cache) flux_rcache_begin(app->cache);

	flux_engine_collect(app->engine, app_ctx(app), app_root(app));
	app_ensure_shared_brush(app, gfx);

	int64_t       now_ticks = flux_perf_now();
	FluxRenderCtx rc        = app_render_context(app, gfx, dpi, now_ticks);
	app->last_frame_ticks   = now_ticks;

	bool anims_active       = false;
	rc.animations_active    = &anims_active;
	app_sync_tooltip_theme(app, &rc);
	if (app->tooltip) app_sync_auto_tooltip(app);

	flux_gfx_begin(gfx);
	flux_gfx_clear(gfx, app_clear_color(app));
	if (app->xtk_backend) flux_xtk_setrc(app->xtk_backend, &rc);
	flux_engine_execute(app->engine, &rc);
	flux_gfx_end(gfx);
	flux_gfx_present(gfx, kFluxAppPresentUseVsync);

	if (anims_active) flux_app_redraw(app);
}

struct FluxRenderBackend {
	char const *name;
	void        (*render_frame)(FluxApp *app, FluxGfx *gfx, FluxDpi dpi);
};

static FluxRenderBackend const  g_backend_d2d         = {"d2d", app_render_classic};
static FluxRenderBackend const  g_backend_composition = {"composition", app_render_composition};

static FluxRenderBackend const *app_select_backend(FluxGfx *gfx) {
	if (!flux_graphics_composition_enabled(gfx)) return &g_backend_d2d;
	if (GetSystemMetrics(SM_REMOTESESSION)) return &g_backend_d2d;
	return &g_backend_composition;
}

static void app_pump_uia_focus(FluxApp *app) {
	XentNodeId focused = flux_input_focus(app->input);
	if (focused == app->last_uia_focus) return;
	app->last_uia_focus = focused;

	FluxUiaContext info = {app_ctx(app), app_root(app), flux_app_hwnd(app), app_store(app)};
	flux_uia_raisefocus(&info, focused);
}

static void app_render(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app_ctx(app) || app_root(app) == XENT_NODE_INVALID) return;

	if (app->frame_cb) app->frame_cb(app->frame_cb_ctx);
	app_pump_uia_focus(app);

	FluxGfx *gfx = flux_app_gfx(app);
	FluxDpi  dpi = flux_window_dpi(app->window);

	if (!app->backend) app->backend = app_select_backend(gfx);
	app->backend->render_frame(app, gfx, dpi);
	for (uint32_t i = 0; i < app->deferred_count; i++) {
		flux_list_notify_stale(app_store(app), app->deferred [i]);
		flux_scroll_report(app_store(app), app->deferred [i]);
	}
	app->deferred_count = 0;
}

static void app_update_cursor_and_tooltip(FluxApp *app, float x, float y, bool is_touch) {
	XentNodeId hovered = flux_input_hover(app->input);
	int        cursor  = flux_control_cursor_for(app_ctx(app), app_store(app), hovered);
	flux_window_setcursor(app->window, is_touch ? FLUX_CURSOR_ARROW : cursor);

	if (!app->tooltip) return;
	if (is_touch) {
		flux_tooltip_hover(app->tooltip, XENT_NODE_INVALID, NULL, NULL);
		return;
	}

	FluxNodeData *hover_nd      = (hovered != XENT_NODE_INVALID) ? flux_store_find(app_store(app), hovered) : NULL;
	FluxRect      screen_bounds = {0};
	if (hovered != XENT_NODE_INVALID) {
		HWND    hwnd   = flux_window_hwnd(app->window);
		FluxDpi tdpi   = flux_window_dpi(app->window);
		float   tscale = tdpi.dpi_x / FLUX_DPI_BASE;
		POINT   pt     = {( LONG ) (x * tscale), ( LONG ) (y * tscale)};
		ClientToScreen(hwnd, &pt);
		screen_bounds.x = ( float ) pt.x;
		screen_bounds.y = ( float ) pt.y;
		screen_bounds.h = FLUX_APP_TOOLTIP_ANCHOR_H;
	}
	flux_tooltip_hover(app->tooltip, hovered, hover_nd, &screen_bounds);
}

static bool app_active_menu_visible(FluxApp *app, FluxMenu **out_menu) {
	FluxMenu *menu = flux_app_menu(app);
	if (out_menu) *out_menu = menu;
	return menu && flux_menu_visible(menu);
}

static void app_pointer_move(FluxApp *app, FluxPointerEvent const *ev, bool is_touch) {
	flux_input_dispatch(app->input, app_root(app), ev);
	if (is_touch && app->compose) app_try_interaction_handoff(app, ev);
	else if (is_touch) app_try_dmanip_handoff(app, ev);
	app_update_cursor_and_tooltip(app, ev->x, ev->y, is_touch);
}

static bool app_lightdismiss_popups(FluxApp *app) {
	if (!flux_popup_lightdismissvisible(flux_app_hwnd(app))) return false;
	FluxMenu *menu;
	if (app_active_menu_visible(app, &menu)) flux_menu_dismissall(menu);
	flux_popup_dismissowner(flux_app_hwnd(app));
	return true;
}

static void app_pointer_down(FluxApp *app, FluxPointerEvent const *ev) {
	if (app->tooltip) flux_tooltip_dismiss(app->tooltip);
	if (app_lightdismiss_popups(app)) return;
	flux_input_dispatch(app->input, app_root(app), ev);
}

static void app_pointer_wheel(FluxApp *app, FluxPointerEvent const *ev) {
	if (app_lightdismiss_popups(app)) return;
	flux_input_dispatch(app->input, app_root(app), ev);
}

static void app_pointer_default(FluxApp *app, FluxPointerEvent const *ev) {
	flux_input_dispatch(app->input, app_root(app), ev);
}

static void app_pointer_route(FluxApp *app, FluxPointerEvent const *ev, bool is_touch) {
	switch (ev->kind) {
	case FLUX_POINTER_MOVE         : app_pointer_move(app, ev, is_touch); break;
	case FLUX_POINTER_DOWN         : app_pointer_down(app, ev); break;
	case FLUX_POINTER_WHEEL        : app_pointer_wheel(app, ev); break;
	case FLUX_POINTER_UP           :
	case FLUX_POINTER_HOLD         :
	case FLUX_POINTER_CONTEXT_MENU :
	case FLUX_POINTER_CANCEL       : app_pointer_default(app, ev); break;
	}
}

static void app_pointer(void *ctx, FluxPointerEvent const *ev) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input || app_root(app) == XENT_NODE_INVALID || !ev) return;

	bool const is_touch = (ev->device == FLUX_POINTER_TOUCH);
	app_pointer_route(app, ev, is_touch);
	flux_app_redraw(app);
}

static void app_setting_changed(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app) return;
	if (app->theme && flux_theme_mode(app->theme) == FLUX_THEME_SYSTEM) {
		flux_theme_setmode(app->theme, FLUX_THEME_SYSTEM);
		flux_window_setdark(app->window, flux_theme_systemdark());
	}
	flux_app_redraw(app);
}

static bool app_handle_active_menu_key(FluxApp *app, FluxKey key) {
	FluxMenu *active_menu = flux_app_menu(app);
	return active_menu && flux_menu_onkey(active_menu, key);
}

static bool app_handle_arrow_key(FluxApp *app, FluxKey key) {
	if (key != FLUX_KEY_LEFT && key != FLUX_KEY_UP && key != FLUX_KEY_RIGHT && key != FLUX_KEY_DOWN) return false;
	if (!app_focused_accepts_command(app)) return false;

	flux_input_arrow(app->input, app_root(app), key);
	return true;
}

static bool app_handle_activation_key(FluxApp *app, FluxKey key) {
	if (key != FLUX_KEY_ENTER && key != FLUX_KEY_SPACE) return false;
	if (!app_focused_accepts_command(app)) return false;
	FluxControlType ct = flux_node_type(app_store(app), flux_input_focus(app->input));
	if (ct == FLUX_CONTROL_CHECKBOX || ct == FLUX_CONTROL_RADIO) return false;

	flux_input_activate(app->input);
	return true;
}

static bool app_handle_key_command(FluxApp *app, FluxKey key) {
	if (app_handle_active_menu_key(app, key)) return true;
	if (key == FLUX_KEY_TAB) {
		flux_input_tab(app->input, app_root(app), flux_key_held(FLUX_KEY_SHIFT));
		return true;
	}
	if (flux_input_keydown(app->input, key)) return true;
	if (flux_input_buttonkey(app->input, key, true)) return true;
	if (app_handle_arrow_key(app, key)) return true;
	if (app_handle_activation_key(app, key)) return true;
	if (key != FLUX_KEY_ESCAPE) return false;

	flux_input_escape(app->input);
	return true;
}

static void app_key(void *ctx, FluxKey key, bool down) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;

	if (down && app->tooltip) flux_tooltip_dismiss(app->tooltip);
	if (down) app_handle_key_command(app, key);
	else if (!flux_input_buttonkey(app->input, key, false)) flux_input_keyup(app->input, key);
	flux_app_redraw(app);
}

static void app_char(void *ctx, wchar_t ch) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;
	flux_input_char(app->input, ch);
	flux_app_redraw(app);
}

static void app_ime_composition(void *ctx, wchar_t const *text, int cursor_pos) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;
	if (text) flux_input_ime(app->input, text, ( uint32_t ) wcslen(text), ( uint32_t ) cursor_pos);
	else flux_input_imeend(app->input);
	flux_app_redraw(app);
}

static void app_cleanup_dmanip_tree(FluxApp *app) {
	if (app->dmanip && app_ctx(app) && app_store(app) && app_root(app) != XENT_NODE_INVALID)
		flux_dmanip_cleanup_tree(app_ctx(app), app_store(app), app_root(app));
}

static void app_destroy_scene_runtime(FluxApp *app) {
	if (app->engine) flux_engine_destroy(app->engine);
	if (app->input) flux_input_destroy(app->input);
	if (app->text) flux_text_destroy(app->text);
	app->engine = NULL;
	app->input  = NULL;
	app->text   = NULL;
}

static void app_on_node_removed(void *userdata, XentNodeId id) {
	FluxApp *app = ( FluxApp * ) userdata;
	flux_input_dropnode(app->input, id);
	if (app->cache) flux_render_cache_remove(app->cache, id);
}

static bool app_bind_scene_runtime(FluxApp *app) {
	XentCtx   *ctx   = app_ctx(app);
	FluxStore *store = app_store(app);
	if (!ctx || !store || app_root(app) == XENT_NODE_INVALID) return false;

	app->engine = flux_engine_create(store);
	if (app->engine) flux_engine_bind(app->engine, app->registry, app->xtk_backend);
	app->input = flux_input_create(ctx, store);
	app->text  = flux_text_create();
	if (app->text) flux_text_register(app->text, ctx);

	flux_store_ondel(store, app_on_node_removed, app);
	if (app->tooltip && app->text) flux_tooltip_settext(app->tooltip, app->text);

	return app->engine && app->input;
}

static bool app_replace_scene(FluxApp *app, FluxScene *scene) {
	if (!app || !scene) return false;

	app_cleanup_dmanip_tree(app);
	app_destroy_scene_runtime(app);
	flux_scene_destroy(app->scene);
	app->scene = scene;

	if (!app_bind_scene_runtime(app)) return false;
	flux_app_redraw(app);
	return true;
}

static FluxWindowCfg app_window_config(FluxAppCfg const *cfg) {
	FluxWindowCfg wcfg;
	memset(&wcfg, 0, sizeof(wcfg));
	wcfg.title     = cfg ? cfg->title : L"Fluxent";
	wcfg.width     = cfg ? cfg->width : FLUX_APP_DEFAULT_WIDTH;
	wcfg.height    = cfg ? cfg->height : FLUX_APP_DEFAULT_HEIGHT;
	wcfg.dark_mode = app_mode_dark(cfg ? cfg->theme : FLUX_THEME_SYSTEM);
	wcfg.backdrop  = cfg ? ( int ) cfg->backdrop : 0;
	wcfg.resizable = true;
	return wcfg;
}

static bool app_create_window(FluxApp *app, FluxAppCfg const *cfg) {
	FluxWindowCfg wcfg = app_window_config(cfg);
	HRESULT       hr   = flux_window_create(&wcfg, &app->window);
	return SUCCEEDED(hr);
}

static void *app_uia_provider(void *ctx, void *hwnd) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app_ctx(app) || app_root(app) == XENT_NODE_INVALID) return NULL;

	FluxUiaContext info = {app_ctx(app), app_root(app), ( HWND ) hwnd, app_store(app)};
	return flux_uia_provider_create(&info);
}

static void app_bind_window_callbacks(FluxApp *app) {
	flux_window_onrender(app->window, app_render, app);
	flux_window_onpointer(app->window, app_pointer, app);
	flux_window_onkey(app->window, app_key, app);
	flux_window_onchar(app->window, app_char, app);
	flux_window_onsettings(app->window, app_setting_changed, app);
	flux_window_onime(app->window, app_ime_composition, app);
	flux_window_onuia(app->window, app_uia_provider, app);
	flux_gfx_onredraw(flux_window_gfx(app->window), app_request_render, app);
}

static void app_create_runtime_services(FluxApp *app, FluxThemeMode mode) {
	app->registry = flux_control_registry_create();
	if (app->registry) flux_register_builtins(app->registry);
	app->cache = flux_render_cache_create(FLUX_APP_RENDER_CACHE_CAPACITY);
	app->theme = flux_theme_create();
	if (app->theme) flux_theme_setmode(app->theme, mode);
	app->tooltip = flux_tooltip_create(app->window);
	app_init_dmanip(app);
}

static bool app_create_scene(FluxApp *app) {
	app->scene = flux_scene_create(FLUX_APP_SCENE_INITIAL_CAPACITY);
	return app->scene && app_bind_scene_runtime(app);
}

FluxApp *flux_app_create(FluxAppCfg const *cfg) {
	FluxApp *app = ( FluxApp * ) calloc(1, sizeof(*app));
	if (!app) return NULL;

	if (!app_create_window(app, cfg)) {
		free(app);
		return NULL;
	}

	app_bind_window_callbacks(app);
	app_create_runtime_services(app, cfg ? cfg->theme : FLUX_THEME_SYSTEM);
	app->post_sink = flux_postsink_create();
	if (!app->post_sink || !app_create_scene(app)) {
		flux_app_destroy(app);
		return NULL;
	}

	return app;
}

static void app_cleanup_dmanip(FluxApp *app) {
	if (!app->dmanip) return;
	app_cleanup_dmanip_tree(app);
	flux_dmanip_destroy(app->dmanip);
	app->dmanip = NULL;
}

static void app_release_shared_brush(FluxApp *app) {
	if (!app->shared_brush) return;
	ID2D1SolidColorBrush_Release(app->shared_brush);
	app->shared_brush = NULL;
}

static void app_destroy_render_state(FluxApp *app) {
	if (app->compose) flux_compose_render_destroy(app->compose);
	app->compose = NULL;
	app_release_shared_brush(app);
	if (app->cache) flux_render_cache_destroy(app->cache);
	if (app->theme) flux_theme_destroy(app->theme);
	if (app->engine) flux_engine_destroy(app->engine);
	if (app->xtk_backend) flux_xtk_backend_destroy(app->xtk_backend);
	app->xtk_backend = NULL;
	app->host        = NULL;
	flux_control_registry_destroy(app->registry);
	app->registry = NULL;
	if (app->text) flux_text_destroy(app->text);
}

static void app_destroy_input_state(FluxApp *app) {
	if (app->tooltip) flux_tooltip_destroy(app->tooltip);
	if (app->input) flux_input_destroy(app->input);
}

void flux_app_destroy(FluxApp *app) {
	if (!app) return;
	flux_postsink_destroy(app->post_sink);
	app_cleanup_dmanip(app);
	app_destroy_input_state(app);
	app_destroy_render_state(app);
	flux_scene_destroy(app->scene);
	if (app->window) flux_window_destroy(app->window);
	free(app->deferred);
	free(app);
}

void flux_app_setroot(FluxApp *app, XentCtx *ctx, XentNodeId root, FluxStore *store) {
	if (!app) return;

	FluxScene *scene = flux_scene_borrow(ctx, store, root);
	if (!scene) return;
	( void ) app_replace_scene(app, scene);
}

int flux_app_run(FluxApp *app) {
	if (!app || !app->window) return -1;
	return flux_window_run(app->window);
}

FluxEngine  *flux_app_engine(FluxApp *app) { return app ? app->engine : NULL; }

FluxWindow  *flux_app_window(FluxApp *app) { return app ? app->window : NULL; }

FluxInput   *flux_app_input(FluxApp *app) { return app ? app->input : NULL; }

FluxTheme   *flux_app_theme(FluxApp *app) { return app ? app->theme : NULL; }

FluxTooltip *flux_app_tooltip(FluxApp *app) { return app ? app->tooltip : NULL; }

FluxText    *flux_app_text(FluxApp *app) { return app ? app->text : NULL; }

XentCtx     *flux_app_ctx(FluxApp *app) { return app ? app_ctx(app) : NULL; }

FluxStore   *flux_app_store(FluxApp *app) { return app ? app_store(app) : NULL; }

XentNodeId   flux_app_root(FluxApp *app) { return app ? flux_scene_content(app->scene) : XENT_NODE_INVALID; }

XentNodeId   flux_app_overlay(FluxApp *app) { return app ? flux_scene_overlay(app->scene) : XENT_NODE_INVALID; }

void         flux_app_redraw(FluxApp *app) {
	if (!app || !app->window) return;
	flux_window_redraw(app->window);
}

void flux_app_onframe(FluxApp *app, void (*cb)(void *ctx), void *ctx) {
	if (!app) return;
	app->frame_cb     = cb;
	app->frame_cb_ctx = ctx;
}

FluxGfx  *flux_app_gfx(FluxApp *app) { return app && app->window ? flux_window_gfx(app->window) : NULL; }

FluxMenu *flux_app_menu(FluxApp *app) { return app && app->window ? flux_menu_active(app->window) : NULL; }

HWND      flux_app_hwnd(FluxApp *app) { return app && app->window ? flux_window_hwnd(app->window) : NULL; }

void      flux_app_sethost(FluxApp *app, XtkHost *host, XtkBackend *backend) {
	if (!app) return;
	flux_host_bindapp(host, app);
	app->host        = host;
	app->xtk_backend = backend;
	if (app->engine) flux_engine_bind(app->engine, app->registry, backend);
	FluxStore *store = app_store(app);
	if (store) flux_store_sethost(store, host);
}

XtkHost    *flux_app_host(FluxApp *app) { return app ? app->host : NULL; }

XtkBackend *flux_app_backend(FluxApp *app) { return app ? app->xtk_backend : NULL; }

bool        flux_app_bindruntime(FluxApp *app, XtkRt *rt) {
	if (!app || app->post_rt) return false;
	app->post_rt = rt;
	return true;
}

bool flux_app_invoke(FluxApp *app, void (*fn)(void *ctx), void *ctx) {
	return app && flux_postsink_invoke(app->post_sink, fn, ctx);
}

XtkPostStatus flux_app_post(FluxApp *app, XtkMsg msg) {
	return app ? flux_postsink_send(app->post_sink, app->post_rt, msg, NULL, 0u, false) : XTK_POST_INVALID_MESSAGE;
}

XtkPostStatus flux_app_poststring(FluxApp *app, XtkMsg msg, char const *data, uint32_t len) {
	return app ? flux_postsink_send(app->post_sink, app->post_rt, msg, data, len, true) : XTK_POST_INVALID_MESSAGE;
}
