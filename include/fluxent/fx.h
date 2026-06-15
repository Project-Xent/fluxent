/**
 * @file fx.h
 * @brief FX — Fluxent's declarative layer (Elm-style view over retained controls).
 *
 * Each frame the app's view function builds a throwaway FluxEl description
 * tree from per-frame arena memory; the reconciler diffs it against the
 * previous frame and applies the changes to retained controls. Interaction
 * never calls back into user code directly: controls post FxMsg values that
 * the app's update function consumes before the next view pass.
 *
 * Conventions:
 * - Descriptor structs are written with designated initializers; the zero
 *   value of every field is the sensible default (e.g. `.disabled = false`,
 *   `.size = 0` meaning 14dp text). `(FxDesc){0}` is always valid.
 * - Children are compound-literal arrays terminated by FX_END. NULL entries
 *   are skipped, so `cond ? fx_text(...) : NULL` is conditional rendering.
 * - Every string passed in is copied into the frame arena immediately;
 *   callers may pass stack buffers. Element pointers are only valid until
 *   the next frame begins.
 *
 * @code
 * FluxEl *view(FxUi *ui, Model const *m) {
 *     return fx_scroll(ui, (FluxEl *[]) {
 *       fx_column(ui, (FxStackDesc) {.gap = 12, .padding = {32, 32, 32, 32}}, (FluxEl *[]) {
 *         fx_text(ui, "Counter", (FxTextDesc) {.size = 24}),
 *         fx_text(ui, fx_fmt(ui, "Count: %d", m->count), (FxTextDesc) {0}),
 *         fx_button(ui, "Increment", (FxButtonDesc) {.on_click = fx_msg(MSG_INCREMENT)}),
 *         m->count > 0 ? fx_button(ui, "Reset", (FxButtonDesc) {.on_click = fx_msg(MSG_RESET)}) : NULL,
 *         FX_END }),
 *       FX_END });
 * }
 * @endcode
 */
#ifndef FLUXENT_FX_H
#define FLUXENT_FX_H

#include "flux_component_data.h"
#include "flux_control_type.h"
#include "flux_types.h"
#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct FxUi   FxUi;
typedef struct FluxEl FluxEl;

/**
 * @brief Children-array terminator. NULL cannot terminate because NULL
 * entries inside the array mean "render nothing" (conditional rendering).
 */
extern FluxEl *const  FX_END;

/**
 * @brief A message value posted by a control interaction (Elm-style).
 *
 * Controls never invoke user callbacks; they enqueue the FxMsg bound to them
 * and the runtime hands it to the app's update function. Tag 0 is reserved
 * for "no message" — start app message enums at 1.
 */
typedef struct FxMsg {
	int32_t tag; /**< 0 = none. */

	union
	{
		int32_t i;
		float   f;
		double  d;
		bool    b;
		void   *ptr;
	};
} FxMsg;

/** @brief Make a payload-less message. */
static FxMsg inline fx_msg(int32_t tag) { return ( FxMsg ) {.tag = tag}; }

/** @brief Make a message carrying an int payload (e.g. a list index). */
static FxMsg inline fx_msg_i(int32_t tag, int32_t i) { return ( FxMsg ) {.tag = tag, .i = i}; }

/* -------------------------------------------------------------------------
 * Descriptors (all fields optional; zero = default)
 * ---------------------------------------------------------------------- */

typedef struct FxTextDesc {
	float          size;   /**< Font size in DIPs; 0 = 14. */
	FluxFontWeight weight; /**< 0 = regular. */
	FluxTextAlign  align;  /**< 0 = left. */
	FluxColor      color;  /**< 0 alpha = theme default. */
} FxTextDesc;

typedef struct FxButtonDesc {
	FluxButtonStyle style; /**< 0 = standard. */
	bool            disabled;
	char const     *icon;  /**< Segoe Fluent Icons name, or NULL. */
	FxMsg           on_click;
} FxButtonDesc;

typedef struct FxToggleDesc {
	bool  checked;
	bool  disabled;
	FxMsg on_change; /**< Delivered with .b = the new checked state. */
} FxToggleDesc;

typedef struct FxSliderDesc {
	float             min;
	float             max;            /**< 0 (with min 0) = 100. */
	float             value;
	float             step;           /**< Snap grid (WinUI StepFrequency); 0 = the WinUI default of 1. */
	float             tick;           /**< Tick mark interval (WinUI TickFrequency); 0 = no ticks. */
	FluxTickPlacement tick_placement; /**< Zero = FLUX_TICK_INLINE, the WinUI default. */
	bool              snap_to_ticks;  /**< Snap the value to tick multiples instead of step multiples. */
	float             small_change;   /**< Arrow-key delta; 0 = the WinUI default of 1. */
	float             large_change;   /**< PageUp/Down delta; 0 = the WinUI default of 10. */
	bool              disabled;
	FxMsg             on_change;      /**< Delivered with .f = the new value. */
} FxSliderDesc;

