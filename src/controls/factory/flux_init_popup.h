/**
 * @file flux_init_popup.h
 * @brief Private Desc + init prototypes for popup/flyout-hosted controls:
 * ComboBox / AutoSuggestBox / ContentDialog / InfoBar / TeachingTip.
 */
#ifndef FLUX_INIT_POPUP_H
#define FLUX_INIT_POPUP_H

#include "fluxent/fluxent.h"
#include "controls/data/flux_component_data.h"

#include <stdbool.h>
#include <stdint.h>

typedef struct FluxComboBoxDesc {
	XentCtx           *ctx;
	FluxStore         *store;
	XentNodeId         parent;
	FluxWindow        *window;         /**< For the drop-down popup + coordinate mapping. */
	FluxText          *text;           /**< Shared text renderer for drop-down items. */
	FluxTheme         *theme;          /**< Theme source for the drop-down. */
	char const *const *items;          /**< UTF-8 item strings; the control deep-copies them. */
	int                item_count;
	int                selected_index; /**< Initial selection, or -1. */
	char const        *placeholder;
	void               (*on_select)(void *, int);
	void              *userdata;
} FluxComboBoxDesc;

/** @brief Create a combo box node with a drop-down list of string items. */
XentNodeId flux_combo_init(FluxComboBoxDesc const *info);

void       flux_prop_combo_selected(FluxStore *store, XentNodeId id, int index);
void       flux_prop_combo_placeholder(FluxStore *store, XentNodeId id, char const *placeholder);
void       flux_prop_combo_items(FluxStore *store, XentNodeId id, char const *const *items, int count);

/* -------------------------------------------------------------------------
 * AutoSuggestBox
 * ---------------------------------------------------------------------- */

typedef struct FluxAutoSuggestDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxApp    *app; /**< Hosts the inner TextBox (IME etc.). */
	FluxWindow *window;
	FluxText   *text;
	FluxTheme  *theme;
	char const *placeholder;
	char const *query_icon;                               /**< Segoe Fluent Icons name; NULL hides the button. */

	void        (*on_text)(void *ctx, char const *text);  /**< User input only (WinUI Reason=UserInput). */
	void        (*on_query)(void *ctx, char const *text); /**< Enter / query button / suggestion click. */
	void        (*on_chosen)(void *ctx, int index);       /**< Arrow-key preview committed to the box. */
	void       *text_ctx;
	void       *query_ctx;
	void       *chosen_ctx;
} FluxAutoSuggestDesc;

/** @brief Create an AutoSuggestBox (TextBox + query button + suggestions popup). */
XentNodeId flux_suggest_init(FluxAutoSuggestDesc const *info);

/** @brief Replace the suggestion strings; opens/closes the list per WinUI rules. */
void       flux_suggest_set_suggestions(FluxStore *store, XentNodeId asb, char const *const *items, int count);

/** @brief Programmatic text (Reason=ProgrammaticChange; does not post on_text). */
void       flux_suggest_set_content(FluxStore *store, XentNodeId asb, char const *text);

/** @brief Replace the inner TextBox placeholder. */
void       flux_suggest_set_placeholder(FluxStore *store, XentNodeId asb, char const *placeholder);

/** @brief Enable / disable the box + button. */
void       flux_suggest_set_enabled(FluxStore *store, XentNodeId asb, bool enabled);
void       flux_suggest_set_query_icon(FluxStore *store, XentNodeId asb, char const *icon);

/* -------------------------------------------------------------------------
 * ContentDialog
 * ---------------------------------------------------------------------- */

typedef struct FluxContentDialogDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  overlay_parent; /**< Full-window container to host the dialog (e.g. app root). */
	FluxInput  *input;          /**< For the modal focus trap. */
	FluxWindow *window;         /**< For repaint requests. */
	FluxTheme  *theme;          /**< Card background follows this theme. */
	char const *title;
	char const *primary_text;   /**< NULL omits the button. Accent-styled. */
	char const *secondary_text; /**< NULL omits the button. */
	char const *close_text;     /**< NULL omits the button. */
	float       width;          /**< Card width, clamped to [320,548]. */
	float       content_height; /**< Height reserved for the body content region. */
	void        (*on_result)(void *, FluxDialogResult);
	void       *userdata;
	XentNodeId *out_content; /**< Optional: receives the content node to fill. */
} FluxContentDialogDesc;

/** @brief Create a content dialog (detached). Show it with flux_dialog_show. */
XentNodeId flux_dialog_init(FluxContentDialogDesc const *info);

/** @brief Show the content dialog modally (attaches overlay, traps focus). */
void       flux_dialog_show(FluxStore *store, XentNodeId dialog);

