/**
 * @file fluxent.h
 * @brief Main umbrella header for the Fluxent UI framework.
 *
 * Include this header to access all Fluxent public APIs:
 * - Application lifecycle (FluxApp)
 * - Window management (FluxWindow)
 * - Controls and rendering (FluxEngine)
 * - Theming and styling (FluxTheme)
 *
 * @code
 * #include <fluxent/fluxent.h>
 *
 * int main(void) {
 *     FluxAppConfig cfg = { .title = L"My App", .width = 800, .height = 600 };
 *     FluxApp *app = flux_app_create(&cfg);
 *     flux_app_run(app);
 *     flux_app_destroy(app);
 * }
 * @endcode
 */

#ifndef FLUXENT_H
#define FLUXENT_H

#include "flux_engine.h"
#include "flux_component_data.h"
#include "flux_theme.h"
#include "flux_controls.h"
#include "flux_plugin.h"
#include "flux_popup.h"
#include "flux_tooltip.h"
#include "flux_flyout.h"
#include "flux_menu_flyout.h"

#ifndef COBJMACROS
  #define COBJMACROS
#endif

#include <windows.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FluxApp    FluxApp;
typedef struct FluxWindow FluxWindow;
typedef struct FluxInput  FluxInput;

typedef enum FluxBackdropType
{
	FLUX_BACKDROP_NONE = 0,
	FLUX_BACKDROP_MICA,
	FLUX_BACKDROP_MICA_ALT,
	FLUX_BACKDROP_ACRYLIC,
} FluxBackdropType;

typedef struct FluxAppConfig {
	wchar_t const   *title;
	int              width;
	int              height;
	bool             dark_mode;
	FluxBackdropType backdrop;
} FluxAppConfig;

HRESULT           flux_app_create(FluxAppConfig const *cfg, FluxApp **out);
void              flux_app_destroy(FluxApp *app);
void              flux_app_set_root(FluxApp *app, XentContext *ctx, XentNodeId root, FluxNodeStore *store);
int               flux_app_run(FluxApp *app);

FluxEngine       *flux_app_get_engine(FluxApp *app);
FluxWindow       *flux_app_get_window(FluxApp *app);
FluxInput        *flux_app_get_input(FluxApp *app);
FluxThemeManager *flux_app_get_theme(FluxApp *app);
FluxTooltip      *flux_app_get_tooltip(FluxApp *app);
FluxTextRenderer *flux_app_get_text_renderer(FluxApp *app);
XentContext      *flux_app_get_context(FluxApp *app);
FluxNodeStore    *flux_app_get_store(FluxApp *app);

XentNodeId        flux_create_button(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata
);

XentNodeId
flux_create_text(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *content, float font_size);

XentNodeId flux_create_slider(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, float min, float max, float value,
  void (*on_change)(void *, float), void *userdata
);

XentNodeId flux_create_checkbox(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, bool checked,
  void (*on_change)(void *, FluxCheckState), void *userdata
);

XentNodeId flux_create_radio(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, bool checked,
  void (*on_change)(void *, FluxCheckState), void *userdata
);

XentNodeId flux_create_switch(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, bool on,
  void (*on_change)(void *, FluxCheckState), void *userdata
);

XentNodeId
flux_create_progress(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, float value, float max_value);

XentNodeId flux_create_card(XentContext *ctx, FluxNodeStore *store, XentNodeId parent);

XentNodeId flux_create_divider(XentContext *ctx, FluxNodeStore *store, XentNodeId parent);

XentNodeId flux_create_textbox(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *placeholder,
  void (*on_change)(void *, char const *), void *userdata
);

XentNodeId flux_app_create_textbox(
  FluxApp *app, XentNodeId parent, char const *placeholder, void (*on_change)(void *, char const *), void *userdata
);

XentNodeId flux_app_create_password_box(
  FluxApp *app, XentNodeId parent, char const *placeholder, void (*on_change)(void *, char const *), void *userdata
);

XentNodeId flux_app_create_number_box(
  FluxApp *app, XentNodeId parent, double min_val, double max_val, double step, void (*on_value_change)(void *, double),
  void *userdata
);

/* ---- Phase 1 convenience constructors ---- */

XentNodeId flux_create_password_box(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *placeholder,
  void (*on_change)(void *, char const *), void *userdata
);

XentNodeId flux_create_number_box(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, double min_val, double max_val, double step,
  void (*on_value_change)(void *, double), void *userdata
);

XentNodeId flux_create_hyperlink(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, char const *url,
  void (*on_click)(void *), void *userdata
);

XentNodeId flux_create_repeat_button(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata
);

XentNodeId
flux_create_progress_ring(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, float value, float max_value);

XentNodeId flux_create_info_badge(
  XentContext *ctx, FluxNodeStore *store, XentNodeId parent, FluxInfoBadgeMode mode, int32_t value
);

#ifdef __cplusplus
}
#endif

#endif
