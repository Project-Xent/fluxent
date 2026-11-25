#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_ids.h"
#include "a11y/flux_uia_tree.h"
#include "controls/flux_module.h"
#include "controls/factory/flux_init_nav.h"
#include "popup/flux_menu_internal.h"
#include "store/flux_node_data.h"

#include "controls/data/flux_progress_data.h"

#include <fluxent/flux_popup.h>

#include <stdio.h>
#include <wchar.h>

enum
{
	PROP_CONTROL_TYPE       = 30003,
	PROP_LOCALIZED_TYPE     = 30004,
	PROP_NAME               = 30005,
	PROP_HAS_FOCUS          = 30008,
	PROP_FOCUSABLE          = 30009,
	PROP_ENABLED            = 30010,
	PROP_AUTOMATION_ID      = 30011,
	PROP_ACCELERATOR_KEY    = 30006,
	PROP_ACCESS_KEY         = 30007,
	PROP_ITEM_TYPE          = 30021,
	PROP_LABELED_BY         = 30018,
	PROP_DESCRIBED_BY       = 30105,
	PROP_FLOWS_TO           = 30106,
	PROP_FLOWS_FROM         = 30148,
	PROP_ITEM_STATUS        = 30026,
	PROP_FULL_DESCRIPTION   = 30159,
	PROP_CLASS_NAME         = 30012,
	PROP_HELP_TEXT          = 30013,
	PROP_CLICKABLE_POINT    = 30014,
	PROP_CULTURE            = 30015,
	PROP_CONTROL_ELEMENT    = 30016,
	PROP_CONTENT_ELEMENT    = 30017,
	PROP_PASSWORD           = 30019,
	PROP_OFFSCREEN          = 30022,
	PROP_ORIENTATION        = 30023,
	PROP_FRAMEWORK_ID       = 30024,
	PROP_REQUIRED_FOR_FORM  = 30025,
	PROP_DATA_VALID         = 30103,
	PROP_CONTROLLER_FOR     = 30104,
	PROP_LOCALIZED_LANDMARK = 30158,
	PROP_LIVE_SETTING       = 30135,
	PROP_PERIPHERAL         = 30150,
	PROP_POSITION_IN_SET    = 30152,
	PROP_SIZE_OF_SET        = 30153,
	PROP_LEVEL              = 30154,
	PROP_LANDMARK_TYPE      = 30157,
	PROP_HEADING_LEVEL      = 30173,
	PROP_DIALOG             = 30174,
	UIA_HEADING_LEVEL_NONE  = 80050,
	UIA_LANDMARK_CUSTOM     = 80000,
	UIA_LANDMARK_NAVIGATION = 80003,
};

typedef void (*UiaPropFn)(FluxUia *self, int role, VARIANT *out);

typedef struct UiaPropRow {
	PROPERTYID id;
	UiaPropFn  fn;
} UiaPropRow;

BSTR flux_uia_bstr(char const *utf8) {
	if (!utf8) return NULL;
	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
	if (wlen <= 1) return NULL;
	BSTR bstr = SysAllocStringLen(NULL, ( UINT ) (wlen - 1));
	if (bstr) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, bstr, wlen);
	return bstr;
}

static void put_bool(VARIANT *out, bool value) {
	out->vt      = VT_BOOL;
	out->boolVal = value ? VARIANT_TRUE : VARIANT_FALSE;
}

static void put_i4(VARIANT *out, int value) {
	out->vt   = VT_I4;
	out->lVal = value;
}

static void put_wide(VARIANT *out, wchar_t const *text) {
	out->bstrVal = text && text [0] ? SysAllocString(text) : NULL;
	out->vt      = out->bstrVal ? VT_BSTR : VT_EMPTY;
}

static void put_point(VARIANT *out, double x, double y) {
	SAFEARRAY *sa = SafeArrayCreateVector(VT_R8, 0, 2);
	LONG       i0 = 0, i1 = 1;
	if (!sa) return;
	SafeArrayPutElement(sa, &i0, &x);
	SafeArrayPutElement(sa, &i1, &y);
	out->vt     = VT_R8 | VT_ARRAY;
	out->parray = sa;
}

