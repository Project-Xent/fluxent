#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_graphics.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_text.h"
#include "flux_render_internal.h"
#include "fluxent/flux_theme.h"
#include "fluxent/flux_popup.h"

#include <math.h>
#include <shellapi.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ── Renderer forward declarations (for lazy registration) ─────────── */
extern void flux_draw_container(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_text(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_button(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_toggle(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_checkbox(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_radio(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_switch(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_slider(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_textbox(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_scroll(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_scroll_overlay(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *);
extern void flux_draw_progress(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_card(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_divider(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_hyperlink(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_password_box(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_number_box(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_progress_ring(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);
extern void flux_draw_info_badge(const FluxRenderContext *, const FluxRenderSnapshot *, const FluxRect *, const FluxControlState *);

/* Frame timing via QueryPerformanceCounter */
static int64_t flux_perf_freq(void) {
    static int64_t freq = 0;
    if (freq == 0) {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        freq = f.QuadPart;
    }
    return freq;
}

static int64_t flux_perf_now(void) {
    LARGE_INTEGER t;
    QueryPerformanceCounter(&t);
    return t.QuadPart;
}

static double flux_perf_seconds(int64_t ticks) {
    int64_t freq = flux_perf_freq();
    if (freq == 0) return 0.0;
    return (double)ticks / (double)freq;
}

static float flux_compute_dt(int64_t prev, int64_t now) {
    if (prev == 0) return 0.0f;
    int64_t freq = flux_perf_freq();
    if (freq == 0) return 0.016f;
    float dt = (float)(now - prev) / (float)freq;
    /* Clamp to sane range: avoid huge jumps on first frame or after stalls */
    if (dt < 0.0f) dt = 0.0f;
    if (dt > 0.1f) dt = 0.1f;
    return dt;
}

struct FluxApp {
    FluxWindow       *window;
    FluxEngine       *engine;
    FluxInput        *input;
    FluxTextRenderer *text;
    FluxRenderCache  *cache;
    FluxThemeManager *theme;

    XentContext       *ctx;
    XentNodeId         root;
    FluxNodeStore     *store;

    ID2D1SolidColorBrush *shared_brush;
    int64_t           last_frame_ticks;
};

static void app_render(void *ctx) {
    FluxApp *app = (FluxApp *)ctx;
    if (!app || !app->ctx || app->root == XENT_NODE_INVALID) {
        return;
    }

    FluxGraphics *gfx = flux_window_get_graphics(app->window);

    FluxSize size = flux_window_client_size(app->window);
    FluxDpiInfo dpi = flux_window_dpi(app->window);
    float scale = dpi.dpi_x / 96.0f;
    float w = size.w / scale;
    float h = size.h / scale;

    xent_layout(app->ctx, app->root, w, h);

    if (app->store)
        flux_node_store_attach_userdata(app->store, app->ctx);

    if (app->cache)
        flux_render_cache_begin_frame(app->cache);

    flux_engine_collect(app->engine, app->ctx, app->root);

    /* Create shared brush on first use */
    if (!app->shared_brush) {
        ID2D1DeviceContext *d2d = flux_graphics_get_d2d_context(gfx);
        if (d2d) {
            D2D1_COLOR_F black = {0, 0, 0, 1};
            D2D1_BRUSH_PROPERTIES bp;
            bp.opacity = 1.0f;
            bp.transform._11 = 1; bp.transform._12 = 0;
            bp.transform._21 = 0; bp.transform._22 = 1;
            bp.transform._31 = 0; bp.transform._32 = 0;
            ID2D1RenderTarget_CreateSolidColorBrush(
                (ID2D1RenderTarget *)d2d, &black, &bp, &app->shared_brush);
        }
    }

    /* Compute frame delta time */
    int64_t now_ticks = flux_perf_now();
    float dt = flux_compute_dt(app->last_frame_ticks, now_ticks);
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
    rc.is_dark = app->theme && flux_theme_get_mode(app->theme) == FLUX_THEME_DARK;
    bool anims_active = false;
    rc.animations_active = &anims_active;

    /* Begin draw -> clear -> execute -> end draw -> present -> commit */
    flux_graphics_begin_draw(gfx);
    flux_graphics_clear(gfx, flux_color_rgba(0, 0, 0, 0));

    flux_engine_execute(app->engine, &rc);

    flux_graphics_end_draw(gfx);
    flux_graphics_present(gfx, true);
    flux_graphics_commit(gfx);

    /* If any control reported active animations, request another frame */
    if (anims_active)
        flux_window_request_render(app->window);
}

static void app_mouse(void *ctx, float x, float y, int button, bool down) {
    FluxApp *app = (FluxApp *)ctx;
    if (!app || !app->input || app->root == XENT_NODE_INVALID)
        return;

    if (button == -1) {
        flux_input_pointer_move(app->input, app->root, x, y);

        /* Switch cursor shape based on hovered control type */
        XentNodeId hovered = flux_input_get_hovered(app->input);
        int cursor = FLUX_CURSOR_ARROW;
        if (hovered != XENT_NODE_INVALID) {
            XentControlType ct = xent_get_control_type(app->ctx, hovered);
            switch (ct) {
            case XENT_CONTROL_TEXT_INPUT: {
                /* I-beam in text area; arrow over delete button */
                FluxNodeData *tb_nd = flux_node_store_get(app->store, hovered);
                bool tb_in_btn = false;
                if (tb_nd && tb_nd->hover_local_x >= 0.0f && tb_nd->state.focused) {
                    XentRect tb_rect = {0};
                    xent_get_layout_rect(app->ctx, hovered, &tb_rect);
                    /* Check if text present (content pointer is non-null and non-empty) */
                    FluxTextBoxData *tbd = (FluxTextBoxData *)tb_nd->component_data;
                    bool has_text_cur = (tbd && tbd->content && tbd->content[0]);
                    if (has_text_cur && !tbd->readonly) {
                        float del_start = tb_rect.width - 30.0f;
                        if (tb_nd->hover_local_x >= del_start)
                            tb_in_btn = true;
                    }
                }
                cursor = tb_in_btn ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
                break;
            }
            case XENT_CONTROL_PASSWORD_BOX: {
                /* Arrow over reveal button (rightmost 30px when focused+has_text),
                   I-beam everywhere else — mirrors TextBox delete-button logic. */
                FluxNodeData *pb_nd = flux_node_store_get(app->store, hovered);
                bool pb_in_btn = false;
                if (pb_nd && pb_nd->state.focused) {
                    FluxPasswordBoxData *pbd = (FluxPasswordBoxData *)pb_nd->component_data;
                    bool has_text_pb = (pbd && pbd->content && pbd->content[0]);
                    if (has_text_pb) {
                        XentRect pb_rect = {0};
                        xent_get_layout_rect(app->ctx, hovered, &pb_rect);
                        float reveal_start = pb_rect.width - 30.0f; /* PB_REVEAL_BTN_W */
                        if (pb_nd->hover_local_x >= reveal_start)
                            pb_in_btn = true;
                    }
                }
                cursor = pb_in_btn ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
                break;
            }
            case XENT_CONTROL_NUMBER_BOX: {
                /* I-beam in text area; arrow over spin buttons or delete button */
                FluxNodeData *nb_nd = flux_node_store_get(app->store, hovered);
                bool in_btn_area = false;
                if (nb_nd && nb_nd->hover_local_x >= 0.0f) {
                    XentRect nb_rect = {0};
                    xent_get_layout_rect(app->ctx, hovered, &nb_rect);
                    bool spin_inline = xent_get_semantic_expanded(app->ctx, hovered);
                    float spin_w = spin_inline ? 76.0f : 0.0f;

                    /* Spin area: rightmost 76px when inline */
                    if (spin_inline) {
                        float spin_start = nb_rect.width - spin_w;
                        if (nb_nd->hover_local_x >= spin_start)
                            in_btn_area = true;
                    }

                    /* X button: 34px to the left of spin area (or right edge) */
                    if (!in_btn_area && nb_nd->state.focused) {
                        FluxTextBoxData *nbd = (FluxTextBoxData *)nb_nd->component_data;
                        bool has_text_nb = (nbd && nbd->content && nbd->content[0] && !nbd->readonly);
                        if (has_text_nb) {
                            float del_start = nb_rect.width - 40.0f - spin_w;
                            float del_end = del_start + 40.0f;
                            if (nb_nd->hover_local_x >= del_start && nb_nd->hover_local_x < del_end)
                                in_btn_area = true;
                        }
                    }
                }
                cursor = in_btn_area ? FLUX_CURSOR_ARROW : FLUX_CURSOR_IBEAM;
                break;
            }
            case XENT_CONTROL_BUTTON:
            case XENT_CONTROL_TOGGLE_BUTTON:
            case XENT_CONTROL_CHECKBOX:
            case XENT_CONTROL_RADIO:
            case XENT_CONTROL_SWITCH:
            case XENT_CONTROL_SLIDER:
            case XENT_CONTROL_HYPERLINK:
            case XENT_CONTROL_REPEAT_BUTTON:
                cursor = FLUX_CURSOR_HAND;
                break;
            default:
                break;
            }
        }
        flux_window_set_cursor(app->window, cursor);
    } else if (down) {
        flux_input_pointer_down(app->input, app->root, x, y);
    } else {
        flux_input_pointer_up(app->input, app->root, x, y);
    }

    flux_window_request_render(app->window);
}

static void app_setting_changed(void *ctx) {
    FluxApp *app = (FluxApp *)ctx;
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
    FluxApp *app = (FluxApp *)ctx;
    if (!app || !app->input) return;

    if (down) {
        /* Focus navigation keys */
        switch (vk) {
        case VK_TAB:
            flux_input_tab(app->input, app->root,
                           (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            flux_window_request_render(app->window);
            return;
        case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN: {
            /* Only use arrow keys for focus navigation if the focused node
               is NOT a text input (text inputs need arrow keys for cursor). */
            XentNodeId focused = flux_input_get_focused(app->input);
            if (focused != XENT_NODE_INVALID && app->ctx) {
                XentControlType ct = xent_get_control_type(app->ctx, focused);
                if (ct != XENT_CONTROL_TEXT_INPUT &&
                    ct != XENT_CONTROL_PASSWORD_BOX &&
                    ct != XENT_CONTROL_NUMBER_BOX) {
                    flux_input_arrow(app->input, app->root, (int)vk);
                    flux_window_request_render(app->window);
                    return;
                }
            }
            break;
        }
        case VK_RETURN:
        case VK_SPACE: {
            XentNodeId focused = flux_input_get_focused(app->input);
            if (focused != XENT_NODE_INVALID && app->ctx) {
                XentControlType ct = xent_get_control_type(app->ctx, focused);
                /* Don't intercept Enter/Space for text inputs */
                if (ct != XENT_CONTROL_TEXT_INPUT &&
                    ct != XENT_CONTROL_PASSWORD_BOX &&
                    ct != XENT_CONTROL_NUMBER_BOX) {
                    flux_input_activate(app->input);
                    flux_window_request_render(app->window);
                    return;
                }
            }
            break;
        }
        case VK_ESCAPE:
            flux_input_escape(app->input);
            flux_window_request_render(app->window);
            return;
        default:
            break;
        }
        flux_input_key_down(app->input, vk);
    }
    flux_window_request_render(app->window);
}

static void app_char(void *ctx, wchar_t ch) {
    FluxApp *app = (FluxApp *)ctx;
    if (!app || !app->input) return;
    flux_input_char(app->input, ch);
    flux_window_request_render(app->window);
}

static void app_ime_composition(void *ctx, const wchar_t *text, int cursor_pos) {
    FluxApp *app = (FluxApp *)ctx;
    if (!app || !app->input) return;
    if (text) {
        flux_input_ime_composition(app->input, text, (uint32_t)wcslen(text), (uint32_t)cursor_pos);
    } else {
        flux_input_ime_end(app->input);
    }
    flux_window_request_render(app->window);
}

static void app_context_menu(void *ctx, float x, float y) {
    FluxApp *app = (FluxApp *)ctx;
    if (!app || !app->input || app->root == XENT_NODE_INVALID) return;
    flux_input_context_menu(app->input, app->root, x, y,
                            flux_window_hwnd(app->window));
    flux_window_request_render(app->window);
}

static void app_scroll(void *ctx, float x, float y, float delta) {
    FluxApp *app = (FluxApp *)ctx;
    if (!app || !app->input || app->root == XENT_NODE_INVALID) return;
    flux_input_scroll(app->input, app->root, x, y, delta);
    flux_window_request_render(app->window);
}

HRESULT flux_app_create(const FluxAppConfig *cfg, FluxApp **out) {
    if (!out) return E_INVALIDARG;
    *out = NULL;

    FluxApp *app = (FluxApp *)calloc(1, sizeof(*app));
    if (!app) return E_OUTOFMEMORY;

    app->root = XENT_NODE_INVALID;

    /* Register core renderers that have no dedicated flux_create_*() factory */
    flux_register_renderer(XENT_CONTROL_CONTAINER,  flux_draw_container, NULL);
    flux_register_renderer(XENT_CONTROL_SCROLL,     flux_draw_scroll,    flux_draw_scroll_overlay);
    flux_register_renderer(XENT_CONTROL_IMAGE,      flux_draw_container, NULL);
    flux_register_renderer(XENT_CONTROL_LIST,       flux_draw_container, NULL);
    flux_register_renderer(XENT_CONTROL_TAB,        flux_draw_container, NULL);
    flux_register_renderer(XENT_CONTROL_CANVAS,     flux_draw_container, NULL);
    flux_register_renderer(XENT_CONTROL_CUSTOM,     flux_draw_container, NULL);

    FluxWindowConfig wcfg;
    memset(&wcfg, 0, sizeof(wcfg));
    wcfg.title    = cfg ? cfg->title : L"Fluxent";
    wcfg.width    = cfg ? cfg->width : 800;
    wcfg.height   = cfg ? cfg->height : 600;
    wcfg.dark_mode = cfg ? cfg->dark_mode : false;
    wcfg.backdrop  = cfg ? (int)cfg->backdrop : 0;
    wcfg.resizable = true;

    HRESULT hr = flux_window_create(&wcfg, &app->window);
    if (FAILED(hr)) {
        free(app);
        return hr;
    }

    flux_window_set_render_callback(app->window, app_render, app);
    flux_window_set_mouse_callback(app->window, app_mouse, app);
    flux_window_set_key_callback(app->window, app_key, app);
    flux_window_set_char_callback(app->window, app_char, app);
    flux_window_set_setting_changed_callback(app->window, app_setting_changed, app);
    flux_window_set_ime_composition_callback(app->window, app_ime_composition, app);
    flux_window_set_context_menu_callback(app->window, app_context_menu, app);
    flux_window_set_scroll_callback(app->window, app_scroll, app);

    app->cache = flux_render_cache_create(512);
    app->theme = flux_theme_create();
    if (app->theme) {
        flux_theme_set_mode(app->theme, FLUX_THEME_SYSTEM);
    }

    *out = app;
    return S_OK;
}

void flux_app_destroy(FluxApp *app) {
    if (!app) return;
    if (app->shared_brush) {
        ID2D1SolidColorBrush_Release(app->shared_brush);
        app->shared_brush = NULL;
    }
    if (app->cache) flux_render_cache_destroy(app->cache);
    if (app->theme) flux_theme_destroy(app->theme);
    if (app->engine) flux_engine_destroy(app->engine);
    if (app->input)  flux_input_destroy(app->input);
    if (app->text)   flux_text_renderer_destroy(app->text);
    if (app->window) flux_window_destroy(app->window);
    free(app);
}

void flux_app_set_root(FluxApp *app, XentContext *ctx, XentNodeId root,
                       FluxNodeStore *store) {
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
    if (app->text)
        flux_text_renderer_register(app->text, ctx);

    if (store)
        flux_node_store_attach_userdata(store, ctx);

    flux_window_request_render(app->window);
}

int flux_app_run(FluxApp *app) {
    if (!app || !app->window) return -1;
    return flux_window_run(app->window);
}

FluxEngine *flux_app_get_engine(FluxApp *app) {
    return app ? app->engine : NULL;
}

FluxWindow *flux_app_get_window(FluxApp *app) {
    return app ? app->window : NULL;
}

FluxInput *flux_app_get_input(FluxApp *app) {
    return app ? app->input : NULL;
}

FluxThemeManager *flux_app_get_theme(FluxApp *app) {
    return app ? app->theme : NULL;
}

static XentNodeId create_node_with_parent(XentContext *ctx, FluxNodeStore *store,
                                          XentNodeId parent, XentControlType type) {
    XentNodeId node = xent_create_node(ctx);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_control_type(ctx, node, type);

    if (parent != XENT_NODE_INVALID)
        xent_append_child(ctx, parent, node);

    flux_node_store_get_or_create(store, node);
    xent_set_userdata(ctx, node, flux_node_store_get(store, node));

    return node;
}

XentNodeId flux_create_button(XentContext *ctx, FluxNodeStore *store,
                              XentNodeId parent, const char *label,
                              void (*on_click)(void *), void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_BUTTON, flux_draw_button, NULL);
    flux_register_renderer(XENT_CONTROL_TOGGLE_BUTTON, flux_draw_toggle, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_BUTTON);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxButtonData *bd = (FluxButtonData *)calloc(1, sizeof(FluxButtonData));
        if (bd) {
            bd->label = label;
            bd->font_size = 14.0f;
            bd->style = FLUX_BUTTON_STANDARD;
            bd->enabled = true;
            bd->on_click = on_click;
            bd->on_click_ctx = userdata;
        }
        nd->component_data = bd;
        nd->on_click = on_click;
        nd->on_click_ctx = userdata;
    }

    xent_set_semantic_role(ctx, node, XENT_SEMANTIC_BUTTON);
    if (label)
        xent_set_semantic_label(ctx, node, label);

    return node;
}

XentNodeId flux_create_text(XentContext *ctx, FluxNodeStore *store,
                            XentNodeId parent, const char *content,
                            float font_size) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_TEXT, flux_draw_text, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_TEXT);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_text(ctx, node, content ? content : "");
    if (font_size > 0.0f)
        xent_set_font_size(ctx, node, font_size);

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxTextData *td = (FluxTextData *)calloc(1, sizeof(FluxTextData));
        if (td) {
            td->content = content;
            td->font_size = font_size > 0.0f ? font_size : 14.0f;
            td->font_weight = FLUX_FONT_REGULAR;
            td->alignment = FLUX_TEXT_LEFT;
            td->vertical_alignment = FLUX_TEXT_TOP;
            td->wrap = true;
        }
        nd->component_data = td;
    }

    xent_set_semantic_role(ctx, node, XENT_SEMANTIC_TEXT);
    if (content)
        xent_set_semantic_label(ctx, node, content);

    return node;
}

/* ---- Slider input bridge ---- */
typedef struct FluxSliderInputData {
    FluxSliderData base;   /* must be first — renderer casts component_data to FluxSliderData* */
    XentContext   *ctx;
    XentNodeId     node;
} FluxSliderInputData;

static void slider_move_trampoline(void *ctx, float local_x, float local_y) {
    FluxSliderInputData *sid = (FluxSliderInputData *)ctx;
    FluxSliderData *sd = &sid->base;
    if (!sd->enabled) return;

    XentRect rect = {0};
    if (!xent_get_layout_rect(sid->ctx, sid->node, &rect)) return;

    float pad = 10.0f;   /* matches thumb_outer in flux_slider.c */
    float track_w = rect.width - pad * 2.0f;
    if (track_w <= 0.0f) return;

    float pct = (local_x - pad) / track_w;
    if (pct < 0.0f) pct = 0.0f;
    if (pct > 1.0f) pct = 1.0f;

    float value = sd->min_value + pct * (sd->max_value - sd->min_value);
    if (sd->step > 0.0f) {
        float half = sd->step * 0.5f;
        value = ((int)((value + half) / sd->step)) * sd->step;
    }
    if (value < sd->min_value) value = sd->min_value;
    if (value > sd->max_value) value = sd->max_value;

    sd->current_value = value;
    if (sd->on_change)
        sd->on_change(sd->on_change_ctx, value);
}

XentNodeId flux_create_slider(XentContext *ctx, FluxNodeStore *store,
                              XentNodeId parent,
                              float min, float max, float value,
                              void (*on_change)(void *, float), void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_SLIDER, flux_draw_slider, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_SLIDER);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_semantic_value(ctx, node, value, min, max);

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxSliderInputData *sid = (FluxSliderInputData *)calloc(1, sizeof(FluxSliderInputData));
        if (sid) {
            sid->base.min_value = min;
            sid->base.max_value = max;
            sid->base.current_value = value;
            sid->base.step = 0.0f;
            sid->base.enabled = true;
            sid->base.on_change = on_change;
            sid->base.on_change_ctx = userdata;
            sid->ctx = ctx;
            sid->node = node;
        }
        nd->component_data = &sid->base;
        nd->on_pointer_move = slider_move_trampoline;
        nd->on_pointer_move_ctx = sid;
    }

    return node;
}

/* ---- Checkbox click trampoline ---- */
static void checkbox_click_trampoline(void *ctx) {
    FluxCheckboxData *cd = (FluxCheckboxData *)ctx;
    if (!cd || !cd->enabled) return;
    cd->state = (cd->state == FLUX_CHECK_CHECKED)
              ? FLUX_CHECK_UNCHECKED
              : FLUX_CHECK_CHECKED;
    if (cd->on_change)
        cd->on_change(cd->on_change_ctx, cd->state);
}

XentNodeId flux_create_checkbox(XentContext *ctx, FluxNodeStore *store,
                                XentNodeId parent, const char *label,
                                bool checked,
                                void (*on_change)(void *, FluxCheckState),
                                void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_CHECKBOX, flux_draw_checkbox, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_CHECKBOX);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_semantic_checked(ctx, node, checked ? 1 : 0);

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxCheckboxData *cd = (FluxCheckboxData *)calloc(1, sizeof(FluxCheckboxData));
        if (cd) {
            cd->label = label;
            cd->state = checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
            cd->enabled = true;
            cd->on_change = on_change;
            cd->on_change_ctx = userdata;
        }
        nd->component_data = cd;
        nd->on_click = checkbox_click_trampoline;
        nd->on_click_ctx = cd;
    }

    if (label)
        xent_set_semantic_label(ctx, node, label);

    return node;
}

XentNodeId flux_create_radio(XentContext *ctx, FluxNodeStore *store,
                             XentNodeId parent, const char *label,
                             bool checked,
                             void (*on_change)(void *, FluxCheckState),
                             void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_RADIO, flux_draw_radio, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_RADIO);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_semantic_checked(ctx, node, checked ? 1 : 0);

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxCheckboxData *cd = (FluxCheckboxData *)calloc(1, sizeof(FluxCheckboxData));
        if (cd) {
            cd->label = label;
            cd->state = checked ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
            cd->enabled = true;
            cd->on_change = on_change;
            cd->on_change_ctx = userdata;
        }
        nd->component_data = cd;
        nd->on_click = checkbox_click_trampoline;
        nd->on_click_ctx = cd;
    }

    if (label)
        xent_set_semantic_label(ctx, node, label);

    return node;
}

XentNodeId flux_create_switch(XentContext *ctx, FluxNodeStore *store,
                              XentNodeId parent, const char *label,
                              bool on,
                              void (*on_change)(void *, FluxCheckState),
                              void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_SWITCH, flux_draw_switch, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_SWITCH);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_semantic_checked(ctx, node, on ? 1 : 0);

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxCheckboxData *cd = (FluxCheckboxData *)calloc(1, sizeof(FluxCheckboxData));
        if (cd) {
            cd->label = label;
            cd->state = on ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
            cd->enabled = true;
            cd->on_change = on_change;
            cd->on_change_ctx = userdata;
        }
        nd->component_data = cd;
        nd->on_click = checkbox_click_trampoline;
        nd->on_click_ctx = cd;
    }

    if (label)
        xent_set_semantic_label(ctx, node, label);

    return node;
}

XentNodeId flux_create_progress(XentContext *ctx, FluxNodeStore *store,
                                XentNodeId parent,
                                float value, float max_value) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_PROGRESS, flux_draw_progress, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_PROGRESS);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_semantic_value(ctx, node, value, 0.0f, max_value);

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxProgressData *pd = (FluxProgressData *)calloc(1, sizeof(FluxProgressData));
        if (pd) {
            pd->value = value;
            pd->max_value = max_value;
            pd->indeterminate = false;
        }
        nd->component_data = pd;
    }

    return node;
}

XentNodeId flux_create_card(XentContext *ctx, FluxNodeStore *store,
                            XentNodeId parent) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_CARD, flux_draw_card, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_CARD);
    return node;
}

