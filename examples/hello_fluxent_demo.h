#ifndef HELLO_FLUXENT_DEMO_H
#define HELLO_FLUXENT_DEMO_H

#include "fluxent/flux_node_store.h"
#include "fluxent/fluxent.h"
#include <xent/xent.h>
#include <stdbool.h>

/** @brief Default main window width for hello_fluxent showcase apps. */
#define HELLO_FLUXENT_DEMO_WINDOW_W         640
/** @brief Default main window height for hello_fluxent showcase apps. */
#define HELLO_FLUXENT_DEMO_WINDOW_H         760
/** @brief Node store / Xent arena initial capacity for hello_fluxent demos. */
#define HELLO_FLUXENT_DEMO_STORE_CAPACITY   512
/** @brief Scroll content height matching the static demo column layout extent. */
#define HELLO_FLUXENT_DEMO_SCROLL_CONTENT_H 2200
/** @brief Scroll content width used to size the scroll viewport model. */
#define HELLO_FLUXENT_DEMO_SCROLL_CONTENT_W HELLO_FLUXENT_DEMO_WINDOW_W

#define RADIO_GROUP_MAX                     8

typedef struct RadioGroup {
	FluxNodeStore *store;
	XentNodeId     nodes [RADIO_GROUP_MAX];
	int            count;
} RadioGroup;

typedef struct RadioGroupMember {
	RadioGroup *group;
	int         index;
} RadioGroupMember;

typedef struct CounterState {
	int            count;
	XentNodeId     label_node;
	FluxNodeStore *store;
	char           buf [64];
} CounterState;

typedef struct RepeatCounterState {
	int            count;
	XentNodeId     label_node;
	FluxNodeStore *store;
	char           buf [64];
} RepeatCounterState;

typedef struct ToggleState {
	bool           toggled;
	XentNodeId     node;
	FluxNodeStore *store;
	XentContext   *ctx;
} ToggleState;

typedef struct Demo {
	XentContext        *ctx;
	FluxNodeStore      *store;
	FluxApp            *app;
	XentNodeId          scroll_root;
	XentNodeId          root;
	FluxNodeData       *scroll_nd;
	XentNodeId          footer_div;
	XentNodeId          footer;
	XentNodeId          spacer;
	CounterState       *counter;
	RepeatCounterState *repeat;
	ToggleState        *toggle;
	RadioGroup         *radio;
	RadioGroupMember   *members;
	FluxFlyout         *flyout;
	FluxMenuFlyout     *menu;
} Demo;

bool       demo_init(Demo *d);
XentNodeId make_row(XentContext *ctx, XentNodeId parent, float gap, float h);
XentNodeId make_section(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *text);
void       add_divider(Demo *d);
XentNodeId demo_button(Demo *d, XentNodeId parent, char const *label, void (*on_click)(void *), void *userdata);
XentNodeId demo_slider(
  Demo *d, XentNodeId parent, float min, float max, float value, void (*on_change)(void *, float), void *userdata
);
XentNodeId demo_progress(Demo *d, XentNodeId parent, float value, float max_value);
XentNodeId demo_progress_ring(Demo *d, XentNodeId parent, float value, float max_value);
XentNodeId demo_badge(Demo *d, XentNodeId parent, FluxInfoBadgeMode mode, int32_t value);
XentNodeId
demo_create_text(XentContext *ctx, FluxNodeStore *store, XentNodeId parent, char const *content, float font_size);
XentNodeId demo_create_card(XentContext *ctx, FluxNodeStore *store, XentNodeId parent);
XentNodeId demo_create_divider(XentContext *ctx, FluxNodeStore *store, XentNodeId parent);
void       demo_make_scroll_root(Demo *d);
void       demo_make_root(Demo *d);
void       demo_add_title(Demo *d);
void       demo_add_buttons(Demo *d);
void       demo_add_toggle(Demo *d);
void       demo_add_repeat(Demo *d);
void       demo_add_hyperlinks(Demo *d);
void       demo_add_check_switch(Demo *d);
void       demo_add_radio(Demo *d);
void       demo_build_static_content(Demo *d);
bool       demo_create_app(Demo *d);
void       demo_add_dynamic_content(Demo *d);
void       demo_destroy(Demo *d);

#endif
