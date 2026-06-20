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
#include "flux_control_type.h"
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

typedef struct FluxTextCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	char const    *content;
	float          font_size;
} FluxTextCreateInfo;

typedef struct FluxContainerCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
} FluxContainerCreateInfo;

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

typedef struct FluxDropDownButtonCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	char const    *label;
	char const    *icon_name;
} FluxDropDownButtonCreateInfo;

typedef struct FluxSplitButtonCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	char const    *label;
	char const    *icon_name;
	void           (*on_click)(void *); /**< Primary action (left zone). */
	void          *userdata;
} FluxSplitButtonCreateInfo;

typedef struct FluxProgressCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	float          value;
	float          max_value;
} FluxProgressCreateInfo;

typedef struct FluxImageCreateInfo {
	XentContext     *ctx;
	FluxNodeStore   *store;
	XentNodeId       parent;
	char const      *source;  /**< File path / URI. */
	FluxImageStretch stretch; /**< Scaling mode (FLUX_IMAGE_UNIFORM recommended). */
} FluxImageCreateInfo;

typedef struct FluxInfoBadgeCreateInfo {
	XentContext      *ctx;
	FluxNodeStore    *store;
	XentNodeId        parent;
	FluxInfoBadgeMode mode;
	int32_t           value;
} FluxInfoBadgeCreateInfo;

typedef struct FluxContentDialogCreateInfo {
	XentContext      *ctx;
	FluxNodeStore    *store;
	XentNodeId        overlay_parent; /**< Full-window container to host the dialog (e.g. app root). */
	FluxInput        *input;          /**< For the modal focus trap. */
	FluxWindow       *window;         /**< For repaint requests. */
	FluxThemeManager *theme;          /**< Card background follows this theme. */
	char const       *title;
	char const       *primary_text;   /**< NULL omits the button. Accent-styled. */
	char const       *secondary_text; /**< NULL omits the button. */
	char const       *close_text;     /**< NULL omits the button. */
	float             width;          /**< Card width, clamped to [320,548]. */
	float             content_height; /**< Height reserved for the body content region. */
	void              (*on_result)(void *, FluxDialogResult);
	void             *userdata;
	XentNodeId       *out_content; /**< Optional: receives the content node to fill. */
} FluxContentDialogCreateInfo;

typedef struct FluxComboBoxCreateInfo {
	XentContext       *ctx;
	FluxNodeStore     *store;
	XentNodeId         parent;
	FluxWindow        *window;         /**< For the drop-down popup + coordinate mapping. */
	FluxTextRenderer  *text;           /**< Shared text renderer for drop-down items. */
	FluxThemeManager  *theme;          /**< Theme source for the drop-down. */
	char const *const *items;          /**< UTF-8 item strings; the control deep-copies them. */
	int                item_count;
	int                selected_index; /**< Initial selection, or -1. */
	char const        *placeholder;
	void               (*on_select)(void *, int);
	void              *userdata;
} FluxComboBoxCreateInfo;

typedef struct FluxExpanderCreateInfo {
	XentContext   *ctx;
	FluxNodeStore *store;
	XentNodeId     parent;
	FluxWindow    *window;         /**< For repaint while the content slides; may be NULL. */
	char const    *header;         /**< Optional convenience label placed in the header. */
	bool           expanded;
	float          width;          /**< Control width in DIPs. */
	float          content_height; /**< Height reserved for the content region when expanded. */
	void           (*on_toggle)(void *, bool);
	void          *userdata;
	XentNodeId    *out_content; /**< Optional: receives the content node to fill. */
	XentNodeId    *out_header;  /**< Optional: receives the header node for arbitrary content. */
} FluxExpanderCreateInfo;

typedef struct FluxInfoBarCreateInfo {
	XentContext        *ctx;
	FluxNodeStore      *store;
	XentNodeId          parent;
	FluxInfoBarSeverity severity;
	char const         *title;
	char const         *message;
	bool                is_closable;
	void                (*on_close)(void *);
	void               *userdata;
} FluxInfoBarCreateInfo;

