#include "a11y/flux_uia_app.h"
#include "a11y/flux_uia_object.h"
#include "a11y/flux_uia_ids.h"
#include "controls/flux_module.h"
#include "store/flux_node_data.h"

#define MUXC L"Microsoft.UI.Xaml.Controls."

typedef struct UiaKindRow {
	FluxControlType type;
	wchar_t const  *klass;
	wchar_t const  *lctype;
} UiaKindRow;

static UiaKindRow const kUiaKinds [] = {
  {FLUX_CONTROL_TEXT,                L"TextBlock",               NULL                    },
  {FLUX_CONTROL_BUTTON,              L"Button",                  NULL                    },
  {FLUX_CONTROL_TOGGLE_BUTTON,       L"ToggleButton",            NULL                    },
  {FLUX_CONTROL_CHECKBOX,            L"CheckBox",                NULL                    },
  {FLUX_CONTROL_RADIO,               L"RadioButton",             NULL                    },
  {FLUX_CONTROL_SWITCH,              L"ToggleSwitch",            L"toggle switch"        },
  {FLUX_CONTROL_SLIDER,              L"Slider",                  NULL                    },
  {FLUX_CONTROL_TEXT_INPUT,          L"TextBox",                 NULL                    },
  {FLUX_CONTROL_SCROLL,              L"ScrollViewer",            NULL                    },
  {FLUX_CONTROL_IMAGE,               L"Image",                   NULL                    },
  {FLUX_CONTROL_PROGRESS,            MUXC L"ProgressBar",        NULL                    },
  {FLUX_CONTROL_PROGRESS_RING,       MUXC L"ProgressRing",       L"ProgressRing"         },
  {FLUX_CONTROL_LIST,                L"ListView",                NULL                    },
  {FLUX_CONTROL_LIST_BOX,            L"ListBox",                 NULL                    },
  {FLUX_CONTROL_GRID_VIEW,           L"GridView",                NULL                    },
  {FLUX_CONTROL_PASSWORD_BOX,        L"PasswordBox",             NULL                    },
  {FLUX_CONTROL_NUMBER_BOX,          MUXC L"NumberBox",          NULL                    },
  {FLUX_CONTROL_HYPERLINK,           L"Hyperlink",               NULL                    },
  {FLUX_CONTROL_REPEAT_BUTTON,       L"RepeatButton",            NULL                    },
  {FLUX_CONTROL_TOOLTIP,             L"ToolTip",                 NULL                    },
  {FLUX_CONTROL_FLYOUT,              L"Flyout",                  NULL                    },
  {FLUX_CONTROL_MENU_FLYOUT,         L"MenuFlyout",              NULL                    },
  {FLUX_CONTROL_MENU_PRESENTER,      L"MenuFlyout",              NULL                    },
  {FLUX_CONTROL_DROPDOWN_BUTTON,     MUXC L"DropDownButton",     NULL                    },
  {FLUX_CONTROL_SPLIT_BUTTON,        MUXC L"SplitButton",        NULL                    },
  {FLUX_CONTROL_TOGGLE_SPLIT_BUTTON, MUXC L"ToggleSplitButton",  NULL                    },
  {FLUX_CONTROL_COMBO_BOX,           L"ComboBox",                NULL                    },
  {FLUX_CONTROL_COMBO_ITEM,          L"ComboBoxItem",            NULL                    },
  {FLUX_CONTROL_EXPANDER,            MUXC L"Expander",           NULL                    },
  {FLUX_CONTROL_INFO_BAR,            MUXC L"InfoBar",            NULL                    },
  {FLUX_CONTROL_CONTENT_DIALOG,      L"Popup",                   NULL                    },
  {FLUX_CONTROL_MENU_BAR_ITEM,       MUXC L"MenuBarItem",        NULL                    },
  {FLUX_CONTROL_NAV_VIEW_ITEM,       MUXC L"NavigationViewItem", NULL                    },
  {FLUX_CONTROL_TAB_VIEW,            MUXC L"TabView",            NULL                    },
  {FLUX_CONTROL_TAB_VIEW_ITEM,       MUXC L"TabViewItem",        NULL                    },
  {FLUX_CONTROL_ITEMS_VIEW,          MUXC L"ItemsView",          NULL                    },
  {FLUX_CONTROL_FLIP_VIEW,           L"FlipView",                NULL                    },
  {FLUX_CONTROL_PIPS_PAGER,          MUXC L"PipsPager",          NULL                    },
  {FLUX_CONTROL_AUTO_SUGGEST,        L"AutoSuggestBox",          NULL                    },
  {FLUX_CONTROL_SUGGEST_ITEM,        L"ListViewItem",            NULL                    },
  {FLUX_CONTROL_RADIO_BUTTONS,       MUXC L"RadioButtons",       NULL                    },
  {FLUX_CONTROL_RATING,              NULL,                       L"Rating Slider"        },
  {FLUX_CONTROL_SELECTOR_BAR,        MUXC L"ItemsView",          NULL                    },
  {FLUX_CONTROL_SELECTOR_BAR_ITEM,   MUXC L"SelectorBarItem",    L"SelectorBarItem"      },
  {FLUX_CONTROL_BREADCRUMB_ITEM,     MUXC L"BreadcrumbBarItem",  L"breadcrumb bar item"  },
  {FLUX_CONTROL_TEACHING_TIP,        MUXC L"TeachingTip",        NULL                    },
  {FLUX_CONTROL_TREE_VIEW,           L"ListView",                NULL                    },
  {FLUX_CONTROL_TREE_ITEM,           MUXC L"TreeViewItem",       NULL                    },
  {FLUX_CONTROL_PERSON_PICTURE,      MUXC L"PersonPicture",      NULL                    },
  {FLUX_CONTROL_PAGER,               MUXC L"PagerControl",       NULL                    },
  {FLUX_CONTROL_SPLIT_VIEW_PANE,     L"SplitViewPane",           NULL                    },
  {FLUX_CONTROL_TITLE_BAR,           MUXC L"TitleBar",           NULL                    },
  {FLUX_CONTROL_SCROLL_BAR,          L"ScrollBar",               NULL                    },
  {FLUX_CONTROL_LOOPING_SELECTOR,    L"LoopingSelector",         NULL                    },
  {FLUX_CONTROL_LOOPING_ITEM,        L"LoopingSelectorItem",     NULL                    },
  {FLUX_CONTROL_DATE_PICKER,         L"DatePicker",              NULL                    },
  {FLUX_CONTROL_TIME_PICKER,         L"TimePicker",              NULL                    },
  {FLUX_CONTROL_CALENDAR_VIEW,       L"CalendarView",            NULL                    },
  {FLUX_CONTROL_CALENDAR_DAY,        L"CalendarViewDayItem",     NULL                    },
  {FLUX_CONTROL_COLOR_SPECTRUM,      MUXC L"ColorSpectrum",      L"2D slider"            },
  {FLUX_CONTROL_COLOR_SLIDER,        L"Slider",                  NULL                    },
  {FLUX_CONTROL_COMMANDBAR,          L"ApplicationBar",          NULL                    },
  {FLUX_CONTROL_APPBARBUTTON,        L"AppBarButton",            L"app bar button"       },
  {FLUX_CONTROL_APPBARTOGGLE,        L"AppBarToggleButton",      L"app bar toggle button"},
  {FLUX_CONTROL_CMDMORE,             L"Button",                  NULL                    },
};

