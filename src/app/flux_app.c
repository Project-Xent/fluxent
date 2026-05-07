#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_graphics.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_text.h"
#include "input/flux_dmanip.h"
#include "render/flux_render_internal.h"
#include "fluxent/flux_theme.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_tooltip.h"
#include "runtime/flux_time.h"
#include "input/flux_dmanip_sync.h"
#include "controls/behavior/flux_control_cursor.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/factory/flux_factory.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

struct FluxApp {
	FluxWindow           *window;
	FluxEngine           *engine;
	FluxInput            *input;
	FluxTextRenderer     *text;
	FluxRenderCache      *cache;
	FluxThemeManager     *theme;

	XentContext          *ctx;
	XentNodeId            root;
	FluxNodeStore        *store;

	ID2D1SolidColorBrush *shared_brush;
	int64_t               last_frame_ticks;

	FluxTooltip          *tooltip;

	FluxDManip           *dmanip;
};

static void app_ensure_shared_brush(FluxApp *app, FluxGraphics *gfx) {
	if (app->shared_brush) return;
	ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
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
	if (flux_dmanip_handoff_touch_pan(app->dmanip, app->input, app->store, ev->pointer_id))
		flux_window_abandon_pointer(app->window, ev->pointer_id);
}

static void app_init_dmanip(FluxApp *app) {
	char  val [8] = {0};
	DWORD n       = GetEnvironmentVariableA("FLUXENT_DISABLE_DMANIP", val, sizeof(val));
	if (n > 0 && val [0] == '1') return;
	HWND hwnd = flux_window_hwnd(app->window);
	if (hwnd) ( void ) flux_dmanip_create(hwnd, &app->dmanip);
}

static bool app_focused_accepts_command(FluxApp *app) {
	XentNodeId focused = flux_input_get_focused(app->input);
	if (focused == XENT_NODE_INVALID || !app->ctx) return false;

	XentControlType ct = xent_get_control_type(app->ctx, focused);
	return ct != XENT_CONTROL_TEXT_INPUT && ct != XENT_CONTROL_PASSWORD_BOX && ct != XENT_CONTROL_NUMBER_BOX;
}

static void app_render(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->ctx || app->root == XENT_NODE_INVALID) return;

	FluxGraphics *gfx   = flux_window_get_graphics(app->window);
	FluxSize      size  = flux_window_client_size(app->window);
	FluxDpiInfo   dpi   = flux_window_dpi(app->window);
	float         scale = dpi.dpi_x / 96.0f;
	float         w     = size.w / scale;
	float         h     = size.h / scale;

	if (app->dmanip) flux_dmanip_tick(app->dmanip);
	xent_layout(app->ctx, app->root, w, h);
	if (app->dmanip) flux_dmanip_sync_tree(app->dmanip, app->ctx, app->store, app->root, scale);
	if (app->store) flux_node_store_attach_userdata(app->store, app->ctx);
	if (app->cache) flux_render_cache_begin_frame(app->cache);

	flux_engine_collect(app->engine, app->ctx, app->root);
	app_ensure_shared_brush(app, gfx);

	int64_t now_ticks     = flux_perf_now();
	float   dt            = flux_compute_dt(app->last_frame_ticks, now_ticks);
	app->last_frame_ticks = now_ticks;

	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d   = flux_graphics_get_d2d_context(gfx);
	rc.brush = app->shared_brush;
	rc.cache = app->cache;
	rc.text  = app->text;
	rc.dpi   = dpi;
	rc.theme = app->theme ? flux_theme_colors(app->theme) : NULL;
	rc.now   = flux_perf_seconds(now_ticks);
	rc.dt    = dt;
	if (app->theme) {
		FluxThemeMode m = flux_theme_get_mode(app->theme);
		rc.is_dark      = (m == FLUX_THEME_DARK) || (m == FLUX_THEME_SYSTEM && flux_theme_system_is_dark());
	}
	else { rc.is_dark = false; }
	bool anims_active    = false;
	rc.animations_active = &anims_active;

	if (app->tooltip) {
		flux_tooltip_set_text_renderer(app->tooltip, app->text);
		flux_tooltip_set_theme(app->tooltip, rc.theme, rc.is_dark);
	}

	flux_graphics_begin_draw(gfx);
	flux_graphics_clear(gfx, flux_color_rgba(0, 0, 0, 0));
	flux_engine_execute(app->engine, &rc);
	flux_graphics_end_draw(gfx);
	flux_graphics_present(gfx, true);

	if (anims_active) flux_window_request_render(app->window);
}

