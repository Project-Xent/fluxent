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

        /* Switch cursor shape based on hovered control type */
        XentNodeId hovered = flux_input_get_hovered(app->input);
        int cursor = FLUX_CURSOR_ARROW;
        if (hovered != XENT_NODE_INVALID) {
            XentControlType ct = xent_get_control_type(app->ctx, hovered);
            switch (ct) {
            case XENT_CONTROL_TEXT_INPUT:
                cursor = FLUX_CURSOR_IBEAM;
                break;
            case XENT_CONTROL_BUTTON:
            case XENT_CONTROL_TOGGLE_BUTTON:
            case XENT_CONTROL_CHECKBOX:
            case XENT_CONTROL_RADIO:
            case XENT_CONTROL_SWITCH:
            case XENT_CONTROL_SLIDER:
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
    if (down)
        flux_input_key_down(app->input, vk);
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

XentNodeId flux_create_radio(XentContext *ctx, FluxNodeStore *store,
                             XentNodeId parent, const char *label,
                             bool checked,
                             void (*on_change)(void *, FluxCheckState),
                             void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;

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

    XentNodeId node = create_node_with_parent(ctx, store, parent, XENT_CONTROL_CARD);
    return node;
}

XentNodeId flux_create_divider(XentContext *ctx, FluxNodeStore *store,
                               XentNodeId parent) {
    if (!ctx || !store) return XENT_NODE_INVALID;

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

    case VK_RETURN:
        if (tb->base.on_submit)
            tb->base.on_submit(tb->base.on_submit_ctx);
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

    int u16_pos = flux_text_hit_test(tr, tb->buffer, &ts,
                                      max_w + tb->base.scroll_offset_x,
                                      text_x, 0.0f);
    uint32_t byte_pos = tb_utf16_to_byte_offset(tb->buffer, (uint32_t)u16_pos);

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
    (void)local_y;
    uint32_t old_cursor = tb->base.cursor_position;

    float text_x = local_x - 10.0f;
    text_x += tb->base.scroll_offset_x;

    FluxTextStyle ts = tb_make_style(tb);

    XentRect rect = {0};
    xent_get_layout_rect(tb->ctx, tb->node, &rect);
    float max_w = rect.width - 10.0f - 6.0f;

    int u16_pos = flux_text_hit_test(tr, tb->buffer, &ts,
                                      max_w + tb->base.scroll_offset_x,
                                      text_x, 0.0f);
    uint32_t byte_pos = tb_utf16_to_byte_offset(tb->buffer, (uint32_t)u16_pos);

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

/* ---- TextBox focus/blur ---- */
static void tb_on_focus(void *ctx) {
    FluxTextBoxInputData *tb = (FluxTextBoxInputData *)ctx;
    FluxNodeData *nd = flux_node_store_get(tb->store, tb->node);
    if (nd) nd->state.focused = 1;
    tb_update_ime_position(tb);
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
}

/* ---- Public factory ---- */
XentNodeId flux_create_textbox(XentContext *ctx, FluxNodeStore *store,
                               XentNodeId parent, const char *placeholder,
                               void (*on_change)(void *, const char *),
                               void *userdata) {
    if (!ctx || !store) return XENT_NODE_INVALID;

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