typedef struct FxTextBoxDesc {
	char const *content;   /**< NULL = uncontrolled (keep what the user typed). */
	bool        disabled;
	FxMsg       on_change; /**< Delivered with .ptr = transient UTF-8 text (copy to keep). */
} FxTextBoxDesc;

typedef struct FxProgressDesc {
	float value;
	float max; /**< 0 = 100. */
	bool  indeterminate;
} FxProgressDesc;

typedef struct FxBadgeDesc {
	FluxInfoBadgeMode mode; /**< 0 = dot. */
	int32_t           value;
	char const       *icon; /**< Icon name for FLUX_BADGE_ICON. */
} FxBadgeDesc;

typedef struct FxImageDesc {
	FluxImageStretch stretch; /**< 0 = none; FLUX_IMAGE_UNIFORM recommended. */
} FxImageDesc;

typedef struct FxHyperlinkDesc {
	char const *url; /**< Opened on click; NULL = message only. */
	bool        disabled;
	FxMsg       on_click;
} FxHyperlinkDesc;

typedef struct FxStackDesc {
	float           gap;
	XentInsets      padding;
	XentFlexAlign   align;   /**< Cross-axis alignment; 0 = start. */
	XentFlexJustify justify; /**< Main-axis distribution; 0 = start. */
	bool            fill;    /**< Fill the main axis instead of hugging content. */
} FxStackDesc;

typedef struct FxComboDesc {
	char const *const *items;     /**< UTF-8 item strings (copied into the arena). */
	int                count;
	int                selected;  /**< -1 = show the placeholder. */
	FxMsg              on_select; /**< Delivered with .i = the selected index. */
} FxComboDesc;

typedef struct FxExpanderDesc {
	bool  expanded;
	float width;          /**< Pinned control width in DIPs; 0 = 600. */
	float content_height; /**< Content-region height when expanded; 0 = 120. */
	FxMsg on_toggle;      /**< Delivered with .b = the new expanded state. */
} FxExpanderDesc;

typedef struct FxInfoBarDesc {
	FluxInfoBarSeverity severity; /**< 0 = informational. */
	bool                closable;
	char const         *message;  /**< Body text; set by fx_info_bar. */
	FxMsg               on_close; /**< Close button; hide by not rendering the bar. */
} FxInfoBarDesc;

typedef struct FxNumberDesc {
	double min;
	double max;  /**< 0 (with min 0) = 100. */
	double step; /**< Spin/wheel increment; 0 = 1. */
	double value;
	bool   disabled;
	bool   hide_spin; /**< Hide the +/- spin buttons. */
	FxMsg  on_change; /**< Delivered with .d = the new value. */
} FxNumberDesc;

typedef struct FxMenuItemDesc {
	char const *label; /**< NULL = separator. */
	char const *icon;  /**< Segoe Fluent Icons name, or NULL. */
	bool        disabled;
	FxMsg       on_click;
} FxMenuItemDesc;

typedef struct FxDropdownDesc {
	FluxButtonStyle       style; /**< 0 = standard. */
	bool                  disabled;
	char const           *icon;  /**< Segoe Fluent Icons name, or NULL. */
	FxMenuItemDesc const *items; /**< Menu shown on invoke (copied into the arena). */
	int                   item_count;
} FxDropdownDesc;

typedef struct FxSplitDesc {
	bool                  disabled;
	char const           *icon;     /**< Segoe Fluent Icons name, or NULL. */
	FxMsg                 on_click; /**< Primary (left) zone. */
	FxMenuItemDesc const *items;    /**< Secondary-zone menu (copied into the arena). */
	int                   item_count;
} FxSplitDesc;

typedef struct FxTabDesc {
	char const *icon;  /**< Segoe Fluent Icons name, or NULL. */
	char const *label;
} FxTabDesc;

/**
 * @brief TabView. Tabs are mount-time (key the element by the tab list to
 * change it); the element's children render in the selected tab's content
 * region and follow the selection — render them from the model, exactly like
 * a nav view page. Closing is model-driven: the close button only posts
 * on_close; remove the tab from the model and remount via the key.
 */
typedef struct FxTabViewDesc {
	FxTabDesc const *tabs;      /**< Copied into the arena. */
	int              tab_count;
	int              selected;
	FxMsg            on_select; /**< Delivered with .i = the tab index. */
	FxMsg            on_close;  /**< Close button; .i = the tab index. */
	FxMsg            on_add;    /**< (+) button; tag 0 hides it. */
} FxTabViewDesc;

typedef struct FxMenuDesc {
	char const           *title; /**< Top-level menu caption (File, Edit, ...). */
	FxMenuItemDesc const *items;
	int                   item_count;
} FxMenuDesc;