XentNodeId flux_create_divider(XentContext *ctx, FluxNodeStore *store,
                               XentNodeId parent) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_DIVIDER, flux_draw_divider, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_DIVIDER);
    return node;
}

/* ---- TextBox input infrastructure ---- */

#define FLUX_TEXTBOX_INITIAL_CAP 128
#define TB_UNDO_MAX 50
#define TB_TYPING_MERGE_MS 1000

typedef struct TbEditHistory {
    char     *text;
    uint32_t  len;
    uint32_t  sel_start;
    uint32_t  sel_end;
} TbEditHistory;

typedef struct FluxTextBoxInputData {
    FluxTextBoxData  base;    /* must be first — renderer casts to FluxTextBoxData* */
    char            *buffer;
    uint32_t         buf_cap;
    uint32_t         buf_len;
    XentContext      *ctx;
    XentNodeId        node;
    FluxNodeStore    *store;
    FluxApp          *app;    /* used to get text renderer and window */

    /* Undo/redo */
    TbEditHistory    undo_stack[TB_UNDO_MAX];
    int              undo_top;   /* index of next free slot, -1 means empty */
    TbEditHistory    redo_stack[TB_UNDO_MAX];
    int              redo_top;

    /* Typing merge for undo grouping */
    ULONGLONG        last_typing_time;
    bool             last_op_was_typing;

    /* IME composition (owned wchar_t buffer) */
    wchar_t         *ime_buf;
    uint32_t         ime_buf_cap;

    /* Drag selection state */
    bool             dragging;
    uint32_t         drag_anchor;  /* byte offset of anchor for shift-drag */

    /* Surrogate pair buffering for U+10000+ characters (emoji, CJK ext, etc.) */
    wchar_t          high_surrogate;

    /* ---- NumberBox extension (valid when control_type == NUMBER_BOX) ---- */
    double           nb_value;              /* current numeric Value (NaN = empty) */
    double           nb_minimum;            /* default: -1e308 */
    double           nb_maximum;            /* default: 1e308 */
    double           nb_small_change;       /* default: 1.0 (arrows, spin buttons) */
    double           nb_large_change;       /* default: 10.0 (PageUp/PageDown) */
    bool             nb_is_wrap_enabled;    /* default: false */
    uint8_t          nb_spin_placement;     /* FluxNBSpinPlacement: 0=Hidden, 2=Inline */
    uint8_t          nb_validation;         /* FluxNBValidation: 0=Overwrite, 1=Disabled */
    bool             nb_value_updating;     /* re-entrancy guard */
    bool             nb_text_updating;      /* re-entrancy guard */
    void           (*nb_on_value_change)(void *ctx, double value);
    void            *nb_on_value_change_ctx;
    bool             nb_stepping;           /* true after keyboard step — suppress caret/selection */
    bool             nb_focus_from_click;   /* set by pointer_down so on_focus knows not to enter stepping */
} FluxTextBoxInputData;