static UiaKindRow const *uia_kind_row(FluxControlType type) {
	for (size_t i = 0; i < sizeof(kUiaKinds) / sizeof(kUiaKinds [0]); i++)
		if (kUiaKinds [i].type == type) return &kUiaKinds [i];
	return NULL;
}

static FluxControlType uia_items_owner(FluxUiaContext const *info, XentNodeId node) {
	for (XentNodeId p = xent_node_parent(info->ctx, node); p != XENT_NODE_INVALID; p = xent_node_parent(info->ctx, p)) {
		FluxControlType t = flux_node_type(info->store, p);
		if (t == FLUX_CONTROL_LIST || t == FLUX_CONTROL_LIST_BOX || t == FLUX_CONTROL_GRID_VIEW) return t;
		if (t == FLUX_CONTROL_TREE_VIEW || t == FLUX_CONTROL_TAB_VIEW || t == FLUX_CONTROL_ITEMS_VIEW) return t;
	}
	return FLUX_CONTROL_LIST;
}

static wchar_t const *uia_item_class(FluxControlType owner) {
	switch (owner) {
	case FLUX_CONTROL_LIST_BOX   : return L"ListBoxItem";
	case FLUX_CONTROL_GRID_VIEW  : return L"GridViewItem";
	case FLUX_CONTROL_TREE_VIEW  : return MUXC L"TreeViewItem";
	case FLUX_CONTROL_TAB_VIEW   : return MUXC L"TabViewItem";
	case FLUX_CONTROL_ITEMS_VIEW : return MUXC L"ItemContainer";
	default                      : return L"ListViewItem";
	}
}

