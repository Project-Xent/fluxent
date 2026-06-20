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
#include "app/flux_scene.h"
#include "runtime/flux_time.h"
#include "input/flux_dmanip_sync.h"
#include "controls/behavior/flux_control_cursor.h"
#include "controls/draw/flux_control_draw.h"
#include "controls/factory/flux_factory.h"
#include "compose/flux_compose_render.h"
#include "compose/flux_uia.h"
#include "graphics/flux_graphics_compose.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

#define FLUX_APP_DISABLE_DMANIP_ENV_CAP 8
#define FLUX_APP_DEFAULT_WIDTH          800
#define FLUX_APP_DEFAULT_HEIGHT         600
#define FLUX_APP_RENDER_CACHE_CAPACITY  512
#define FLUX_APP_SCENE_INITIAL_CAPACITY 512
#define FLUX_APP_TOOLTIP_ANCHOR_H       20.0f

/** @brief Present the frame with vsync enabled (default FluxApp render path). */
static bool const                kFluxAppPresentUseVsync = true;

typedef struct FluxRenderBackend FluxRenderBackend;

struct FluxApp {
	FluxWindow              *window;
	FluxEngine              *engine;
	FluxControlRegistry     *registry; /**< Control type → renderer table; built once, shared by both render paths. */
	FluxInput               *input;
	FluxTextRenderer        *text;
	FluxRenderCache         *cache;
	FluxThemeManager        *theme;
	FluxScene               *scene;

	ID2D1SolidColorBrush    *shared_brush;
	int64_t                  last_frame_ticks;
	XentNodeId               last_uia_focus; /**< Last node reported to UIA as focused (focus-event de-dup). */

	FluxTooltip             *tooltip;

	FluxDManip              *dmanip;

	FluxComposeRender       *compose; /**< Retained-composition path (FLUX_USE_COMPOSITION). */
	FluxRenderBackend const *backend; /**< Chosen render strategy (d2d or composition); set on first frame. */

	void                     (*frame_cb)(void *ctx); /**< Runs at frame start, before layout (xtk message pump). */
	void                    *frame_cb_ctx;
};

static XentContext   *app_ctx(FluxApp const *app) { return app ? flux_scene_context(app->scene) : NULL; }

static FluxNodeStore *app_store(FluxApp const *app) { return app ? flux_scene_store(app->scene) : NULL; }

static XentNodeId     app_root(FluxApp const *app) { return app ? flux_scene_root(app->scene) : XENT_NODE_INVALID; }

static void           app_ensure_shared_brush(FluxApp *app, FluxGraphics *gfx) {
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
	if (flux_dmanip_handoff_touch_pan(app->dmanip, app->input, app_store(app), ev->pointer_id))
		flux_window_abandon_pointer(app->window, ev->pointer_id);
}

/* Composition counterpart of the DManip handoff: when a touch pan is promoted over
 * a scroll node, hand the pointer to that node's InteractionTracker. */
static void app_try_interaction_handoff(FluxApp *app, FluxPointerEvent const *ev) {
	if (!app->compose || !flux_input_take_pan_promoted(app->input)) return;
	XentNodeId target = flux_input_get_touch_pan_target(app->input);
	if (target == XENT_NODE_INVALID) return;
	if (flux_compose_render_redirect_scroll(app->compose, target, ev->pointer_id))
		flux_window_abandon_pointer(app->window, ev->pointer_id);
}

static void app_init_dmanip(FluxApp *app) {
	char  val [FLUX_APP_DISABLE_DMANIP_ENV_CAP] = {0};
	DWORD n                                     = GetEnvironmentVariableA("FLUXENT_DISABLE_DMANIP", val, sizeof(val));
	if (n > 0 && val [0] == '1') return;
	HWND hwnd = flux_window_hwnd(app->window);
	if (hwnd) ( void ) flux_dmanip_create(hwnd, &app->dmanip);
}

