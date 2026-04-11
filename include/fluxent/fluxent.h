#ifndef FLUXENT_H
#define FLUXENT_H

#include "flux_engine.h"
#include "flux_component_data.h"
#include "flux_theme.h"
#include "flux_controls.h"
#include "flux_plugin.h"

#ifndef COBJMACROS
#define COBJMACROS
#endif

#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FluxApp    FluxApp;
typedef struct FluxWindow FluxWindow;
typedef struct FluxInput  FluxInput;

typedef enum FluxBackdropType {
    FLUX_BACKDROP_NONE = 0,
    FLUX_BACKDROP_MICA,
    FLUX_BACKDROP_MICA_ALT,
    FLUX_BACKDROP_ACRYLIC,
} FluxBackdropType;

typedef struct FluxAppConfig {
    const wchar_t   *title;
    int              width;
    int              height;
    bool             dark_mode;
    FluxBackdropType backdrop;
} FluxAppConfig;

HRESULT flux_app_create(const FluxAppConfig *cfg, FluxApp **out);
void    flux_app_destroy(FluxApp *app);
void    flux_app_set_root(FluxApp *app, XentContext *ctx, XentNodeId root,
                          FluxNodeStore *store);
int     flux_app_run(FluxApp *app);

FluxEngine       *flux_app_get_engine(FluxApp *app);
FluxWindow       *flux_app_get_window(FluxApp *app);
FluxInput        *flux_app_get_input(FluxApp *app);
FluxThemeManager *flux_app_get_theme(FluxApp *app);

XentNodeId flux_create_button(XentContext *ctx, FluxNodeStore *store,
                              XentNodeId parent, const char *label,
                              void (*on_click)(void *), void *userdata);

XentNodeId flux_create_text(XentContext *ctx, FluxNodeStore *store,
                            XentNodeId parent, const char *content,
                            float font_size);

XentNodeId flux_create_slider(XentContext *ctx, FluxNodeStore *store,
                              XentNodeId parent,
                              float min, float max, float value,
                              void (*on_change)(void *, float), void *userdata);

XentNodeId flux_create_checkbox(XentContext *ctx, FluxNodeStore *store,
                                XentNodeId parent, const char *label,
                                bool checked,
                                void (*on_change)(void *, FluxCheckState),
                                void *userdata);

XentNodeId flux_create_radio(XentContext *ctx, FluxNodeStore *store,
                             XentNodeId parent, const char *label,
                             bool checked,
                             void (*on_change)(void *, FluxCheckState),
                             void *userdata);

XentNodeId flux_create_switch(XentContext *ctx, FluxNodeStore *store,
                              XentNodeId parent, const char *label,
                              bool on,
                              void (*on_change)(void *, FluxCheckState),
                              void *userdata);

XentNodeId flux_create_progress(XentContext *ctx, FluxNodeStore *store,
                                XentNodeId parent,
                                float value, float max_value);

XentNodeId flux_create_card(XentContext *ctx, FluxNodeStore *store,
                            XentNodeId parent);

XentNodeId flux_create_divider(XentContext *ctx, FluxNodeStore *store,
                               XentNodeId parent);

XentNodeId flux_create_textbox(XentContext *ctx, FluxNodeStore *store,
                               XentNodeId parent, const char *placeholder,
                               void (*on_change)(void *, const char *),
                               void *userdata);

XentNodeId flux_app_create_textbox(FluxApp *app, XentNodeId parent,
                                   const char *placeholder,
                                   void (*on_change)(void *, const char *),
                                   void *userdata);

#ifdef __cplusplus
}
#endif

#endif