static wchar_t const *uia_popup_class(FluxUiaContext const *info, XentNodeId node) {
	XentNodeId top = node;
	for (XentNodeId p = node; p != XENT_NODE_INVALID; p = xent_node_parent(info->ctx, p)) top = p;
	FluxNodeData const *nd    = flux_store_find(info->store, top);
	FluxControlType     owner = nd && nd->popup ? flux_node_type(info->store, nd->uia_owner) : FLUX_CONTROL_CONTAINER;
	if (owner == FLUX_CONTROL_TIME_PICKER) return L"TimePickerFlyoutPresenter";
	return owner == FLUX_CONTROL_DATE_PICKER ? L"DatePickerFlyoutPresenter" : NULL;
}

static wchar_t const *uia_sub_class(FluxUiaContext const *info, XentNodeId node, int sub, int role) {
	FluxControlType t = flux_node_type(info->store, node);
	if (t == FLUX_CONTROL_SLIDER) return L"Thumb";
	if (t == FLUX_CONTROL_LOOPING_SELECTOR) return L"LoopingSelectorItem";
	if (t == FLUX_CONTROL_CALENDAR_VIEW && sub >= FLUX_UIA_SUB_HEAD)
		return sub < FLUX_UIA_SUB_LONE ? L"TextBlock" : NULL;
	if (t == FLUX_CONTROL_CALENDAR_VIEW)
		return role == FLUX_UIA_DATAITEM ? L"CalendarViewDayItem" : L"CalendarViewItem";
	return uia_item_class(t);
}

static wchar_t const *uia_app_class(FluxUiaContext const *info, XentNodeId node) {
	int32_t landmark = 0;
	if (flux_uia_appnum(info->store, node, FLUX_A11Y_LANDMARK, &landmark) && landmark > 0) return L"LandmarkTarget";
	if (flux_uia_apptext(info->store, node, FLUX_A11Y_LANDMARK_NAME)) return L"LandmarkTarget";
	return flux_uia_apptext(info->store, node, FLUX_A11Y_NAME) ? L"NamedContainerAutomationPeer" : NULL;
}

wchar_t const *flux_uia_class(FluxUiaContext const *info, XentNodeId node, int sub, int role) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	if (sub >= 0) return uia_sub_class(info, node, sub, role);
	if (h && h->klass) return h->klass;
	FluxControlType t = flux_node_type(info->store, node);
	if (t == FLUX_CONTROL_LIST_ITEM) return uia_item_class(uia_items_owner(info, node));
	if (t == FLUX_CONTROL_GROUP_HEADER)
		return uia_items_owner(info, node) == FLUX_CONTROL_GRID_VIEW ? L"GridViewHeaderItem" : L"ListViewHeaderItem";
	if (t == FLUX_CONTROL_PICKER_FLYOUT) return uia_popup_class(info, node);
	UiaKindRow const *row = uia_kind_row(t);
	return row ? row->klass : uia_app_class(info, node);
}

wchar_t const *flux_uia_lctype(FluxUiaContext const *info, XentNodeId node, int sub) {
	FluxUiaHooks const *h = flux_module_uia(info->store, node);
	if (sub >= 0) return NULL;
	if (h && h->klass) return h->lctype;
	FluxControlType t = flux_node_type(info->store, node);
	if (t == FLUX_CONTROL_LIST_ITEM && uia_items_owner(info, node) == FLUX_CONTROL_ITEMS_VIEW) return L"ItemContainer";
	UiaKindRow const *row = uia_kind_row(t);
	return row ? row->lctype : NULL;
}