static void tb_ensure_cap(FluxTextBoxInputData *tb, uint32_t needed) {
    if (needed <= tb->buf_cap) return;
    uint32_t cap = tb->buf_cap;
    while (cap < needed) cap = cap * 2;
    char *nb = (char *)realloc(tb->buffer, cap);
    if (!nb) return;
    tb->buffer = nb;
    tb->buf_cap = cap;
    tb->base.content = tb->buffer;
}

static uint32_t tb_utf8_char_len(const char *s, uint32_t pos) {
    if (pos >= (uint32_t)strlen(s)) return 0;
    uint8_t c = (uint8_t)s[pos];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static uint32_t tb_utf8_prev(const char *s, uint32_t pos) {
    if (pos == 0) return 0;
    uint32_t p = pos - 1;
    while (p > 0 && ((uint8_t)s[p] & 0xC0) == 0x80) p--;
    return p;
}

static uint32_t tb_utf8_next(const char *s, uint32_t pos) {
    uint32_t len = (uint32_t)strlen(s);
    if (pos >= len) return len;
    return pos + tb_utf8_char_len(s, pos);
}

static uint32_t tb_utf16_to_byte_offset(const char *s, uint32_t utf16_pos) {
    uint32_t byte_off = 0;
    uint32_t u16_count = 0;
    uint32_t len = (uint32_t)strlen(s);
    while (byte_off < len && u16_count < utf16_pos) {
        uint32_t clen = tb_utf8_char_len(s, byte_off);
        u16_count += (clen == 4) ? 2 : 1;
        byte_off += clen;
    }
    return byte_off;
}

static uint32_t tb_byte_to_utf16_offset(const char *s, uint32_t byte_pos) {
    uint32_t byte_off = 0;
    uint32_t u16_count = 0;
    uint32_t len = (uint32_t)strlen(s);
    while (byte_off < len && byte_off < byte_pos) {
        uint32_t clen = tb_utf8_char_len(s, byte_off);
        u16_count += (clen == 4) ? 2 : 1;
        byte_off += clen;
    }
    return u16_count;
}

static void tb_delete_range(FluxTextBoxInputData *tb, uint32_t from, uint32_t to) {
    if (from >= to || to > tb->buf_len) return;
    memmove(tb->buffer + from, tb->buffer + to, tb->buf_len - to);
    tb->buf_len -= (to - from);
    tb->buffer[tb->buf_len] = '\0';
}

static void tb_insert_utf8(FluxTextBoxInputData *tb, uint32_t pos, const char *s, uint32_t slen) {
    tb_ensure_cap(tb, tb->buf_len + slen + 1);
    memmove(tb->buffer + pos + slen, tb->buffer + pos, tb->buf_len - pos);
    memcpy(tb->buffer + pos, s, slen);
    tb->buf_len += slen;
    tb->buffer[tb->buf_len] = '\0';
}

static void tb_delete_selection(FluxTextBoxInputData *tb) {
    uint32_t s = tb->base.selection_start < tb->base.selection_end
                 ? tb->base.selection_start : tb->base.selection_end;
    uint32_t e = tb->base.selection_start > tb->base.selection_end
                 ? tb->base.selection_start : tb->base.selection_end;
    if (s == e) return;
    tb_delete_range(tb, s, e);
    tb->base.cursor_position = s;
    tb->base.selection_start = s;
    tb->base.selection_end   = s;
}

static bool tb_has_selection(const FluxTextBoxInputData *tb) {
    return tb->base.selection_start != tb->base.selection_end;
}

static FluxTextStyle tb_make_style(const FluxTextBoxInputData *tb) {
    FluxTextStyle ts;
    memset(&ts, 0, sizeof(ts));
    ts.font_family = tb->base.font_family;
    ts.font_size = tb->base.font_size > 0.0f ? tb->base.font_size : 14.0f;
    ts.font_weight = FLUX_FONT_REGULAR;
    ts.text_align = FLUX_TEXT_LEFT;
    ts.vert_align = FLUX_TEXT_VCENTER;
    ts.word_wrap = false;
    return ts;
}

static void tb_update_scroll(FluxTextBoxInputData *tb) {
    FluxTextRenderer *tr = tb->app ? tb->app->text : NULL;
    if (!tr) return;

    XentRect rect = {0};
    xent_get_layout_rect(tb->ctx, tb->node, &rect);
    float visible_w = rect.width - 10.0f - 6.0f; /* FLUX_TEXTBOX_PAD_L + PAD_R */
    if (visible_w <= 0.0f) return;

    FluxTextStyle ts = tb_make_style(tb);
    uint32_t u16_cursor = tb_byte_to_utf16_offset(tb->buffer, tb->base.cursor_position);
    FluxRect caret = flux_text_caret_rect(tr, tb->buffer, &ts,
                                           visible_w + tb->base.scroll_offset_x,
                                           (int)u16_cursor);
    float cx = caret.x;

    if (cx - tb->base.scroll_offset_x < 0.0f) {
        tb->base.scroll_offset_x = cx;
    } else if (cx - tb->base.scroll_offset_x > visible_w) {
        tb->base.scroll_offset_x = cx - visible_w;
    }
    if (tb->base.scroll_offset_x < 0.0f)
        tb->base.scroll_offset_x = 0.0f;
}

static void tb_notify_change(FluxTextBoxInputData *tb) {
    if (tb->base.on_change)
        tb->base.on_change(tb->base.on_change_ctx, tb->buffer);
}

/* ---- IME candidate window follow cursor ---- */
static void tb_update_ime_position(FluxTextBoxInputData *tb) {
    if (!tb->app || !tb->app->window || !tb->app->text) return;

    FluxTextRenderer *tr = tb->app->text;
    XentRect rect = {0};
    xent_get_layout_rect(tb->ctx, tb->node, &rect);

    /* Compute caret position in textbox-local coords (TOP-aligned for correct Y) */
    FluxTextStyle ts = tb_make_style(tb);
    ts.vert_align = FLUX_TEXT_TOP;
    float visible_w = rect.width - 10.0f - 6.0f;

    /*
     * During IME composition the real visual caret sits AFTER the
     * composition text, but cursor_position (byte offset) hasn't
     * moved yet.  Build a temporary display string that includes
     * the composition, then hit-test at (composition_start + comp_cursor).
     */
    float caret_x = 0.0f, caret_y = 0.0f, caret_h = 0.0f;

    if (tb->base.composition_text && tb->base.composition_length > 0 && tb->buffer[0]) {
        /* Convert original text to wide */
        int orig_wlen = MultiByteToWideChar(CP_UTF8, 0, tb->buffer, -1, NULL, 0) - 1;
        int comp_len = (int)tb->base.composition_length;
        int total = orig_wlen + comp_len + 1;
        wchar_t *wtmp = (wchar_t *)_alloca(total * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, tb->buffer, -1, wtmp, orig_wlen + 1);

        int ins = (int)MultiByteToWideChar(CP_UTF8, 0, tb->buffer,
                                            (int)tb->base.cursor_position, NULL, 0);
        if (ins > orig_wlen) ins = orig_wlen;

        /* [0..ins] + composition + [ins..end] */
        wchar_t *composed = (wchar_t *)_alloca((orig_wlen + comp_len + 1) * sizeof(wchar_t));
        memcpy(composed, wtmp, ins * sizeof(wchar_t));
        memcpy(composed + ins, tb->base.composition_text, comp_len * sizeof(wchar_t));
        memcpy(composed + ins + comp_len, wtmp + ins, (orig_wlen - ins) * sizeof(wchar_t));
        composed[orig_wlen + comp_len] = 0;

        /* Convert back to UTF-8 for caret_rect */
        int u8len = WideCharToMultiByte(CP_UTF8, 0, composed, orig_wlen + comp_len,
                                         NULL, 0, NULL, NULL);
        char *u8tmp = (char *)_alloca(u8len + 1);
        WideCharToMultiByte(CP_UTF8, 0, composed, orig_wlen + comp_len,
                            u8tmp, u8len, NULL, NULL);
        u8tmp[u8len] = 0;

        int hit_u16 = ins + (int)tb->base.composition_cursor;
        FluxRect caret = flux_text_caret_rect(tr, u8tmp, &ts,
                                               visible_w + tb->base.scroll_offset_x,
                                               hit_u16);
        caret_x = caret.x;
        caret_y = caret.y;
        caret_h = caret.h;
    } else {
        uint32_t u16_cursor = tb_byte_to_utf16_offset(tb->buffer, tb->base.cursor_position);
        FluxRect caret = flux_text_caret_rect(tr, tb->buffer, &ts,
                                               visible_w + tb->base.scroll_offset_x,
                                               (int)u16_cursor);
        caret_x = caret.x;
        caret_y = caret.y;
        caret_h = caret.h;
    }

    /* Textbox absolute position + padding + caret offset - scroll */
    float cx = rect.x + 10.0f + caret_x - tb->base.scroll_offset_x;
    float cy = rect.y + 5.0f + caret_y;
    float ch = caret_h > 0.0f ? caret_h : (ts.font_size > 0.0f ? ts.font_size : 14.0f) * 1.2f;

    /* Convert from DIPs to physical pixels */
    FluxDpiInfo dpi = flux_window_dpi(tb->app->window);
    float scale = dpi.dpi_x / 96.0f;

    flux_window_set_ime_position(tb->app->window,
                                 (int)(cx * scale),
                                 (int)(cy * scale),
                                 (int)(ch * scale));
}

/* ---- Undo/redo helpers ---- */
static void tb_push_undo(FluxTextBoxInputData *tb) {
    if (tb->undo_top >= TB_UNDO_MAX) {
        /* Shift down, discard oldest */
        free(tb->undo_stack[0].text);
        memmove(&tb->undo_stack[0], &tb->undo_stack[1],
                (TB_UNDO_MAX - 1) * sizeof(TbEditHistory));
        tb->undo_top = TB_UNDO_MAX - 1;
    }
    TbEditHistory *h = &tb->undo_stack[tb->undo_top++];
    h->text = (char *)malloc(tb->buf_len + 1);
    if (h->text) {
        memcpy(h->text, tb->buffer, tb->buf_len + 1);
        h->len = tb->buf_len;
    }
    h->sel_start = tb->base.selection_start;
    h->sel_end = tb->base.selection_end;

    /* Clear redo stack on new action */
    for (int i = 0; i < tb->redo_top; i++)
        free(tb->redo_stack[i].text);
    tb->redo_top = 0;
}

static void tb_undo(FluxTextBoxInputData *tb) {
    if (tb->undo_top <= 0) return;

    /* Push current state to redo */
    if (tb->redo_top < TB_UNDO_MAX) {
        TbEditHistory *rh = &tb->redo_stack[tb->redo_top++];
        rh->text = (char *)malloc(tb->buf_len + 1);
        if (rh->text) {
            memcpy(rh->text, tb->buffer, tb->buf_len + 1);
            rh->len = tb->buf_len;
        }
        rh->sel_start = tb->base.selection_start;
        rh->sel_end = tb->base.selection_end;
    }

    TbEditHistory *h = &tb->undo_stack[--tb->undo_top];
    if (h->text) {
        tb_ensure_cap(tb, h->len + 1);
        memcpy(tb->buffer, h->text, h->len + 1);
        tb->buf_len = h->len;
        tb->base.content = tb->buffer;
    }
    tb->base.selection_start = h->sel_start;
    tb->base.selection_end = h->sel_end;
    tb->base.cursor_position = h->sel_end;
    free(h->text);
    h->text = NULL;

    tb_update_scroll(tb);
    tb_notify_change(tb);
}

static void tb_redo(FluxTextBoxInputData *tb) {
    if (tb->redo_top <= 0) return;

    /* Push current state to undo */
    if (tb->undo_top < TB_UNDO_MAX) {
        TbEditHistory *uh = &tb->undo_stack[tb->undo_top++];
        uh->text = (char *)malloc(tb->buf_len + 1);
        if (uh->text) {
            memcpy(uh->text, tb->buffer, tb->buf_len + 1);
            uh->len = tb->buf_len;
        }
        uh->sel_start = tb->base.selection_start;
        uh->sel_end = tb->base.selection_end;
    }

    TbEditHistory *h = &tb->redo_stack[--tb->redo_top];
    if (h->text) {
        tb_ensure_cap(tb, h->len + 1);
        memcpy(tb->buffer, h->text, h->len + 1);
        tb->buf_len = h->len;
        tb->base.content = tb->buffer;
    }
    tb->base.selection_start = h->sel_start;
    tb->base.selection_end = h->sel_end;
    tb->base.cursor_position = h->sel_end;
    free(h->text);
    h->text = NULL;

    tb_update_scroll(tb);
    tb_notify_change(tb);
}

static void __attribute__((unused)) tb_free_undo_redo(FluxTextBoxInputData *tb) {
    for (int i = 0; i < tb->undo_top; i++) free(tb->undo_stack[i].text);
    for (int i = 0; i < tb->redo_top; i++) free(tb->redo_stack[i].text);
    tb->undo_top = 0;
    tb->redo_top = 0;
}

/* ---- Word boundary helpers ---- */
static uint32_t tb_word_start(const char *s, uint32_t pos) {
    if (pos == 0) return 0;
    uint32_t p = tb_utf8_prev(s, pos);
    /* Skip spaces first */
    while (p > 0 && (s[p] == ' ' || s[p] == '\t'))
        p = tb_utf8_prev(s, p);
    /* Then skip non-spaces */
    while (p > 0 && s[tb_utf8_prev(s, p)] != ' ' && s[tb_utf8_prev(s, p)] != '\t')
        p = tb_utf8_prev(s, p);
    return p;
}

static uint32_t tb_word_end(const char *s, uint32_t pos) {
    uint32_t len = (uint32_t)strlen(s);
    if (pos >= len) return len;
    uint32_t p = pos;
    /* Skip non-spaces first */
    while (p < len && s[p] != ' ' && s[p] != '\t')
        p = tb_utf8_next(s, p);
    /* Then skip spaces */
    while (p < len && (s[p] == ' ' || s[p] == '\t'))
        p = tb_utf8_next(s, p);
    return p;
}

/* ---- Clipboard helpers ---- */
static void tb_perform_copy(FluxTextBoxInputData *tb) {
    if (!tb_has_selection(tb)) return;
    uint32_t s = tb->base.selection_start < tb->base.selection_end
                 ? tb->base.selection_start : tb->base.selection_end;
    uint32_t e = tb->base.selection_start > tb->base.selection_end
                 ? tb->base.selection_start : tb->base.selection_end;
    uint32_t sel_len = e - s;
    int wlen = MultiByteToWideChar(CP_UTF8, 0, tb->buffer + s, (int)sel_len, NULL, 0);
    if (wlen > 0 && OpenClipboard(NULL)) {
        EmptyClipboard();
        HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (wlen + 1) * sizeof(wchar_t));
        if (hg) {
            wchar_t *p = (wchar_t *)GlobalLock(hg);
            MultiByteToWideChar(CP_UTF8, 0, tb->buffer + s, (int)sel_len, p, wlen);
            p[wlen] = 0;
            GlobalUnlock(hg);
            SetClipboardData(CF_UNICODETEXT, hg);
        }
        CloseClipboard();
    }
}

