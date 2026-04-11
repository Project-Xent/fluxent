#include "fluxent/fluxent.h"
#include "fluxent/flux_node_store.h"
#include <xent/xent.h>
#include <stdlib.h>


/* ── Radio group ─────────────────────────────────────────────────────── */

#define RADIO_GROUP_MAX 8

typedef struct {
    FluxNodeStore *store;
    XentNodeId     nodes[RADIO_GROUP_MAX];
    int            count;
} RadioGroup;

typedef struct {
    RadioGroup *group;
    int         index;
} RadioGroupMember;

static void radio_group_trampoline(void *ctx) {
    RadioGroupMember *m = (RadioGroupMember *)ctx;
    if (!m) return;
    RadioGroup *g = m->group;
    for (int i = 0; i < g->count; i++) {
        FluxNodeData *nd = flux_node_store_get(g->store, g->nodes[i]);
        if (!nd || !nd->component_data) continue;
        FluxCheckboxData *cd = (FluxCheckboxData *)nd->component_data;
        if (!cd->enabled) continue;
        FluxCheckState new_state = (i == m->index) ? FLUX_CHECK_CHECKED : FLUX_CHECK_UNCHECKED;
        cd->state = new_state;
        if (i == m->index && cd->on_change)
            cd->on_change(cd->on_change_ctx, new_state);
    }
}

/* ── Counter state ───────────────────────────────────────────────────── */

typedef struct {
    int            count;
    XentNodeId     label_node;
    FluxNodeStore *store;
    char           buf[32];
} CounterState;

static void on_counter_click(void *ctx) {
    CounterState *cs = (CounterState *)ctx;
    cs->count++;
    snprintf(cs->buf, sizeof(cs->buf), "Clicked: %d", cs->count);
    flux_text_set_content(cs->store, cs->label_node, cs->buf);
}

/* ── Placeholder callbacks ───────────────────────────────────────────── */

static void on_check_change(void *ctx, FluxCheckState s)  { (void)ctx; (void)s; }
static void on_switch_change(void *ctx, FluxCheckState s) { (void)ctx; (void)s; }
static void on_radio_change(void *ctx, FluxCheckState s)  { (void)ctx; (void)s; }
static void on_slider_change(void *ctx, float v)          { (void)ctx; (void)v; }
static void on_textbox_change(void *ctx, const char *text) { (void)ctx; (void)text; }

/* ── Layout helpers ──────────────────────────────────────────────────── */

static XentNodeId make_row(XentContext *ctx, XentNodeId parent, float gap, float h) {
    XentNodeId row = xent_create_node(ctx);
    xent_set_protocol(ctx, row, XENT_PROTOCOL_FLEX);
    xent_set_flex_direction(ctx, row, XENT_FLEX_ROW);
    xent_set_flex_align_items(ctx, row, XENT_FLEX_ALIGN_CENTER);
    xent_set_gap(ctx, row, gap);
    xent_set_size(ctx, row, 360, h);
    xent_append_child(ctx, parent, row);
    return row;
}

