#include "fluxent/fluxent.h"
#include <xent/xent.h>

static void on_hello_click(void *ctx) {
    (void)ctx;
}

static void on_accent_click(void *ctx) {
    (void)ctx;
}

static void on_check_change(void *ctx, FluxCheckState state) {
    (void)ctx;
    (void)state;
}

static void on_slider_change(void *ctx, float value) {
    (void)ctx;
    (void)value;
}

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
    (void)hInst; (void)hPrev; (void)cmdLine; (void)showCmd;

    XentConfig xcfg = {0};
    xcfg.initial_capacity = 64;
    XentContext *ctx = xent_create_context(&xcfg);
    if (!ctx) return 1;

    FluxNodeStore *store = flux_node_store_create(64);
    if (!store) { xent_destroy_context(ctx); return 1; }

    XentNodeId root = xent_create_node(ctx);
    xent_set_protocol(ctx, root, XENT_PROTOCOL_FLEX);
    xent_set_flex_direction(ctx, root, XENT_FLEX_COLUMN);
    xent_set_flex_align_items(ctx, root, XENT_FLEX_ALIGN_CENTER);
    xent_set_flex_justify_content(ctx, root, XENT_FLEX_JUSTIFY_CENTER);
    xent_set_padding(ctx, root, 40, 40, 40, 40);
    xent_set_gap(ctx, root, 16);

    XentNodeId title = flux_create_text(ctx, store, root, "Hello, Fluxent!", 28.0f);
    xent_set_size(ctx, title, 300, 40);

    XentNodeId subtitle = flux_create_text(ctx, store, root, "A pure-C Fluent Design UI framework", 14.0f);
    xent_set_size(ctx, subtitle, 320, 24);

    XentNodeId btn_row = xent_create_node(ctx);
    xent_set_protocol(ctx, btn_row, XENT_PROTOCOL_FLEX);
    xent_set_flex_direction(ctx, btn_row, XENT_FLEX_ROW);
    xent_set_gap(ctx, btn_row, 8);
    xent_set_size(ctx, btn_row, 256, 32);
    xent_append_child(ctx, root, btn_row);

    XentNodeId btn1 = flux_create_button(ctx, store, btn_row, "Click Me", on_hello_click, NULL);
    xent_set_size(ctx, btn1, 120, 32);

    XentNodeId btn2 = flux_create_button(ctx, store, btn_row, "Accent", on_accent_click, NULL);
    xent_set_size(ctx, btn2, 120, 32);
    flux_button_set_style(store, btn2, FLUX_BUTTON_ACCENT);

    XentNodeId chk = flux_create_checkbox(ctx, store, root, "Enable something", false,
                                          on_check_change, NULL);
    xent_set_size(ctx, chk, 200, 32);

    XentNodeId sld = flux_create_slider(ctx, store, root, 0.0f, 100.0f, 50.0f,
                                        on_slider_change, NULL);
    xent_set_size(ctx, sld, 300, 32);

    FluxAppConfig cfg = {0};
    cfg.title = L"Hello Fluxent";
    cfg.width = 600;
    cfg.height = 400;
    cfg.dark_mode = flux_theme_system_is_dark();
    cfg.backdrop = FLUX_BACKDROP_MICA;

    FluxApp *app = NULL;
    HRESULT hr = flux_app_create(&cfg, &app);
    if (FAILED(hr)) {
        flux_node_store_destroy(store);
        xent_destroy_context(ctx);
        return 1;
    }

    flux_app_set_root(app, ctx, root, store);

    int result = flux_app_run(app);

    flux_app_destroy(app);
    flux_node_store_destroy(store);
    xent_destroy_context(ctx);
    return result;
}