static void app_update_cursor_and_tooltip(FluxApp *app, float x, float y, bool is_touch) {
	XentNodeId hovered = flux_input_get_hovered(app->input);
	int        cursor  = flux_control_cursor_for_node(app->ctx, app->store, hovered);
	flux_window_set_cursor(app->window, is_touch ? FLUX_CURSOR_ARROW : cursor);

	if (!app->tooltip) return;
	if (is_touch) {
		flux_tooltip_on_hover(app->tooltip, XENT_NODE_INVALID, NULL, NULL);
		return;
	}

	FluxNodeData *hover_nd      = (hovered != XENT_NODE_INVALID) ? flux_node_store_get(app->store, hovered) : NULL;
	FluxRect      screen_bounds = {0};
	if (hovered != XENT_NODE_INVALID) {
		HWND        hwnd   = flux_window_hwnd(app->window);
		FluxDpiInfo tdpi   = flux_window_dpi(app->window);
		float       tscale = tdpi.dpi_x / 96.0f;
		POINT       pt     = {( LONG ) (x * tscale), ( LONG ) (y * tscale)};
		ClientToScreen(hwnd, &pt);
		screen_bounds.x = ( float ) pt.x;
		screen_bounds.y = ( float ) pt.y;
		screen_bounds.h = 20.0f;
	}
	flux_tooltip_on_hover(app->tooltip, hovered, hover_nd, &screen_bounds);
}

static bool app_active_menu_visible(FluxApp *app, FluxMenuFlyout **out_menu) {
	FluxMenuFlyout *menu = flux_menu_flyout_get_active(app->window);
	if (out_menu) *out_menu = menu;
	return menu && flux_menu_flyout_is_visible(menu);
}

static void app_pointer_move(FluxApp *app, FluxPointerEvent const *ev, bool is_touch) {
	flux_input_dispatch(app->input, app->root, ev);
	if (is_touch) app_try_dmanip_handoff(app, ev);
	app_update_cursor_and_tooltip(app, ev->x, ev->y, is_touch);
}

static void app_pointer_down(FluxApp *app, FluxPointerEvent const *ev) {
	if (app->tooltip) flux_tooltip_dismiss(app->tooltip);
	if (ev->changed_button == FLUX_POINTER_BUTTON_LEFT) {
		FluxMenuFlyout *menu;
		if (app_active_menu_visible(app, &menu)) flux_menu_flyout_dismiss(menu);
	}
	flux_input_dispatch(app->input, app->root, ev);
}

static void app_pointer_wheel(FluxApp *app, FluxPointerEvent const *ev) {
	FluxMenuFlyout *menu;
	if (app_active_menu_visible(app, &menu)) {
		flux_menu_flyout_dismiss(menu);
		return;
	}

	flux_popup_dismiss_all_for_owner(flux_window_hwnd(app->window));
	flux_input_dispatch(app->input, app->root, ev);
}

static void app_pointer_default(FluxApp *app, FluxPointerEvent const *ev) {
	flux_input_dispatch(app->input, app->root, ev);
}

static void app_pointer_route(FluxApp *app, FluxPointerEvent const *ev, bool is_touch) {
	switch (ev->kind) {
	case FLUX_POINTER_MOVE         : app_pointer_move(app, ev, is_touch); break;
	case FLUX_POINTER_DOWN         : app_pointer_down(app, ev); break;
	case FLUX_POINTER_WHEEL        : app_pointer_wheel(app, ev); break;
	case FLUX_POINTER_UP           :
	case FLUX_POINTER_CONTEXT_MENU :
	case FLUX_POINTER_CANCEL       : app_pointer_default(app, ev); break;
	}
}