static bool app_focused_accepts_command(FluxApp *app) {
	XentNodeId focused = flux_input_get_focused(app->input);
	if (focused == XENT_NODE_INVALID || !app_ctx(app)) return false;

	FluxControlType ct = flux_get_control_type(app_ctx(app), focused);
	return ct != FLUX_CONTROL_TEXT_INPUT && ct != FLUX_CONTROL_PASSWORD_BOX && ct != FLUX_CONTROL_NUMBER_BOX;
}

static void app_request_render(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (app) flux_app_request_render(app);
}

static float    app_dpi_scale(FluxDpiInfo dpi) { return dpi.dpi_x / FLUX_DPI_BASE; }

static FluxSize app_client_dips(FluxApp const *app, FluxDpiInfo dpi) {
	FluxSize px    = flux_window_client_size(app->window);
	float    scale = app_dpi_scale(dpi);
	return (FluxSize) {px.w / scale, px.h / scale};
}

static bool app_theme_is_dark(FluxThemeManager *theme) {
	if (!theme) return false;

	FluxThemeMode mode = flux_theme_get_mode(theme);
	return (mode == FLUX_THEME_DARK) || (mode == FLUX_THEME_SYSTEM && flux_theme_system_is_dark());
}

static void app_prepare_layout_tree(FluxApp *app, FluxDpiInfo dpi) {
	FluxSize       dips  = app_client_dips(app, dpi);
	XentContext   *ctx   = app_ctx(app);
	FluxNodeStore *store = app_store(app);
	XentNodeId     root  = app_root(app);
	float          scale = app_dpi_scale(dpi);

	if (app->dmanip) flux_dmanip_tick(app->dmanip);
	xent_layout(ctx, root, dips.w, dips.h);
	if (app->dmanip) flux_dmanip_sync_tree(app->dmanip, ctx, store, root, scale);
	if (store) flux_node_store_attach_userdata(store, ctx);
}

