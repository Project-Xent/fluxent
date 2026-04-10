#include "fluxent/flux_render_snapshot.h"

#include <string.h>

void flux_snapshot_build(FluxRenderSnapshot *snap,
                         const XentContext *ctx,
                         XentNodeId node,
                         const FluxNodeData *nd) {
    memset(snap, 0, sizeof(*snap));
    snap->id = (uint64_t)node;
    snap->type = xent_get_control_type(ctx, node);
    snap->enabled = xent_get_semantic_enabled(ctx, node);

    if (!nd) {
        return;
    }

    snap->background    = nd->visuals.background;
    snap->border_color  = nd->visuals.border_color;
    snap->corner_radius = nd->visuals.corner_radius;
    snap->border_width  = nd->visuals.border_width;
    snap->opacity       = nd->visuals.opacity;

    if (!nd->component_data) {
        return;
    }

    switch (snap->type) {
    case XENT_CONTROL_TEXT: {
        const FluxTextData *t = (const FluxTextData *)nd->component_data;
        snap->text_content       = t->content;
        snap->font_family        = t->font_family;
        snap->text_color         = t->text_color;
        snap->font_size          = t->font_size;
        snap->font_weight        = t->font_weight;
        snap->text_alignment     = t->alignment;
        snap->text_vert_alignment = t->vertical_alignment;
        snap->max_lines          = t->max_lines;
        snap->word_wrap          = t->wrap;
        break;
    }
    case XENT_CONTROL_BUTTON:
    case XENT_CONTROL_TOGGLE_BUTTON: {
        const FluxButtonData *b = (const FluxButtonData *)nd->component_data;
        snap->label        = b->label;
        snap->icon_name    = b->icon_name;
        snap->text_color   = b->label_color;
        snap->font_size    = b->font_size;
        snap->button_style = b->style;
        snap->is_checked   = b->is_checked;
        snap->enabled      = b->enabled;
        break;
    }
    case XENT_CONTROL_CHECKBOX:
    case XENT_CONTROL_RADIO:
    case XENT_CONTROL_SWITCH: {
        const FluxCheckboxData *c = (const FluxCheckboxData *)nd->component_data;
        snap->label       = c->label;
        snap->check_state = c->state;
        snap->enabled     = c->enabled;
        break;
    }
    case XENT_CONTROL_SLIDER: {
        const FluxSliderData *s = (const FluxSliderData *)nd->component_data;
        snap->min_value     = s->min_value;
        snap->max_value     = s->max_value;
        snap->current_value = s->current_value;
        snap->enabled       = s->enabled;
        break;
    }
    case XENT_CONTROL_TEXT_INPUT: {
        const FluxTextBoxData *tb = (const FluxTextBoxData *)nd->component_data;
        snap->text_content    = tb->content;
        snap->placeholder     = tb->placeholder;
        snap->font_family     = tb->font_family;
        snap->font_size       = tb->font_size;
        snap->text_color      = tb->text_color;
        snap->cursor_position = tb->cursor_position;
        snap->selection_start = tb->selection_start;
        snap->selection_end   = tb->selection_end;
        snap->scroll_offset_x = tb->scroll_offset_x;
        snap->enabled         = tb->enabled;
        break;
    }
    case XENT_CONTROL_SCROLL: {
        const FluxScrollData *sc = (const FluxScrollData *)nd->component_data;
        snap->scroll_x         = sc->scroll_x;
        snap->scroll_y         = sc->scroll_y;
        snap->scroll_content_w = sc->content_w;
        snap->scroll_content_h = sc->content_h;
        snap->scroll_h_vis     = sc->h_vis;
        snap->scroll_v_vis     = sc->v_vis;
        break;
    }
    case XENT_CONTROL_PROGRESS: {
        const FluxProgressData *p = (const FluxProgressData *)nd->component_data;
        snap->current_value = p->value;
        snap->max_value     = p->max_value;
        break;
    }
    default:
        break;
    }
}