static FluxUiaHooks const *prop_hooks(FluxUia const *self) { return flux_module_uia(self->info.store, self->node); }

static char const         *prop_app(FluxUia const *self, int slot) {
	return self->sub < 0 ? flux_uia_apptext(self->info.store, self->node, slot) : NULL;
}

static bool prop_num(FluxUia const *self, int slot, int32_t *out) {
	return self->sub < 0 && flux_uia_appnum(self->info.store, self->node, slot, out);
}

static void put_utf8(VARIANT *out, char const *text) {
	out->bstrVal = flux_uia_bstr(text);
	out->vt      = out->bstrVal ? VT_BSTR : VT_EMPTY;
}

static void prop_app_text(FluxUia *self, int slot, VARIANT *out) {
	char const *text = prop_app(self, slot);
	if (text) put_utf8(out, text);
}

static FluxUiaSub const *prop_sub(FluxUia const *self) {
	FluxUiaHooks const *h = prop_hooks(self);
	return h ? h->sub : NULL;
}

bool flux_uia_focusable(FluxUia const *self) {
	FluxUiaSub const *s = self->sub >= 0 ? prop_sub(self) : NULL;
	if (!xent_sem_enabled(self->info.ctx, self->node)) return false;
	if (s && s->focusable) return s->focusable(self->info.store, self->node, self->sub);
	if (self->sub >= 0) return s && s->focused;
	return xent_get_focusable(self->info.ctx, self->node);
}

static XentNodeId uia_find_popup_focus(FluxUiaContext const *info, FluxNodeData const *nd) {
	if (!nd || nd->uia_popup == XENT_NODE_INVALID) return XENT_NODE_INVALID;
	FluxNodeData const *popup = flux_store_find(info->store, nd->uia_popup);
	if (!popup || !popup->popup || !flux_popup_visible(popup->popup)) return XENT_NODE_INVALID;
	return flux_uia_find_focused(info, nd->uia_popup);
}

XentNodeId flux_uia_find_focused(FluxUiaContext const *info, XentNodeId node) {
	FluxNodeData *nd = info->store ? flux_store_find(info->store, node) : NULL;
	if (nd && nd->state.focused) return node;
	for (XentNodeId c = xent_node_first(info->ctx, node); c != XENT_NODE_INVALID; c = xent_node_next(info->ctx, c)) {
		XentNodeId hit = flux_uia_find_focused(info, c);
		if (hit != XENT_NODE_INVALID) return hit;
	}
	return uia_find_popup_focus(info, nd);
}

static void prop_control_type(FluxUia *self, int role, VARIANT *out) {
	( void ) self;
	put_i4(out, role);
}

static void prop_localized_type(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	if (prop_app(self, FLUX_A11Y_CONTROL_TYPE)) put_utf8(out, prop_app(self, FLUX_A11Y_CONTROL_TYPE));
	else put_wide(out, flux_uia_lctype(&self->info, self->node, self->sub));
}

static void prop_accelerator(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	prop_app_text(self, FLUX_A11Y_ACCELERATOR, out);
}

static void prop_access_key(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	char const *key = self->sub < 0 ? flux_menubar_item_access_key(self->info.store, self->node) : NULL;
	if (!key && self->sub < 0) key = flux_menu_row_access_key(self->info.store, self->node);
	if (key) put_utf8(out, key);
	else prop_app_text(self, FLUX_A11Y_ACCESS_KEY, out);
}

static void prop_item_type(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	prop_app_text(self, FLUX_A11Y_ITEM_TYPE, out);
}

static void prop_item_status(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	prop_app_text(self, FLUX_A11Y_ITEM_STATUS, out);
}

static void prop_description(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	prop_app_text(self, FLUX_A11Y_DESCRIPTION, out);
}

static bool prop_flag(FluxUia const *self, int slot, bool fallback) {
	int32_t v = 0;
	return prop_num(self, slot, &v) ? v != 0 : fallback;
}

static void prop_required(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	put_bool(out, prop_flag(self, FLUX_A11Y_REQUIRED, false));
}

static void prop_valid(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	put_bool(out, prop_flag(self, FLUX_A11Y_VALID, true));
}