static FluxRenderContext app_render_context(FluxApp *app, FluxGraphics *gfx, FluxDpiInfo dpi, int64_t now_ticks) {
	FluxRenderContext rc;
	memset(&rc, 0, sizeof(rc));
	rc.d2d     = flux_graphics_get_d2d_context(gfx);
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

static void app_sync_tooltip_theme(FluxApp *app, FluxRenderContext const *rc) {
	if (!app->tooltip) return;

	flux_tooltip_set_text_renderer(app->tooltip, app->text);
	flux_tooltip_set_theme(app->tooltip, rc->theme, rc->is_dark);
}

static void app_ensure_compose(FluxApp *app, FluxGraphics *gfx) {
	if (app->compose) return;
	FluxCompositor                *c    = flux_graphics_compositor(gfx);
	WUC_CompositionGraphicsDevice *gdev = flux_graphics_graphics_device(gfx);
	WUC_Visual                    *root = flux_graphics_composition_root(gfx);
	if (c && gdev && root) app->compose = flux_compose_render_create(c, gdev, root, app->registry);
}

/* Retained-composition frame: reconcile the layout into a WUC visual tree and
 * paint each node into its own surface via the existing renderers. The swap
 * chain / immediate-mode execute path is bypassed entirely in this mode. */
static void app_render_composition(FluxApp *app, FluxGraphics *gfx, FluxDpiInfo dpi) {
	app_prepare_layout_tree(app, dpi);
	if (app->cache) flux_render_cache_begin_frame(app->cache);
	app_ensure_compose(app, gfx);
	if (!app->compose) return;

	int64_t           now_ticks = flux_perf_now();
	FluxRenderContext rc        = app_render_context(app, gfx, dpi, now_ticks);
	app->last_frame_ticks       = now_ticks;

	bool anims_active           = false;
	rc.animations_active        = &anims_active;
	app_sync_tooltip_theme(app, &rc);

	flux_compose_render_frame(app->compose, app_ctx(app), app_store(app), app_root(app), &rc);

	if (anims_active) flux_app_request_render(app);
}

/* Immediate-mode swapchain frame: collect the layout into a command list and
 * execute it into the D2D swap chain. The classic (default) render path. */
static void app_render_classic(FluxApp *app, FluxGraphics *gfx, FluxDpiInfo dpi) {
	app_prepare_layout_tree(app, dpi);
	if (app->cache) flux_render_cache_begin_frame(app->cache);

	flux_engine_collect(app->engine, app_ctx(app), app_root(app));
	app_ensure_shared_brush(app, gfx);

	int64_t           now_ticks = flux_perf_now();
	FluxRenderContext rc        = app_render_context(app, gfx, dpi, now_ticks);
	app->last_frame_ticks       = now_ticks;

	bool anims_active           = false;
	rc.animations_active        = &anims_active;
	app_sync_tooltip_theme(app, &rc);

	flux_graphics_begin_draw(gfx);
	flux_graphics_clear(gfx, flux_color_rgba(0, 0, 0, 0));
	flux_engine_execute(app->engine, &rc);
	flux_graphics_end_draw(gfx);
	flux_graphics_present(gfx, kFluxAppPresentUseVsync);

	if (anims_active) flux_app_request_render(app);
}

/* The render backend is the cohesive strategy chosen once by capability: the
 * immediate-mode D2D swapchain path, or the retained WUC composition path. The
 * animator (tween / flux_compose_anim) and scroller (DManip / InteractionTracker)
 * are facets of the same choice and join this strategy as they are abstracted. */
struct FluxRenderBackend {
	char const *name;
	void        (*render_frame)(FluxApp *app, FluxGraphics *gfx, FluxDpiInfo dpi);
};

static FluxRenderBackend const  g_backend_d2d         = {"d2d", app_render_classic};
static FluxRenderBackend const  g_backend_composition = {"composition", app_render_composition};

/* Capability gate for the composition backend. It costs per-node GPU surfaces
 * (memory scales with visible nodes), so fall back to the immediate-mode d2d
 * backend where composition is a poor fit: not requested, or a remote session
 * (DWM composition degrades badly over RDP). */
static FluxRenderBackend const *app_select_backend(FluxGraphics *gfx) {
	if (!flux_graphics_composition_enabled(gfx)) return &g_backend_d2d;
	if (GetSystemMetrics(SM_REMOTESESSION)) return &g_backend_d2d;
	return &g_backend_composition;
}

static void app_pump_uia_focus(FluxApp *app) {
	XentNodeId focused = flux_input_get_focused(app->input);
	if (focused == app->last_uia_focus) return;
	app->last_uia_focus = focused;

	FluxUiaContext info = {app_ctx(app), app_root(app), flux_app_get_hwnd(app), app_store(app)};
	flux_uia_raise_focus_changed(&info, focused);
}

static void app_render(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app_ctx(app) || app_root(app) == XENT_NODE_INVALID) return;

	if (app->frame_cb) app->frame_cb(app->frame_cb_ctx);
	app_pump_uia_focus(app);

	FluxGraphics *gfx = flux_app_get_graphics(app);
	FluxDpiInfo   dpi = flux_window_dpi(app->window);

	if (!app->backend) app->backend = app_select_backend(gfx);
	app->backend->render_frame(app, gfx, dpi);
}

