#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_graphics.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_text.h"
#include "flux_render_internal.h"
#include "input/flux_dmanip.h"
#include "fluxent/flux_theme.h"
#include "fluxent/flux_popup.h"
#include "fluxent/flux_tooltip.h"
#include "engine/flux_time.h"
#include "engine/flux_dmanip_sync.h"
#include "controls/flux_factory.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Renderer forward declarations (for lazy registration) ─────────── */
extern void
flux_draw_container(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void
flux_draw_scroll(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *, FluxControlState const *);
extern void flux_draw_scroll_overlay(FluxRenderContext const *, FluxRenderSnapshot const *, FluxRect const *);

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

	/* DirectManipulation manager (per-window).  NULL if creation failed
	 * (e.g. DManip unavailable) — in that case we fall back to the manual
	 * touch-pan path in flux_input.c. */
	FluxDManip           *dmanip;
};

static void app_render(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->ctx || app->root == XENT_NODE_INVALID) return;

	FluxGraphics *gfx   = flux_window_get_graphics(app->window);

	FluxSize      size  = flux_window_client_size(app->window);
	FluxDpiInfo   dpi   = flux_window_dpi(app->window);
	float         scale = dpi.dpi_x / 96.0f;
	float         w     = size.w / scale;
	float         h     = size.h / scale;

	/* Pump DManip first — delivers any pending content-updated callbacks
	 * so FluxScrollData.scroll_x/y is current before layout uses it. */
	if (app->dmanip) flux_dmanip_tick(app->dmanip);

	xent_layout(app->ctx, app->root, w, h);

	/* Sync every SCROLL node's DManip viewport against the new layout. */
	if (app->dmanip) flux_dmanip_sync_tree(app->dmanip, app->ctx, app->store, app->root, scale);

	if (app->store) flux_node_store_attach_userdata(app->store, app->ctx);

	if (app->cache) flux_render_cache_begin_frame(app->cache);

	flux_engine_collect(app->engine, app->ctx, app->root);

		/* Create shared brush on first use */
		if (!app->shared_brush) {
			ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
				if (d2d) {
					D2D1_COLOR_F          black = {0, 0, 0, 1};
					D2D1_BRUSH_PROPERTIES bp;
					bp.opacity       = 1.0f;
					bp.transform._11 = 1;
					bp.transform._12 = 0;
					bp.transform._21 = 0;
					bp.transform._22 = 1;
					bp.transform._31 = 0;
					bp.transform._32 = 0;
					ID2D1RenderTarget_CreateSolidColorBrush(
					  ( ID2D1RenderTarget * ) d2d, &black, &bp, &app->shared_brush
					);
				}
		}

	/* Compute frame delta time */
	int64_t now_ticks     = flux_perf_now();
	float   dt            = flux_compute_dt(app->last_frame_ticks, now_ticks);
	app->last_frame_ticks = now_ticks;

	/* Build render context */
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

		/* Keep tooltip rendering resources in sync */
		if (app->tooltip) {
			flux_tooltip_set_text_renderer(app->tooltip, app->text);
			flux_tooltip_set_theme(app->tooltip, rc.theme, rc.is_dark);
		}

	/* Begin draw -> clear -> execute -> end draw -> present */
	/* Note: DComp commit is not needed every frame - DComp handles sync internally */
	flux_graphics_begin_draw(gfx);
	flux_graphics_clear(gfx, flux_color_rgba(0, 0, 0, 0));

	flux_engine_execute(app->engine, &rc);

	flux_graphics_end_draw(gfx);
	flux_graphics_present(gfx, true);

	/* If any control reported active animations, request another frame */
	if (anims_active) flux_window_request_render(app->window);
}