/** @brief Menu bar. The menu set is mount-time; item messages stay live. */
typedef struct FxMenuBarDesc {
	FxMenuDesc const *menus; /**< Copied into the arena. */
	int               menu_count;
} FxMenuBarDesc;

/**
 * @brief ContentDialog (modal). Controlled by .open: shown while true, hidden
 * while false. Any button (or Escape) closes the retained dialog and posts
 * on_result — set the model's open flag false there. The element's children
 * fill the dialog body. App-hosted only (headless runtimes skip it).
 */
typedef struct FxDialogDesc {
	bool        open;
	char const *primary;        /**< Accent button label; NULL omits it. */
	char const *secondary;      /**< NULL omits it. */
	char const *close;          /**< NULL omits it. */
	float       width;          /**< Card width, clamped to [320, 548]; 0 = 400. */
	float       content_height; /**< Body region height; 0 = 120. */
	FxMsg       on_result;      /**< Delivered with .i = FluxDialogResult. */
} FxDialogDesc;

typedef enum FxNavKind
{
	FX_NAV_ITEM = 0, /**< Selectable menu item. */
	FX_NAV_HEADER,   /**< Section caption (not selectable). */
	FX_NAV_SEPARATOR,
	FX_NAV_FOOTER,   /**< Selectable footer item (e.g. Settings). */
	FX_NAV_CHILD,    /**< Child of the nearest preceding FX_NAV_ITEM; the parent
	                      grows a chevron and expands inline (one level deep). */
} FxNavKind;

typedef struct FxNavItemDesc {
	FxNavKind   kind;
	char const *icon;  /**< Segoe Fluent Icons name, or NULL. */
	char const *label; /**< Display text (header caption for FX_NAV_HEADER). */
} FxNavItemDesc;

/**
 * @brief NavigationView spine. The item list is applied at mount only (the
 * retained control has no item-removal API); selection and the message stay
 * live. The element's children fill the content host beside the pane.
 */
typedef struct FxNavDesc {
	FxNavItemDesc const *items;     /**< Copied into the arena. */
	int                  item_count;
	int                  selected;  /**< Index into items[] (headers/separators count). */
	FxMsg                on_select; /**< Delivered with .i = the selected item index. */
} FxNavDesc;

/* -------------------------------------------------------------------------
 * Element tree (public for the reconciler and tests; treat as read-only)
 * ---------------------------------------------------------------------- */

struct FluxEl {
	FluxControlType type;
	char const     *key;    /**< Arena copy; NULL = positional matching. */

	float           width;  /**< NAN = auto. */
	float           height; /**< NAN = auto. */
	float           grow;   /**< Flex grow factor; 0 = none. */
	XentFlexAlign   align_self;
	XentInsets      margin;

	char const     *text;      /**< Primary string: content/label/placeholder/source. */
	char const     *tooltip;   /**< Hover tooltip text, or NULL (fx_tooltip). */
	bool            stack_row; /**< Container main axis is horizontal. */

	union
	{
		FxTextDesc      text_props;
		FxButtonDesc    button;
		FxToggleDesc    toggle;
		FxSliderDesc    slider;
		FxTextBoxDesc   textbox;
		FxProgressDesc  progress;
		FxBadgeDesc     badge;
		FxImageDesc     image;
		FxHyperlinkDesc hyperlink;
		FxStackDesc     stack;
		FxComboDesc     combo;
		FxExpanderDesc  expander;
		FxInfoBarDesc   info_bar;
		FxNumberDesc    number;
		FxDropdownDesc  dropdown;
		FxSplitDesc     split;
		FxTabViewDesc   tab_view;
		FxMenuBarDesc   menu_bar;
		FxDialogDesc    dialog;
		FxNavDesc       nav;
	};

	FluxEl **children; /**< Arena array of child_count valid pointers. */
	int      child_count;
};

/* -------------------------------------------------------------------------
 * Frame context
 * ---------------------------------------------------------------------- */

/** @brief Create a build context (owns the frame arena). */
FxUi                        *fx_ui_create(void);

/** @brief Destroy the build context and its arena. */
void                         fx_ui_destroy(FxUi *ui);

/** @brief Start a frame: resets the arena; all prior FluxEl pointers die. */
void                         fx_ui_begin(FxUi *ui);

/** @brief Copy a string into the frame arena (NULL passes through). */
char                        *fx_strdup(FxUi *ui, char const *s);

/** @brief printf into the frame arena — for transient labels. */
char                        *fx_fmt(FxUi *ui, char const *fmt, ...);

/* -------------------------------------------------------------------------
 * Builders
 * ---------------------------------------------------------------------- */