static void app_update_cursor_and_tooltip(FluxApp *app, float x, float y, bool is_touch) {
	XentNodeId hovered = flux_input_get_hovered(app->input);
	int        cursor  = flux_control_cursor_for_node(app_ctx(app), app_store(app), hovered);
	flux_window_set_cursor(app->window, is_touch ? FLUX_CURSOR_ARROW : cursor);

	if (!app->tooltip) return;
	if (is_touch) {
		flux_tooltip_on_hover(app->tooltip, XENT_NODE_INVALID, NULL, NULL);
		return;
	}

	FluxNodeData *hover_nd      = (hovered != XENT_NODE_INVALID) ? flux_node_store_get(app_store(app), hovered) : NULL;
	FluxRect      screen_bounds = {0};
	if (hovered != XENT_NODE_INVALID) {
		HWND        hwnd   = flux_window_hwnd(app->window);
		FluxDpiInfo tdpi   = flux_window_dpi(app->window);
		float       tscale = tdpi.dpi_x / FLUX_DPI_BASE;
		POINT       pt     = {( LONG ) (x * tscale), ( LONG ) (y * tscale)};
		ClientToScreen(hwnd, &pt);
		screen_bounds.x = ( float ) pt.x;
		screen_bounds.y = ( float ) pt.y;
		screen_bounds.h = FLUX_APP_TOOLTIP_ANCHOR_H;
	}
	flux_tooltip_on_hover(app->tooltip, hovered, hover_nd, &screen_bounds);
}

static bool app_active_menu_visible(FluxApp *app, FluxMenuFlyout **out_menu) {
	FluxMenuFlyout *menu = flux_app_get_active_menu_flyout(app);
	if (out_menu) *out_menu = menu;
	return menu && flux_menu_flyout_is_visible(menu);
}

static void app_pointer_move(FluxApp *app, FluxPointerEvent const *ev, bool is_touch) {
	flux_input_dispatch(app->input, app_root(app), ev);
	if (is_touch && app->compose) app_try_interaction_handoff(app, ev);
	else if (is_touch) app_try_dmanip_handoff(app, ev);
	app_update_cursor_and_tooltip(app, ev->x, ev->y, is_touch);
}

/* WinUI light-dismiss: a press or wheel landing outside an open flyout / MenuFlyout /
 * ComboBox drop-down closes it (and its whole submenu chain) and is absorbed -- it does
 * not also activate the control underneath. Every popup is a separate window, so any
 * input in the main window is by definition outside them. Returns true if anything was
 * dismissed (and the caller should swallow the event). */
static bool app_lightdismiss_popups(FluxApp *app) {
	if (!flux_popup_lightdismiss_visible(flux_app_get_hwnd(app))) return false;
	FluxMenuFlyout *menu;
	if (app_active_menu_visible(app, &menu)) flux_menu_flyout_dismiss_all(menu);
	flux_popup_dismiss_all_for_owner(flux_app_get_hwnd(app));
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
	case FLUX_POINTER_CONTEXT_MENU :
	case FLUX_POINTER_CANCEL       : app_pointer_default(app, ev); break;
	}
}

static void app_pointer(void *ctx, FluxPointerEvent const *ev) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input || app_root(app) == XENT_NODE_INVALID || !ev) return;

	bool const is_touch = (ev->device == FLUX_POINTER_TOUCH);
	app_pointer_route(app, ev, is_touch);
	flux_app_request_render(app);
}

static void app_setting_changed(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app) return;
	if (app->theme) {
		flux_theme_set_mode(app->theme, FLUX_THEME_SYSTEM);
		flux_window_set_dark_mode(app->window, flux_theme_system_is_dark());
	}
	flux_app_request_render(app);
}

static bool app_handle_active_menu_key(FluxApp *app, unsigned int vk) {
	FluxMenuFlyout *active_menu = flux_app_get_active_menu_flyout(app);
	return active_menu && flux_menu_flyout_on_key(active_menu, vk);
}

static bool app_handle_arrow_key(FluxApp *app, unsigned int vk) {
	if (vk != VK_LEFT && vk != VK_UP && vk != VK_RIGHT && vk != VK_DOWN) return false;
	if (!app_focused_accepts_command(app)) return false;

	flux_input_arrow(app->input, app_root(app), ( int ) vk);
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
		flux_input_tab(app->input, app_root(app), (GetKeyState(VK_SHIFT) & 0x8000) != 0);
		return true;
	}
	/* The focused control gets first refusal (Slider arrows, ComboBox list, text edit);
	 * only keys it leaves unconsumed fall through to app-level navigation below. */
	if (flux_input_key_down(app->input, vk)) return true;
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
	if (down) app_handle_key_command(app, vk);
	else flux_input_key_up(app->input, vk);
	flux_app_request_render(app);
}