static void tb_perform_paste(FluxTextBoxInputData *tb) {
    if (tb->base.readonly) return;
    if (!OpenClipboard(NULL)) return;
    HANDLE hg = GetClipboardData(CF_UNICODETEXT);
    if (!hg) { CloseClipboard(); return; }
    const wchar_t *wclip = (const wchar_t *)GlobalLock(hg);
    if (!wclip) { CloseClipboard(); return; }

    tb_push_undo(tb);
    tb->last_op_was_typing = false;
    if (tb_has_selection(tb))
        tb_delete_selection(tb);

    int wclip_len = (int)wcslen(wclip);
    /* Strip newlines for single-line */
    wchar_t filtered_stack[256];
    wchar_t *filtered = filtered_stack;
    if (!tb->base.multiline) {
        if (wclip_len + 1 > 256)
            filtered = (wchar_t *)malloc((wclip_len + 1) * sizeof(wchar_t));
        int fi = 0;
        for (int i = 0; i < wclip_len; i++) {
            if (wclip[i] != '\n' && wclip[i] != '\r')
                filtered[fi++] = wclip[i];
        }
        filtered[fi] = 0;
        wclip = filtered;
        wclip_len = fi;
    }

    int u8len = WideCharToMultiByte(CP_UTF8, 0, wclip, wclip_len, NULL, 0, NULL, NULL);
    if (u8len > 0) {
        if (tb->base.max_length > 0 && tb->buf_len + (uint32_t)u8len > tb->base.max_length)
            u8len = (int)(tb->base.max_length > tb->buf_len ? tb->base.max_length - tb->buf_len : 0);
        if (u8len > 0) {
            char u8_stack[512];
            char *u8buf = u8_stack;
            if (u8len + 1 > 512)
                u8buf = (char *)malloc(u8len + 1);
            WideCharToMultiByte(CP_UTF8, 0, wclip, wclip_len, u8buf, u8len, NULL, NULL);
            u8buf[u8len] = 0;
            tb_insert_utf8(tb, tb->base.cursor_position, u8buf, (uint32_t)u8len);
            tb->base.cursor_position += (uint32_t)u8len;
            tb->base.selection_start = tb->base.cursor_position;
            tb->base.selection_end   = tb->base.cursor_position;
            if (u8buf != u8_stack) free(u8buf);
        }
    }

    GlobalUnlock(hg);
    CloseClipboard();
    if (!tb->base.multiline && filtered != filtered_stack) free(filtered);

    tb_update_scroll(tb);
    tb_notify_change(tb);
}

/* Forward declarations for NumberBox core logic (defined after tb_on_context_menu) */
static bool nb_is_number_box(FluxTextBoxInputData *tb);
static void nb_update_text_to_value(FluxTextBoxInputData *tb);
static void nb_validate_input(FluxTextBoxInputData *tb);
static void nb_step_value(FluxTextBoxInputData *tb, double change);

/* ---- TextBox key handler ---- */
static void tb_on_key(void *ctx, unsigned int vk, bool down) {
    if (!down) return;
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    if (!tb->base.enabled) return;
    uint32_t old_cursor = tb->base.cursor_position;

    bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    switch (vk) {
    case VK_LEFT:
        if (tb->base.cursor_position > 0) {
            uint32_t np;
            if (ctrl) {
                np = tb_word_start(tb->buffer, tb->base.cursor_position);
            } else {
                np = tb_utf8_prev(tb->buffer, tb->base.cursor_position);
            }
            tb->base.cursor_position = np;
            if (!shift) {
                tb->base.selection_start = np;
                tb->base.selection_end   = np;
            } else {
                tb->base.selection_end = np;
            }
        } else if (!shift) {
            tb->base.selection_start = 0;
            tb->base.selection_end   = 0;
        }
        tb_update_scroll(tb);
        break;

    case VK_RIGHT: {
        uint32_t len = tb->buf_len;
        if (tb->base.cursor_position < len) {
            uint32_t np;
            if (ctrl) {
                np = tb_word_end(tb->buffer, tb->base.cursor_position);
            } else {
                np = tb_utf8_next(tb->buffer, tb->base.cursor_position);
            }
            tb->base.cursor_position = np;
            if (!shift) {
                tb->base.selection_start = np;
                tb->base.selection_end   = np;
            } else {
                tb->base.selection_end = np;
            }
        } else if (!shift) {
            tb->base.selection_start = len;
            tb->base.selection_end   = len;
        }
        tb_update_scroll(tb);
        break;
    }

    case VK_HOME:
        tb->base.cursor_position = 0;
        if (!shift) {
            tb->base.selection_start = 0;
            tb->base.selection_end   = 0;
        } else {
            tb->base.selection_end = 0;
        }
        tb_update_scroll(tb);
        break;

    case VK_END:
        tb->base.cursor_position = tb->buf_len;
        if (!shift) {
            tb->base.selection_start = tb->buf_len;
            tb->base.selection_end   = tb->buf_len;
        } else {
            tb->base.selection_end = tb->buf_len;
        }
        tb_update_scroll(tb);
        break;

    case VK_BACK:
        if (tb->base.readonly) break;
        tb_push_undo(tb);
        tb->last_op_was_typing = false;
        if (tb_has_selection(tb)) {
            tb_delete_selection(tb);
        } else if (tb->base.cursor_position > 0) {
            uint32_t prev = tb_utf8_prev(tb->buffer, tb->base.cursor_position);
            tb_delete_range(tb, prev, tb->base.cursor_position);
            tb->base.cursor_position = prev;
            tb->base.selection_start = prev;
            tb->base.selection_end   = prev;
        }
        tb_update_scroll(tb);
        tb_notify_change(tb);
        break;

    case VK_DELETE:
        if (tb->base.readonly) break;
        tb_push_undo(tb);
        tb->last_op_was_typing = false;
        if (tb_has_selection(tb)) {
            tb_delete_selection(tb);
        } else if (tb->base.cursor_position < tb->buf_len) {
            uint32_t next = tb_utf8_next(tb->buffer, tb->base.cursor_position);
            tb_delete_range(tb, tb->base.cursor_position, next);
        }
        tb_update_scroll(tb);
        tb_notify_change(tb);
        break;

    /* ---- NumberBox keyboard shortcuts (WinUI OnNumberBoxKeyDown) ---- */
    case VK_UP:
        if (nb_is_number_box(tb)) {
            nb_step_value(tb, tb->nb_small_change);
            tb->nb_stepping = true;
            tb->base.readonly = true;
            tb->base.selection_start = 0;
            tb->base.selection_end   = 0;
            tb->base.cursor_position = 0;
            return; /* consume — don't let textbox handle it */
        }
        break;
    case VK_DOWN:
        if (nb_is_number_box(tb)) {
            nb_step_value(tb, -tb->nb_small_change);
            tb->nb_stepping = true;
            tb->base.readonly = true;
            tb->base.selection_start = 0;
            tb->base.selection_end   = 0;
            tb->base.cursor_position = 0;
            return;
        }
        break;
    case VK_PRIOR: /* PageUp */
        if (nb_is_number_box(tb)) {
            nb_step_value(tb, tb->nb_large_change);
            tb->nb_stepping = true;
            tb->base.readonly = true;
            tb->base.selection_start = 0;
            tb->base.selection_end   = 0;
            tb->base.cursor_position = 0;
            return;
        }
        break;
    case VK_NEXT: /* PageDown */
        if (nb_is_number_box(tb)) {
            nb_step_value(tb, -tb->nb_large_change);
            tb->nb_stepping = true;
            tb->base.readonly = true;
            tb->base.selection_start = 0;
            tb->base.selection_end   = 0;
            tb->base.cursor_position = 0;
            return;
        }
        break;

    case VK_RETURN:
        if (nb_is_number_box(tb)) {
            tb->nb_stepping = false;
            tb->base.readonly = false;
            nb_validate_input(tb);
            return;
        }
        if (tb->base.on_submit)
            tb->base.on_submit(tb->base.on_submit_ctx);
        break;

    case VK_ESCAPE:
        if (nb_is_number_box(tb)) {
            tb->nb_stepping = false;
            tb->base.readonly = false;
            nb_update_text_to_value(tb); /* revert to current Value */
            return;
        }
        break;

    case 'A':
        if (ctrl) {
            tb->base.selection_start = 0;
            tb->base.selection_end   = tb->buf_len;
            tb->base.cursor_position = tb->buf_len;
        }
        break;

    case 'C':
        if (ctrl) tb_perform_copy(tb);
        break;

    case 'X':
        if (ctrl && tb_has_selection(tb) && !tb->base.readonly) {
            tb_perform_copy(tb);
            tb_push_undo(tb);
            tb->last_op_was_typing = false;
            tb_delete_selection(tb);
            tb_update_scroll(tb);
            tb_notify_change(tb);
        }
        break;

    case 'V':
        if (ctrl && !tb->base.readonly) {
            tb_perform_paste(tb);
        }
        break;

    case 'Z':
        if (ctrl) {
            tb_undo(tb);
            return;
        }
        break;

    case 'Y':
        if (ctrl) {
            tb_redo(tb);
            return;
        }
        break;

    default:
        break;
    }

    if (tb->base.cursor_position != old_cursor)
        tb_update_ime_position(tb);
}