static void prop_peripheral(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	put_bool(out, prop_flag(self, FLUX_A11Y_PERIPHERAL, false));
}

static bool prop_content_role(FluxUiaHooks const *h) {
	static int const roles [] = {
	  FLUX_UIA_BUTTON, FLUX_UIA_LISTITEM, FLUX_UIA_TABITEM, FLUX_UIA_TREEITEM, FLUX_UIA_MENUITEM, FLUX_UIA_DATAITEM,
	  FLUX_UIA_HEADERITEM, FLUX_UIA_HYPERLINK};
	for (size_t i = 0; h && i < sizeof(roles) / sizeof(roles [0]); i++)
		if (h->role == roles [i]) return true;
	return false;
}

static void prop_content_text(FluxUia *self, char *text, size_t size) {
	XentNodeId  c       = xent_node_first(self->info.ctx, self->node);
	char const *content = c != XENT_NODE_INVALID && flux_node_type(self->info.store, c) == FLUX_CONTROL_TEXT
	                      ? xent_node_text(self->info.ctx, c)
	                      : NULL;
	if (content) snprintf(text, size, "%s", content);
}

typedef struct UiaNameAncestor {
	XentNodeId                    node;
	struct UiaNameAncestor const *parent;
} UiaNameAncestor;

static char const *prop_name_text(FluxUia *self, char *text, size_t size, UiaNameAncestor const *ancestors);

static char const *prop_name_from_label(FluxUia *self, char *text, size_t size, UiaNameAncestor const *ancestors) {
	XentNodeId by = XENT_NODE_INVALID;
	if (!flux_uia_refs(&self->info, self->node, FLUX_A11Y_LABELED_BY, &by, 1) || by == self->node) return NULL;
	for (UiaNameAncestor const *a = ancestors; a; a = a->parent)
		if (a->node == by) return NULL;
	UiaNameAncestor here = {.node = self->node, .parent = ancestors};
	FluxUia         peer = {.info = self->info, .node = by, .sub = -1};
	return prop_name_text(&peer, text, size, &here);
}

static char const *prop_sub_name(FluxUia *self, char *text, size_t size) {
	FluxUiaSub const *s = prop_sub(self);
	if (s && s->name) s->name(self->info.store, self->node, self->sub, text, size);
	return text;
}

static char const *prop_name_text(FluxUia *self, char *text, size_t size, UiaNameAncestor const *ancestors) {
	if (self->sub >= 0) return prop_sub_name(self, text, size);
	char const *app_name = prop_app(self, FLUX_A11Y_NAME);
	if (app_name) return app_name;
	char const *from_label = prop_name_from_label(self, text, size, ancestors);
	if (from_label && from_label [0]) return from_label;
	char const *label = xent_sem_label(self->info.ctx, self->node);
	if (label && label [0]) return label;
	FluxUiaHooks const *h = prop_hooks(self);
	if (h && h->name) h->name(self->info.store, self->node, text, size);
	if (!text [0] && prop_content_role(h)) prop_content_text(self, text, size);
	return text;
}

static void prop_name(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	char text [256] = {0};
	out->bstrVal    = flux_uia_bstr(prop_name_text(self, text, sizeof(text), NULL));
	out->vt         = out->bstrVal ? VT_BSTR : VT_EMPTY;
}

static XentNodeId prop_focus_owner(FluxUia const *self) {
	FluxNodeData const *nd = flux_store_find(self->info.store, self->node);
	XentNodeId          o  = nd && !nd->popup ? nd->uia_owner : XENT_NODE_INVALID;
	if (self->sub < 0 && o != XENT_NODE_INVALID && flux_uia_delegate(&self->info, o) == self->node) return o;
	return self->node;
}

static void prop_has_focus(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	XentNodeId          at  = prop_focus_owner(self);
	FluxNodeData const *nd  = flux_store_find(self->info.store, at);
	FluxUiaSub const   *s   = self->sub >= 0 ? prop_sub(self) : NULL;
	bool                on  = nd && nd->state.focused && GetFocus() == self->info.hwnd;
	on                     &= flux_uia_delegate(&self->info, at) == self->node;
	on                     &= self->sub >= 0 || flux_uia_focussub(&self->info, self->node) < 0;
	put_bool(out, on && (self->sub < 0 || (s && s->focused && s->focused(self->info.store, self->node, self->sub))));
}

