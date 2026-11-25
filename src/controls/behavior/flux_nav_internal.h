#ifndef FLUX_NAV_INTERNAL_H
#define FLUX_NAV_INTERNAL_H

#include "controls/data/flux_nav_view_data.h"

#define FLUX_NAV_TOP_MORE_W     40.0f
#define FLUX_NAV_TOP_GRID_PAD   4.0f
#define FLUX_NAV_TOP_CUSTOM_MIN 48.0f
#define FLUX_NAV_OVF_ROW_H      36.0f
// SplitView PaneRoot applies NavigationViewBorderThickness (1), cancelling PaneContentGrid's -1,3 horizontally
#define FLUX_NAV_GRID_MARGIN_Y  4.0f

void                  flux_nav_repaint(FluxNavViewData *d);
void                  flux_nav_anim_start(FluxNavViewData *d);
void                  flux_nav_tween_snap(FluxNavTween *tw, float v);
bool                  flux_nav_closed_compact(FluxNavViewData const *d);
bool                  flux_nav_pane_overlay(FluxNavViewData const *d);
bool                  flux_nav_flyout_repeater(FluxNavViewData const *d, FluxNavViewItem const *it);
float                 flux_nav_host_full_h(FluxNavViewData const *d, int parent);
float                 flux_nav_text_w(FluxNavViewData const *d, char const *text, uint16_t weight);
void                  flux_nav_sync_hosts(FluxNavViewData *d);
void                  flux_nav_home_host(FluxNavViewData *d, FluxNavViewItem *it);
void                  flux_nav_graft_host(FluxNavViewData *d, FluxNavViewItem *it, bool on);
void                  flux_nav_place(FluxNavViewData *d, XentNodeId parent, XentNodeId node, XentNodeId before);
void                  flux_nav_apply_selection(FluxNavViewData *d, int index, bool notify);
void                  flux_nav_refresh_items(FluxNavViewData *d);
uint8_t               flux_nav_draw_depth(FluxNavViewData const *d, FluxNavViewItem const *it);
bool                  flux_nav_shows_label(FluxNavViewData const *d, FluxNavViewItem const *it);
FluxRect              flux_nav_indicator_rect(FluxRect row, uint8_t place, uint8_t depth, bool in_flyout);
FluxNavIndicatorFrame flux_nav_indicator_frame(FluxNavViewItem const *it, bool rtl);

bool                  flux_nav_fly_ensure(FluxNavViewData *d);
void                  flux_nav_fly_open(FluxNavViewData *d, int owner);
void                  flux_nav_fly_close(FluxNavViewData *d);
void                  flux_nav_fly_refresh(FluxNavViewData *d);
void                  flux_nav_fly_destroy(FluxNavViewData *d);
bool                  flux_nav_fly_shows(FluxNavViewData const *d, FluxNavViewItem const *it);
bool                  flux_nav_fly_key(FluxNavViewItem *it, FluxKey key);

void                  flux_nav_top_setup(FluxNavViewData *d);
void                  flux_nav_top_layout(FluxNavViewData *d, bool top);
void                  flux_nav_top_arrange(FluxNavViewData *d);
void                  flux_nav_top_bring(FluxNavViewData *d, int index);
float                 flux_nav_top_item_w(FluxNavViewData const *d, FluxNavViewItem *it);
bool                  flux_nav_top_key(void *ctx, FluxKey key, bool down);

FluxNavViewData      *flux_nav_data(FluxStore *store, XentNodeId nav);
void                  flux_nav_settings_setup(FluxNavViewData *d);
void                  flux_nav_set_settings_visible(FluxStore *store, XentNodeId nav, bool visible);
void                  flux_nav_set_always_show_header(FluxStore *store, XentNodeId nav, bool always);
void                  flux_nav_set_back_button(FluxStore *store, XentNodeId nav, uint32_t visibility);
void                  flux_nav_set_back_enabled(FluxStore *store, XentNodeId nav, bool enabled);
float                 flux_nav_overlay_button_width(FluxNavViewData const *d);
float                 flux_nav_top_back_width(FluxNavViewData const *d);
void                  flux_nav_set_pane_title(FluxStore *store, XentNodeId nav, char const *title);
void                  flux_nav_open_for_search(FluxNavViewData *d);
void                  flux_nav_on_query(FluxStore *store, XentNodeId origin, int chosen);

void                  flux_nav_chrome_setup(FluxNavViewData *d);
void                  flux_nav_chrome_sync(FluxNavViewData *d);
bool                  flux_nav_chrome_sample(FluxNavViewData *d);
float                 flux_nav_chrome_width(FluxNavViewData const *d);
XentNodeId            flux_nav_slot(FluxStore *store, XentNodeId nav, uint32_t slot);
void                  flux_nav_slot_changed(FluxStore *store, XentNodeId node);

#endif
