#include "fluxent/fluxent.h"
#include "fluxent/flux_node_store.h"
#include <xent/xent.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>


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
    char           buf[64];
} CounterState;

static void on_counter_click(void *ctx) {
    CounterState *cs = (CounterState *)ctx;
    cs->count++;
    snprintf(cs->buf, sizeof(cs->buf), "Clicked: %d", cs->count);
    flux_text_set_content(cs->store, cs->label_node, cs->buf);
}

/* ── RepeatButton counter ────────────────────────────────────────────── */

typedef struct {
    int            count;
    XentNodeId     label_node;
    FluxNodeStore *store;
    char           buf[64];
} RepeatCounterState;

static void on_repeat_click(void *ctx) {
    RepeatCounterState *rs = (RepeatCounterState *)ctx;
    rs->count++;
    snprintf(rs->buf, sizeof(rs->buf), "Repeated: %d", rs->count);
    flux_text_set_content(rs->store, rs->label_node, rs->buf);
}

/* ── ToggleButton state ──────────────────────────────────────────────── */

typedef struct {
    bool           toggled;
    XentNodeId     node;
    FluxNodeStore *store;
    XentContext   *ctx;
} ToggleState;

static void on_toggle_click(void *ctx_ptr) {
    ToggleState *ts = (ToggleState *)ctx_ptr;
    ts->toggled = !ts->toggled;
    FluxNodeData *nd = flux_node_store_get(ts->store, ts->node);
    if (nd && nd->component_data) {
        FluxButtonData *bd = (FluxButtonData *)nd->component_data;
        bd->is_checked = ts->toggled;
    }
    xent_set_semantic_checked(ts->ctx, ts->node, ts->toggled ? 1 : 0);
}

/* ── Placeholder callbacks ───────────────────────────────────────────── */

static void on_check_change(void *ctx, FluxCheckState s)   { (void)ctx; (void)s; }
static void on_switch_change(void *ctx, FluxCheckState s)  { (void)ctx; (void)s; }
static void on_radio_change(void *ctx, FluxCheckState s)   { (void)ctx; (void)s; }
static void on_slider_change(void *ctx, float v)           { (void)ctx; (void)v; }
static void on_textbox_change(void *ctx, const char *t)    { (void)ctx; (void)t; }
static void on_password_change(void *ctx, const char *t)   { (void)ctx; (void)t; }
static void on_number_change(void *ctx, double v)          { (void)ctx; (void)v; }
static void on_hyperlink_click(void *ctx)                  { (void)ctx; }

/* ── Layout helpers ──────────────────────────────────────────────────── */

static XentNodeId make_row(XentContext *ctx, XentNodeId parent, float gap, float h) {
    XentNodeId row = xent_create_node(ctx);
    xent_set_protocol(ctx, row, XENT_PROTOCOL_FLEX);
    xent_set_flex_direction(ctx, row, XENT_FLEX_ROW);
    xent_set_flex_align_items(ctx, row, XENT_FLEX_ALIGN_CENTER);
    xent_set_gap(ctx, row, gap);
    xent_set_size(ctx, row, 480, h);
    xent_append_child(ctx, parent, row);
    return row;
}

static XentNodeId make_section(XentContext *ctx, FluxNodeStore *store,
                               XentNodeId parent, const char *text) {
    XentNodeId t = flux_create_text(ctx, store, parent, text, 16.0f);
    xent_set_size(ctx, t, 480, 24);
    flux_text_set_weight(store, t, FLUX_FONT_SEMI_BOLD);
    return t;
}

/* ── Entry point ─────────────────────────────────────────────────────── */