static void prop_focusable(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	XentNodeId at = prop_focus_owner(self);
	FluxUia    o  = *self;
	o.node        = at;
	bool on       = flux_uia_focusable(&o) && (self->sub >= 0 || flux_uia_delegate(&self->info, at) == self->node);
	put_bool(out, on);
}

static void prop_controller_for(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	FluxUiaHooks const *h      = prop_hooks(self);
	XentNodeId          target = self->sub < 0 && h && h->controls ? h->controls(self->info.store, self->node) : 0;
	if (self->sub >= 0) return;
	flux_uia_refs_prop(&self->info, self->node, FLUX_A11Y_CONTROLLED, target ? target : XENT_NODE_INVALID, out);
}

static void prop_ref_list(FluxUia *self, int slot, VARIANT *out) {
	if (self->sub < 0) flux_uia_refs_prop(&self->info, self->node, slot, XENT_NODE_INVALID, out);
}

static void prop_described_by(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	prop_ref_list(self, FLUX_A11Y_DESCRIBED_BY, out);
}

static void prop_flows_to(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	prop_ref_list(self, FLUX_A11Y_FLOWS_TO, out);
}

static void prop_flows_from(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	prop_ref_list(self, FLUX_A11Y_FLOWS_FROM, out);
}

static void prop_labeled_by(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	XentNodeId by = XENT_NODE_INVALID;
	FluxUia   *p  = self->sub < 0 && flux_uia_refs(&self->info, self->node, FLUX_A11Y_LABELED_BY, &by, 1)
	                ? flux_uia_alloc(&self->info, by)
	                : NULL;
	if (!p) return;
	out->vt      = VT_UNKNOWN;
	out->punkVal = ( IUnknown * ) &p->lpVtblSimple;
}

static void prop_help_text(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	FluxUiaHooks const *h = prop_hooks(self);
	if (prop_app(self, FLUX_A11Y_HELP)) put_utf8(out, prop_app(self, FLUX_A11Y_HELP));
	else if (self->sub < 0 && h) put_wide(out, h->help);
}

static void prop_localized_landmark(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	FluxUiaHooks const *h = prop_hooks(self);
	if (prop_app(self, FLUX_A11Y_LANDMARK_NAME)) put_utf8(out, prop_app(self, FLUX_A11Y_LANDMARK_NAME));
	else if (self->sub < 0 && h) put_wide(out, h->lmtype);
}

static void prop_enabled(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	FluxUiaSub const *s  = self->sub >= 0 ? prop_sub(self) : NULL;
	bool              on = xent_sem_enabled(self->info.ctx, self->node);
	if (on && s && s->enabled) on = s->enabled(self->info.store, self->node, self->sub);
	put_bool(out, on);
}

static void prop_automation_id(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	char text [24];
	if (self->sub >= 0) return;
	if (prop_app(self, FLUX_A11Y_ID)) {
		put_utf8(out, prop_app(self, FLUX_A11Y_ID));
		return;
	}
	snprintf(text, sizeof(text), "%llu", ( unsigned long long ) self->node);
	put_utf8(out, text);
}

static void prop_class_name(FluxUia *self, int role, VARIANT *out) {
	put_wide(out, flux_uia_class(&self->info, self->node, self->sub, role));
}

static void prop_clickable_point(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	struct UiaRect r;
	if (!flux_uia_bounds(&self->info, self->node, self->sub, &r) || (r.width == 0.0 && r.height == 0.0)) return;
	put_point(out, r.left + r.width / 2.0, r.top + r.height / 2.0);
}

static void prop_culture(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	int32_t culture = 0;
	if (prop_num(self, FLUX_A11Y_CULTURE, &culture) && culture != 0) put_i4(out, culture);
	else put_i4(out, ( int ) MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT));
}

static bool prop_in_view(FluxUia const *self) {
	FluxNodeData const *nd = flux_store_find(self->info.store, self->node);
	if (self->sub >= 0 || !nd || nd->component_type != FLUX_CONTROL_PROGRESS_RING) return true;
	FluxProgressRingData const *ring = ( FluxProgressRingData const * ) nd->component_data;
	return !ring || !ring->inactive;
}

