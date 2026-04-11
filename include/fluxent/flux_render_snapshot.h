#ifndef FLUX_RENDER_SNAPSHOT_H
#define FLUX_RENDER_SNAPSHOT_H

#include "flux_component_data.h"
#include "flux_node_store.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxRenderSnapshot {
    uint64_t           id;
    XentControlType    type;

    FluxColor  background;
    FluxColor  border_color;
    float      corner_radius;
    float      border_width;
    float      opacity;

    const char    *text_content;
    const char    *placeholder;
    const char    *font_family;
    float          font_size;
    FluxColor      text_color;
    FluxTextAlign  text_alignment;
    FluxTextVAlign text_vert_alignment;
    FluxFontWeight font_weight;
    uint8_t        max_lines;
    bool           word_wrap;

    const char     *label;
    const char     *icon_name;
    FluxButtonStyle button_style;
    bool            is_checked;

    FluxCheckState check_state;

    float min_value;
    float max_value;
    float current_value;
    float step;

    float scroll_x, scroll_y;
    float scroll_content_w, scroll_content_h;
    FluxScrollBarVis scroll_h_vis;
    FluxScrollBarVis scroll_v_vis;

    uint32_t cursor_position;
    uint32_t selection_start;
    uint32_t selection_end;
    float    scroll_offset_x;

    const wchar_t *composition_text;
    uint32_t       composition_length;
    uint32_t       composition_cursor;

    FluxColor  selection_color;
    bool       readonly;
    bool       indeterminate;

    bool enabled;
} FluxRenderSnapshot;

void flux_snapshot_build(FluxRenderSnapshot *snap,
                         const XentContext *ctx,
                         XentNodeId node,
                         const FluxNodeData *nd);

#ifdef __cplusplus
}
#endif

#endif