/* ---- TextBox char handler ---- */
static void tb_on_char(void *ctx, wchar_t ch) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    if (!tb->base.enabled) return;
    if (tb->base.readonly) return;

    /* NumberBox: typing a character exits stepping mode → enter edit mode */
    if (nb_is_number_box(tb) && tb->nb_stepping) {
        tb->nb_stepping = false;
        tb->base.readonly = false;
    }
    uint32_t old_cursor = tb->base.cursor_position;

    /* Filter control characters */
    if (ch < 0x20 && ch != '\t') return;
    if (ch == 0x7F) return;  /* DEL */
    if (ch == '\t' && !tb->base.multiline) return;

    /*
     * Surrogate pair handling for U+10000+ (emoji, CJK Extension B, etc.)
     * Windows sends two WM_CHAR messages: high surrogate (0xD800–0xDBFF)
     * followed by low surrogate (0xDC00–0xDFFF).
     */
    char u8[4];
    int u8len = 0;

    if (ch >= 0xD800 && ch <= 0xDBFF) {
        /* High surrogate — buffer it, wait for the low half */
        tb->high_surrogate = ch;
        return;
    }

    if (ch >= 0xDC00 && ch <= 0xDFFF) {
        if (tb->high_surrogate == 0) return;  /* orphan low surrogate */
        /* Combine into a full code point and encode as 4-byte UTF-8 */
        uint32_t cp = 0x10000
            + ((uint32_t)(tb->high_surrogate - 0xD800) << 10)
            + ((uint32_t)(ch - 0xDC00));
        tb->high_surrogate = 0;
        u8[0] = (char)(0xF0 | (cp >> 18));
        u8[1] = (char)(0x80 | ((cp >> 12) & 0x3F));
        u8[2] = (char)(0x80 | ((cp >> 6) & 0x3F));
        u8[3] = (char)(0x80 | (cp & 0x3F));
        u8len = 4;
    } else {
        /* Stale high surrogate without a matching low — discard it */
        tb->high_surrogate = 0;

        if (ch < 0x80) {
            u8[0] = (char)ch;
            u8len = 1;
        } else if (ch < 0x800) {
            u8[0] = (char)(0xC0 | (ch >> 6));
            u8[1] = (char)(0x80 | (ch & 0x3F));
            u8len = 2;
        } else {
            u8[0] = (char)(0xE0 | (ch >> 12));
            u8[1] = (char)(0x80 | ((ch >> 6) & 0x3F));
            u8[2] = (char)(0x80 | (ch & 0x3F));
            u8len = 3;
        }
    }

    /* max_length check */
    if (tb->base.max_length > 0) {
        uint32_t after_len = tb->buf_len + (uint32_t)u8len;
        if (tb_has_selection(tb)) {
            uint32_t s = tb->base.selection_start < tb->base.selection_end
                         ? tb->base.selection_start : tb->base.selection_end;
            uint32_t e = tb->base.selection_start > tb->base.selection_end
                         ? tb->base.selection_start : tb->base.selection_end;
            after_len -= (e - s);
        }
        if (after_len > tb->base.max_length) return;
    }

    /* Undo grouping: merge consecutive typing within threshold */
    {
        ULONGLONG now_ms = GetTickCount64();
        bool is_space = (ch == ' ' || ch == '\t');

        if (tb->last_op_was_typing && !is_space &&
            (now_ms - tb->last_typing_time < TB_TYPING_MERGE_MS)) {
            tb->last_typing_time = now_ms;
        } else {
            tb_push_undo(tb);
            tb->last_typing_time = now_ms;
            tb->last_op_was_typing = true;
        }
    }

    if (tb_has_selection(tb))
        tb_delete_selection(tb);

    tb_insert_utf8(tb, tb->base.cursor_position, u8, (uint32_t)u8len);
    tb->base.cursor_position += (uint32_t)u8len;
    tb->base.selection_start = tb->base.cursor_position;
    tb->base.selection_end   = tb->base.cursor_position;

    tb_update_scroll(tb);
    tb_notify_change(tb);

    if (tb->base.cursor_position != old_cursor)
        tb_update_ime_position(tb);
}

/* ---- PasswordBox: build mask string for hit-testing ---- */
/* Each codepoint → ● (U+25CF = 3 UTF-8 bytes: E2 97 8F) */
static uint32_t pb_build_mask(const char *src, uint32_t src_len, char *dst, uint32_t dst_cap) {
    uint32_t out = 0;
    const unsigned char *p = (const unsigned char *)src;
    const unsigned char *end = p + src_len;
    while (p < end && *p) {
        int skip;
        if (*p < 0x80)       skip = 1;
        else if (*p < 0xE0)  skip = 2;
        else if (*p < 0xF0)  skip = 3;
        else                 skip = 4;
        if (out + 3 < dst_cap) {
            dst[out++] = (char)0xE2;
            dst[out++] = (char)0x97;
            dst[out++] = (char)0x8F;
        }
        for (int i = 0; i < skip && p < end; i++) p++;
    }
    if (out < dst_cap) dst[out] = '\0';
    else if (dst_cap > 0) dst[dst_cap - 1] = '\0';
    return out;
}

/* Convert a byte offset in mask text back to byte offset in original text.
   Mask: each codepoint is 3 bytes. Original: variable-length. */
static uint32_t pb_mask_offset_to_original(const char *original, uint32_t mask_byte_offset) {
    /* mask_byte_offset / 3 = codepoint index. Walk original to find that codepoint's byte offset. */
    uint32_t target_cp = mask_byte_offset / 3;
    uint32_t cp = 0;
    const unsigned char *p = (const unsigned char *)original;
    while (*p && cp < target_cp) {
        int skip;
        if (*p < 0x80)       skip = 1;
        else if (*p < 0xE0)  skip = 2;
        else if (*p < 0xF0)  skip = 3;
        else                 skip = 4;
        for (int i = 0; i < skip && *p; i++) p++;
        cp++;
    }
    return (uint32_t)(p - (const unsigned char *)original);
}

/* ---- TextBox click handler (position cursor via drag) ---- */
/* ---- TextBox drag handler (position cursor / extend selection) ---- */
static void tb_on_pointer_move(void *ctx, float local_x, float local_y) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    FluxTextRenderer *tr = tb->app ? tb->app->text : NULL;
    if (!tb->base.enabled || !tr) return;
    (void)local_y;

    float text_x = local_x - 10.0f; /* FLUX_TEXTBOX_PAD_L */
    text_x += tb->base.scroll_offset_x;

    FluxTextStyle ts = tb_make_style(tb);

    XentRect rect = {0};
    xent_get_layout_rect(tb->ctx, tb->node, &rect);
    float max_w = rect.width - 10.0f - 6.0f;

    /* TextBox / NumberBox: limit drag hit-test to text column (exclude buttons on right) */
    XentControlType move_ct = xent_get_control_type(tb->ctx, tb->node);
    if (move_ct == XENT_CONTROL_NUMBER_BOX) {
        /* NumberBox: always subtract spin area (if inline) + X button (if visible) */
        float reserved_mv = 0.0f;
        if (tb->nb_spin_placement == 2) reserved_mv += 76.0f;
        bool has_text_mv_nb = (tb->buffer && tb->buf_len > 0);
        FluxNodeData *nd_mv_nb = flux_node_store_get(tb->store, tb->node);
        bool mv_nb_focused = (nd_mv_nb && nd_mv_nb->state.focused);
        if (has_text_mv_nb && mv_nb_focused && !tb->base.readonly)
            reserved_mv += 40.0f;
        if (reserved_mv > 0.0f)
            max_w = (rect.width - reserved_mv) - 10.0f - 6.0f;
    } else if (move_ct == XENT_CONTROL_TEXT_INPUT) {
        bool has_text_mv_tb = (tb->buffer && tb->buf_len > 0);
        FluxNodeData *nd_mv_tb = flux_node_store_get(tb->store, tb->node);
        bool mv_tb_focused = (nd_mv_tb && nd_mv_tb->state.focused);
        if (has_text_mv_tb && mv_tb_focused && !tb->base.readonly) {
            max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
        }
    }

    /* PasswordBox: limit drag hit-test to Column 0 when reveal button is visible */
    if (move_ct == XENT_CONTROL_PASSWORD_BOX) {
        bool has_text_mv = (tb->buffer && tb->buf_len > 0);
        FluxNodeData *nd_mv = flux_node_store_get(tb->store, tb->node);
        bool mv_focused = (nd_mv && nd_mv->state.focused);
        if (has_text_mv && mv_focused) {
            max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
        }
    }

    /* NumberBox: limit drag hit-test to Column 0 when spin buttons visible */
    if (move_ct == XENT_CONTROL_NUMBER_BOX) {
        if (tb->nb_spin_placement == 2) { /* Inline */
            max_w = (rect.width - 76.0f) - 10.0f - 6.0f;
        }
    }

    /* For PasswordBox, hit-test against masked text (unless revealed) */
    bool move_use_mask = (move_ct == XENT_CONTROL_PASSWORD_BOX && tb->buf_len > 0
                          && !xent_get_semantic_checked(tb->ctx, tb->node));
    char move_mask[2048];
    const char *hit_text = tb->buffer;
    if (move_use_mask) {
        pb_build_mask(tb->buffer, tb->buf_len, move_mask, sizeof(move_mask));
        hit_text = move_mask;
    }

    int u16_pos = flux_text_hit_test(tr, hit_text, &ts,
                                      max_w + tb->base.scroll_offset_x,
                                      text_x, 0.0f);

    uint32_t byte_pos;
    if (move_use_mask) {
        /* Convert mask UTF-16 position → mask byte offset → original byte offset */
        uint32_t mask_byte = tb_utf16_to_byte_offset(hit_text, (uint32_t)u16_pos);
        byte_pos = pb_mask_offset_to_original(tb->buffer, mask_byte);
    } else {
        byte_pos = tb_utf16_to_byte_offset(tb->buffer, (uint32_t)u16_pos);
    }

    if (tb->dragging) {
        /* Drag selection: anchor stays, cursor moves */
        tb->base.selection_start = tb->drag_anchor;
        tb->base.selection_end   = byte_pos;
        tb->base.cursor_position = byte_pos;
    } else {
        /* No drag in progress — just position cursor */
        tb->base.cursor_position = byte_pos;
        tb->base.selection_start = byte_pos;
        tb->base.selection_end   = byte_pos;
    }
}