/* ═══════════════════════════════════════════════════════════════════════
 *  Pointer event pipeline
 *
 *  Win32 messages arrive at flux_window.c and are translated into
 *  FluxPointerEvent structs.  This single handler is the ONLY consumer
 *  in the app layer — it runs policy (tooltip, cursor, light-dismiss,
 *  DManip hand-off) and then forwards the event to flux_input_dispatch
 *  which handles the per-control routing.
 *
 *  Keep this function readable: one `case` per event kind, no magic
 *  button numbers, no "if down / else up" branching.
 * ═══════════════════════════════════════════════════════════════════════ */

/* Update cursor shape + tooltip hover based on the currently hovered
 * node.  Called after every FLUX_POINTER_MOVE. */
static void app_update_cursor_and_tooltip(FluxApp *app, float x, float y, bool is_touch) {
	XentNodeId hovered = flux_input_get_hovered(app->input);

	int        cursor  = FLUX_CURSOR_ARROW;
		if (hovered != XENT_NODE_INVALID) {
			XentControlType ct = xent_get_control_type(app->ctx, hovered);
				switch (ct) {
					case XENT_CONTROL_TEXT_INPUT : {
						FluxNodeData *tb_nd     = flux_node_store_get(app->store, hovered);
						bool          tb_in_btn = false;
							if (tb_nd && tb_nd->hover_local_x >= 0.0f && tb_nd->state.focused) {
								XentRect tb_rect = {0};
								xent_get_layout_rect(app->ctx, hovered, &tb_rect);
								FluxTextBoxData *tbd          = ( FluxTextBoxData * ) tb_nd->component_data;
								bool             has_text_cur = (tbd && tbd->content && tbd->content [0]);
									if (has_text_cur && !tbd->readonly) {
										float del_start = tb_rect.width - 30.0f;
										if (tb_nd->hover_local_x >= del_start) tb_in_btn = true;
									}
							}
						cursor = tb_in_btn ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
						break;
					}
					case XENT_CONTROL_PASSWORD_BOX : {
						FluxNodeData *pb_nd     = flux_node_store_get(app->store, hovered);
						bool          pb_in_btn = false;
							if (pb_nd && pb_nd->state.focused) {
								FluxPasswordBoxData *pbd         = ( FluxPasswordBoxData * ) pb_nd->component_data;
								bool                 has_text_pb = (pbd && pbd->content && pbd->content [0]);
									if (has_text_pb) {
										XentRect pb_rect = {0};
										xent_get_layout_rect(app->ctx, hovered, &pb_rect);
										float reveal_start = pb_rect.width - 30.0f;
										if (pb_nd->hover_local_x >= reveal_start) pb_in_btn = true;
									}
							}
						cursor = pb_in_btn ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
						break;
					}
					case XENT_CONTROL_NUMBER_BOX : {
						FluxNodeData *nb_nd       = flux_node_store_get(app->store, hovered);
						bool          in_btn_area = false;
							if (nb_nd && nb_nd->hover_local_x >= 0.0f) {
								XentRect nb_rect = {0};
								xent_get_layout_rect(app->ctx, hovered, &nb_rect);
								bool  spin_inline = xent_get_semantic_expanded(app->ctx, hovered);
								float spin_w      = spin_inline ? 76.0f : 0.0f;
									if (spin_inline) {
										float spin_start = nb_rect.width - spin_w;
										if (nb_nd->hover_local_x >= spin_start) in_btn_area = true;
									}
									if (!in_btn_area && nb_nd->state.focused) {
										FluxTextBoxData *nbd = ( FluxTextBoxData * ) nb_nd->component_data;
										bool has_text_nb = (nbd && nbd->content && nbd->content [0] && !nbd->readonly);
											if (has_text_nb) {
												float del_start = nb_rect.width - 40.0f - spin_w;
												float del_end   = del_start + 40.0f;
												if (nb_nd->hover_local_x >= del_start && nb_nd->hover_local_x < del_end)
													in_btn_area = true;
											}
									}
							}
						cursor = in_btn_area ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
						break;
					}
				case XENT_CONTROL_BUTTON :
				case XENT_CONTROL_TOGGLE_BUTTON :
				case XENT_CONTROL_CHECKBOX :
				case XENT_CONTROL_RADIO :
				case XENT_CONTROL_SWITCH :
				case XENT_CONTROL_SLIDER :
				case XENT_CONTROL_HYPERLINK :
				case XENT_CONTROL_REPEAT_BUTTON : cursor = FLUX_CURSOR_HAND; break;
				default                         : break;
				}
		}
	flux_window_set_cursor(app->window, is_touch ? FLUX_CURSOR_ARROW : cursor);

	/* Tooltip: touch suppresses.  Mouse/pen anchors to cursor in screen
	 * coords so the balloon appears just below the pointer. */
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
			screen_bounds.h = 20.0f; /* cursor height — tooltip anchors below */
		}
	flux_tooltip_on_hover(app->tooltip, hovered, hover_nd, &screen_bounds);
}

