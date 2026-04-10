#include "fluxent/flux_engine.h"

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

static const FluxControlRenderer kRenderers[XENT_CONTROL_CUSTOM + 1] = {
    [XENT_CONTROL_CONTAINER]     = { flux_draw_container,  NULL },
    [XENT_CONTROL_TEXT]          = { flux_draw_text,        NULL },
    [XENT_CONTROL_BUTTON]        = { flux_draw_button,      NULL },
    [XENT_CONTROL_TOGGLE_BUTTON] = { flux_draw_toggle,      NULL },
    [XENT_CONTROL_CHECKBOX]      = { flux_draw_checkbox,    NULL },
    [XENT_CONTROL_RADIO]         = { flux_draw_radio,       NULL },
    [XENT_CONTROL_SWITCH]        = { flux_draw_switch,      NULL },
    [XENT_CONTROL_SLIDER]        = { flux_draw_slider,      NULL },
    [XENT_CONTROL_TEXT_INPUT]    = { flux_draw_textbox,     NULL },
    [XENT_CONTROL_SCROLL]        = { flux_draw_scroll,      flux_draw_scroll_overlay },
    [XENT_CONTROL_IMAGE]         = { flux_draw_container,   NULL },
    [XENT_CONTROL_PROGRESS]      = { flux_draw_progress,    NULL },
    [XENT_CONTROL_LIST]          = { flux_draw_container,   NULL },
    [XENT_CONTROL_TAB]           = { flux_draw_container,   NULL },
    [XENT_CONTROL_CARD]          = { flux_draw_card,        NULL },
    [XENT_CONTROL_DIVIDER]       = { flux_draw_divider,     NULL },
    [XENT_CONTROL_CANVAS]        = { flux_draw_container,   NULL },
    [XENT_CONTROL_CUSTOM]        = { flux_draw_container,   NULL },
};

void flux_dispatch_render(const FluxRenderContext *rc,
                          const FluxRenderSnapshot *snap,
                          const FluxRect *bounds,
                          const FluxControlState *state) {
    uint32_t idx = (uint32_t)snap->type;
    if (idx <= XENT_CONTROL_CUSTOM && kRenderers[idx].draw) {
        kRenderers[idx].draw(rc, snap, bounds, state);
    }
}

void flux_dispatch_render_overlay(const FluxRenderContext *rc,
                                  const FluxRenderSnapshot *snap,
                                  const FluxRect *bounds) {
    uint32_t idx = (uint32_t)snap->type;
    if (idx <= XENT_CONTROL_CUSTOM && kRenderers[idx].draw_overlay) {
        kRenderers[idx].draw_overlay(rc, snap, bounds);
    }
}