static int32_t prop_view(FluxUia const *self) {
	int32_t view = 0;
	return prop_num(self, FLUX_A11Y_VIEW, &view) ? view : 0;
}

static void prop_control_element(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	put_bool(out, prop_in_view(self) && prop_view(self) != 2);
}

static void prop_content_element(FluxUia *self, int role, VARIANT *out) {
	bool on = prop_in_view(self) && (self->sub >= 0 || role != FLUX_UIA_SCROLLBAR);
	put_bool(out, on && prop_view(self) == 0);
}

static void prop_password(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	put_bool(out, self->sub < 0 && flux_node_type(self->info.store, self->node) == FLUX_CONTROL_PASSWORD_BOX);
}

static void prop_offscreen(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	struct UiaRect r;
	put_bool(out, !flux_uia_bounds(&self->info, self->node, self->sub, &r));
}

static void prop_orientation(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	FluxUiaHooks const *h = prop_hooks(self);
	put_i4(out, self->sub < 0 && h && h->orient ? h->orient(self->info.store, self->node) : OrientationType_None);
}

static void prop_framework_id(FluxUia *self, int role, VARIANT *out) {
	( void ) self;
	( void ) role;
	put_wide(out, FLUX_UIA_FRAMEWORK);
}

static void prop_live_setting(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	int32_t live = 0;
	put_i4(out, prop_num(self, FLUX_A11Y_LIVE, &live) ? live : Off);
}

static void prop_set_info(FluxUia const *self, int *pos, int *size, int *level) {
	FluxUiaHooks const *h       = prop_hooks(self);
	FluxUiaSub const   *s       = prop_sub(self);
	int32_t             app [3] = {0, 0, 0};
	*pos = *size = *level = -1;
	if (self->sub >= 0 && s && s->setinfo) s->setinfo(self->info.store, self->node, self->sub, pos, size, level);
	if (self->sub < 0 && h && h->setinfo) h->setinfo(self->info.store, self->node, pos, size, level);
	if (prop_num(self, FLUX_A11Y_POSITION, &app [0]) && app [0] > 0) *pos = app [0];
	if (prop_num(self, FLUX_A11Y_SIZE, &app [1]) && app [1] > 0) *size = app [1];
	if (prop_num(self, FLUX_A11Y_LEVEL, &app [2]) && app [2] > 0) *level = app [2];
}

static void prop_position(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	int pos, size, level;
	prop_set_info(self, &pos, &size, &level);
	put_i4(out, pos);
}

static void prop_size(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	int pos, size, level;
	prop_set_info(self, &pos, &size, &level);
	put_i4(out, size);
}

static void prop_level(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	int pos, size, level;
	prop_set_info(self, &pos, &size, &level);
	put_i4(out, level);
}

static void prop_landmark(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	FluxControlType     t = self->sub < 0 ? flux_node_type(self->info.store, self->node) : FLUX_CONTROL_CONTAINER;
	FluxUiaHooks const *h = prop_hooks(self);
	int                 landmark = h && self->sub < 0 ? h->landmark : 0;
	int32_t             app      = 0;
	if (t == FLUX_CONTROL_INFO_BAR) landmark = UIA_LANDMARK_CUSTOM;
	if (t == FLUX_CONTROL_PAGER) landmark = UIA_LANDMARK_NAVIGATION;
	if (prop_num(self, FLUX_A11Y_LANDMARK, &app)) landmark = app > 0 ? UIA_LANDMARK_CUSTOM + app - 1 : 0;
	if (!landmark && prop_app(self, FLUX_A11Y_LANDMARK_NAME)) landmark = UIA_LANDMARK_CUSTOM;
	put_i4(out, landmark);
}

static void prop_heading_level(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	int32_t level = 0;
	put_i4(out, UIA_HEADING_LEVEL_NONE + (prop_num(self, FLUX_A11Y_HEADING, &level) ? level : 0));
}