static void app_pointer(void *ctx, FluxPointerEvent const *ev) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input || app->root == XENT_NODE_INVALID || !ev) return;

	bool const is_touch = (ev->device == FLUX_POINTER_TOUCH);
	app_pointer_route(app, ev, is_touch);
	flux_window_request_render(app->window);
}

static void app_setting_changed(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app) return;
	if (app->theme) {
		flux_theme_set_mode(app->theme, FLUX_THEME_SYSTEM);
		flux_window_set_dark_mode(app->window, flux_theme_system_is_dark());
	}
	flux_window_request_render(app->window);
}

static bool app_handle_active_menu_key(FluxApp *app, unsigned int vk) {
	FluxMenuFlyout *active_menu = flux_menu_flyout_get_active(app->window);
	return active_menu && flux_menu_flyout_on_key(active_menu, vk);
}

static bool app_handle_arrow_key(FluxApp *app, unsigned int vk) {
	if (vk != VK_LEFT && vk != VK_UP && vk != VK_RIGHT && vk != VK_DOWN) return false;
	if (!app_focused_accepts_command(app)) return false;

	flux_input_arrow(app->input, app->root, ( int ) vk);
	return true;
}

static bool app_handle_activation_key(FluxApp *app, unsigned int vk) {
	if (vk != VK_RETURN && vk != VK_SPACE) return false;
	if (!app_focused_accepts_command(app)) return false;

	flux_input_activate(app->input);
	return true;
}

static bool app_handle_key_command(FluxApp *app, unsigned int vk) {
	if (app_handle_active_menu_key(app, vk)) return true;
	if (vk == VK_TAB) {
		flux_input_tab(app->input, app->root, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
		return true;
	}
	if (app_handle_arrow_key(app, vk)) return true;
	if (app_handle_activation_key(app, vk)) return true;
	if (vk != VK_ESCAPE) return false;

	flux_input_escape(app->input);
	return true;
}

static void app_key(void *ctx, unsigned int vk, bool down) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;

	if (down && app->tooltip) flux_tooltip_dismiss(app->tooltip);
	if (down && !app_handle_key_command(app, vk)) flux_input_key_down(app->input, vk);
	flux_window_request_render(app->window);
}

static void app_char(void *ctx, wchar_t ch) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;
	flux_input_char(app->input, ch);
	flux_window_request_render(app->window);
}

static void app_ime_composition(void *ctx, wchar_t const *text, int cursor_pos) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;
	if (text) flux_input_ime_composition(app->input, text, ( uint32_t ) wcslen(text), ( uint32_t ) cursor_pos);
	else flux_input_ime_end(app->input);
	flux_window_request_render(app->window);
}

static void app_register_core_renderers(FluxNodeStore *store) {
	if (!store) return;
	flux_node_store_register_renderer(store, XENT_CONTROL_CONTAINER, flux_draw_container, NULL);
	flux_node_store_register_renderer(store, XENT_CONTROL_SCROLL, flux_draw_scroll, flux_draw_scroll_overlay);
	flux_node_store_register_renderer(store, XENT_CONTROL_IMAGE, flux_draw_container, NULL);
	flux_node_store_register_renderer(store, XENT_CONTROL_LIST, flux_draw_container, NULL);
	flux_node_store_register_renderer(store, XENT_CONTROL_TAB, flux_draw_container, NULL);
	flux_node_store_register_renderer(store, XENT_CONTROL_CANVAS, flux_draw_container, NULL);
	flux_node_store_register_renderer(store, XENT_CONTROL_CUSTOM, flux_draw_container, NULL);
}