typedef struct FluxMenuBarCreateInfo {
	XentContext      *ctx;
	FluxNodeStore    *store;
	XentNodeId        parent;
	FluxWindow       *window; /**< Owner for the drop-down flyouts + coordinate mapping. */
	FluxTextRenderer *text;   /**< Measures item titles and draws the flyouts. */
	FluxThemeManager *theme;  /**< Theme source for the flyouts. */
} FluxMenuBarCreateInfo;

typedef enum FluxNavDisplayMode FluxNavDisplayMode;

typedef struct FluxNavViewCreateInfo {
	XentContext      *ctx;
	FluxNodeStore    *store;
	XentNodeId        parent;
	FluxWindow       *window;   /**< For repaint, adaptive width, coordinate mapping. */
	FluxTextRenderer *text;     /**< Measures/draws item labels. */
	FluxThemeManager *theme;    /**< Children-flyout chrome (Compact/Top hierarchical items). */
	float             width;    /**< Initial control size (DIPs); tracks the window when it fills it.
	                             *   <= 0 fills the window and tracks the client size on both axes. */
	float             height;
	int               mode;     /**< Initial FluxNavDisplayMode (ignored when adaptive). */
	bool              adaptive; /**< Auto-switch Expanded/Compact/Minimal by window width. */
	void              (*on_selection_changed)(void *ctx, int index);
	void             *userdata;
	XentNodeId       *out_content; /**< Optional: receives the content host node to fill. */
} FluxNavViewCreateInfo;

/** @brief Create a Fluxent application, or NULL on failure. */
FluxApp          *flux_app_create(FluxAppConfig const *cfg);
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
/** @brief Get the app root node. */
XentNodeId        flux_app_get_root(FluxApp *app);

/** @brief Request rendering for the next compositor frame on the app window. */
void              flux_app_request_render(FluxApp *app);

/**
 * @brief Install a per-frame callback that runs at frame start, after input
 * dispatch and before layout/paint. The xtk runtime uses this to drain its
 * message queue and reconcile the view. NULL clears it.
 */
void              flux_app_set_frame_callback(FluxApp *app, void (*cb)(void *ctx), void *ctx);

/** @brief Get the Direct2D-backed graphics context owned by the app's window. */
FluxGraphics     *flux_app_get_graphics(FluxApp *app);

/** @brief Deepest visible MenuFlyout leaf for the app's window, if any. */
FluxMenuFlyout   *flux_app_get_active_menu_flyout(FluxApp *app);

/** @brief Native HWND for the application's primary window. */
HWND              flux_app_get_hwnd(FluxApp *app);

/** @brief Create a button node. */
XentNodeId        flux_create_button(FluxButtonCreateInfo const *info);

/** @brief Create a plain text leaf node with the requested font size. */
XentNodeId        flux_create_text(FluxTextCreateInfo const *info);

/** @brief Create a slider node. */
XentNodeId        flux_create_slider(FluxSliderCreateInfo const *info);

/** @brief Create a checkbox node. */
XentNodeId        flux_create_checkbox(FluxToggleCreateInfo const *info);

/** @brief Create a radio button node. */
XentNodeId        flux_create_radio(FluxToggleCreateInfo const *info);

/** @brief Create a switch node. */
XentNodeId        flux_create_switch(FluxToggleCreateInfo const *info);

/** @brief Create a drop-down button node. Bind a flyout with flux_node_set_flyout_ex. */
XentNodeId        flux_create_dropdown_button(FluxDropDownButtonCreateInfo const *info);

/** @brief Create a split button node (primary action + dropdown chevron). */
XentNodeId        flux_create_split_button(FluxSplitButtonCreateInfo const *info);

/** @brief Create a progress bar node. */
XentNodeId        flux_create_progress(FluxProgressCreateInfo const *info);

/** @brief Create a card node. */
XentNodeId        flux_create_card(FluxContainerCreateInfo const *info);

/**
 * @brief Create a scroll viewport (flex column). The engine derives the
 * scrollable content extent from the laid-out children each frame; write
 * FluxScrollData.content_manual to take over the extent by hand.
 */
XentNodeId        flux_create_scroll(FluxContainerCreateInfo const *info);

/** @brief Create a divider node. */
XentNodeId        flux_create_divider(FluxContainerCreateInfo const *info);

/** @brief Create a text box node. */
XentNodeId        flux_create_textbox(FluxTextBoxCreateInfo const *info);