FluxEl                      *fx_text(FxUi *ui, char const *content, FxTextDesc desc);
FluxEl                      *fx_button(FxUi *ui, char const *label, FxButtonDesc desc);
FluxEl                      *fx_checkbox(FxUi *ui, char const *label, FxToggleDesc desc);
FluxEl                      *fx_radio(FxUi *ui, char const *label, FxToggleDesc desc);
FluxEl                      *fx_switch(FxUi *ui, FxToggleDesc desc);
FluxEl                      *fx_slider(FxUi *ui, FxSliderDesc desc);
FluxEl                      *fx_textbox(FxUi *ui, char const *placeholder, FxTextBoxDesc desc);
FluxEl                      *fx_password(FxUi *ui, char const *placeholder, FxTextBoxDesc desc);
FluxEl                      *fx_progress(FxUi *ui, FxProgressDesc desc);
FluxEl                      *fx_progress_ring(FxUi *ui, FxProgressDesc desc);
FluxEl                      *fx_badge(FxUi *ui, FxBadgeDesc desc);
FluxEl                      *fx_image(FxUi *ui, char const *source, FxImageDesc desc);
FluxEl                      *fx_hyperlink(FxUi *ui, char const *label, FxHyperlinkDesc desc);
FluxEl                      *fx_divider(FxUi *ui);
FluxEl                      *fx_spacer(FxUi *ui);
FluxEl                      *fx_repeat_button(FxUi *ui, char const *label, FxButtonDesc desc);
FluxEl                      *fx_combo_box(FxUi *ui, char const *placeholder, FxComboDesc desc);
FluxEl                      *fx_info_bar(FxUi *ui, char const *title, char const *message, FxInfoBarDesc desc);
FluxEl                      *fx_number_box(FxUi *ui, FxNumberDesc desc);
FluxEl                      *fx_dropdown_button(FxUi *ui, char const *label, FxDropdownDesc desc);
FluxEl                      *fx_split_button(FxUi *ui, char const *label, FxSplitDesc desc);
FluxEl                      *fx_menu_bar(FxUi *ui, FxMenuBarDesc desc);

FluxEl                      *fx_column(FxUi *ui, FxStackDesc desc, FluxEl *children []);
FluxEl                      *fx_row(FxUi *ui, FxStackDesc desc, FluxEl *children []);
FluxEl                      *fx_card(FxUi *ui, FxStackDesc desc, FluxEl *children []);
FluxEl                      *fx_scroll(FxUi *ui, FluxEl *children []);
FluxEl                      *fx_expander(FxUi *ui, char const *header, FxExpanderDesc desc, FluxEl *children []);
FluxEl                      *fx_nav_view(FxUi *ui, FxNavDesc desc, FluxEl *children []);
FluxEl                      *fx_tab_view(FxUi *ui, FxTabViewDesc desc, FluxEl *children []);
FluxEl                      *fx_content_dialog(FxUi *ui, char const *title, FxDialogDesc desc, FluxEl *children []);

/**
 * @brief Allocate a children buffer for loop-built lists: capacity + 1 slots,
 * NULL-filled with FX_END pre-placed in the last slot. Fill any subset of
 * [0..capacity); remaining NULL slots are skipped like conditionals.
 */
FluxEl                     **fx_children_alloc(FxUi *ui, int capacity);

/* -------------------------------------------------------------------------
 * Modifiers (return the same element)
 * ---------------------------------------------------------------------- */

/** @brief Stable identity for keyed reconciliation (list items). */
FluxEl                      *fx_keyed(FxUi *ui, char const *key, FluxEl *el);

/** @brief Attach a hover tooltip to any element. */
FluxEl                      *fx_tooltip(FxUi *ui, FluxEl *el, char const *text);

/* -------------------------------------------------------------------------
 * Runtime (Elm loop)
 * ---------------------------------------------------------------------- */

typedef struct FluxAppConfig FluxAppConfig;

/** @brief Consume one message and evolve the model. */
typedef void                 (*FxUpdateFn)(void *model, FxMsg msg);

/** @brief Build the whole UI for the current model (pure; runs per change). */
typedef FluxEl              *(*FxViewFn)(FxUi *ui, void *model);

/**
 * @brief Create the app window and drive update/view until it closes.
 *
 * Each frame: queued control messages are drained through @p update, then
 * (only if anything changed) @p view rebuilds the element tree and the
 * reconciler applies the diff to retained controls. Returns the app's exit
 * code.
 */
int                          fx_app_run(FluxAppConfig const *cfg, void *model, FxUpdateFn update, FxViewFn view);

/** @brief Explicit size; pass NAN to keep an axis automatic. */
FluxEl                      *fx_sized(FluxEl *el, float width, float height);

FluxEl                      *fx_grow(FluxEl *el, float grow);
FluxEl                      *fx_align(FluxEl *el, XentFlexAlign align_self);
FluxEl                      *fx_margin(FluxEl *el, XentInsets margin);

#ifdef __cplusplus
}
#endif

#endif