HRESULT flux_app_create(FluxAppConfig const *cfg, FluxApp **out) {
	if (!out) return E_INVALIDARG;
	*out         = NULL;

	FluxApp *app = ( FluxApp * ) calloc(1, sizeof(*app));
	if (!app) return E_OUTOFMEMORY;

	app->root = XENT_NODE_INVALID;

	FluxWindowConfig wcfg;
	memset(&wcfg, 0, sizeof(wcfg));
	wcfg.title     = cfg ? cfg->title : L"Fluxent";
	wcfg.width     = cfg ? cfg->width : 800;
	wcfg.height    = cfg ? cfg->height : 600;
	wcfg.dark_mode = cfg ? cfg->dark_mode : false;
	wcfg.backdrop  = cfg ? ( int ) cfg->backdrop : 0;
	wcfg.resizable = true;

	HRESULT hr     = flux_window_create(&wcfg, &app->window);
	if (FAILED(hr)) {
		free(app);
		return hr;
	}

	flux_window_set_render_callback(app->window, app_render, app);
	flux_window_set_pointer_callback(app->window, app_pointer, app);
	flux_window_set_key_callback(app->window, app_key, app);
	flux_window_set_char_callback(app->window, app_char, app);
	flux_window_set_setting_changed_callback(app->window, app_setting_changed, app);
	flux_window_set_ime_composition_callback(app->window, app_ime_composition, app);

	app->cache = flux_render_cache_create(512);
	app->theme = flux_theme_create();
	if (app->theme) flux_theme_set_mode(app->theme, FLUX_THEME_SYSTEM);

	app->tooltip = flux_tooltip_create(app->window);
	app_init_dmanip(app);

	*out = app;
	return S_OK;
}

static void app_cleanup_dmanip(FluxApp *app) {
	if (!app->dmanip) return;
	if (app->ctx && app->store && app->root != XENT_NODE_INVALID)
		flux_dmanip_cleanup_tree(app->ctx, app->store, app->root);
	flux_dmanip_destroy(app->dmanip);
	app->dmanip = NULL;
}

static void app_release_shared_brush(FluxApp *app) {
	if (!app->shared_brush) return;
	ID2D1SolidColorBrush_Release(app->shared_brush);
	app->shared_brush = NULL;
}

static void app_destroy_render_state(FluxApp *app) {
	app_release_shared_brush(app);
	if (app->cache) flux_render_cache_destroy(app->cache);
	if (app->theme) flux_theme_destroy(app->theme);
	if (app->engine) flux_engine_destroy(app->engine);
	if (app->text) flux_text_renderer_destroy(app->text);
}

static void app_destroy_input_state(FluxApp *app) {
	if (app->tooltip) flux_tooltip_destroy(app->tooltip);
	if (app->input) flux_input_destroy(app->input);
}

void flux_app_destroy(FluxApp *app) {
	if (!app) return;
	app_cleanup_dmanip(app);
	app_destroy_input_state(app);
	app_destroy_render_state(app);
	if (app->window) flux_window_destroy(app->window);
	free(app);
}

void flux_app_set_root(FluxApp *app, XentContext *ctx, XentNodeId root, FluxNodeStore *store) {
	if (!app) return;

	app->ctx   = ctx;
	app->root  = root;
	app->store = store;
	app_register_core_renderers(store);

	if (app->engine) flux_engine_destroy(app->engine);
	app->engine = flux_engine_create(store);

	if (app->input) flux_input_destroy(app->input);
	app->input = flux_input_create(ctx, store);

	if (app->text) flux_text_renderer_destroy(app->text);
	app->text = flux_text_renderer_create();
	if (app->text) flux_text_renderer_register(app->text, ctx);

	if (store) flux_node_store_attach_userdata(store, ctx);

	if (app->tooltip && app->text) flux_tooltip_set_text_renderer(app->tooltip, app->text);

	flux_window_request_render(app->window);
}

int flux_app_run(FluxApp *app) {
	if (!app || !app->window) return -1;
	return flux_window_run(app->window);
}

FluxEngine       *flux_app_get_engine(FluxApp *app) { return app ? app->engine : NULL; }

FluxWindow       *flux_app_get_window(FluxApp *app) { return app ? app->window : NULL; }

FluxInput        *flux_app_get_input(FluxApp *app) { return app ? app->input : NULL; }

FluxThemeManager *flux_app_get_theme(FluxApp *app) { return app ? app->theme : NULL; }

FluxTooltip      *flux_app_get_tooltip(FluxApp *app) { return app ? app->tooltip : NULL; }

FluxTextRenderer *flux_app_get_text_renderer(FluxApp *app) { return app ? app->text : NULL; }

XentContext      *flux_app_get_context(FluxApp *app) { return app ? app->ctx : NULL; }

FluxNodeStore    *flux_app_get_store(FluxApp *app) { return app ? app->store : NULL; }