/** @brief Create a text box node using the app-owned context and store. */
XentNodeId        flux_app_create_textbox(FluxAppTextBoxCreateInfo const *info);

/** @brief Create a password box node using the app-owned context and store. */
XentNodeId        flux_app_create_password_box(FluxAppTextBoxCreateInfo const *info);

/** @brief Create a number box node using the app-owned context and store. */
XentNodeId        flux_app_create_number_box(FluxAppNumberBoxCreateInfo const *info);

/** @brief Create a password box node. */
XentNodeId        flux_create_password_box(FluxTextBoxCreateInfo const *info);

/** @brief Create a number box node. */
XentNodeId        flux_create_number_box(FluxNumberBoxCreateInfo const *info);

/** @brief Create a hyperlink button node. */
XentNodeId        flux_create_hyperlink(FluxHyperlinkCreateInfo const *info);

/** @brief Create a repeat button node. */
XentNodeId        flux_create_repeat_button(FluxButtonCreateInfo const *info);

/** @brief Create a progress ring node. */
XentNodeId        flux_create_progress_ring(FluxProgressCreateInfo const *info);

/** @brief Create an info badge node. */
XentNodeId        flux_create_info_badge(FluxInfoBadgeCreateInfo const *info);

/** @brief Create an image node (WIC-decoded bitmap with a stretch mode). */
XentNodeId        flux_create_image(FluxImageCreateInfo const *info);

/** @brief Create a combo box node with a drop-down list of string items. */
XentNodeId        flux_create_combo_box(FluxComboBoxCreateInfo const *info);

/** @brief Create a content dialog (detached). Show it with flux_content_dialog_show. */
XentNodeId        flux_create_content_dialog(FluxContentDialogCreateInfo const *info);

/** @brief Show the content dialog modally (attaches overlay, traps focus). */
void              flux_content_dialog_show(FluxNodeStore *store, XentNodeId dialog);

/** @brief Hide the content dialog (detaches overlay, releases the focus trap). */
void              flux_content_dialog_hide(FluxNodeStore *store, XentNodeId dialog);

/** @brief Get the dialog's body content node (the child the caller fills). */
XentNodeId        flux_content_dialog_content_node(FluxNodeStore *store, XentNodeId dialog);

/** @brief Create an info bar node (status banner with severity icon + close). */
XentNodeId        flux_create_info_bar(FluxInfoBarCreateInfo const *info);

/** @brief Create an expander node; fills out_content with the child to populate. */
XentNodeId        flux_create_expander(FluxExpanderCreateInfo const *info);

/** @brief Get the expander's content node (the child the caller fills). */
XentNodeId        flux_expander_content_node(FluxNodeStore *store, XentNodeId id);

/** @brief Get the expander's header node (host for arbitrary header content). */
XentNodeId        flux_expander_header_node(FluxNodeStore *store, XentNodeId id);

/** @brief Create a menu bar (horizontal row of top-level menus). */
XentNodeId        flux_create_menu_bar(FluxMenuBarCreateInfo const *info);

/**
 * @brief Append a top-level menu titled @p title and return its (empty) flyout.
 * Populate the returned flyout with flux_menu_flyout_add_item / _add_separator.
 * @p title is copied. Returns NULL if the bar is full/invalid.
 */
FluxMenuFlyout   *flux_menu_bar_add_menu(FluxNodeStore *store, XentNodeId bar, char const *title);

/** @brief Create a left-nav NavigationView; out_content receives the content host to fill. */
XentNodeId        flux_create_nav_view(FluxNavViewCreateInfo const *info);

/**
 * @brief Append a menu item (icon name + label) and return its selection index.
 * @p icon_name and @p label are copied. The first menu item is auto-selected.
 */
int  flux_nav_view_add_item(FluxNodeStore *store, XentNodeId nav, char const *icon_name, char const *label);

/** @brief Append a footer item (e.g. Settings); returns its selection index. */
int  flux_nav_view_add_footer_item(FluxNodeStore *store, XentNodeId nav, char const *icon_name, char const *label);

/** @brief Programmatically select an item by index (fires on_selection_changed). */
void flux_nav_view_select(FluxNodeStore *store, XentNodeId nav, int index);

