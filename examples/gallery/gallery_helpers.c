#include "gallery.h"

XtkNavItemDesc const kNavItems [PAGE_COUNT] = {
  [PAGE_HOME]        = {XTK_NAV_ITEM,      "Home",     "Home"             },
  [PAGE_SEP_1]       = {XTK_NAV_SEPARATOR, NULL,       NULL               },
  [PAGE_CAT_BASIC]   = {XTK_NAV_ITEM,      "Keyboard", "Basic input"      },
  [PAGE_BUTTON]      = {XTK_NAV_CHILD,     NULL,       "Button"           },
  [PAGE_SPLIT]       = {XTK_NAV_CHILD,     NULL,       "SplitButton"      },
  [PAGE_CHECKBOX]    = {XTK_NAV_CHILD,     NULL,       "CheckBox"         },
  [PAGE_RADIO]       = {XTK_NAV_CHILD,     NULL,       "RadioButton"      },
  [PAGE_COMBO]       = {XTK_NAV_CHILD,     NULL,       "ComboBox"         },
  [PAGE_SLIDER]      = {XTK_NAV_CHILD,     NULL,       "Slider"           },
  [PAGE_TOGGLE]      = {XTK_NAV_CHILD,     NULL,       "ToggleSwitch"     },
  [PAGE_CAT_TEXT]    = {XTK_NAV_ITEM,      "Font",     "Text"             },
  [PAGE_TEXTBOX]     = {XTK_NAV_CHILD,     NULL,       "TextBox"          },
  [PAGE_NUMBERBOX]   = {XTK_NAV_CHILD,     NULL,       "NumberBox"        },
  [PAGE_TYPOGRAPHY]  = {XTK_NAV_CHILD,     NULL,       "Typography"       },
  [PAGE_CAT_STATUS]  = {XTK_NAV_ITEM,      "View",     "Status & info"    },
  [PAGE_INFOBAR]     = {XTK_NAV_CHILD,     NULL,       "InfoBar"          },
  [PAGE_BADGE]       = {XTK_NAV_CHILD,     NULL,       "InfoBadge"        },
  [PAGE_PROGRESS]    = {XTK_NAV_CHILD,     NULL,       "Progress"         },
  [PAGE_TOOLTIP]     = {XTK_NAV_CHILD,     NULL,       "ToolTip"          },
  [PAGE_CAT_MEDIA]   = {XTK_NAV_ITEM,      "Camera",   "Media"            },
  [PAGE_IMAGE]       = {XTK_NAV_CHILD,     NULL,       "Image"            },
  [PAGE_CAT_MENUS]   = {XTK_NAV_ITEM,      "More",     "Menus & layout"   },
  [PAGE_MENUS]       = {XTK_NAV_CHILD,     NULL,       "Menus & links"    },
  [PAGE_TABVIEW]     = {XTK_NAV_CHILD,     NULL,       "TabView"          },
  [PAGE_EXPANDER]    = {XTK_NAV_CHILD,     NULL,       "Expander"         },
  [PAGE_LISTVIEW]    = {XTK_NAV_CHILD,     NULL,       "ListView"         },
  [PAGE_CAT_DIALOGS] = {XTK_NAV_ITEM,      "Flag",     "Dialogs & flyouts"},
  [PAGE_DIALOG]      = {XTK_NAV_CHILD,     NULL,       "ContentDialog"    },
  [PAGE_SETTINGS]    = {XTK_NAV_FOOTER,    "Settings", "Settings"         },
};

XtkEl *page_header(XtkUi *ui, char const *title, char const *blurb) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8},
	  (XtkEl *[]) {
		xtk_text(ui, title, (XtkTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}),
		xtk_text(ui, blurb, (XtkTextDesc) {.size = 13}), XTK_END}
	);
}

XtkEl *example(XtkUi *ui, char const *caption, XtkEl *content) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {xtk_text(ui, caption, (XtkTextDesc) {.size = 14}), content, XTK_END}
	);
}

XtkEl *output_col(XtkUi *ui, char const *value) {
	return xtk_sized(
	  xtk_column(
		ui, (XtkStackDesc) {.gap = 4},
		(XtkEl *[]) {
		  xtk_text(ui, "Output:", (XtkTextDesc) {.size = 12, .weight = FLUX_FONT_SEMI_BOLD}),
		  xtk_text(ui, value, (XtkTextDesc) {.size = 14}), XTK_END}
	  ),
	  260, NAN
	);
}

XtkEl *demo_card(XtkUi *ui, XtkEl *control, char const *output, XtkEl *extra) {
	return xtk_card(
	  ui, (XtkStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		xtk_row(
		  ui, (XtkStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER, .fill = true},
		  (XtkEl *[]) {control, xtk_spacer(ui), output ? output_col(ui, output) : NULL, extra, XTK_END}
		),
		XTK_END}
	);
}

XtkEl *page_category(XtkUi *ui, char const *title, char const *blurb) {
	return xtk_column(
	  ui, (XtkStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (XtkEl *[]) {
		page_header(ui, title, blurb),
		xtk_text(ui, "Expand this group in the navigation pane to pick a control.", (XtkTextDesc) {.size = 13}), XTK_END}
	);
}