static void prop_dialog(FluxUia *self, int role, VARIANT *out) {
	( void ) role;
	FluxControlType     type = self->sub < 0 ? flux_node_type(self->info.store, self->node) : FLUX_CONTROL_CONTAINER;
	FluxUiaHooks const *h    = self->sub < 0 ? flux_module_uia(self->info.store, self->node) : NULL;
	put_bool(out, prop_flag(self, FLUX_A11Y_DIALOG, type == FLUX_CONTROL_INFO_BAR || (h && h->dialog)));
}

static UiaPropRow const kUiaProps [] = {
  {PROP_CONTROL_TYPE,       prop_control_type      },
  {PROP_LOCALIZED_TYPE,     prop_localized_type    },
  {PROP_NAME,               prop_name              },
  {PROP_HAS_FOCUS,          prop_has_focus         },
  {PROP_FOCUSABLE,          prop_focusable         },
  {PROP_ENABLED,            prop_enabled           },
  {PROP_AUTOMATION_ID,      prop_automation_id     },
  {PROP_CLASS_NAME,         prop_class_name        },
  {PROP_LABELED_BY,         prop_labeled_by        },
  {PROP_DESCRIBED_BY,       prop_described_by      },
  {PROP_FLOWS_TO,           prop_flows_to          },
  {PROP_FLOWS_FROM,         prop_flows_from        },
  {PROP_ACCELERATOR_KEY,    prop_accelerator       },
  {PROP_ACCESS_KEY,         prop_access_key        },
  {PROP_ITEM_TYPE,          prop_item_type         },
  {PROP_ITEM_STATUS,        prop_item_status       },
  {PROP_FULL_DESCRIPTION,   prop_description       },
  {PROP_HELP_TEXT,          prop_help_text         },
  {PROP_CLICKABLE_POINT,    prop_clickable_point   },
  {PROP_CULTURE,            prop_culture           },
  {PROP_CONTROL_ELEMENT,    prop_control_element   },
  {PROP_CONTENT_ELEMENT,    prop_content_element   },
  {PROP_PASSWORD,           prop_password          },
  {PROP_OFFSCREEN,          prop_offscreen         },
  {PROP_ORIENTATION,        prop_orientation       },
  {PROP_FRAMEWORK_ID,       prop_framework_id      },
  {PROP_REQUIRED_FOR_FORM,  prop_required          },
  {PROP_DATA_VALID,         prop_valid             },
  {PROP_CONTROLLER_FOR,     prop_controller_for    },
  {PROP_LOCALIZED_LANDMARK, prop_localized_landmark},
  {PROP_LIVE_SETTING,       prop_live_setting      },
  {PROP_PERIPHERAL,         prop_peripheral        },
  {PROP_POSITION_IN_SET,    prop_position          },
  {PROP_SIZE_OF_SET,        prop_size              },
  {PROP_LEVEL,              prop_level             },
  {PROP_LANDMARK_TYPE,      prop_landmark          },
  {PROP_HEADING_LEVEL,      prop_heading_level     },
  {PROP_DIALOG,             prop_dialog            },
};

static void prop_root(FluxUia *self, PROPERTYID id, VARIANT *out) {
	RECT  client = {0};
	POINT at     = {0, 0};
	if (id == PROP_CONTROL_TYPE) put_i4(out, FLUX_UIA_WINDOW);
	if (id == PROP_FRAMEWORK_ID) put_wide(out, FLUX_UIA_FRAMEWORK);
	if (id == PROP_HAS_FOCUS && flux_uia_find_focused(&self->info, self->node) == XENT_NODE_INVALID)
		put_bool(out, true);
	if (id != PROP_CLICKABLE_POINT) return;
	GetClientRect(self->info.hwnd, &client);
	at = (POINT) {client.right / 2, client.bottom / 2};
	ClientToScreen(self->info.hwnd, &at);
	put_point(out, at.x, at.y);
}

void flux_uia_property(FluxUia *self, int role, PROPERTYID id, VARIANT *out) {
	if (self->sub < 0 && self->node == self->info.root) {
		prop_root(self, id, out);
		return;
	}
	for (size_t i = 0; i < sizeof(kUiaProps) / sizeof(kUiaProps [0]); i++)
		if (kUiaProps [i].id == id) {
			kUiaProps [i].fn(self, role, out);
			return;
		}
}