/** @brief Hide the content dialog (detaches overlay, releases the focus trap). */
void       flux_dialog_hide(FluxStore *store, XentNodeId dialog);

/** @brief Keep portal parenting in sync with the declarative open flag.
 * Reconcile parents the el under the page; this re-parks or re-overlays each frame. */
void       flux_dialog_sync_host(FluxStore *store, XentNodeId dialog, bool open);
void       flux_dialog_configure(
  FluxStore *store, XentNodeId dialog, char const *title, char const *primary, char const *secondary, char const *close,
  float width, float content_height
);

/** @brief Get the dialog's body content node (the child the caller fills). */
XentNodeId flux_dialogbody_node(FluxStore *store, XentNodeId dialog);

/* -------------------------------------------------------------------------
 * InfoBar
 * ---------------------------------------------------------------------- */

typedef struct FluxInfoBarDesc {
	XentCtx            *ctx;
	FluxStore          *store;
	XentNodeId          parent;
	FluxInfoBarSeverity severity;
	char const         *title;
	char const         *message;
	bool                is_closable;
	void                (*on_close)(void *);
	void               *userdata;
} FluxInfoBarDesc;

/** @brief Create an info bar node (status banner with severity icon + close). */
XentNodeId flux_infobar_init(FluxInfoBarDesc const *info);

/* -------------------------------------------------------------------------
 * TeachingTip
 * ---------------------------------------------------------------------- */

typedef struct FluxTeachingTipDesc {
	XentCtx                 *ctx;
	FluxStore               *store;
	XentNodeId               parent;           /**< Anchor/target node; also the tree parent. */
	FluxWindow              *window;           /**< Owner window for the popup + anchor mapping. */
	FluxText                *text;             /**< Shared text renderer for the tip surface. */
	FluxTheme               *theme;            /**< Theme source for the tip surface. */
	FluxInput               *input;            /**< Esc/F6 focus handling; may be NULL. */
	char const              *title;            /**< Deep-copied; NULL hides the title row. */
	char const              *subtitle;         /**< Deep-copied; NULL hides the subtitle row. */
	uint32_t                 icon_glyph;       /**< Segoe Fluent Icons codepoint; 0 = none. */
	char const              *action_text;      /**< NULL omits the action button. */
	bool                     action_accent;    /**< Accent-style the action button. */
	char const              *close_text;       /**< NULL = header X instead of footer Close. */
	FluxTeachingTipPlacement preferred_placement;
	bool                     untargeted;       /**< Window-relative; ignore the anchor. */
	bool                     light_dismiss;    /**< Outside press dismisses; acrylic background. */
	int8_t                   tail_visibility;  /**< FLUX_TIP_TAIL_AUTO / _VISIBLE / _COLLAPSED. */
	float                    placement_margin; /**< Extra gap to the target/window edge. */
	void                     (*on_action)(void *ctx);
	void                    *action_ctx;
	void                     (*on_close)(void *ctx, FluxTeachingTipCloseReason reason);
	void                    *close_ctx;
} FluxTeachingTipDesc;

/** @brief Create a teaching tip anchored to @p parent (closed until shown). */
XentNodeId flux_teachtip_init(FluxTeachingTipDesc const *info);
void       flux_teachtip_retarget(XentCtx *ctx, FluxStore *store, XentNodeId tip);

/** @brief Solve placement and show the tip (expand animation). */
void       flux_teachtip_show(FluxStore *store, XentNodeId tip);

/** @brief Contract-animate then dismiss; reason Programmatic. */
void       flux_teachtip_hide(FluxStore *store, XentNodeId tip);

/** @brief Whether the tip is currently open. */
bool       flux_teachtip_is_open(FluxStore *store, XentNodeId tip);

/** @brief Retarget the tail anchor (XENT_NODE_INVALID = untargeted). */
void       flux_teachtip_set_target(FluxStore *store, XentNodeId tip, XentNodeId target);

/** @brief Set the preferred placement (repositions while open). */
void       flux_teachtip_set_placement(FluxStore *store, XentNodeId tip, FluxTeachingTipPlacement placement);

/** @brief Replace title/subtitle (re-measures + repositions while open). */
void       flux_teachtip_set_texts(FluxStore *store, XentNodeId tip, char const *title, char const *subtitle);
void       flux_teachtip_configure(
  FluxStore *store, XentNodeId tip, char const *icon, char const *action_text, bool action_accent,
  char const *close_text, bool light_dismiss, int tail_visibility, float placement_margin
);

/** @brief Solved placement while open; FLUX_TIP_PLACE_AUTO otherwise (test hook). */
FluxTeachingTipPlacement flux_teachtip_effective_placement(FluxStore *store, XentNodeId tip);

#endif