/** @brief Get the content host node (the child the caller fills / swaps). */
XentNodeId  flux_nav_view_content_node(FluxNodeStore *store, XentNodeId nav);

/**
 * @brief Set PaneDisplayMode (FluxNavPaneDisplayMode). Auto re-enables adaptive
 * threshold behavior; Left/LeftCompact/LeftMinimal/Top are sticky.
 */
void        flux_nav_view_set_pane_display_mode(FluxNodeStore *store, XentNodeId nav, int mode);

/** @brief Append a NavigationViewItemSeparator (1px rule) to the menu list. */
int         flux_nav_view_add_separator(FluxNodeStore *store, XentNodeId nav);

/** @brief Append a NavigationViewItemHeader (section caption) to the menu list. */
int         flux_nav_view_add_header(FluxNodeStore *store, XentNodeId nav, char const *label);

/** @brief Item label by index, owned by the control (NULL for separators/invalid). */
char const *flux_nav_view_item_label(FluxNodeStore *store, XentNodeId nav, int index);

/**
 * @brief Append a child under @p parent_index (hierarchical item). Children
 * expand inline in Expanded/Minimal (31px indent per depth) and open in a
 * flyout from Compact/Top, like WinUI's hierarchical NavigationView.
 */
int         flux_nav_view_add_child_item(
  FluxNodeStore *store, XentNodeId nav, int parent_index, char const *icon_name, char const *label
);

typedef struct FluxTabViewCreateInfo {
	XentContext      *ctx;
	FluxNodeStore    *store;
	XentNodeId        parent;
	FluxWindow       *window;          /**< For repaint + coordinate mapping. */
	FluxTextRenderer *text;            /**< Measures/draws tab headers. */
	FluxInput        *input;           /**< Arrow-key focus moves between tab headers. */
	int               width_mode;      /**< FluxTabWidthMode; 0 = Equal (WinUI default). */
	int               close_overlay;   /**< FluxTabCloseOverlayMode; 0 = Auto (always visible). */
	bool              disable_reorder; /**< Opt out of drag-reorder (CanReorderTabs defaults true). */
	void              (*on_selection_changed)(void *ctx, int index);
	bool              (*on_close_requested)(void *ctx, int index); /**< Veto hook; NULL = always close. */
	void              (*on_tab_closed)(void *ctx, int index);
	void              (*on_add_tab)(void *ctx);                    /**< NULL hides the (+) button. */
	void             *userdata;
} FluxTabViewCreateInfo;

/** @brief Create a TabView (tab strip over a content host). */
XentNodeId flux_create_tab_view(FluxTabViewCreateInfo const *info);

/**
 * @brief Append a tab (icon name + label) and return its index; out_content
 * receives the content subtree to fill. @p icon_name / @p label are copied.
 * The first tab is auto-selected.
 */
int        flux_tab_view_add_tab(
  FluxNodeStore *store, XentNodeId tabview, char const *icon_name, char const *label, XentNodeId *out_content
);

/** @brief Select a tab by index (fires on_selection_changed, swaps content). */
void flux_tab_view_select(FluxNodeStore *store, XentNodeId tabview, int index);

/** @brief Close a tab by index (collapse animation, then detach; fires on_tab_closed). */
void flux_tab_view_close(FluxNodeStore *store, XentNodeId tabview, int index);

/** @brief Per-tab TabViewItem.IsClosable: hides the close glyph and blocks Ctrl+F4/middle-click. */
void flux_tab_view_set_closable(FluxNodeStore *store, XentNodeId tabview, int index, bool closable);

/** @brief Switch TabWidthMode (FluxTabWidthMode) and re-lay the strip. */
void flux_tab_view_set_width_mode(FluxNodeStore *store, XentNodeId tabview, int width_mode);

/** @brief Place a caller-built node before the tabs (TabStripHeader). */
void flux_tab_view_set_strip_header(FluxNodeStore *store, XentNodeId tabview, XentNodeId node);

/** @brief Place a caller-built node after the (+) button (TabStripFooter). */
void flux_tab_view_set_strip_footer(FluxNodeStore *store, XentNodeId tabview, XentNodeId node);

#ifdef __cplusplus
}
#endif

#endif