/* ---- TextBox pointer-down handler (single/double/triple click) ---- */
static void tb_on_pointer_down(void *ctx, float local_x, float local_y, int click_count) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    FluxTextRenderer *tr = tb->app ? tb->app->text : NULL;
    if (!tb->base.enabled || !tr) return;

    uint32_t old_cursor = tb->base.cursor_position;

    XentControlType ct = xent_get_control_type(tb->ctx, tb->node);

    /* --- TextBox / NumberBox: check if click is on the DeleteButton (X) ---
       TextBox: Width=30, visible when has_text && focused && !readonly
       NumberBox: MinWidth=34, visible only when spin buttons NOT inline */
    {
        bool del_eligible = false;
        float del_w = 30.0f;
        float del_right_offset = 0.0f; /* extra offset for spin buttons to the right */
        if (ct == XENT_CONTROL_TEXT_INPUT) {
            del_eligible = true;
        } else if (ct == XENT_CONTROL_NUMBER_BOX) {
            del_eligible = true;
            del_w = 40.0f;
            /* When spin inline, X sits left of the 76px spin area */
            if (tb->nb_spin_placement == 2)
                del_right_offset = 76.0f;
        }
        if (del_eligible) {
            bool has_text_del = (tb->buffer && tb->buf_len > 0);
            FluxNodeData *nd_del = flux_node_store_get(tb->store, tb->node);
            bool del_focused = (nd_del && nd_del->state.focused);
            bool show_delete = has_text_del && del_focused && !tb->base.readonly;
            if (show_delete) {
                XentRect rect = {0};
                xent_get_layout_rect(tb->ctx, tb->node, &rect);
                float delete_x = rect.width - del_w - del_right_offset;
                float delete_x_end = delete_x + del_w;
                if (local_x >= delete_x && local_x < delete_x_end) {
                    /* Clear all text */
                    if (nb_is_number_box(tb) && tb->nb_stepping) {
                        tb->nb_stepping = false;
                        tb->base.readonly = false;
                    }
                    tb_push_undo(tb);
                    tb->last_op_was_typing = false;
                    tb->buffer[0] = '\0';
                    tb->buf_len = 0;
                    tb->base.content = tb->buffer;
                    tb->base.cursor_position = 0;
                    tb->base.selection_start = 0;
                    tb->base.selection_end   = 0;
                    tb_update_scroll(tb);
                    tb_notify_change(tb);
                    return;
                }
            }
        }
    }

    /* --- PasswordBox: check if click is on the reveal button --- */
    if (ct == XENT_CONTROL_PASSWORD_BOX) {
        XentRect rect = {0};
        xent_get_layout_rect(tb->ctx, tb->node, &rect);
        /* WinUI: RevealButton visible only when has_text AND focused */
        bool has_text_pb = (tb->buffer && tb->buf_len > 0);
        FluxNodeData *nd_pb = flux_node_store_get(tb->store, tb->node);
        bool pb_focused = (nd_pb && nd_pb->state.focused);
        bool show_reveal = has_text_pb && pb_focused;
        if (show_reveal) {
            float reveal_x = rect.width - 30.0f;
            if (local_x >= reveal_x) {
                /* Press-to-reveal: set checked=1 on press (cleared on pointer_up / blur) */
                xent_set_semantic_checked(tb->ctx, tb->node, 1);
                return;
            }
        }
    }

    /* NumberBox spin-button clicks are fully intercepted at the input layer
       (flux_input_pointer_down) — they never reach here.  Only text-area
       clicks arrive, so exit stepping mode → enter edit mode. */
    if (nb_is_number_box(tb)) {
        /* Tell the upcoming on_focus call that this focus came from a click,
           so it should NOT enter stepping mode. */
        tb->nb_focus_from_click = true;
        if (tb->nb_stepping) {
            tb->nb_stepping = false;
            tb->base.readonly = false;
        }
    }
    (void)local_y;

    float text_x = local_x - 10.0f;
    text_x += tb->base.scroll_offset_x;

    FluxTextStyle ts = tb_make_style(tb);

    XentRect rect = {0};
    xent_get_layout_rect(tb->ctx, tb->node, &rect);
    float max_w = rect.width - 10.0f - 6.0f;

    /* TextBox / NumberBox: limit hit-test to text column (exclude buttons on right) */
    if (ct == XENT_CONTROL_NUMBER_BOX) {
        float reserved_dn = 0.0f;
        if (tb->nb_spin_placement == 2) reserved_dn += 76.0f;
        bool has_text_dn = (tb->buffer && tb->buf_len > 0);
        FluxNodeData *nd_dn = flux_node_store_get(tb->store, tb->node);
        bool dn_focused = (nd_dn && nd_dn->state.focused);
        if (has_text_dn && dn_focused && !tb->base.readonly)
            reserved_dn += 40.0f;
        if (reserved_dn > 0.0f)
            max_w = (rect.width - reserved_dn) - 10.0f - 6.0f;
    } else if (ct == XENT_CONTROL_TEXT_INPUT) {
        bool has_text_del2 = (tb->buffer && tb->buf_len > 0);
        FluxNodeData *nd_del2 = flux_node_store_get(tb->store, tb->node);
        bool del2_focused = (nd_del2 && nd_del2->state.focused);
        if (has_text_del2 && del2_focused && !tb->base.readonly) {
            max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
        }
    }

    /* PasswordBox: limit hit-test to Column 0 when reveal button is visible */
    if (ct == XENT_CONTROL_PASSWORD_BOX) {
        bool has_text_pb2 = (tb->buffer && tb->buf_len > 0);
        FluxNodeData *nd_pb2 = flux_node_store_get(tb->store, tb->node);
        bool pb2_focused = (nd_pb2 && nd_pb2->state.focused);
        if (has_text_pb2 && pb2_focused) {
            max_w = (rect.width - 30.0f) - 10.0f - 6.0f;
        }
    }

    /* NumberBox: limit hit-test to Column 0 when spin buttons visible */
    if (ct == XENT_CONTROL_NUMBER_BOX) {
        if (tb->nb_spin_placement == 2) { /* Inline */
            max_w = (rect.width - 76.0f) - 10.0f - 6.0f;
        }
    }

    /* For PasswordBox, hit-test against masked text (unless revealed) */
    bool down_use_mask = (ct == XENT_CONTROL_PASSWORD_BOX && tb->buf_len > 0
                          && !xent_get_semantic_checked(tb->ctx, tb->node));
    char down_mask[2048];
    const char *hit_text = tb->buffer;
    if (down_use_mask) {
        pb_build_mask(tb->buffer, tb->buf_len, down_mask, sizeof(down_mask));
        hit_text = down_mask;
    }

    int u16_pos = flux_text_hit_test(tr, hit_text, &ts,
                                      max_w + tb->base.scroll_offset_x,
                                      text_x, 0.0f);

    uint32_t byte_pos;
    if (down_use_mask) {
        uint32_t mask_byte = tb_utf16_to_byte_offset(hit_text, (uint32_t)u16_pos);
        byte_pos = pb_mask_offset_to_original(tb->buffer, mask_byte);
    } else {
        byte_pos = tb_utf16_to_byte_offset(tb->buffer, (uint32_t)u16_pos);
    }

    if (click_count >= 3) {
        /* Triple click: select all */
        tb->base.selection_start = 0;
        tb->base.selection_end   = tb->buf_len;
        tb->base.cursor_position = tb->buf_len;
        tb->dragging = false;
    } else if (click_count == 2) {
        /* Double click: select word */
        uint32_t ws = tb_word_start(tb->buffer, byte_pos);
        uint32_t we = tb_word_end(tb->buffer, byte_pos);
        tb->base.selection_start = ws;
        tb->base.selection_end   = we;
        tb->base.cursor_position = we;
        tb->dragging = false;
    } else {
        /* Single click: position cursor, begin drag */
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shift) {
            /* Shift-click extends selection */
            tb->base.selection_end = byte_pos;
            tb->base.cursor_position = byte_pos;
        } else {
            tb->base.cursor_position = byte_pos;
            tb->base.selection_start = byte_pos;
            tb->base.selection_end   = byte_pos;
        }
        tb->dragging = true;
        tb->drag_anchor = tb->base.selection_start;
    }

    if (tb->base.cursor_position != old_cursor)
        tb_update_ime_position(tb);
}

/* ---- TextBox IME composition handler ---- */
static void tb_on_ime_composition(void *ctx, const wchar_t *text, uint32_t len, uint32_t cursor) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;

    if (!text || len == 0) {
        /* End composition — update IME position to final cursor */
        tb_update_ime_position(tb);
        tb->base.composition_text = NULL;
        tb->base.composition_length = 0;
        tb->base.composition_cursor = 0;
        if (tb->ime_buf) { free(tb->ime_buf); tb->ime_buf = NULL; }
        tb->ime_buf_cap = 0;
        return;
    }

    /* On first non-empty composition, delete any selected text */
    if (tb->base.composition_text == NULL && tb_has_selection(tb)) {
        tb_push_undo(tb);
        tb_delete_selection(tb);
        tb_notify_change(tb);
    }

    /* Store composition text */
    uint32_t needed = len + 1;
    if (needed > tb->ime_buf_cap) {
        free(tb->ime_buf);
        tb->ime_buf_cap = needed;
        tb->ime_buf = (wchar_t *)malloc(needed * sizeof(wchar_t));
    }
    if (tb->ime_buf) {
        memcpy(tb->ime_buf, text, len * sizeof(wchar_t));
        tb->ime_buf[len] = 0;
    }
    tb->base.composition_text = tb->ime_buf;
    tb->base.composition_length = len;
    tb->base.composition_cursor = cursor;

    /* Move IME candidate window to follow the composition cursor */
    tb_update_ime_position(tb);
}

/* ---- TextBox context menu ---- */
#define TB_CM_CUT    1
#define TB_CM_COPY   2
#define TB_CM_PASTE  3
#define TB_CM_SELALL 4

static void tb_on_context_menu(void *ctx, float x, float y) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    if (!tb->base.enabled) return;
    if (!tb->app || !tb->app->window) return;

    HWND hwnd = flux_window_hwnd(tb->app->window);
    (void)x; (void)y;

    HMENU menu = CreatePopupMenu();
    if (!menu) return;

    bool has_sel = tb_has_selection(tb);
    bool can_paste = IsClipboardFormatAvailable(CF_UNICODETEXT);

    if (!tb->base.readonly)
        AppendMenuW(menu, MF_STRING | (has_sel ? 0 : MF_GRAYED), TB_CM_CUT, L"Cut\tCtrl+X");
    AppendMenuW(menu, MF_STRING | (has_sel ? 0 : MF_GRAYED), TB_CM_COPY, L"Copy\tCtrl+C");
    if (!tb->base.readonly)
        AppendMenuW(menu, MF_STRING | (can_paste ? 0 : MF_GRAYED), TB_CM_PASTE, L"Paste\tCtrl+V");
    AppendMenuW(menu, MF_SEPARATOR, 0, NULL);
    AppendMenuW(menu, MF_STRING, TB_CM_SELALL, L"Select All\tCtrl+A");

    POINT pt;
    GetCursorPos(&pt);
    int cmd = (int)TrackPopupMenu(menu, TPM_RETURNCMD | TPM_NONOTIFY,
                                   pt.x, pt.y, 0, hwnd, NULL);
    DestroyMenu(menu);

    switch (cmd) {
    case TB_CM_CUT:
        if (has_sel && !tb->base.readonly) {
            tb_perform_copy(tb);
            tb_push_undo(tb);
            tb_delete_selection(tb);
            tb_update_scroll(tb);
            tb_notify_change(tb);
        }
        break;
    case TB_CM_COPY:
        if (has_sel) tb_perform_copy(tb);
        break;
    case TB_CM_PASTE:
        if (!tb->base.readonly) tb_perform_paste(tb);
        break;
    case TB_CM_SELALL:
        tb->base.selection_start = 0;
        tb->base.selection_end   = tb->buf_len;
        tb->base.cursor_position = tb->buf_len;
        break;
    }
}

/* ════════════════════════════════════════════════════════════════════
   NumberBox core logic (mirrors WinUI NumberBox.cpp)
   ════════════════════════════════════════════════════════════════════ */

static bool nb_is_number_box(FluxTextBoxInputData *tb) {
    return xent_get_control_type(tb->ctx, tb->node) == XENT_CONTROL_NUMBER_BOX;
}

