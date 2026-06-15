#include "gallery.h"

FxNavItemDesc const kNavItems [PAGE_COUNT] = {
  [PAGE_HOME]        = {FX_NAV_ITEM,      "Home",     "Home"             },
  [PAGE_SEP_1]       = {FX_NAV_SEPARATOR, NULL,       NULL               },
  [PAGE_CAT_BASIC]   = {FX_NAV_ITEM,      "Keyboard", "Basic input"      },
  [PAGE_BUTTON]      = {FX_NAV_CHILD,     NULL,       "Button"           },
  [PAGE_SPLIT]       = {FX_NAV_CHILD,     NULL,       "SplitButton"      },
  [PAGE_CHECKBOX]    = {FX_NAV_CHILD,     NULL,       "CheckBox"         },
  [PAGE_RADIO]       = {FX_NAV_CHILD,     NULL,       "RadioButton"      },
  [PAGE_COMBO]       = {FX_NAV_CHILD,     NULL,       "ComboBox"         },
  [PAGE_SLIDER]      = {FX_NAV_CHILD,     NULL,       "Slider"           },
  [PAGE_TOGGLE]      = {FX_NAV_CHILD,     NULL,       "ToggleSwitch"     },
  [PAGE_CAT_TEXT]    = {FX_NAV_ITEM,      "Font",     "Text"             },
  [PAGE_TEXTBOX]     = {FX_NAV_CHILD,     NULL,       "TextBox"          },
  [PAGE_NUMBERBOX]   = {FX_NAV_CHILD,     NULL,       "NumberBox"        },
  [PAGE_TYPOGRAPHY]  = {FX_NAV_CHILD,     NULL,       "Typography"       },
  [PAGE_CAT_STATUS]  = {FX_NAV_ITEM,      "View",     "Status & info"    },
  [PAGE_INFOBAR]     = {FX_NAV_CHILD,     NULL,       "InfoBar"          },
  [PAGE_BADGE]       = {FX_NAV_CHILD,     NULL,       "InfoBadge"        },
  [PAGE_PROGRESS]    = {FX_NAV_CHILD,     NULL,       "Progress"         },
  [PAGE_TOOLTIP]     = {FX_NAV_CHILD,     NULL,       "ToolTip"          },
  [PAGE_CAT_MEDIA]   = {FX_NAV_ITEM,      "Camera",   "Media"            },
  [PAGE_IMAGE]       = {FX_NAV_CHILD,     NULL,       "Image"            },
  [PAGE_CAT_MENUS]   = {FX_NAV_ITEM,      "More",     "Menus & layout"   },
  [PAGE_MENUS]       = {FX_NAV_CHILD,     NULL,       "Menus & links"    },
  [PAGE_TABVIEW]     = {FX_NAV_CHILD,     NULL,       "TabView"          },
  [PAGE_EXPANDER]    = {FX_NAV_CHILD,     NULL,       "Expander"         },
  [PAGE_CAT_DIALOGS] = {FX_NAV_ITEM,      "Flag",     "Dialogs & flyouts"},
  [PAGE_DIALOG]      = {FX_NAV_CHILD,     NULL,       "ContentDialog"    },
  [PAGE_SETTINGS]    = {FX_NAV_FOOTER,    "Settings", "Settings"         },
};

FluxEl *page_header(FxUi *ui, char const *title, char const *blurb) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 8},
	  (FluxEl *[]) {
		fx_text(ui, title, (FxTextDesc) {.size = 28, .weight = FLUX_FONT_SEMI_BOLD}),
		fx_text(ui, blurb, (FxTextDesc) {.size = 13}), FX_END}
	);
}

FluxEl *example(FxUi *ui, char const *caption, FluxEl *content) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 8, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {fx_text(ui, caption, (FxTextDesc) {.size = 14}), content, FX_END}
	);
}

FluxEl *output_col(FxUi *ui, char const *value) {
	return fx_sized(
	  fx_column(
		ui, (FxStackDesc) {.gap = 4},
		(FluxEl *[]) {
		  fx_text(ui, "Output:", (FxTextDesc) {.size = 12, .weight = FLUX_FONT_SEMI_BOLD}),
		  fx_text(ui, value, (FxTextDesc) {.size = 14}), FX_END}
	  ),
	  260, NAN
	);
}

FluxEl *demo_card(FxUi *ui, FluxEl *control, char const *output, FluxEl *extra) {
	return fx_card(
	  ui, (FxStackDesc) {.align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		fx_row(
		  ui, (FxStackDesc) {.gap = 16, .align = XENT_FLEX_ALIGN_CENTER, .fill = true},
		  (FluxEl *[]) {control, fx_spacer(ui), output ? output_col(ui, output) : NULL, extra, FX_END}
		),
		FX_END}
	);
}

FluxEl *page_category(FxUi *ui, char const *title, char const *blurb) {
	return fx_column(
	  ui, (FxStackDesc) {.gap = 20, .align = XENT_FLEX_ALIGN_STRETCH},
	  (FluxEl *[]) {
		page_header(ui, title, blurb),
		fx_text(ui, "Expand this group in the navigation pane to pick a control.", (FxTextDesc) {.size = 13}), FX_END}
	);
}
