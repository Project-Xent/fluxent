#ifndef FLUX_COMPONENT_DATA_H
#define FLUX_COMPONENT_DATA_H

#include "flux_types.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum FluxButtonStyle {
    FLUX_BUTTON_STANDARD,
    FLUX_BUTTON_SUBTLE,
    FLUX_BUTTON_TEXT,
    FLUX_BUTTON_ACCENT,
} FluxButtonStyle;

typedef enum FluxCheckState {
    FLUX_CHECK_UNCHECKED,
    FLUX_CHECK_CHECKED,
    FLUX_CHECK_INDETERMINATE,
} FluxCheckState;

typedef enum FluxTextAlign {
    FLUX_TEXT_LEFT,
    FLUX_TEXT_CENTER,
    FLUX_TEXT_RIGHT,
    FLUX_TEXT_JUSTIFY,
} FluxTextAlign;

typedef enum FluxTextVAlign {
    FLUX_TEXT_TOP,
    FLUX_TEXT_VCENTER,
    FLUX_TEXT_BOTTOM,
} FluxTextVAlign;

typedef enum FluxFontWeight {
    FLUX_FONT_THIN       = 100,
    FLUX_FONT_EXTRA_LIGHT = 200,
    FLUX_FONT_LIGHT      = 300,
    FLUX_FONT_REGULAR    = 400,
    FLUX_FONT_MEDIUM     = 500,
    FLUX_FONT_SEMI_BOLD  = 600,
    FLUX_FONT_BOLD       = 700,
    FLUX_FONT_EXTRA_BOLD = 800,
    FLUX_FONT_BLACK      = 900,
} FluxFontWeight;

typedef enum FluxScrollBarVis {
    FLUX_SCROLL_AUTO,
    FLUX_SCROLL_ALWAYS,
    FLUX_SCROLL_NEVER,
} FluxScrollBarVis;

typedef struct FluxTextData {
    const char    *content;
    const char    *font_family;
    FluxColor      text_color;
    float          font_size;
    FluxFontWeight font_weight;
    FluxTextAlign  alignment;
    FluxTextVAlign vertical_alignment;
    uint8_t        max_lines;
    bool           wrap;
    bool           selectable;
} FluxTextData;

typedef struct FluxButtonData {
    const char      *label;
    const char      *icon_name;
    FluxColor        label_color;
    FluxColor        hover_color;
    FluxColor        pressed_color;
    float            font_size;
    FluxButtonStyle  style;
    bool             enabled;
    bool             is_checked;
    void           (*on_click)(void *ctx);
    void            *on_click_ctx;
} FluxButtonData;

typedef struct FluxSliderData {
    float      min_value;
    float      max_value;
    float      current_value;
    float      step;
    FluxColor  track_color;
    FluxColor  fill_color;
    FluxColor  thumb_color;
    bool       enabled;
    void     (*on_change)(void *ctx, float value);
    void      *on_change_ctx;
} FluxSliderData;

typedef struct FluxCheckboxData {
    const char    *label;
    FluxColor      check_color;
    FluxColor      box_color;
    FluxCheckState state;
    bool           enabled;
    void         (*on_change)(void *ctx, FluxCheckState new_state);
    void          *on_change_ctx;
} FluxCheckboxData;

typedef struct FluxScrollData {
    float           scroll_x;
    float           scroll_y;
    float           content_w;
    float           content_h;
    FluxScrollBarVis h_vis;
    FluxScrollBarVis v_vis;
} FluxScrollData;

typedef struct FluxTextBoxData {
    const char *content;
    const char *placeholder;
    const char *font_family;
    float       font_size;
    FluxColor   text_color;
    FluxColor   placeholder_color;
    FluxColor   selection_color;
    uint32_t    cursor_position;
    uint32_t    selection_start;
    uint32_t    selection_end;
    float       scroll_offset_x;
    uint32_t    max_length;
    bool        enabled;
    bool        readonly;
    bool        multiline;

    /* IME composition state */
    const wchar_t *composition_text;
    uint32_t       composition_length;
    uint32_t       composition_cursor;

    void      (*on_change)(void *ctx, const char *text);
    void       *on_change_ctx;
    void      (*on_submit)(void *ctx);
    void       *on_submit_ctx;
} FluxTextBoxData;

