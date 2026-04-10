#include "fluxent/fluxent.h"
#include "fluxent/flux_window.h"
#include "fluxent/flux_graphics.h"
#include "fluxent/flux_input.h"
#include "fluxent/flux_text.h"
#include "flux_render_internal.h"
#include "fluxent/flux_theme.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

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

HRESULT flux_app_create(const FluxAppConfig *cfg, FluxApp **out) {
    if (!out) return E_INVALIDARG;
    *out = NULL;

    FluxApp *app = (FluxApp *)calloc(1, sizeof(*app));
    if (!app) return E_OUTOFMEMORY;

    app->root = XENT_NODE_INVALID;

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
    flux_window_set_setting_changed_callback(app->window, app_setting_changed, app);

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