static void app_char(void *ctx, wchar_t ch) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;
	flux_input_char(app->input, ch);
	flux_app_request_render(app);
}

static void app_ime_composition(void *ctx, wchar_t const *text, int cursor_pos) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;
	if (text) flux_input_ime_composition(app->input, text, ( uint32_t ) wcslen(text), ( uint32_t ) cursor_pos);
	else flux_input_ime_end(app->input);
	flux_app_request_render(app);
}

static void app_cleanup_dmanip_tree(FluxApp *app) {
	if (app->dmanip && app_ctx(app) && app_store(app) && app_root(app) != XENT_NODE_INVALID)
		flux_dmanip_cleanup_tree(app_ctx(app), app_store(app), app_root(app));
}

static void app_destroy_scene_runtime(FluxApp *app) {
	if (app->engine) flux_engine_destroy(app->engine);
	if (app->input) flux_input_destroy(app->input);
	if (app->text) flux_text_renderer_destroy(app->text);
	app->engine = NULL;
	app->input  = NULL;
	app->text   = NULL;
}

static void app_on_node_removed(void *userdata, XentNodeId id) {
	FluxApp *app = ( FluxApp * ) userdata;
	flux_input_node_destroyed(app->input, id);
	if (app->cache) flux_render_cache_remove(app->cache, id);
}

static bool app_bind_scene_runtime(FluxApp *app) {
	XentContext   *ctx   = app_ctx(app);
	FluxNodeStore *store = app_store(app);
	if (!ctx || !store || app_root(app) == XENT_NODE_INVALID) return false;

	app->engine = flux_engine_create(store, app->registry);
	app->input  = flux_input_create(ctx, store);
	app->text   = flux_text_renderer_create();
	if (app->text) flux_text_renderer_register(app->text, ctx);

	flux_node_store_attach_userdata(store, ctx);
	flux_node_store_set_remove_listener(store, app_on_node_removed, app);
	if (app->tooltip && app->text) flux_tooltip_set_text_renderer(app->tooltip, app->text);

	return app->engine && app->input;
}

static bool app_replace_scene(FluxApp *app, FluxScene *scene) {
	if (!app || !scene) return false;

	app_cleanup_dmanip_tree(app);
	app_destroy_scene_runtime(app);
	flux_scene_destroy(app->scene);
	app->scene = scene;

	if (!app_bind_scene_runtime(app)) return false;
	flux_app_request_render(app);
	return true;
}

static FluxWindowConfig app_window_config(FluxAppConfig const *cfg) {
	FluxWindowConfig wcfg;
	memset(&wcfg, 0, sizeof(wcfg));
	wcfg.title     = cfg ? cfg->title : L"Fluxent";
	wcfg.width     = cfg ? cfg->width : FLUX_APP_DEFAULT_WIDTH;
	wcfg.height    = cfg ? cfg->height : FLUX_APP_DEFAULT_HEIGHT;
	wcfg.dark_mode = cfg ? cfg->dark_mode : false;
	wcfg.backdrop  = cfg ? ( int ) cfg->backdrop : 0;
	wcfg.resizable = true;
	return wcfg;
}

static bool app_create_window(FluxApp *app, FluxAppConfig const *cfg) {
	FluxWindowConfig wcfg = app_window_config(cfg);
	HRESULT          hr   = flux_window_create(&wcfg, &app->window);
	return SUCCEEDED(hr);
}

static void *app_uia_provider(void *ctx, void *hwnd) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app_ctx(app) || app_root(app) == XENT_NODE_INVALID) return NULL;

	FluxUiaContext info = {app_ctx(app), app_root(app), ( HWND ) hwnd, app_store(app)};
	return flux_uia_provider_create(&info);
}

