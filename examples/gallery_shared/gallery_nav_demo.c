#include "gallery_model.h"

char const *const    kNavDemoModes [GALLERY_NAV_DEMO_MODES]     = {"LeftCompact", "LeftMinimal", "Top", "Left", "Auto"};
XtkNavPaneMode const kNavDemoPaneModes [GALLERY_NAV_DEMO_MODES] = {
  XTK_NAV_PANE_LEFT_COMPACT,
  XTK_NAV_PANE_LEFT_MINIMAL,
  XTK_NAV_PANE_TOP,
  XTK_NAV_PANE_LEFT,
  XTK_NAV_PANE_AUTO,
};

XtkNavItemDesc const kNavDemoItems [GALLERY_NAV_DEMO_ITEMS] = {
  {.kind = XTK_NAV_HEADER, .label = "Tools"},
  {.kind = XTK_NAV_ITEM, .icon = "Home", .label = "Home"},
  {.kind = XTK_NAV_ITEM, .icon = "Folder", .label = "Documents"},
  {.kind = XTK_NAV_ITEM, .icon = "Favorite", .label = "Favorites"},
  {.kind = XTK_NAV_FOOTER_HEADER, .label = "Info"},
  {.kind = XTK_NAV_FOOTER, .icon = "Help", .label = "Help"},
};

char const *const kNavDemoSuggestions [2] = {"Documents", "Favorites"};

char const       *gallery_nav_demo_title(int selected) {
	if (selected == XTK_NAV_SETTINGS) return "Settings";
	if (( unsigned ) selected < GALLERY_NAV_DEMO_ITEMS) return kNavDemoItems [selected].label;
	return "None";
}