/* Format Value → text and update the buffer + TextBox display */
static void nb_update_text_to_value(FluxTextBoxInputData *tb) {
    double v = tb->nb_value;
    char tmp[64];
    int n;
    if (v != v) { /* NaN */
        tmp[0] = '\0';
        n = 0;
    } else if (v == (double)(long long)v && v >= -1e15 && v <= 1e15) {
        n = snprintf(tmp, sizeof(tmp), "%lld", (long long)v);
    } else {
        n = snprintf(tmp, sizeof(tmp), "%.6g", v);
    }
    if (n < 0) n = 0;
    if ((uint32_t)n >= tb->buf_cap) {
        tb_ensure_cap(tb, (uint32_t)n + 1);
    }
    memcpy(tb->buffer, tmp, (size_t)n + 1);
    tb->buf_len = (uint32_t)n;
    tb->base.content = tb->buffer;

    tb->nb_text_updating = true;
    /* Move cursor to end */
    tb->base.cursor_position = tb->buf_len;
    tb->base.selection_start = tb->buf_len;
    tb->base.selection_end   = tb->buf_len;
    tb_update_scroll(tb);
    tb->nb_text_updating = false;
}

/* Coerce Value to [min, max] if validation mode is Overwrite */
static void nb_coerce_value(FluxTextBoxInputData *tb) {
    double v = tb->nb_value;
    if (v != v) return; /* NaN — leave as-is */
    if (tb->nb_validation == 0) { /* Overwrite */
        if (v > tb->nb_maximum) tb->nb_value = tb->nb_maximum;
        else if (v < tb->nb_minimum) tb->nb_value = tb->nb_minimum;
    }
}

/* Update semantic properties so snapshot/renderer can see current state */
static void nb_sync_semantics(FluxTextBoxInputData *tb) {
    xent_set_semantic_value(tb->ctx, tb->node,
        (float)tb->nb_value, (float)tb->nb_minimum, (float)tb->nb_maximum);
    xent_set_semantic_expanded(tb->ctx, tb->node,
        tb->nb_spin_placement == 2); /* Inline */
}

/* Set Value with change detection, coercion, and event firing */
static void nb_set_value(FluxTextBoxInputData *tb, double new_value) {
    if (tb->nb_value_updating) return;
    double old = tb->nb_value;
    tb->nb_value_updating = true;

    tb->nb_value = new_value;
    nb_coerce_value(tb);
    new_value = tb->nb_value;

    bool changed = false;
    if (new_value != old) {
        if (!(new_value != new_value && old != old)) /* not both NaN */
            changed = true;
    }

    if (changed && tb->nb_on_value_change)
        tb->nb_on_value_change(tb->nb_on_value_change_ctx, new_value);

    nb_update_text_to_value(tb);
    nb_sync_semantics(tb);
    tb->nb_value_updating = false;
}

/* Parse current text → update Value (called on blur / Enter) */
static void nb_validate_input(FluxTextBoxInputData *tb) {
    if (!tb->buffer || tb->buf_len == 0) {
        /* Empty text → NaN */
        nb_set_value(tb, 0.0 / 0.0); /* NaN */
        return;
    }
    /* Trim whitespace */
    char *start = tb->buffer;
    while (*start == ' ' || *start == '\t') start++;
    char *end_ptr = NULL;
    double parsed = strtod(start, &end_ptr);
    if (end_ptr == start || *end_ptr != '\0') {
        /* Invalid input */
        if (tb->nb_validation == 0) { /* Overwrite → revert to current Value */
            nb_update_text_to_value(tb);
        }
        return;
    }
    if (parsed == tb->nb_value) {
        /* Same value but user may have typed "1+0" or extra zeros — normalize display */
        nb_update_text_to_value(tb);
    } else {
        nb_set_value(tb, parsed);
    }
}

/* Step Value by `change`, with optional wrapping */
static void nb_step_value(FluxTextBoxInputData *tb, double change) {
    /* Validate current text first (WinUI behavior) */
    nb_validate_input(tb);

    double v = tb->nb_value;
    if (v != v) return; /* NaN — can't step */

    v += change;

    if (tb->nb_is_wrap_enabled) {
        if (v > tb->nb_maximum) v = tb->nb_minimum;
        else if (v < tb->nb_minimum) v = tb->nb_maximum;
    }

    nb_set_value(tb, v);
}

/* ---- TextBox focus/blur ---- */
static void tb_on_focus(void *ctx) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    FluxNodeData *nd = flux_node_store_get(tb->store, tb->node);
    if (nd) nd->state.focused = 1;
    tb_update_ime_position(tb);

    /* NumberBox: if focus came from Tab (not a click), enter stepping mode —
       no selection, no caret. Clicking enters edit mode directly. */
    if (nb_is_number_box(tb)) {
        if (tb->nb_focus_from_click) {
            /* Focus triggered by click → stay in edit mode, select all (WinUI behavior) */
            tb->nb_focus_from_click = false;
            tb->base.selection_start = 0;
            tb->base.selection_end   = tb->buf_len;
            tb->base.cursor_position = tb->buf_len;
        } else {
            /* Focus triggered by Tab → stepping mode */
            tb->nb_stepping = true;
            tb->base.readonly = true;
            tb->base.selection_start = 0;
            tb->base.selection_end   = 0;
            tb->base.cursor_position = 0;
        }
    }
}

static void tb_on_blur(void *ctx) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    FluxNodeData *nd = flux_node_store_get(tb->store, tb->node);
    if (nd) nd->state.focused = 0;
    tb->base.selection_start = tb->base.cursor_position;
    tb->base.selection_end   = tb->base.cursor_position;
    /* Clear IME composition */
    tb->base.composition_text = NULL;
    tb->base.composition_length = 0;
    tb->base.composition_cursor = 0;
    /* Clear drag state */
    tb->dragging = false;

    /* PasswordBox: clear reveal on blur (press-to-reveal ends when focus lost) */
    if (xent_get_control_type(tb->ctx, tb->node) == XENT_CONTROL_PASSWORD_BOX) {
        xent_set_semantic_checked(tb->ctx, tb->node, 0);
    }

    /* NumberBox: validate input on blur (WinUI OnNumberBoxLostFocus) */
    if (nb_is_number_box(tb)) {
        nb_validate_input(tb);
    }
}

/* ---- Public factory ---- */
XentNodeId flux_create_textbox(XentContext *ctx, FluxNodeStore *store,
                               XentNodeId parent, const char *placeholder,
                               void (*on_change)(void *, const char *),
                               void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_TEXT_INPUT, flux_draw_textbox, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_TEXT_INPUT);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxTextBoxInputData *tb = (FluxTextBoxInputData *)calloc(1, sizeof(FluxTextBoxInputData));
        if (tb) {
            tb->buf_cap = FLUX_TEXTBOX_INITIAL_CAP;
            tb->buffer = (char *)calloc(1, tb->buf_cap);
            tb->buf_len = 0;
            tb->buffer[0] = '\0';
            tb->base.content = tb->buffer;
            tb->base.placeholder = placeholder;
            tb->base.font_size = 14.0f;
            tb->base.enabled = true;
            tb->base.on_change = on_change;
            tb->base.on_change_ctx = userdata;
            tb->ctx = ctx;
            tb->node = node;
            tb->store = store;
            tb->app = NULL;
        }
        nd->component_data = &tb->base;
        nd->on_pointer_move = tb_on_pointer_move;
        nd->on_pointer_move_ctx = tb;
        nd->on_focus = tb_on_focus;
        nd->on_focus_ctx = tb;
        nd->on_blur = tb_on_blur;
        nd->on_blur_ctx = tb;
        nd->on_key = tb_on_key;
        nd->on_key_ctx = tb;
        nd->on_char = tb_on_char;
        nd->on_char_ctx = tb;
        nd->on_pointer_down = tb_on_pointer_down;
        nd->on_pointer_down_ctx = tb;
        nd->on_ime_composition = tb_on_ime_composition;
        nd->on_ime_composition_ctx = tb;
        nd->on_context_menu = tb_on_context_menu;
        nd->on_context_menu_ctx = tb;
    }

    xent_set_semantic_role(ctx, node, XENT_SEMANTIC_CUSTOM);
    if (placeholder)
        xent_set_semantic_label(ctx, node, placeholder);

    return node;
}

XentNodeId flux_app_create_textbox(FluxApp *app, XentNodeId parent,
                                   const char *placeholder,
                                   void (*on_change)(void *, const char *),
                                   void *userdata) {
    if (!app || !app->ctx || !app->store) return XENT_NODE_INVALID;

    XentNodeId node = flux_create_textbox(app->ctx, app->store, parent,
                                          placeholder, on_change, userdata);
    if (node == XENT_NODE_INVALID) return node;

    /* Wire the app pointer so text renderer is accessible */
    FluxNodeData *nd = flux_node_store_get(app->store, node);
    if (nd && nd->component_data) {
        FluxTextBoxInputData *tb = (FluxTextBoxInputData *)nd->component_data;
        tb->app = app;
    }

    return node;
}

XentNodeId flux_app_create_password_box(FluxApp *app, XentNodeId parent,
                                        const char *placeholder,
                                        void (*on_change)(void *, const char *),
                                        void *userdata) {
    if (!app || !app->ctx || !app->store) return XENT_NODE_INVALID;

    XentNodeId node = flux_create_password_box(app->ctx, app->store, parent,
                                               placeholder, on_change, userdata);
    if (node == XENT_NODE_INVALID) return node;

    /* Wire the app pointer so text renderer is accessible */
    FluxNodeData *nd = flux_node_store_get(app->store, node);
    if (nd && nd->component_data) {
        FluxTextBoxInputData *tb = (FluxTextBoxInputData *)nd->component_data;
        tb->app = app;
    }

    return node;
}

XentNodeId flux_app_create_number_box(FluxApp *app, XentNodeId parent,
                                      double min_val, double max_val, double step,
                                      void (*on_value_change)(void *, double),
                                      void *userdata) {
    if (!app || !app->ctx || !app->store) return XENT_NODE_INVALID;

    XentNodeId node = flux_create_number_box(app->ctx, app->store, parent,
                                              min_val, max_val, step,
                                              on_value_change, userdata);
    if (node == XENT_NODE_INVALID) return node;

    /* Wire the app pointer so text renderer is accessible */
    FluxNodeData *nd = flux_node_store_get(app->store, node);
    if (nd && nd->component_data) {
        FluxTextBoxInputData *tb = (FluxTextBoxInputData *)nd->component_data;
        tb->app = app;
    }

    return node;
}

/* ---- Phase 1 convenience constructors ---- */

XentNodeId flux_create_password_box(XentContext *ctx, FluxNodeStore *store,
                                    XentNodeId parent, const char *placeholder,
                                    void (*on_change)(void *, const char *),
                                    void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_PASSWORD_BOX, flux_draw_password_box, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_PASSWORD_BOX);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxTextBoxInputData *tb = (FluxTextBoxInputData *)calloc(1, sizeof(FluxTextBoxInputData));
        if (tb) {
            tb->buf_cap = FLUX_TEXTBOX_INITIAL_CAP;
            tb->buffer = (char *)calloc(1, tb->buf_cap);
            tb->buf_len = 0;
            tb->buffer[0] = '\0';
            tb->base.content = tb->buffer;
            tb->base.placeholder = placeholder;
            tb->base.font_size = 14.0f;
            tb->base.enabled = true;
            tb->base.on_change = on_change;
            tb->base.on_change_ctx = userdata;
            tb->ctx = ctx;
            tb->node = node;
            tb->store = store;
            tb->app = NULL;
        }
        /* For PasswordBox, we wrap the component_data in a FluxPasswordBoxData.
           But since the renderer uses flux_draw_textbox and casts to FluxTextBoxData*,
           we keep using FluxTextBoxInputData and just set the control type. The
           mask rendering is handled specially in the renderer. */
        nd->component_data = &tb->base;
        nd->on_pointer_move = tb_on_pointer_move;
        nd->on_pointer_move_ctx = tb;
        nd->on_focus = tb_on_focus;
        nd->on_focus_ctx = tb;
        nd->on_blur = tb_on_blur;
        nd->on_blur_ctx = tb;
        nd->on_key = tb_on_key;
        nd->on_key_ctx = tb;
        nd->on_char = tb_on_char;
        nd->on_char_ctx = tb;
        nd->on_pointer_down = tb_on_pointer_down;
        nd->on_pointer_down_ctx = tb;
        nd->on_ime_composition = tb_on_ime_composition;
        nd->on_ime_composition_ctx = tb;
        nd->on_context_menu = tb_on_context_menu;
        nd->on_context_menu_ctx = tb;
    }

    xent_set_semantic_role(ctx, node, XENT_SEMANTIC_CUSTOM);
    if (placeholder)
        xent_set_semantic_label(ctx, node, placeholder);
    xent_set_focusable(ctx, node, true);

    /* Grid layout: WinUI PasswordBox template
       3 rows (Auto, *, Auto) simplified to 1 row Star (no Header/Description yet)
       2 columns: *(text area), Auto(RevealButton Width=30) → Pixel(30) since no child nodes */
    xent_set_protocol(ctx, node, XENT_PROTOCOL_GRID);
    {
        XentGridSizeMode row_modes[] = { XENT_GRID_STAR };
        float            row_vals[]  = { 1.0f };
        xent_set_grid_rows(ctx, node, row_modes, row_vals, 1);

        XentGridSizeMode col_modes[] = { XENT_GRID_STAR, XENT_GRID_PIXEL };
        float            col_vals[]  = { 1.0f, 30.0f };
        xent_set_grid_columns(ctx, node, col_modes, col_vals, 2);
    }

    return node;
}