typedef struct FluxProgressData {
    float     value;
    float     max_value;
    FluxColor fill_color;
    FluxColor track_color;
    bool      indeterminate;
} FluxProgressData;

typedef struct FluxImageData {
    const char *source;
    float       natural_w;
    float       natural_h;
} FluxImageData;

typedef enum FluxInfoBadgeMode {
    FLUX_BADGE_DOT,
    FLUX_BADGE_NUMBER,
    FLUX_BADGE_ICON,
} FluxInfoBadgeMode;

typedef enum FluxNBSpinPlacement {
    FLUX_NB_SPIN_HIDDEN  = 0,
    FLUX_NB_SPIN_COMPACT = 1,
    FLUX_NB_SPIN_INLINE  = 2,
} FluxNBSpinPlacement;

typedef enum FluxNBValidation {
    FLUX_NB_VALIDATE_OVERWRITE = 0,
    FLUX_NB_VALIDATE_DISABLED  = 1,
} FluxNBValidation;

typedef struct FluxPasswordBoxData {
    const char *content;
    const char *placeholder;
    const char *font_family;
    float       font_size;
    FluxColor   text_color;
    FluxColor   placeholder_color;
    FluxColor   selection_color;
    uint32_t    cursor_position;
    uint32_t    selection_start;
    uint32_t    selection_end;
    float       scroll_offset_x;
    uint32_t    max_length;
    bool        enabled;
    bool        readonly;
    wchar_t     mask_char;        /* default '●' (U+25CF) */
    bool        show_plain;       /* true = reveal password */

    /* IME composition state */
    const wchar_t *composition_text;
    uint32_t       composition_length;
    uint32_t       composition_cursor;

    void      (*on_change)(void *ctx, const char *text);
    void       *on_change_ctx;
    void      (*on_submit)(void *ctx);
    void       *on_submit_ctx;
} FluxPasswordBoxData;

typedef struct FluxNumberBoxData {
    const char *content;           /* text representation */
    const char *placeholder;
    const char *font_family;
    float       font_size;
    FluxColor   text_color;
    FluxColor   placeholder_color;
    FluxColor   selection_color;
    uint32_t    cursor_position;
    uint32_t    selection_start;
    uint32_t    selection_end;
    float       scroll_offset_x;
    bool        enabled;
    bool        readonly;
    double      min_value;
    double      max_value;
    double      step;
    double      current_value;
    bool        spin_enabled;

    /* IME composition state */
    const wchar_t *composition_text;
    uint32_t       composition_length;
    uint32_t       composition_cursor;

    void      (*on_change)(void *ctx, const char *text);
    void       *on_change_ctx;
    void      (*on_value_change)(void *ctx, double value);
    void       *on_value_change_ctx;
    void      (*on_submit)(void *ctx);
    void       *on_submit_ctx;
} FluxNumberBoxData;

typedef struct FluxHyperlinkData {
    const char      *label;
    const char      *url;
    const char      *icon_name;
    FluxColor        label_color;
    float            font_size;
    bool             enabled;
    bool             visited;
    void           (*on_click)(void *ctx);
    void            *on_click_ctx;
} FluxHyperlinkData;

typedef struct FluxRepeatButtonData {
    const char      *label;
    const char      *icon_name;
    FluxColor        label_color;
    float            font_size;
    FluxButtonStyle  style;
    bool             enabled;
    uint32_t         repeat_delay_ms;     /* default 400 */
    uint32_t         repeat_interval_ms;  /* default 50  */
    void           (*on_click)(void *ctx);
    void            *on_click_ctx;
} FluxRepeatButtonData;

typedef struct FluxProgressRingData {
    float     value;
    float     max_value;
    FluxColor fill_color;
    FluxColor track_color;
    bool      indeterminate;
} FluxProgressRingData;

typedef struct FluxInfoBadgeData {
    FluxInfoBadgeMode mode;
    int32_t           value;       /* number to display in NUMBER mode */
    const char       *icon_name;   /* icon for ICON mode */
    FluxColor         background;
} FluxInfoBadgeData;

#ifdef __cplusplus
}
#endif

#endif