static void app_bind_window_callbacks(FluxApp *app) {
	flux_window_set_render_callback(app->window, app_render, app);
	flux_window_set_pointer_callback(app->window, app_pointer, app);
	flux_window_set_key_callback(app->window, app_key, app);
	flux_window_set_char_callback(app->window, app_char, app);
	flux_window_set_setting_changed_callback(app->window, app_setting_changed, app);
	flux_window_set_ime_composition_callback(app->window, app_ime_composition, app);
	flux_window_set_uia_provider_callback(app->window, app_uia_provider, app);
	flux_graphics_set_redraw_callback(flux_window_get_graphics(app->window), app_request_render, app);
}

static void app_create_runtime_services(FluxApp *app) {
	app->registry = flux_control_registry_create();
	if (app->registry) flux_register_builtins(app->registry);
	app->cache = flux_render_cache_create(FLUX_APP_RENDER_CACHE_CAPACITY);
	app->theme = flux_theme_create();
	if (app->theme) flux_theme_set_mode(app->theme, FLUX_THEME_SYSTEM);
	app->tooltip = flux_tooltip_create(app->window);
	app_init_dmanip(app);
}

static bool app_create_scene(FluxApp *app) {
	app->scene = flux_scene_create(FLUX_APP_SCENE_INITIAL_CAPACITY);
	return app->scene && app_bind_scene_runtime(app);
}

FluxApp *flux_app_create(FluxAppConfig const *cfg) {
	FluxApp *app = ( FluxApp * ) calloc(1, sizeof(*app));
	if (!app) return NULL;

	if (!app_create_window(app, cfg)) {
		free(app);
		return NULL;
	}

	app_bind_window_callbacks(app);
	app_create_runtime_services(app);
	if (!app_create_scene(app)) {
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
	/* Destroy before the window/graphics: holds WUC visuals tied to the target. */
	if (app->compose) flux_compose_render_destroy(app->compose);
	app->compose = NULL;
	app_release_shared_brush(app);
	if (app->cache) flux_render_cache_destroy(app->cache);
	if (app->theme) flux_theme_destroy(app->theme);
	if (app->engine) flux_engine_destroy(app->engine);
	flux_control_registry_destroy(app->registry); /* after both render paths that reference it */
	app->registry = NULL;
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
	flux_scene_destroy(app->scene);
	if (app->window) flux_window_destroy(app->window);
	free(app);
}

void flux_app_set_root(FluxApp *app, XentContext *ctx, XentNodeId root, FluxNodeStore *store) {
	if (!app) return;

	FluxScene *scene = flux_scene_borrow(ctx, store, root);
	if (!scene) return;
	( void ) app_replace_scene(app, scene);
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

XentContext      *flux_app_get_context(FluxApp *app) { return app ? app_ctx(app) : NULL; }

FluxNodeStore    *flux_app_get_store(FluxApp *app) { return app ? app_store(app) : NULL; }

XentNodeId        flux_app_get_root(FluxApp *app) { return app ? app_root(app) : XENT_NODE_INVALID; }

void              flux_app_request_render(FluxApp *app) {
	if (!app || !app->window) return;
	flux_window_request_render(app->window);
}

void flux_app_set_frame_callback(FluxApp *app, void (*cb)(void *ctx), void *ctx) {
	if (!app) return;
	app->frame_cb     = cb;
	app->frame_cb_ctx = ctx;
}

FluxGraphics *flux_app_get_graphics(FluxApp *app) {
	return app && app->window ? flux_window_get_graphics(app->window) : NULL;
}

FluxMenuFlyout *flux_app_get_active_menu_flyout(FluxApp *app) {
	return app && app->window ? flux_menu_flyout_get_active(app->window) : NULL;
}

HWND flux_app_get_hwnd(FluxApp *app) { return app && app->window ? flux_window_hwnd(app->window) : NULL; }