static void app_pointer(void *ctx, FluxPointerEvent const *ev) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input || app->root == XENT_NODE_INVALID || !ev) return;

	bool const is_touch = (ev->device == FLUX_POINTER_TOUCH);

		switch (ev->kind) {
		case FLUX_POINTER_MOVE :
			flux_input_dispatch(app->input, app->root, ev);

				/* If fi_pointer_move just promoted a finger drag to a real
			     * pan (slop threshold crossed), hand the contact to DManip
			     * now — the input layer has already cancelled whatever
			     * control was pressed underneath, so it's safe to release
			     * the pid to the system. */
				if (is_touch && app->dmanip && ev->pointer_id != 0 && flux_input_take_pan_promoted(app->input)) {
					XentNodeId tgt = flux_input_get_touch_pan_target(app->input);
						if (tgt != XENT_NODE_INVALID) {
							FluxNodeData *nd = flux_node_store_get(app->store, tgt);
								if (nd && nd->component_data) {
									FluxScrollData *sd = ( FluxScrollData * ) nd->component_data;
										if (sd->dmanip_viewport) {
											FluxDManipViewport *vp = ( FluxDManipViewport * ) sd->dmanip_viewport;
											/* Push current offset into DManip so its
											 * transform starts from the right origin. */
											flux_dmanip_viewport_sync_transform(vp, sd->scroll_x, sd->scroll_y);
												if (SUCCEEDED(flux_dmanip_viewport_set_contact(vp, ev->pointer_id))) {
													/* DManip owns the gesture now; our manual
													 * touch-pan path must not also translate. */
													flux_input_clear_touch_pan(app->input);
													/* Windows stops delivering WM_POINTER* for
													 * this pid after SetContact; clear our
													 * primary_touch_pid latch so the next
													 * finger isn't mistaken for a 2nd contact. */
													flux_window_abandon_pointer(app->window, ev->pointer_id);
												}
										}
								}
						}
				}

			app_update_cursor_and_tooltip(app, ev->x, ev->y, is_touch);
			break;

		case FLUX_POINTER_DOWN :
			if (app->tooltip) flux_tooltip_dismiss(app->tooltip);

				/* Light-dismiss: primary press on the main window dismisses
			     * any open MenuFlyout.  The popup is WS_EX_NOACTIVATE so it
			     * never sees WM_ACTIVATE / WA_INACTIVE — we must do it here. */
				if (ev->changed_button == FLUX_POINTER_BUTTON_LEFT) {
					FluxMenuFlyout *m = flux_menu_flyout_get_active();
					if (m && flux_menu_flyout_is_visible(m)) flux_menu_flyout_dismiss(m);
				}

			flux_input_dispatch(app->input, app->root, ev);
			/* DManip handoff is intentionally NOT here.  Doing it on DOWN
			 * eats every pure tap (Windows stops delivering WM_POINTER*
			 * for the contact after SetContact, so the matching UP never
			 * fires and on_click never runs).  Instead we wait until
			 * fi_pointer_move declares the gesture a pan — see the MOVE
			 * branch below. */
			break;

		case FLUX_POINTER_UP        : flux_input_dispatch(app->input, app->root, ev); break;

			case FLUX_POINTER_WHEEL : {
				/* Light-dismiss menus on any scroll so they don't float detached
				 * from their anchor once the page moves underneath. */
				FluxMenuFlyout *m = flux_menu_flyout_get_active();
					if (m && flux_menu_flyout_is_visible(m)) {
						flux_menu_flyout_dismiss(m);
						flux_window_request_render(app->window);
						return;
					}
				flux_popup_dismiss_all_for_owner(flux_window_hwnd(app->window));
				flux_input_dispatch(app->input, app->root, ev);
				break;
			}

		case FLUX_POINTER_CONTEXT_MENU : flux_input_dispatch(app->input, app->root, ev); break;

		case FLUX_POINTER_CANCEL       : flux_input_dispatch(app->input, app->root, ev); break;
		}

	flux_window_request_render(app->window);
}