XentNodeId flux_create_number_box(XentContext *ctx, FluxNodeStore *store,
                                  XentNodeId parent,
                                  double min_val, double max_val, double step,
                                  void (*on_value_change)(void *, double),
                                  void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_NUMBER_BOX, flux_draw_number_box, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_NUMBER_BOX);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxTextBoxInputData *tb = (FluxTextBoxInputData *)calloc(1, sizeof(FluxTextBoxInputData));
        if (tb) {
            tb->buf_cap = FLUX_TEXTBOX_INITIAL_CAP;
            tb->buffer = (char *)calloc(1, tb->buf_cap);
            tb->buf_len = 0;
            tb->base.font_size = 14.0f;
            tb->base.enabled = true;
            tb->ctx = ctx;
            tb->node = node;
            tb->store = store;
            tb->app = NULL;

            /* NumberBox-specific initialization (WinUI defaults) */
            tb->nb_minimum = min_val;
            tb->nb_maximum = max_val;
            tb->nb_small_change = step;
            tb->nb_large_change = step * 10.0;
            tb->nb_value = min_val; /* start at minimum (not NaN) */
            tb->nb_is_wrap_enabled = false;
            tb->nb_spin_placement = 2; /* FLUX_NB_SPIN_INLINE — visible by default for our API */
            tb->nb_validation = 0;     /* InvalidInputOverwritten */
            tb->nb_on_value_change = on_value_change;
            tb->nb_on_value_change_ctx = userdata;

            /* Initialize text from value */
            nb_update_text_to_value(tb);
        }
        nd->component_data = &tb->base;
        nd->on_pointer_move = tb_on_pointer_move;
        nd->on_pointer_move_ctx = tb;
        nd->on_focus = tb_on_focus;
        nd->on_focus_ctx = tb;
        nd->on_blur = tb_on_blur;
        nd->on_blur_ctx = tb;
        nd->on_key = tb_on_key;
        nd->on_key_ctx = tb;
        nd->on_char = tb_on_char;
        nd->on_char_ctx = tb;
        nd->on_pointer_down = tb_on_pointer_down;
        nd->on_pointer_down_ctx = tb;
        nd->on_ime_composition = tb_on_ime_composition;
        nd->on_ime_composition_ctx = tb;
        nd->on_context_menu = tb_on_context_menu;
        nd->on_context_menu_ctx = tb;
    }

    xent_set_semantic_role(ctx, node, XENT_SEMANTIC_CUSTOM);
    xent_set_semantic_value(ctx, node, (float)min_val, (float)min_val, (float)max_val);
    xent_set_semantic_expanded(ctx, node, true); /* Inline spin visible */
    xent_set_focusable(ctx, node, true);

    /* Grid layout */
    xent_set_protocol(ctx, node, XENT_PROTOCOL_GRID);
    {
        XentGridSizeMode row_modes[] = { XENT_GRID_STAR };
        float            row_vals[]  = { 1.0f };
        xent_set_grid_rows(ctx, node, row_modes, row_vals, 1);

        XentGridSizeMode col_modes[] = { XENT_GRID_STAR, XENT_GRID_PIXEL, XENT_GRID_PIXEL };
        float            col_vals[]  = { 1.0f, 40.0f, 36.0f };
        xent_set_grid_columns(ctx, node, col_modes, col_vals, 3);
    }

    return node;
}

static void hyperlink_on_click(void *ctx) {
    FluxHyperlinkData *hd = (FluxHyperlinkData *)ctx;
    if (hd && hd->url && hd->url[0]) {
        /* Convert URL to wide string and open */
        int wlen = MultiByteToWideChar(CP_UTF8, 0, hd->url, -1, NULL, 0);
        wchar_t *wurl = (wchar_t *)_alloca(wlen * sizeof(wchar_t));
        MultiByteToWideChar(CP_UTF8, 0, hd->url, -1, wurl, wlen);
        ShellExecuteW(NULL, L"open", wurl, NULL, NULL, SW_SHOWNORMAL);
    }
    /* Also fire user's on_click if provided */
    if (hd && hd->on_click)
        hd->on_click(hd->on_click_ctx);
}

XentNodeId flux_create_hyperlink(XentContext *ctx, FluxNodeStore *store,
                                 XentNodeId parent, const char *label,
                                 const char *url,
                                 void (*on_click)(void *), void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_HYPERLINK, flux_draw_hyperlink, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_HYPERLINK);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxHyperlinkData *hd = (FluxHyperlinkData *)calloc(1, sizeof(FluxHyperlinkData));
        if (hd) {
            hd->label = label;
            hd->url = url;
            hd->font_size = 14.0f;
            hd->enabled = true;
            hd->on_click = on_click;
            hd->on_click_ctx = userdata;
        }
        nd->component_data = hd;
        nd->on_click = hyperlink_on_click;
        nd->on_click_ctx = hd;
    }

    xent_set_semantic_role(ctx, node, XENT_SEMANTIC_BUTTON);
    if (label)
        xent_set_semantic_label(ctx, node, label);
    xent_set_focusable(ctx, node, true);

    /* Grid layout: WinUI NumberBox template
       3 rows (Auto, *, Auto) simplified to 1 row Star
       3 columns: *(InputBox), Auto(UpSpin), Auto(DownSpin)
       InputBox has ColumnSpan=3 (full width), spin buttons overlay.
       Auto cols: Up Margin="4" → 40px, Down Margin="0,4,4,4" → 36px */
    xent_set_protocol(ctx, node, XENT_PROTOCOL_GRID);
    {
        XentGridSizeMode row_modes[] = { XENT_GRID_STAR };
        float            row_vals[]  = { 1.0f };
        xent_set_grid_rows(ctx, node, row_modes, row_vals, 1);

        XentGridSizeMode col_modes[] = { XENT_GRID_STAR, XENT_GRID_PIXEL, XENT_GRID_PIXEL };
        float            col_vals[]  = { 1.0f, 40.0f, 36.0f };
        xent_set_grid_columns(ctx, node, col_modes, col_vals, 3);
    }

    return node;
}

typedef struct FluxRepeatButtonInputData {
    FluxRepeatButtonData base;
    XentNodeId           node;
    UINT_PTR             timer_id;
    FluxWindow          *window;    /* for requesting redraws */
} FluxRepeatButtonInputData;

static FluxRepeatButtonInputData *s_active_repeat = NULL;

static void CALLBACK repeat_timer_proc(HWND hwnd, UINT msg, UINT_PTR id, DWORD elapsed) {
    (void)hwnd; (void)msg; (void)elapsed;
    if (s_active_repeat && s_active_repeat->timer_id == id) {
        /* Fire on_click */
        if (s_active_repeat->base.on_click)
            s_active_repeat->base.on_click(s_active_repeat->base.on_click_ctx);

        /* After initial delay, switch to repeat interval */
        uint32_t interval = s_active_repeat->base.repeat_interval_ms;
        if (interval < 10) interval = 50;
        KillTimer(NULL, id);
        s_active_repeat->timer_id = SetTimer(NULL, 0, interval, repeat_timer_proc);
    }
}

static void repeat_on_pointer_down(void *ctx, float x, float y, int click_count) {
    (void)x; (void)y; (void)click_count;
    FluxRepeatButtonInputData *rb = (FluxRepeatButtonInputData *)ctx;
    if (!rb) return;

    /* Fire initial click immediately */
    if (rb->base.on_click)
        rb->base.on_click(rb->base.on_click_ctx);

    /* Start repeat timer with initial delay */
    s_active_repeat = rb;
    uint32_t delay = rb->base.repeat_delay_ms;
    if (delay < 10) delay = 400;
    rb->timer_id = SetTimer(NULL, 0, delay, repeat_timer_proc);
}

static void repeat_stop_timer(FluxRepeatButtonInputData *rb) {
    if (!rb) return;
    if (rb->timer_id) {
        KillTimer(NULL, rb->timer_id);
        rb->timer_id = 0;
    }
    if (s_active_repeat == rb) s_active_repeat = NULL;
}

static void repeat_on_blur(void *ctx) {
    repeat_stop_timer((FluxRepeatButtonInputData *)ctx);
}

/* Called on pointer_up (via nd->on_click). Stops the repeat timer.
   Does NOT fire the user callback — repeated clicks already happened
   via the timer during pointer-down. */
static void repeat_on_pointer_up(void *ctx) {
    repeat_stop_timer((FluxRepeatButtonInputData *)ctx);
}

XentNodeId flux_create_repeat_button(XentContext *ctx, FluxNodeStore *store,
                                     XentNodeId parent, const char *label,
                                     void (*on_click)(void *), void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_REPEAT_BUTTON, flux_draw_button, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_REPEAT_BUTTON);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxRepeatButtonInputData *rb = (FluxRepeatButtonInputData *)calloc(1, sizeof(FluxRepeatButtonInputData));
        if (rb) {
            rb->base.label = label;
            rb->base.font_size = 14.0f;
            rb->base.style = FLUX_BUTTON_STANDARD;
            rb->base.enabled = true;
            rb->base.repeat_delay_ms = 400;
            rb->base.repeat_interval_ms = 50;
            rb->base.on_click = on_click;
            rb->base.on_click_ctx = userdata;
            rb->node = node;
        }
        nd->component_data = &rb->base;
        /* on_click is fired by flux_input_pointer_up — use it to stop
           the repeat timer rather than firing an extra click. */
        nd->on_click = repeat_on_pointer_up;
        nd->on_click_ctx = rb;
        nd->on_pointer_down = repeat_on_pointer_down;
        nd->on_pointer_down_ctx = rb;
        nd->on_blur = repeat_on_blur;
        nd->on_blur_ctx = rb;
    }

    xent_set_semantic_role(ctx, node, XENT_SEMANTIC_BUTTON);
    if (label)
        xent_set_semantic_label(ctx, node, label);
    xent_set_focusable(ctx, node, true);

    return node;
}

XentNodeId flux_create_progress_ring(XentContext *ctx, FluxNodeStore *store,
                                     XentNodeId parent,
                                     float value, float max_value) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_PROGRESS_RING, flux_draw_progress_ring, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_PROGRESS_RING);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    xent_set_semantic_value(ctx, node, value, 0.0f, max_value);

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxProgressRingData *pd = (FluxProgressRingData *)calloc(1, sizeof(FluxProgressRingData));
        if (pd) {
            pd->value = value;
            pd->max_value = max_value;
            pd->indeterminate = (max_value <= 0.0f);
        }
        nd->component_data = pd;
    }

    return node;
}

XentNodeId flux_create_info_badge(XentContext *ctx, FluxNodeStore *store,
                                  XentNodeId parent, FluxInfoBadgeMode mode,
                                  int32_t value) {
    if (!ctx || !store) return XENT_NODE_INVALID;
    flux_register_renderer(XENT_CONTROL_INFO_BADGE, flux_draw_info_badge, NULL);

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_INFO_BADGE);
    if (node == XENT_NODE_INVALID) return XENT_NODE_INVALID;

    FluxNodeData *nd = flux_node_store_get(store, node);
    if (nd) {
        FluxInfoBadgeData *bd = (FluxInfoBadgeData *)calloc(1, sizeof(FluxInfoBadgeData));
        if (bd) {
            bd->mode = mode;
            bd->value = value;
        }
        nd->component_data = bd;
    }

    return node;
}