int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR cmdLine, int showCmd) {
    (void)hInst; (void)hPrev; (void)cmdLine; (void)showCmd;

    XentConfig xcfg = {0};
    xcfg.initial_capacity = 512;
    XentContext *ctx = xent_create_context(&xcfg);
    if (!ctx) return 1;

    FluxNodeStore *store = flux_node_store_create(512);
    if (!store) { xent_destroy_context(ctx); return 1; }

    /* ── Heap-allocated state objects ─────────────────────────────────── */

    CounterState       *counter   = (CounterState *)calloc(1, sizeof(CounterState));
    RepeatCounterState *repeat_st = (RepeatCounterState *)calloc(1, sizeof(RepeatCounterState));
    ToggleState        *toggle_st = (ToggleState *)calloc(1, sizeof(ToggleState));
    RadioGroup         *rg        = (RadioGroup *)calloc(1, sizeof(RadioGroup));
    RadioGroupMember   *members   = (RadioGroupMember *)calloc(RADIO_GROUP_MAX, sizeof(RadioGroupMember));

    if (!counter || !repeat_st || !toggle_st || !rg || !members) return 1;

    counter->store   = store;
    snprintf(counter->buf, sizeof(counter->buf), "Clicked: 0");

    repeat_st->store = store;
    snprintf(repeat_st->buf, sizeof(repeat_st->buf), "Repeated: 0");

    toggle_st->store = store;
    toggle_st->ctx   = ctx;

    rg->store = store;

    /* ═════════════════════════════════════════════════════════════════════
       SCROLL ROOT — wraps everything so the window is scrollable
       ═════════════════════════════════════════════════════════════════════ */

    XentNodeId scroll_root = xent_create_node(ctx);
    xent_set_control_type(ctx, scroll_root, XENT_CONTROL_SCROLL);
    xent_set_protocol(ctx, scroll_root, XENT_PROTOCOL_FLEX);
    xent_set_flex_direction(ctx, scroll_root, XENT_FLEX_COLUMN);

    FluxNodeData *scroll_nd = flux_node_store_get_or_create(store, scroll_root);
    xent_set_userdata(ctx, scroll_root, scroll_nd);
    if (scroll_nd) {
        FluxScrollData *sd = (FluxScrollData *)calloc(1, sizeof(FluxScrollData));
        if (sd) {
            sd->v_vis = FLUX_SCROLL_AUTO;
            sd->h_vis = FLUX_SCROLL_AUTO;
        }
        scroll_nd->component_data = sd;
    }

    /* ── Inner content column ────────────────────────────────────────── */

    XentNodeId root = xent_create_node(ctx);
    xent_set_protocol(ctx, root, XENT_PROTOCOL_FLEX);
    xent_set_flex_direction(ctx, root, XENT_FLEX_COLUMN);
    xent_set_flex_align_items(ctx, root, XENT_FLEX_ALIGN_CENTER);
    xent_set_padding(ctx, root, 32, 32, 32, 32);
    xent_set_gap(ctx, root, 14);
    /* Height must be explicit for scroll to work: flex treats NAN as
       "fill parent", which squishes everything into the viewport.
       Sections 1-11 ~1792 + text inputs ~138 + flyout section ~99 + margin ≈ 2200 */
    xent_set_size(ctx, root, NAN, 2200);
    xent_append_child(ctx, scroll_root, root);

    /* ═════════════════════════════════════════════════════════════════════
       TITLE
       ═════════════════════════════════════════════════════════════════════ */

    XentNodeId title = flux_create_text(ctx, store, root, "Hello, Fluxent!", 28.0f);
    xent_set_size(ctx, title, 480, 40);
    flux_text_set_weight(store, title, FLUX_FONT_BOLD);
    flux_text_set_alignment(store, title, FLUX_TEXT_CENTER);

    XentNodeId subtitle = flux_create_text(ctx, store, root,
        "A pure-C Fluent Design UI — All Controls Showcase", 13.0f);
    xent_set_size(ctx, subtitle, 480, 20);
    flux_text_set_alignment(store, subtitle, FLUX_TEXT_CENTER);

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 1 — Buttons
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "Button");

    /* Standard / Accent / Subtle + Disabled variants */
    {
        XentNodeId row = make_row(ctx, root, 8, 32);

        XentNodeId b1 = flux_create_button(ctx, store, row, "Standard", NULL, NULL);
        xent_set_size(ctx, b1, 110, 32);
        flux_node_set_tooltip(store, b1, "This is a standard button");

        XentNodeId b2 = flux_create_button(ctx, store, row, "Accent", NULL, NULL);
        xent_set_size(ctx, b2, 110, 32);
        flux_button_set_style(store, b2, FLUX_BUTTON_ACCENT);
        flux_node_set_tooltip(store, b2, "This is an accent-styled button for primary actions");

        XentNodeId b3 = flux_create_button(ctx, store, row, "Subtle", NULL, NULL);
        xent_set_size(ctx, b3, 110, 32);
        flux_button_set_style(store, b3, FLUX_BUTTON_SUBTLE);
        flux_node_set_tooltip(store, b3, "Subtle buttons blend into the background");
    }

    {
        XentNodeId row = make_row(ctx, root, 8, 32);

        XentNodeId d1 = flux_create_button(ctx, store, row, "Disabled", NULL, NULL);
        xent_set_size(ctx, d1, 110, 32);
        flux_button_set_enabled(store, d1, false);

        XentNodeId d2 = flux_create_button(ctx, store, row, "Disabled", NULL, NULL);
        xent_set_size(ctx, d2, 110, 32);
        flux_button_set_style(store, d2, FLUX_BUTTON_ACCENT);
        flux_button_set_enabled(store, d2, false);

        XentNodeId d3 = flux_create_button(ctx, store, row, "Disabled", NULL, NULL);
        xent_set_size(ctx, d3, 110, 32);
        flux_button_set_style(store, d3, FLUX_BUTTON_SUBTLE);
        flux_button_set_enabled(store, d3, false);
    }

    /* Icon buttons */
    {
        XentNodeId row = make_row(ctx, root, 8, 32);

        XentNodeId ib1 = flux_create_button(ctx, store, row, "Settings", NULL, NULL);
        xent_set_size(ctx, ib1, 120, 32);
        flux_button_set_icon(store, ib1, "Settings");

        XentNodeId ib2 = flux_create_button(ctx, store, row, "Search", NULL, NULL);
        xent_set_size(ctx, ib2, 110, 32);
        flux_button_set_icon(store, ib2, "Search");
        flux_button_set_style(store, ib2, FLUX_BUTTON_ACCENT);
    }

    /* Counter button */
    {
        XentNodeId row = make_row(ctx, root, 12, 32);

        XentNodeId cbtn = flux_create_button(ctx, store, row, "Count",
                                             on_counter_click, counter);
        xent_set_size(ctx, cbtn, 100, 32);
        flux_button_set_style(store, cbtn, FLUX_BUTTON_ACCENT);
        flux_button_set_icon(store, cbtn, "Add");
        flux_node_set_tooltip(store, cbtn, "Click to increment the counter");

        XentNodeId clbl = flux_create_text(ctx, store, row, counter->buf, 14.0f);
        xent_set_size(ctx, clbl, 140, 32);
        counter->label_node = clbl;
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 2 — ToggleButton
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "ToggleButton");
    {
        XentNodeId row = make_row(ctx, root, 12, 32);

        XentNodeId tb1 = flux_create_button(ctx, store, row, "Toggle Me",
                                            on_toggle_click, toggle_st);
        xent_set_size(ctx, tb1, 130, 32);
        xent_set_control_type(ctx, tb1, XENT_CONTROL_TOGGLE_BUTTON);
        toggle_st->node = tb1;

        XentNodeId tb2 = flux_create_button(ctx, store, row, "Toggled On", NULL, NULL);
        xent_set_size(ctx, tb2, 130, 32);
        xent_set_control_type(ctx, tb2, XENT_CONTROL_TOGGLE_BUTTON);
        {
            FluxNodeData *nd = flux_node_store_get(store, tb2);
            if (nd && nd->component_data) {
                FluxButtonData *bd = (FluxButtonData *)nd->component_data;
                bd->is_checked = true;
            }
        }
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 3 — RepeatButton
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "RepeatButton (hold to repeat)");
    {
        XentNodeId row = make_row(ctx, root, 12, 32);

        XentNodeId rb = flux_create_repeat_button(ctx, store, row, "Hold Me",
                                                  on_repeat_click, repeat_st);
        xent_set_size(ctx, rb, 120, 32);

        XentNodeId rlbl = flux_create_text(ctx, store, row, repeat_st->buf, 14.0f);
        xent_set_size(ctx, rlbl, 160, 32);
        repeat_st->label_node = rlbl;
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 4 — HyperlinkButton
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "HyperlinkButton");
    {
        XentNodeId row = make_row(ctx, root, 16, 32);

        XentNodeId h1 = flux_create_hyperlink(ctx, store, row,
            "Visit GitHub", "https://github.com",
            on_hyperlink_click, NULL);
        xent_set_size(ctx, h1, 130, 32);

        XentNodeId h2 = flux_create_hyperlink(ctx, store, row,
            "xmake.io", "https://xmake.io",
            on_hyperlink_click, NULL);
        xent_set_size(ctx, h2, 100, 32);

        XentNodeId h3 = flux_create_hyperlink(ctx, store, row,
            "Disabled Link", NULL, NULL, NULL);
        xent_set_size(ctx, h3, 120, 32);
        flux_hyperlink_set_enabled(store, h3, false);
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 5 — Checkbox & Switch
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "Checkbox & Switch");
    {
        XentNodeId row = make_row(ctx, root, 20, 32);

        XentNodeId chk1 = flux_create_checkbox(ctx, store, row,
            "Enable feature", false, on_check_change, NULL);
        xent_set_size(ctx, chk1, 160, 32);

        XentNodeId chk2 = flux_create_checkbox(ctx, store, row,
            "Checked", true, on_check_change, NULL);
        xent_set_size(ctx, chk2, 110, 32);

        XentNodeId sw1 = flux_create_switch(ctx, store, row,
            "Wi-Fi", true, on_switch_change, NULL);
        xent_set_size(ctx, sw1, 110, 32);
    }
    /* Disabled variants */
    {
        XentNodeId row = make_row(ctx, root, 20, 32);

        XentNodeId chk_d = flux_create_checkbox(ctx, store, row,
            "Disabled", false, on_check_change, NULL);
        xent_set_size(ctx, chk_d, 120, 32);
        flux_checkbox_set_enabled(store, chk_d, false);

        XentNodeId sw_d = flux_create_switch(ctx, store, row,
            "Disabled", false, on_switch_change, NULL);
        xent_set_size(ctx, sw_d, 120, 32);
        xent_set_semantic_enabled(ctx, sw_d, false);
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 6 — RadioButton
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "RadioButton (mutually exclusive)");
    {
        XentNodeId row = make_row(ctx, root, 16, 32);

        XentNodeId r1 = flux_create_radio(ctx, store, row,
            "Option A", true,  on_radio_change, NULL);
        xent_set_size(ctx, r1, 110, 32);

        XentNodeId r2 = flux_create_radio(ctx, store, row,
            "Option B", false, on_radio_change, NULL);
        xent_set_size(ctx, r2, 110, 32);

        XentNodeId r3 = flux_create_radio(ctx, store, row,
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
                nd->behavior.on_click     = radio_group_trampoline;
                nd->behavior.on_click_ctx = &members[i];
            }
        }
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 7 — Slider
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "Slider");

    XentNodeId sld = flux_create_slider(ctx, store, root,
                                        0.0f, 100.0f, 50.0f,
                                        on_slider_change, NULL);
    xent_set_size(ctx, sld, 480, 32);

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 8 — ProgressBar & ProgressRing
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "ProgressBar");

    /* Determinate progress bar at 65% */
    XentNodeId prog1 = flux_create_progress(ctx, store, root, 65.0f, 100.0f);
    xent_set_size(ctx, prog1, 480, 20);

    /* Indeterminate progress bar */
    XentNodeId prog2 = flux_create_progress(ctx, store, root, 0.0f, 100.0f);
    xent_set_size(ctx, prog2, 480, 20);
    flux_progress_set_indeterminate(store, prog2, true);

    make_section(ctx, store, root, "ProgressRing");
    {
        XentNodeId row = make_row(ctx, root, 20, 48);

        /* Determinate ring at 75% */
        XentNodeId ring1 = flux_create_progress_ring(ctx, store, row, 75.0f, 100.0f);
        xent_set_size(ctx, ring1, 32, 32);

        XentNodeId lbl1 = flux_create_text(ctx, store, row, "75%", 12.0f);
        xent_set_size(ctx, lbl1, 35, 20);

        /* Indeterminate ring (spinning) */
        XentNodeId ring2 = flux_create_progress_ring(ctx, store, row, 0.0f, 0.0f);
        xent_set_size(ctx, ring2, 32, 32);

        XentNodeId lbl2 = flux_create_text(ctx, store, row, "Loading...", 12.0f);
        xent_set_size(ctx, lbl2, 72, 20);

        /* Larger ring at 40% */
        XentNodeId ring3 = flux_create_progress_ring(ctx, store, row, 40.0f, 100.0f);
        xent_set_size(ctx, ring3, 48, 48);

        XentNodeId lbl3 = flux_create_text(ctx, store, row, "40%", 12.0f);
        xent_set_size(ctx, lbl3, 35, 20);
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 9 — InfoBadge
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "InfoBadge");
    {
        XentNodeId row = make_row(ctx, root, 14, 24);

        /* Dot badge */
        XentNodeId bd = flux_create_info_badge(ctx, store, row, FLUX_BADGE_DOT, 0);
        xent_set_size(ctx, bd, 8, 8);
        XentNodeId dl = flux_create_text(ctx, store, row, "Dot", 12.0f);
        xent_set_size(ctx, dl, 28, 20);

        /* Number badge: 3 */
        XentNodeId b3 = flux_create_info_badge(ctx, store, row, FLUX_BADGE_NUMBER, 3);
        xent_set_size(ctx, b3, 16, 16);
        XentNodeId l3 = flux_create_text(ctx, store, row, "= 3", 12.0f);
        xent_set_size(ctx, l3, 28, 20);

        /* Number badge: 42 */
        XentNodeId b42 = flux_create_info_badge(ctx, store, row, FLUX_BADGE_NUMBER, 42);
        xent_set_size(ctx, b42, 24, 16);
        XentNodeId l42 = flux_create_text(ctx, store, row, "= 42", 12.0f);
        xent_set_size(ctx, l42, 36, 20);

        /* Number badge: 999 */
        XentNodeId b999 = flux_create_info_badge(ctx, store, row, FLUX_BADGE_NUMBER, 999);
        xent_set_size(ctx, b999, 28, 16);

        /* Icon badge */
        XentNodeId bi = flux_create_info_badge(ctx, store, row, FLUX_BADGE_ICON, 0);
        xent_set_size(ctx, bi, 16, 16);
        flux_info_badge_set_icon(store, bi, "Info");
        XentNodeId li = flux_create_text(ctx, store, row, "Icon", 12.0f);
        xent_set_size(ctx, li, 36, 20);
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 10 — Card
       ═════════════════════════════════════════════════════════════════════ */

    make_section(ctx, store, root, "Card");
    {
        XentNodeId card = flux_create_card(ctx, store, root);
        /* Explicit height: padding(16+16) + 24 + gap(8) + 44 + gap(8) + 32 = 148 */
        xent_set_size(ctx, card, 480, 148);
        xent_set_protocol(ctx, card, XENT_PROTOCOL_FLEX);
        xent_set_flex_direction(ctx, card, XENT_FLEX_COLUMN);
        xent_set_flex_align_items(ctx, card, XENT_FLEX_ALIGN_START);
        xent_set_padding(ctx, card, 16, 16, 16, 16);
        xent_set_gap(ctx, card, 8);

        XentNodeId ct = flux_create_text(ctx, store, card, "This is a Card", 16.0f);
        xent_set_size(ctx, ct, 440, 24);
        flux_text_set_weight(store, ct, FLUX_FONT_SEMI_BOLD);

        XentNodeId cb = flux_create_text(ctx, store, card,
            "Cards group related content with a subtle background and "
            "border, commonly used in settings pages and dashboards.", 13.0f);
        xent_set_size(ctx, cb, 440, 44);

        XentNodeId cr = make_row(ctx, card, 8, 32);
        xent_set_size(ctx, cr, 440, 32);

        XentNodeId ca = flux_create_button(ctx, store, cr, "Action", NULL, NULL);
        xent_set_size(ctx, ca, 100, 32);
        flux_button_set_style(store, ca, FLUX_BUTTON_ACCENT);

        XentNodeId cc = flux_create_button(ctx, store, cr, "Cancel", NULL, NULL);
        xent_set_size(ctx, cc, 100, 32);
    }

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 11 — Text Input Controls
       (TextBox, PasswordBox, NumberBox — created after app init below)
       Section title is placed here; actual controls appended after
       flux_app_create() so they have access to the text renderer.
       ═════════════════════════════════════════════════════════════════════ */

    XentNodeId textinput_title = make_section(ctx, store, root,
        "TextBox / PasswordBox / NumberBox");
    (void)textinput_title;

    /* ═════════════════════════════════════════════════════════════════════
       FOOTER  (will be detached & re-appended after text inputs)
       ═════════════════════════════════════════════════════════════════════ */

    XentNodeId footer_div = flux_create_divider(ctx, store, root);
    xent_set_size(ctx, footer_div, 480, 1);

    XentNodeId footer = flux_create_text(ctx, store, root,
        "xent-core + fluxent — Mica backdrop — All controls demo", 11.0f);
    xent_set_size(ctx, footer, 480, 20);
    flux_text_set_alignment(store, footer, FLUX_TEXT_CENTER);

    XentNodeId spacer = xent_create_node(ctx);
    xent_set_size(ctx, spacer, 1, 32);
    xent_append_child(ctx, root, spacer);

    /* ═════════════════════════════════════════════════════════════════════
       APP SETUP
       ═════════════════════════════════════════════════════════════════════ */

    FluxAppConfig cfg = {0};
    cfg.title     = L"Hello Fluxent \u2014 All Controls";
    cfg.width     = 640;
    cfg.height    = 760;
    cfg.dark_mode = flux_theme_system_is_dark();
    cfg.backdrop  = FLUX_BACKDROP_MICA;

    FluxApp *app = NULL;
    HRESULT hr = flux_app_create(&cfg, &app);
    if (FAILED(hr)) {
        free(counter); free(repeat_st); free(toggle_st);
        free(rg); free(members);
        flux_node_store_destroy(store);
        xent_destroy_context(ctx);
        return 1;
    }

    flux_app_set_root(app, ctx, scroll_root, store);

    /* Set scroll content size (generous estimate) */
    if (scroll_nd && scroll_nd->component_data) {
        FluxScrollData *sd = (FluxScrollData *)scroll_nd->component_data;
        sd->content_h = 2200.0f;
        sd->content_w = 640.0f;
    }

    /* ═════════════════════════════════════════════════════════════════════
       TEXT INPUT CONTROLS (need FluxApp for text renderer / IME)
       Detach footer items → append text inputs → re-attach footer so
       the text inputs appear in the correct section.
       ═════════════════════════════════════════════════════════════════════ */

    xent_remove_child(ctx, root, footer_div);
    xent_remove_child(ctx, root, footer);
    xent_remove_child(ctx, root, spacer);

    /* TextBox */
    XentNodeId textbox = flux_app_create_textbox(app, root,
        "Type something here...", on_textbox_change, NULL);
    xent_set_size(ctx, textbox, 480, 32);

    /* PasswordBox */
    XentNodeId passbox = flux_app_create_password_box(app, root,
        "Enter password...", on_password_change, NULL);
    xent_set_size(ctx, passbox, 480, 32);

    /* NumberBox */
    XentNodeId numbox = flux_app_create_number_box(app, root,
        -100.0, 100.0, 1.0, on_number_change, NULL);
    xent_set_size(ctx, numbox, 480, 32);

    /* ═════════════════════════════════════════════════════════════════════
       SECTION 12 — Flyout & MenuFlyout
       ═════════════════════════════════════════════════════════════════════ */

    {
        XentNodeId d = flux_create_divider(ctx, store, root);
        xent_set_size(ctx, d, 480, 1);
    }

    make_section(ctx, store, root, "Flyout & MenuFlyout");

    /* --- Shared resources for flyout / menu --- */
    FluxWindow       *win   = flux_app_get_window(app);
    FluxThemeManager *tmgr  = flux_app_get_theme(app);
    FluxTextRenderer *tr    = flux_app_get_text_renderer(app);
    const FluxThemeColors *tc = tmgr ? flux_theme_colors(tmgr) : NULL;
    bool dark = tmgr ? (flux_theme_get_mode(tmgr) == FLUX_THEME_DARK ||
                        (flux_theme_get_mode(tmgr) == FLUX_THEME_SYSTEM &&
                         flux_theme_system_is_dark())) : false;

    /* --- Flyout demo: click a button to pop up a card --- */
    FluxFlyout *demo_flyout = flux_flyout_create(win);
    flux_flyout_set_theme(demo_flyout, tc, dark);
    flux_flyout_set_text_renderer(demo_flyout, tr);
    flux_flyout_set_content_size(demo_flyout, 200, 40);

    /* --- MenuFlyout demo: right-click a button for context menu --- */
    FluxMenuFlyout *demo_menu = flux_menu_flyout_create(win);
    flux_menu_flyout_set_theme(demo_menu, tc, dark);
    flux_menu_flyout_set_text_renderer(demo_menu, tr);

    {
        XentNodeId row = make_row(ctx, root, 12, 32);

        XentNodeId btn_flyout = flux_create_button(ctx, store, row,
            "Click me (Flyout)", NULL, NULL);
        xent_set_size(ctx, btn_flyout, 200, 32);
        flux_button_set_style(store, btn_flyout, FLUX_BUTTON_ACCENT);
        flux_node_set_flyout_ex(store, btn_flyout, demo_flyout,
                                FLUX_PLACEMENT_BOTTOM, ctx, win);
        flux_node_set_tooltip(store, btn_flyout,
            "Click to show a Flyout popup below this button");

        {
            FluxMenuFlyout *theme_submenu = flux_menu_flyout_create(win);
            flux_menu_flyout_set_theme(theme_submenu, tc, dark);
            flux_menu_flyout_set_text_renderer(theme_submenu, tr);

            {
                FluxMenuItemDef item = {0};
                item.type        = FLUX_MENU_ITEM_RADIO;
                item.label       = "System";
                item.radio_group = "theme";
                item.checked     = true;
                item.enabled     = true;
                flux_menu_flyout_add_item(theme_submenu, &item);
            }
            {
                FluxMenuItemDef item = {0};
                item.type        = FLUX_MENU_ITEM_RADIO;
                item.label       = "Light";
                item.radio_group = "theme";
                item.enabled     = true;
                flux_menu_flyout_add_item(theme_submenu, &item);
            }
            {
                FluxMenuItemDef item = {0};
                item.type        = FLUX_MENU_ITEM_RADIO;
                item.label       = "Dark";
                item.radio_group = "theme";
                item.enabled     = true;
                flux_menu_flyout_add_item(theme_submenu, &item);
            }

            FluxMenuFlyout *view_submenu = flux_menu_flyout_create(win);
            flux_menu_flyout_set_theme(view_submenu, tc, dark);
            flux_menu_flyout_set_text_renderer(view_submenu, tr);

            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_TOGGLE;
                item.label   = "Word Wrap";
                item.checked = true;
                item.enabled = true;
                flux_menu_flyout_add_item(view_submenu, &item);
            }
            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_TOGGLE;
                item.label   = "Line Numbers";
                item.checked = false;
                item.enabled = true;
                flux_menu_flyout_add_item(view_submenu, &item);
            }
            flux_menu_flyout_add_separator(view_submenu);
            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_SUBMENU;
                item.label   = "Theme";
                item.enabled = true;
                item.submenu = theme_submenu;
                flux_menu_flyout_add_item(view_submenu, &item);
            }

            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_NORMAL;
                item.label   = "Cut";
                item.accelerator_text = "Ctrl+X";
                item.enabled = true;
                flux_menu_flyout_add_item(demo_menu, &item);
            }
            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_NORMAL;
                item.label   = "Copy";
                item.accelerator_text = "Ctrl+C";
                item.enabled = true;
                flux_menu_flyout_add_item(demo_menu, &item);
            }
            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_NORMAL;
                item.label   = "Paste";
                item.accelerator_text = "Ctrl+V";
                item.enabled = true;
                flux_menu_flyout_add_item(demo_menu, &item);
            }
            flux_menu_flyout_add_separator(demo_menu);
            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_SUBMENU;
                item.label   = "View";
                item.enabled = true;
                item.submenu = view_submenu;
                flux_menu_flyout_add_item(demo_menu, &item);
            }
            flux_menu_flyout_add_separator(demo_menu);
            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_NORMAL;
                item.label   = "Select All";
                item.accelerator_text = "Ctrl+A";
                item.enabled = true;
                flux_menu_flyout_add_item(demo_menu, &item);
            }
            {
                FluxMenuItemDef item = {0};
                item.type    = FLUX_MENU_ITEM_NORMAL;
                item.label   = "Delete";
                item.enabled = false;  /* greyed out */
                flux_menu_flyout_add_item(demo_menu, &item);
            }
        }

        XentNodeId btn_ctx = flux_create_button(ctx, store, row,
            "Right-click (Menu)", NULL, NULL);
        xent_set_size(ctx, btn_ctx, 200, 32);
        flux_node_set_context_flyout_ex(store, btn_ctx, demo_menu, ctx, win);
        flux_node_set_tooltip(store, btn_ctx,
            "Right-click to show a MenuFlyout context menu");
    }

    /* Re-attach footer items after the text inputs */
    xent_append_child(ctx, root, footer_div);
    xent_append_child(ctx, root, footer);
    xent_append_child(ctx, root, spacer);

    /* ── Run ─────────────────────────────────────────────────────────── */

    int result = flux_app_run(app);

    /* ── Cleanup ─────────────────────────────────────────────────────── */

    flux_menu_flyout_destroy(demo_menu);
    flux_flyout_destroy(demo_flyout);
    flux_app_destroy(app);
    flux_node_store_destroy(store);
    xent_destroy_context(ctx);
    free(counter);
    free(repeat_st);
    free(toggle_st);
    free(rg);
    free(members);
    return result;
}