static void app_setting_changed(void *ctx) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app) return;

		/* Re-resolve system theme */
		if (app->theme) {
			flux_theme_set_mode(app->theme, FLUX_THEME_SYSTEM);
			bool dark = flux_theme_system_is_dark();
			flux_window_set_dark_mode(app->window, dark);
		}

	flux_window_request_render(app->window);
}

static void app_key(void *ctx, unsigned int vk, bool down) {
	FluxApp *app = ( FluxApp * ) ctx;
	if (!app || !app->input) return;

	/* Dismiss tooltip on any key press */
	if (down && app->tooltip) flux_tooltip_dismiss(app->tooltip);

		/* Forward to active MenuFlyout first — it consumes arrow/enter/escape */
		if (down) {
			FluxMenuFlyout *active_menu = flux_menu_flyout_get_active();
				if (active_menu && flux_menu_flyout_on_key(active_menu, vk)) {
					flux_window_request_render(app->window);
					return;
				}
		}

		if (down) {
				/* Focus navigation keys */
				switch (vk) {
				case VK_TAB :
					flux_input_tab(app->input, app->root, (GetKeyState(VK_SHIFT) & 0x8000) != 0);
					flux_window_request_render(app->window);
					return;
				case VK_LEFT :
				case VK_UP :
				case VK_RIGHT :
					case VK_DOWN : {
						/* Only use arrow keys for focus navigation if the focused node
						   is NOT a text input (text inputs need arrow keys for cursor). */
						XentNodeId focused = flux_input_get_focused(app->input);
							if (focused != XENT_NODE_INVALID && app->ctx) {
								XentControlType ct = xent_get_control_type(app->ctx, focused);
									if (ct != XENT_CONTROL_TEXT_INPUT
										&& ct != XENT_CONTROL_PASSWORD_BOX
										&& ct != XENT_CONTROL_NUMBER_BOX)
									{
										flux_input_arrow(app->input, app->root, ( int ) vk);
										flux_window_request_render(app->window);
										return;
									}
							}
						break;
					}
				case VK_RETURN :
					case VK_SPACE : {
						XentNodeId focused = flux_input_get_focused(app->input);
							if (focused != XENT_NODE_INVALID && app->ctx) {
								XentControlType ct = xent_get_control_type(app->ctx, focused);
									/* Don't intercept Enter/Space for text inputs */
									if (ct != XENT_CONTROL_TEXT_INPUT
										&& ct != XENT_CONTROL_PASSWORD_BOX
										&& ct != XENT_CONTROL_NUMBER_BOX)
									{
										flux_input_activate(app->input);
										flux_window_request_render(app->window);
										return;
									}
							}
						break;
					}
				case VK_ESCAPE :
					flux_input_escape(app->input);
					flux_window_request_render(app->window);
					return;
				default : break;
				}
			flux_input_key_down(app->input, vk);
		}
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

HRESULT flux_app_create(FluxAppConfig const *cfg, FluxApp **out) {
	if (!out) return E_INVALIDARG;
	*out         = NULL;

	FluxApp *app = ( FluxApp * ) calloc(1, sizeof(*app));
	if (!app) return E_OUTOFMEMORY;

	app->root = XENT_NODE_INVALID;

	/* Register core renderers that have no dedicated flux_create_*() factory */
	flux_register_renderer(XENT_CONTROL_CONTAINER, flux_draw_container, NULL);
	flux_register_renderer(XENT_CONTROL_SCROLL, flux_draw_scroll, flux_draw_scroll_overlay);
	flux_register_renderer(XENT_CONTROL_IMAGE, flux_draw_container, NULL);
	flux_register_renderer(XENT_CONTROL_LIST, flux_draw_container, NULL);
	flux_register_renderer(XENT_CONTROL_TAB, flux_draw_container, NULL);
	flux_register_renderer(XENT_CONTROL_CANVAS, flux_draw_container, NULL);
	flux_register_renderer(XENT_CONTROL_CUSTOM, flux_draw_container, NULL);

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

	/* Create app-level tooltip system */
	app->tooltip = flux_tooltip_create(app->window);

	/* ── DirectManipulation ─────────────────────────────────────────
	 *
	 * Per-window DManip manager.  Touch-pan on a ScrollView hands off
	 * the pointer contact to DManip for WinUI-grade inertia, axis rails,
	 * and edge rubber-band.  The wrapper silently no-ops when creation
	 * fails (e.g. sandboxed context, E_ACCESSDENIED), at which point
	 * flux_input.c's manual touch-pan remains the fallback.
	 *
	 * Opt-out via FLUXENT_DISABLE_DMANIP=1 for debugging. */
	{
		char  val [8]  = {0};
		DWORD n        = GetEnvironmentVariableA("FLUXENT_DISABLE_DMANIP", val, sizeof(val));
		bool  disabled = (n > 0 && val [0] == '1');
			if (!disabled) {
				HWND dm_hwnd = flux_window_hwnd(app->window);
				if (dm_hwnd) ( void ) flux_dmanip_create(dm_hwnd, &app->dmanip);
			}
	}

	*out = app;
	return S_OK;
}

void flux_app_destroy(FluxApp *app) {
	if (!app) return;
	if (app->tooltip) flux_tooltip_destroy(app->tooltip);
		if (app->shared_brush) {
			ID2D1SolidColorBrush_Release(app->shared_brush);
			app->shared_brush = NULL;
		}
	if (app->cache) flux_render_cache_destroy(app->cache);
	if (app->theme) flux_theme_destroy(app->theme);
	if (app->engine) flux_engine_destroy(app->engine);
	if (app->input) flux_input_destroy(app->input);
	if (app->text) flux_text_renderer_destroy(app->text);
	/* Destroy any per-SCROLL DManip viewports before the manager.  The
	 * node store itself doesn't know about DManip so it won't clean them
	 * up on its own; we walk whatever tree is still attached. */
	if (app->dmanip && app->ctx && app->store && app->root != XENT_NODE_INVALID)
		flux_dmanip_cleanup_tree(app->ctx, app->store, app->root);
	/* DManip must be destroyed *before* the window (its event handlers
	 * hold a back-ref to the HWND via the manager). */
	if (app->dmanip) flux_dmanip_destroy(app->dmanip);
	if (app->window) flux_window_destroy(app->window);
	free(app);
}

void flux_app_set_root(FluxApp *app, XentContext *ctx, XentNodeId root, FluxNodeStore *store) {
	if (!app) return;

	app->ctx   = ctx;
	app->root  = root;
	app->store = store;

	if (app->engine) flux_engine_destroy(app->engine);
	app->engine = flux_engine_create(store);

	if (app->input) flux_input_destroy(app->input);
	app->input = flux_input_create(ctx, store);

	if (app->text) flux_text_renderer_destroy(app->text);
	app->text = flux_text_renderer_create();
	if (app->text) flux_text_renderer_register(app->text, ctx);

	if (store) flux_node_store_attach_userdata(store, ctx);

	/* Eagerly set tooltip text renderer so it's available before first render */
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
