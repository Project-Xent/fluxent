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

typedef struct FluxButtonCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	char const    *label;
	void           (*on_click)(void *);
	void          *userdata;
} FluxButtonCreateInfo;

typedef struct FluxSliderCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	float          min;
	float          max;
	float          value;
	void           (*on_change)(void *, float);
	void          *userdata;
} FluxSliderCreateInfo;

typedef struct FluxToggleCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	char const    *label;
	bool           checked;
	void           (*on_change)(void *, FluxCheckState);
	void          *userdata;
} FluxToggleCreateInfo;

typedef struct FluxTextBoxCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	char const    *placeholder;
	void           (*on_change)(void *, char const *);
	void          *userdata;
} FluxTextBoxCreateInfo;

typedef struct FluxAppTextBoxCreateInfo {
	FluxApp    *app;
	XentNodeId  parent;
	char const *placeholder;
	void        (*on_change)(void *, char const *);
	void       *userdata;
} FluxAppTextBoxCreateInfo;

typedef struct FluxNumberBoxCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	double         min_value;
	double         max_value;
	double         step;
	void           (*on_value_change)(void *, double);
	void          *userdata;
} FluxNumberBoxCreateInfo;

typedef struct FluxAppNumberBoxCreateInfo {
	FluxApp   *app;
	XentNodeId parent;
	double     min_value;
	double     max_value;
	double     step;
	void       (*on_value_change)(void *, double);
	void      *userdata;
} FluxAppNumberBoxCreateInfo;

typedef struct FluxHyperlinkCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	char const    *label;
	char const    *url;
	void           (*on_click)(void *);
	void          *userdata;
} FluxHyperlinkCreateInfo;

typedef struct FluxProgressCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	float          value;
	float          max_value;
} FluxProgressCreateInfo;

typedef struct FluxInfoBadgeCreateInfo {
	XentContext      *ctx;
	FluxNodeStore    *store;
	XentNodeId        parent;
	FluxInfoBadgeMode mode;
	int32_t           value;
} FluxInfoBadgeCreateInfo;

/** @brief Create a Fluxent application. */
HRESULT           flux_app_create(FluxAppConfig const *cfg, FluxApp **out);
/** @brief Destroy a Fluxent application. */
void              flux_app_destroy(FluxApp *app);
/** @brief Set the layout root and node store rendered by the app. */
void              flux_app_set_root(FluxApp *app, XentContext *ctx, XentNodeId root, FluxNodeStore *store);
/** @brief Run the application message/render loop. */
int               flux_app_run(FluxApp *app);

/** @brief Get the app render engine. */
FluxEngine       *flux_app_get_engine(FluxApp *app);
/** @brief Get the app window. */
FluxWindow       *flux_app_get_window(FluxApp *app);
/** @brief Get the app input router. */
FluxInput        *flux_app_get_input(FluxApp *app);
/** @brief Get the app theme manager. */
FluxThemeManager *flux_app_get_theme(FluxApp *app);
/** @brief Get the app tooltip manager. */
FluxTooltip      *flux_app_get_tooltip(FluxApp *app);
/** @brief Get the app text renderer. */
FluxTextRenderer *flux_app_get_text_renderer(FluxApp *app);
/** @brief Get the app layout context. */
XentContext      *flux_app_get_context(FluxApp *app);
/** @brief Get the app node store. */
FluxNodeStore    *flux_app_get_store(FluxApp *app);

/** @brief Create a button node. */
XentNodeId        flux_create_button(FluxButtonCreateInfo const *info);

/** @brief Create a text node. */
XentNodeId
flux_create_text(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *content, float font_size);

/** @brief Create a slider node. */
XentNodeId flux_create_slider(FluxSliderCreateInfo const *info);

/** @brief Create a checkbox node. */
XentNodeId flux_create_checkbox(FluxToggleCreateInfo const *info);

/** @brief Create a radio button node. */
XentNodeId flux_create_radio(FluxToggleCreateInfo const *info);

/** @brief Create a switch node. */
XentNodeId flux_create_switch(FluxToggleCreateInfo const *info);

/** @brief Create a progress bar node. */
XentNodeId flux_create_progress(FluxProgressCreateInfo const *info);

/** @brief Create a card node. */
XentNodeId flux_create_card(XentContext *ctx, FluxNodeStore *store, XentNodeId parent);

/** @brief Create a divider node. */
XentNodeId flux_create_divider(XentContext *ctx, FluxNodeStore *store, XentNodeId parent);

/** @brief Create a text box node. */
XentNodeId flux_create_textbox(FluxTextBoxCreateInfo const *info);

/** @brief Create a text box node using the app-owned context and store. */
XentNodeId flux_app_create_textbox(FluxAppTextBoxCreateInfo const *info);

/** @brief Create a password box node using the app-owned context and store. */
XentNodeId flux_app_create_password_box(FluxAppTextBoxCreateInfo const *info);

/** @brief Create a number box node using the app-owned context and store. */
XentNodeId flux_app_create_number_box(FluxAppNumberBoxCreateInfo const *info);

/** @brief Create a password box node. */
XentNodeId flux_create_password_box(FluxTextBoxCreateInfo const *info);

/** @brief Create a number box node. */
XentNodeId flux_create_number_box(FluxNumberBoxCreateInfo const *info);

/** @brief Create a hyperlink button node. */
XentNodeId flux_create_hyperlink(FluxHyperlinkCreateInfo const *info);

/** @brief Create a repeat button node. */
XentNodeId flux_create_repeat_button(FluxButtonCreateInfo const *info);

/** @brief Create a progress ring node. */
XentNodeId flux_create_progress_ring(FluxProgressCreateInfo const *info);

/** @brief Create an info badge node. */
XentNodeId flux_create_info_badge(FluxInfoBadgeCreateInfo const *info);

#ifdef __cplusplus
}
#endif

#endif
