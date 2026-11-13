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
	FluxWindow        *window;
	FluxText          *text;
	FluxTheme         *theme;
	FluxApp           *app;
	FluxInput         *input;
	char const *const *items;
	int                item_count;
	int                selected_index;
	char const        *placeholder;
	bool               editable;
	void               (*on_select)(void *, int);
	void               (*on_submit)(void *, char const *);
	void              *userdata;
} FluxComboBoxDesc;

XentNodeId  flux_combo_init(FluxComboBoxDesc const *info);

void        flux_prop_combo_selected(FluxStore *store, XentNodeId id, int index);
void        flux_prop_combo_placeholder(FluxStore *store, XentNodeId id, char const *placeholder);
void        flux_prop_combo_items(FluxStore *store, XentNodeId id, char const *const *items, int count);
void        flux_prop_combo_editable(FluxStore *store, XentNodeId id, bool editable);
bool        flux_combo_handle_wheel(FluxStore *store, XentNodeId id, float delta);
void        flux_prop_combo_text(FluxStore *store, XentNodeId id, char const *text);

char const *flux_combo_text(FluxStore *store, XentNodeId id);

typedef struct FluxAutoSuggestDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  parent;
	FluxApp    *app;
	FluxWindow *window;
	FluxText   *text;
	FluxTheme  *theme;
	char const *placeholder;
	char const *query_icon;

	void        (*on_text)(void *ctx, char const *text);
	void        (*on_query)(void *ctx, char const *text);
	void        (*on_chosen)(void *ctx, int index);
	void       *text_ctx;
	void       *query_ctx;
	void       *chosen_ctx;
} FluxAutoSuggestDesc;

XentNodeId flux_suggest_init(FluxAutoSuggestDesc const *info);

void       flux_suggest_set_suggestions(FluxStore *store, XentNodeId asb, char const *const *items, int count);

void       flux_suggest_set_content(FluxStore *store, XentNodeId asb, char const *text);

void       flux_suggest_set_placeholder(FluxStore *store, XentNodeId asb, char const *placeholder);

void       flux_suggest_set_enabled(FluxStore *store, XentNodeId asb, bool enabled);
void       flux_suggest_set_query_icon(FluxStore *store, XentNodeId asb, char const *icon);

typedef struct FluxContentDialogDesc {
	XentCtx    *ctx;
	FluxStore  *store;
	XentNodeId  overlay_parent;
	FluxInput  *input;
	FluxWindow *window;
	FluxTheme  *theme;
	char const *title;
	char const *primary_text;
	char const *secondary_text;
	char const *close_text;
	float       width;
	float       content_height;
	void        (*on_result)(void *, FluxDialogResult);
	void       *userdata;
	XentNodeId *out_content;
} FluxContentDialogDesc;

XentNodeId flux_dialog_init(FluxContentDialogDesc const *info);

void       flux_dialog_show(FluxStore *store, XentNodeId dialog);

void       flux_dialog_hide(FluxStore *store, XentNodeId dialog);

void       flux_dialog_sync_host(FluxStore *store, XentNodeId dialog, bool open);
void       flux_dialog_configure(
  FluxStore *store, XentNodeId dialog, char const *title, char const *primary, char const *secondary, char const *close,
  float width, float content_height
);

XentNodeId flux_dialogbody_node(FluxStore *store, XentNodeId dialog);

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

XentNodeId flux_infobar_init(FluxInfoBarDesc const *info);

typedef struct FluxTeachingTipDesc {
	XentCtx                 *ctx;
	FluxStore               *store;
	XentNodeId               parent;
	FluxWindow              *window;
	FluxText                *text;
	FluxTheme               *theme;
	FluxInput               *input;
	char const              *title;
	char const              *subtitle;
	uint32_t                 icon_glyph;
	char const              *action_text;
	bool                     action_accent;
	char const              *close_text;
	FluxTeachingTipPlacement preferred_placement;
	bool                     untargeted;
	bool                     light_dismiss;
	int8_t                   tail_visibility;
	float                    placement_margin;
	void                     (*on_action)(void *ctx);
	void                    *action_ctx;
	void                     (*on_close)(void *ctx, FluxTeachingTipCloseReason reason);
	void                    *close_ctx;
} FluxTeachingTipDesc;

XentNodeId flux_teachtip_init(FluxTeachingTipDesc const *info);
void       flux_teachtip_retarget(XentCtx *ctx, FluxStore *store, XentNodeId tip);

void       flux_teachtip_show(FluxStore *store, XentNodeId tip);

void       flux_teachtip_hide(FluxStore *store, XentNodeId tip);

void       flux_teachtip_set_target(FluxStore *store, XentNodeId tip, XentNodeId target);

void       flux_teachtip_set_placement(FluxStore *store, XentNodeId tip, FluxTeachingTipPlacement placement);

void       flux_teachtip_set_texts(FluxStore *store, XentNodeId tip, char const *title, char const *subtitle);
void       flux_teachtip_configure(
  FluxStore *store, XentNodeId tip, char const *icon, char const *action_text, bool action_accent,
  char const *close_text, bool light_dismiss, int tail_visibility, float placement_margin
);

#endif