/* ── Entry point ─────────────────────────────────────────────────────── */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
    (void)hInst; (void)hPrev; (void)cmdLine; (void)showCmd;

    XentConfig xcfg = {0};
    xcfg.initial_capacity = 128;
    XentContext *ctx = xent_create_context(&xcfg);
    if (!ctx) return 1;

    FluxNodeStore *store = flux_node_store_create(128);
    if (!store) { xent_destroy_context(ctx); return 1; }

    /* ── Counter state (heap so pointer is stable) ───────────────────── */
    CounterState *counter = (CounterState *)calloc(1, sizeof(CounterState));
    if (!counter) { flux_node_store_destroy(store); xent_destroy_context(ctx); return 1; }
    counter->store = store;
    counter->count = 0;
    snprintf(counter->buf, sizeof(counter->buf), "Clicked: 0");

    /* ── Radio group (heap) ──────────────────────────────────────────── */
    RadioGroup       *rg      = (RadioGroup *)calloc(1, sizeof(RadioGroup));
    RadioGroupMember *members = (RadioGroupMember *)calloc(RADIO_GROUP_MAX, sizeof(RadioGroupMember));
    if (!rg || !members) return 1;
    rg->store = store;

    /* ── Root ────────────────────────────────────────────────────────── */

    XentNodeId root = xent_create_node(ctx);
    xent_set_protocol(ctx, root, XENT_PROTOCOL_FLEX);
    xent_set_flex_direction(ctx, root, XENT_FLEX_COLUMN);
    xent_set_flex_align_items(ctx, root, XENT_FLEX_ALIGN_CENTER);
    xent_set_flex_justify_content(ctx, root, XENT_FLEX_JUSTIFY_CENTER);
    xent_set_padding(ctx, root, 40, 40, 40, 40);
    xent_set_gap(ctx, root, 20);

    /* ── Title ───────────────────────────────────────────────────────── */

    XentNodeId title = flux_create_text(ctx, store, root, "Hello, Fluxent!", 28.0f);
    xent_set_size(ctx, title, 320, 36);

    XentNodeId subtitle = flux_create_text(ctx, store, root,
                                           "A pure-C Fluent Design UI framework", 14.0f);
    xent_set_size(ctx, subtitle, 320, 20);

    /* ── Divider ─────────────────────────────────────────────────────── */

    XentNodeId div1 = flux_create_divider(ctx, store, root);
    xent_set_size(ctx, div1, 360, 1);

    /* ── Button row ──────────────────────────────────────────────────── */

    XentNodeId btn_row = make_row(ctx, root, 8, 32);

    XentNodeId btn_std = flux_create_button(ctx, store, btn_row,
                                            "Standard", NULL, NULL);
    xent_set_size(ctx, btn_std, 110, 32);

    XentNodeId btn_acc = flux_create_button(ctx, store, btn_row,
                                            "Accent", NULL, NULL);
    xent_set_size(ctx, btn_acc, 110, 32);
    flux_button_set_style(store, btn_acc, FLUX_BUTTON_ACCENT);

    XentNodeId btn_out = flux_create_button(ctx, store, btn_row,
                                            "Outline", NULL, NULL);
    xent_set_size(ctx, btn_out, 110, 32);
    flux_button_set_style(store, btn_out, FLUX_BUTTON_OUTLINE);

    /* ── Counter row ─────────────────────────────────────────────────── */

    XentNodeId counter_row = make_row(ctx, root, 12, 32);

    XentNodeId counter_btn = flux_create_button(ctx, store, counter_row,
                                                "Count", on_counter_click, counter);
    xent_set_size(ctx, counter_btn, 110, 32);
    flux_button_set_style(store, counter_btn, FLUX_BUTTON_ACCENT);

    XentNodeId counter_label = flux_create_text(ctx, store, counter_row,
                                                counter->buf, 14.0f);
    xent_set_size(ctx, counter_label, 120, 32);
    counter->label_node = counter_label;

    /* ── Checkbox + Switch row ───────────────────────────────────────── */

    XentNodeId toggle_row = make_row(ctx, root, 24, 32);

    XentNodeId chk = flux_create_checkbox(ctx, store, toggle_row,
                                          "Enable feature", false,
                                          on_check_change, NULL);
    xent_set_size(ctx, chk, 160, 32);

    XentNodeId sw = flux_create_switch(ctx, store, toggle_row,
                                       "Dark mode", false,
                                       on_switch_change, NULL);
    xent_set_size(ctx, sw, 140, 32);

    /* ── Radio group ─────────────────────────────────────────────────── */

    XentNodeId radio_row = make_row(ctx, root, 16, 32);

    XentNodeId r1 = flux_create_radio(ctx, store, radio_row,
                                      "Option A", true,  on_radio_change, NULL);
    xent_set_size(ctx, r1, 110, 32);

    XentNodeId r2 = flux_create_radio(ctx, store, radio_row,
                                      "Option B", false, on_radio_change, NULL);
    xent_set_size(ctx, r2, 110, 32);

    XentNodeId r3 = flux_create_radio(ctx, store, radio_row,
                                      "Option C", false, on_radio_change, NULL);
    xent_set_size(ctx, r3, 110, 32);

    /* Wire mutual exclusion */
    rg->nodes[0] = r1; rg->nodes[1] = r2; rg->nodes[2] = r3;
    rg->count = 3;
    for (int i = 0; i < rg->count; i++) {
        members[i].group = rg;
        members[i].index = i;
        FluxNodeData *nd = flux_node_store_get(store, rg->nodes[i]);
        if (nd) {
            nd->on_click     = radio_group_trampoline;
            nd->on_click_ctx = &members[i];
        }
    }

    /* ── Slider ──────────────────────────────────────────────────────── */

    XentNodeId sld = flux_create_slider(ctx, store, root,
                                        0.0f, 100.0f, 50.0f,
                                        on_slider_change, NULL);
    xent_set_size(ctx, sld, 360, 32);

    /* ── Progress bar ────────────────────────────────────────────────── */

    XentNodeId prog = flux_create_progress(ctx, store, root, 65.0f, 100.0f);
    xent_set_size(ctx, prog, 360, 20);

    /* ── Divider ─────────────────────────────────────────────────────── */

    XentNodeId div2 = flux_create_divider(ctx, store, root);
    xent_set_size(ctx, div2, 360, 1);

    /* ── Footer ──────────────────────────────────────────────────────── */

    XentNodeId footer = flux_create_text(ctx, store, root,
                                         "xent-core + fluxent — Mica backdrop", 12.0f);
    xent_set_size(ctx, footer, 320, 16);

    /* ── App ─────────────────────────────────────────────────────────── */

    FluxAppConfig cfg = {0};
    cfg.title    = L"Hello Fluxent";
    cfg.width    = 600;
    cfg.height   = 580;
    cfg.dark_mode = flux_theme_system_is_dark();
    cfg.backdrop  = FLUX_BACKDROP_MICA;

    FluxApp *app = NULL;
    HRESULT hr = flux_app_create(&cfg, &app);
    if (FAILED(hr)) {
        free(counter); free(rg); free(members);
        flux_node_store_destroy(store);
        xent_destroy_context(ctx);
        return 1;
    }

    flux_app_set_root(app, ctx, root, store);

    /* ── TextBox (created after app setup for text renderer access) ── */

    XentNodeId textbox = flux_app_create_textbox(app, root,
                                                  "Type something here...",
                                                  on_textbox_change, NULL);
    xent_set_size(ctx, textbox, 360, 32);

    int result = flux_app_run(app);

    flux_app_destroy(app);
    flux_node_store_destroy(store);
    xent_destroy_context(ctx);
    free(counter);
    free(rg);
    free(members);
    return result;
}