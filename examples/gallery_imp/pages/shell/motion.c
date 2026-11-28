#include "app.h"

#include <fluxent/flux_theme.h>

#include <stdio.h>
#include <string.h>

enum
{
	EX_IMP_TARGET  = 0,
	EX_IMP_MASKS   = 1,
	EX_IMP_STATES  = 7,
	EX_ITEMS_ROW   = 10,
	EX_ITEMS       = 11,
	EX_THEME_KINDS = 19,
	EX_FRAME       = 24,
	EX_SUBPAGE     = 25,
	EX_FRAME_KINDS = 26,
	EX_CONN_HOST   = 32,
	EX_CONN_BODY   = 33,
	EX_CONN_TOGGLE = 34,
	EX_CONN_HERO   = 35,
	EX_CONN_KINDS  = 36,
	EX_RED_KINDS   = 39,
};

typedef struct MotionImplicitWire {
	uint32_t mask;
	float    opacity_dur;
	float    transform_dur;
	float    brush_dur;
} MotionImplicitWire;

typedef struct ConnectedCfgWire {
	uint32_t kind;
	uint32_t shadow;
	float    dip_x;
	float    dip_y;
	float    duration;
} ConnectedCfgWire;

typedef struct MotionPageState {
	int      imp_step;
	uint32_t imp_mask;
	int      theme_kind;
	int      item_ids [8];
	int      item_count;
	int      frame_page;
	int      frame_kind;
	bool     frame_back;
	int      conn_kind;
	bool     conn_detail;
	int      reduced_override;
} MotionPageState;

typedef struct ImpStyleTarget {
	XtkColor bg;
	XtkColor border;
	float    radius;
	float    opacity;
	float    tx;
	float    ty;
	float    sx;
	float    sy;
	float    rot;
} ImpStyleTarget;

static uint32_t const kImpMasks [6] = {
  XTK_MOTION_OPACITY,
  XTK_MOTION_TRANSLATE,
  XTK_MOTION_SCALE,
  XTK_MOTION_ROTATION,
  XTK_MOTION_BACKGROUND | XTK_MOTION_BORDER_COLOR,
  XTK_MOTION_CORNER_RADIUS,
};

static char const *const kImpMaskLabels [6] = {"Opacity", "Translate", "Scale", "Rotation", "Brush", "Radius"};
static uint32_t const    kImpMaskChecks [6] = {
  XTK_MOTION_OPACITY,
  XTK_MOTION_TRANSLATE_X,
  XTK_MOTION_SCALE_X,
  XTK_MOTION_ROTATION,
  XTK_MOTION_BACKGROUND,
  XTK_MOTION_CORNER_RADIUS,
};
static char const *const kImpStateLabels [3] = {"State A", "State B", "State C"};

static int const         kThemeKinds [5]     = {
  XTK_THEME_TRANSITION_ENTRANCE,
  XTK_THEME_TRANSITION_ADD_DELETE,
  XTK_THEME_TRANSITION_CONTENT,
  XTK_THEME_TRANSITION_REORDER,
  XTK_THEME_TRANSITION_POPUP,
};
static char const *const kThemeLabels [5] = {"Entrance", "AddDelete", "Content", "Reorder", "Popup"};

static int const         kFrameKinds [6]  = {
  XTK_NAV_TRANSITION_ENTRANCE,
  XTK_NAV_TRANSITION_SLIDE_RIGHT,
  XTK_NAV_TRANSITION_SLIDE_LEFT,
  XTK_NAV_TRANSITION_SLIDE_BOTTOM,
  XTK_NAV_TRANSITION_DRILL_IN,
  XTK_NAV_TRANSITION_SUPPRESS,
};
static char const *const kFrameLabels [6]
  = {"Entrance", "SlideRight", "SlideLeft", "SlideBottom", "DrillIn", "Suppress"};

static int const         kConnKinds [3]     = {XTK_CONNECTED_GRAVITY, XTK_CONNECTED_DIRECT, XTK_CONNECTED_BASIC};
static char const *const kConnLabels [3]    = {"Gravity", "Direct", "Basic"};

static int const         kReducedModes [3]  = {-1, 0, 1};
static char const *const kReducedLabels [3] = {"System", "Force Off", "Force On"};

static XtkCtrl           find_result_ctrl(XtkBatchResult const *res, XtkLocalId local) {
	if (!res || local == XTK_LOCAL_INVALID) return XTK_CTRL_INVALID;
	for (uint32_t i = 0; i < res->control_count; i++)
		if (res->controls [i].local == local) return res->controls [i].control;
	return XTK_CTRL_INVALID;
}

static void txn_enum(XtkTxn *t, XtkNodeRef r, XtkPropertyId id, uint32_t v) {
	xtk_txn_set(t, r, id, (XtkValue) {.kind = XTK_VALUE_ENUM, .u.u32 = v}, (XtkChangeStamp) {0});
}

static void txn_bool(XtkTxn *t, XtkNodeRef r, XtkPropertyId id, bool v) {
	xtk_txn_set(t, r, id, (XtkValue) {.kind = XTK_VALUE_BOOL, .u.b = v}, (XtkChangeStamp) {0});
}

static void txn_i32(XtkTxn *t, XtkNodeRef r, XtkPropertyId id, int32_t v) {
	xtk_txn_set(t, r, id, (XtkValue) {.kind = XTK_VALUE_I32, .u.i32 = v}, (XtkChangeStamp) {0});
}

static void txn_f32(XtkTxn *t, XtkNodeRef r, XtkPropertyId id, float v) {
	xtk_txn_set(t, r, id, (XtkValue) {.kind = XTK_VALUE_F32, .u.f32 = v}, (XtkChangeStamp) {0});
}

static void txn_color(XtkTxn *t, XtkNodeRef r, XtkPropertyId id, XtkColor v) {
	xtk_txn_set(t, r, id, (XtkValue) {.kind = XTK_VALUE_COLOR, .u.color = v}, (XtkChangeStamp) {0});
}

static void txn_str(XtkTxn *t, XtkNodeRef r, XtkPropertyId id, char const *s) {
	uint32_t len = s ? ( uint32_t ) strlen(s) : 0u;
	xtk_txn_set(
	  t, r, id,
	  (XtkValue) {
		.kind = XTK_VALUE_STRING, .u.string = {.data = ( char * ) s, .len = len}
    },
	  (XtkChangeStamp) {0}
	);
}

static void txn_bytes(XtkTxn *t, XtkNodeRef r, XtkPropertyId id, void const *data, uint32_t len) {
	xtk_txn_set(
	  t, r, id,
	  (XtkValue) {
		.kind = XTK_VALUE_BYTES, .u.bytes = {.data = ( uint8_t * ) ( uintptr_t ) data, .len = len}
    },
	  (XtkChangeStamp) {0}
	);
}

static XtkLocalId pill_btn(PageBuild *b, XtkNodeRef parent, char const *label, bool active, int tag, int i) {
	XtkLocalId btn = ui_button(&b->txn, parent, label, NULL, active ? XTK_BUTTON_ACCENT : XTK_BUTTON_STANDARD);
	xtk_button_oninvoke(&b->txn, xtk_ref_local(btn), page_post, page_bind_i(b, tag, i));
	return btn;
}

static void sync_pills(XtkTxn *t, XtkCtrl const *ctrls, int count, int active_idx) {
	for (int i = 0; i < count; i++)
		txn_enum(
		  t, xtk_ref_existing(ctrls [i]), XTK_PROP_BUTTON_STYLE,
		  i == active_idx ? XTK_BUTTON_ACCENT : XTK_BUTTON_STANDARD
		);
}

static int index_of_i(int const *table, int count, int v) {
	for (int i = 0; i < count; i++)
		if (table [i] == v) return i;
	return -1;
}

static ImpStyleTarget implicit_target_for(int step) {
	if (step == 1) {
		return (ImpStyleTarget) {
		  .bg      = xtk_color_rgba(0xc2, 0x39, 0xb3, 0xcc),
		  .border  = xtk_color_rgb(0xff, 0x8c, 0x00),
		  .radius  = 18.0f,
		  .opacity = 0.72f,
		  .tx      = 110.0f,
		  .sx      = 1.12f,
		  .sy      = 1.12f,
		};
	}
	if (step == 2) {
		return (ImpStyleTarget) {
		  .bg      = xtk_color_rgba(0x00, 0x82, 0x72, 0xd8),
		  .border  = xtk_color_rgb(0x50, 0xe6, 0xff),
		  .radius  = 28.0f,
		  .opacity = 0.95f,
		  .tx      = 210.0f,
		  .ty      = 8.0f,
		  .sx      = 0.92f,
		  .sy      = 0.92f,
		  .rot     = 8.0f,
		};
	}
	return (ImpStyleTarget) {
	  .bg      = xtk_color_rgba(0x00, 0x78, 0xd4, 0xcc),
	  .border  = xtk_color_rgb(0x60, 0xcd, 0xff),
	  .radius  = 8.0f,
	  .opacity = 1.0f,
	  .sx      = 1.0f,
	  .sy      = 1.0f,
	};
}

static void apply_implicit_style(XtkTxn *t, XtkNodeRef r, int step) {
	ImpStyleTarget s = implicit_target_for(step);
	txn_color(t, r, XTK_PROP_STYLE_BG, s.bg);
	txn_color(t, r, XTK_PROP_STYLE_BORDER, s.border);
	txn_f32(t, r, XTK_PROP_STYLE_RADIUS, s.radius);
	txn_f32(t, r, XTK_PROP_STYLE_OPACITY, s.opacity);
	txn_f32(t, r, XTK_PROP_STYLE_TX, s.tx);
	txn_f32(t, r, XTK_PROP_STYLE_TY, s.ty);
	txn_f32(t, r, XTK_PROP_STYLE_SX, s.sx);
	txn_f32(t, r, XTK_PROP_STYLE_SY, s.sy);
	txn_f32(t, r, XTK_PROP_STYLE_ROTATION, s.rot);
}

static void apply_implicit_cfg(XtkTxn *t, XtkNodeRef r, uint32_t mask) {
	MotionImplicitWire wire = {.mask = mask};
	txn_bytes(t, r, XTK_PROP_MOTION_IMPLICIT, &wire, ( uint32_t ) sizeof(wire));
}

static void build_implicit_section(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex = page_example(
	  b, parent, "Implicit Property Transitions (click states rapidly to verify mid-flight retargeting)"
	);
	XtkNodeRef col   = xtk_ref_local(ui_column(&b->txn, ex, 10.0f, XENT_FLEX_ALIGN_STRETCH));
	XtkNodeRef masks = xtk_ref_local(ui_row(&b->txn, col, 6.0f, true));
	for (int i = 0; i < 6; i++) {
		bool active = (m->motion_imp_mask & kImpMaskChecks [i]) != 0u;
		page_keepx(
		  b, EX_IMP_MASKS + i,
		  pill_btn(b, masks, kImpMaskLabels [i], active, MSG_MOTION_IMP_MASK, ( int ) kImpMasks [i])
		);
	}
	XtkNodeRef targets = xtk_ref_local(ui_row(&b->txn, col, 8.0f, false));
	for (int i = 0; i < 3; i++)
		page_keepx(
		  b, EX_IMP_STATES + i,
		  pill_btn(b, targets, kImpStateLabels [i], m->motion_imp_step == i, MSG_MOTION_IMP_STATE, i)
		);

	XtkLocalId card = xtk_card_create(
	  &b->txn, col,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_W_MASK
	           | XTK_CONTAINER_H_MASK
	           | XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_PL_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PR_MASK
	           | XTK_CONTAINER_PB_MASK,
		.w     = 180.0f,
		.h     = 64.0f,
		.dir   = XTK_STACK_COLUMN,
		.items = XENT_FLEX_ALIGN_CENTER,
		.pl    = 14.0f,
		.pt    = 10.0f,
		.pr    = 14.0f,
		.pb    = 10.0f,
	  }
	);
	ui_text(&b->txn, xtk_ref_local(card), "Implicit Target", 13.0f, FLUX_FONT_SEMI_BOLD);
	apply_implicit_cfg(&b->txn, xtk_ref_local(card), m->motion_imp_mask);
	apply_implicit_style(&b->txn, xtk_ref_local(card), m->motion_imp_step);
	page_keepx(b, EX_IMP_TARGET, card);
}

static XtkLocalId create_item_card(XtkTxn *t, XtkNodeRef row, int id, int theme_kind) {
	XtkLocalId card = ui_card(t, row, 0.0f, XENT_FLEX_ALIGN_STRETCH, 8.0f);
	xtk_txn_set(
	  t, xtk_ref_local(card), XTK_PROP_PADDING_LEFT, (XtkValue) {.kind = XTK_VALUE_F32, .u.f32 = 12.0f},
	  (XtkChangeStamp) {0}
	);
	xtk_txn_set(
	  t, xtk_ref_local(card), XTK_PROP_PADDING_RIGHT, (XtkValue) {.kind = XTK_VALUE_F32, .u.f32 = 12.0f},
	  (XtkChangeStamp) {0}
	);
	txn_enum(t, xtk_ref_local(card), XTK_PROP_MOTION_ENTER_EXIT, ( uint32_t ) theme_kind);
	char label [32];
	snprintf(label, sizeof(label), "Item #%d", id);
	ui_text(t, xtk_ref_local(card), label, 13.0f, FLUX_FONT_REGULAR);
	return card;
}

static void build_enter_exit_section(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex
	  = page_example(b, parent, "Enter, Exit & Reorder Theme Transitions (with PVL stagger & unloading ghost storage)");
	XtkNodeRef col   = xtk_ref_local(ui_column(&b->txn, ex, 10.0f, XENT_FLEX_ALIGN_STRETCH));
	XtkNodeRef modes = xtk_ref_local(ui_row(&b->txn, col, 6.0f, true));
	for (int i = 0; i < 5; i++)
		page_keepx(
		  b, EX_THEME_KINDS + i,
		  pill_btn(
			b, modes, kThemeLabels [i], m->motion_theme_kind == kThemeKinds [i], MSG_MOTION_ENTER_KIND, kThemeKinds [i]
		  )
		);
	XtkNodeRef actions = xtk_ref_local(ui_row(&b->txn, col, 8.0f, false));
	page_button(b, actions, "Add item", MSG_MOTION_ITEM_ADD, 0);
	page_button(b, actions, "Remove last", MSG_MOTION_ITEM_REMOVE, 0);
	page_button(b, actions, "Shift order", MSG_MOTION_ITEM_SHUFFLE, 0);

	XtkLocalId row = ui_row(&b->txn, col, 8.0f, true);
	page_keepx(b, EX_ITEMS_ROW, row);
	for (int i = 0; i < m->motion_item_count; i++)
		page_keepx(
		  b, EX_ITEMS + i, create_item_card(&b->txn, xtk_ref_local(row), m->motion_item_ids [i], m->motion_theme_kind)
		);
}

static XtkLocalId create_subpage_card(XtkTxn *t, XtkNodeRef frame, int page_idx, bool is_back) {
	static char const *const kTitles [] = {"Alpha", "Beta", "Gamma", "Delta"};
	XtkLocalId               card       = ui_card(t, frame, 4.0f, XENT_FLEX_ALIGN_STRETCH, 14.0f);
	txn_f32(t, xtk_ref_local(card), XTK_PROP_PADDING_LEFT, 16.0f);
	txn_f32(t, xtk_ref_local(card), XTK_PROP_PADDING_RIGHT, 16.0f);
	char title [32];
	snprintf(title, sizeof(title), "Subpage %s", kTitles [page_idx & 3]);
	ui_text(t, xtk_ref_local(card), title, 16.0f, FLUX_FONT_SEMI_BOLD);
	ui_text(t, xtk_ref_local(card), is_back ? "Direction: Back" : "Direction: Forward", 12.0f, FLUX_FONT_REGULAR);
	return card;
}

static void build_frame_section(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex
	  = page_example(b, parent, "Frame NavigationTransitionInfo (all 6 WinUI transition kinds & Back direction)");
	XtkNodeRef col   = xtk_ref_local(ui_column(&b->txn, ex, 10.0f, XENT_FLEX_ALIGN_STRETCH));
	XtkNodeRef kinds = xtk_ref_local(ui_row(&b->txn, col, 6.0f, true));
	for (int i = 0; i < 6; i++)
		page_keepx(
		  b, EX_FRAME_KINDS + i,
		  pill_btn(
			b, kinds, kFrameLabels [i], m->motion_frame_kind == kFrameKinds [i], MSG_MOTION_FRAME_KIND, kFrameKinds [i]
		  )
		);
	XtkNodeRef nav = xtk_ref_local(ui_row(&b->txn, col, 8.0f, false));
	page_button(b, nav, "Back", MSG_MOTION_FRAME_STEP, -1);
	XtkLocalId next_btn = ui_button(&b->txn, nav, "Next page", NULL, XTK_BUTTON_ACCENT);
	xtk_button_oninvoke(&b->txn, xtk_ref_local(next_btn), page_post, page_bind_i(b, MSG_MOTION_FRAME_STEP, 1));

	int        idx   = m->motion_frame_page & 3;
	XtkLocalId frame = xtk_frame_create(
	  &b->txn, col,
	  &(XtkFrameProps) {
		.set        = XTK_FRAME_W_MASK
	                | XTK_FRAME_H_MASK
	                | XTK_FRAME_TRANSITION_MASK
	                | XTK_FRAME_IS_BACK_MASK
	                | XTK_FRAME_PAGE_KEY_MASK,
		.w          = 480.0f,
		.h          = 90.0f,
		.transition = ( XtkNavTransitionKind ) m->motion_frame_kind,
		.is_back    = m->motion_frame_back,
		.page_key   = idx + 1,
	  }
	);
	page_keepx(b, EX_FRAME, frame);
	page_keepx(b, EX_SUBPAGE, create_subpage_card(&b->txn, xtk_ref_local(frame), idx, m->motion_frame_back));
}

static XtkLocalId create_hero_card(XtkTxn *t, XtkNodeRef parent, int conn_kind, bool detail) {
	XtkLocalId hero = xtk_card_create(
	  t, parent,
	  &(XtkContainerProps) {
		.set = XTK_CONTAINER_W_MASK
	         | XTK_CONTAINER_H_MASK
	         | XTK_CONTAINER_DIR_MASK
	         | XTK_CONTAINER_PL_MASK
	         | XTK_CONTAINER_PT_MASK
	         | XTK_CONTAINER_PR_MASK
	         | XTK_CONTAINER_PB_MASK,
		.w   = detail ? 200.0f : 96.0f,
		.h   = detail ? 100.0f : 64.0f,
		.dir = XTK_STACK_COLUMN,
		.pl  = 12.0f,
		.pt  = 10.0f,
		.pr  = 12.0f,
		.pb  = 10.0f,
	  }
	);
	txn_color(t, xtk_ref_local(hero), XTK_PROP_STYLE_BG, xtk_color_rgba(0x00, 0x78, 0xd4, 0xe0));
	txn_f32(t, xtk_ref_local(hero), XTK_PROP_STYLE_RADIUS, 10.0f);
	ConnectedCfgWire cw = {.kind = ( uint32_t ) conn_kind, .shadow = 1u};
	txn_bytes(t, xtk_ref_local(hero), XTK_PROP_CONNECTED_CONFIG, &cw, ( uint32_t ) sizeof(cw));
	txn_str(t, xtk_ref_local(hero), XTK_PROP_CONNECTED_KEY, "motion-hero");
	ui_text(t, xtk_ref_local(hero), "Hero", 13.0f, FLUX_FONT_SEMI_BOLD);
	return hero;
}

static void
create_conn_body(XtkTxn *t, XtkNodeRef host, int conn_kind, bool detail, XtkLocalId *out_body, XtkLocalId *out_hero) {
	XtkLocalId row = xtk_container_create(
	  t, host,
	  &(XtkContainerProps) {
		.set   = XTK_CONTAINER_DIR_MASK
	           | XTK_CONTAINER_ITEMS_MASK
	           | XTK_CONTAINER_GAP_MASK
	           | XTK_CONTAINER_PT_MASK
	           | XTK_CONTAINER_PB_MASK,
		.dir   = XTK_STACK_ROW,
		.items = XENT_FLEX_ALIGN_CENTER,
		.gap   = detail ? 24.0f : 16.0f,
		.pt    = 18.0f,
		.pb    = 18.0f,
	  }
	);
	if (detail) {
		XtkLocalId coord = xtk_container_create(
		  t, xtk_ref_local(row),
		  &(XtkContainerProps) {
			.set   = XTK_CONTAINER_W_MASK | XTK_CONTAINER_DIR_MASK | XTK_CONTAINER_ITEMS_MASK | XTK_CONTAINER_GAP_MASK,
			.w     = 220.0f,
			.dir   = XTK_STACK_COLUMN,
			.items = XENT_FLEX_ALIGN_STRETCH,
			.gap   = 4.0f,
		  }
		);
		txn_str(t, xtk_ref_local(coord), XTK_PROP_CONNECTED_COORD, "motion-hero");
		ui_text(t, xtk_ref_local(coord), "Coordinated Details", 15.0f, FLUX_FONT_SEMI_BOLD);
		ui_text(t, xtk_ref_local(coord), "Flies along alongside the hero element", 12.0f, FLUX_FONT_REGULAR);
		*out_hero = create_hero_card(t, xtk_ref_local(row), conn_kind, detail);
	}
	else {
		*out_hero = create_hero_card(t, xtk_ref_local(row), conn_kind, detail);
		ui_text(
		  t, xtk_ref_local(row), "Thumbnail state - select a mode and click Expand detail", 12.0f, FLUX_FONT_REGULAR
		);
	}
	*out_body = row;
}

static void build_connected_section(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex
	  = page_example(b, parent, "Connected Animation (Gravity dip & shadow, Direct, Basic + coordinated elements)");
	XtkNodeRef col   = xtk_ref_local(ui_column(&b->txn, ex, 10.0f, XENT_FLEX_ALIGN_STRETCH));
	XtkNodeRef modes = xtk_ref_local(ui_row(&b->txn, col, 6.0f, false));
	for (int i = 0; i < 3; i++)
		page_keepx(
		  b, EX_CONN_KINDS + i,
		  pill_btn(
			b, modes, kConnLabels [i], m->motion_conn_kind == kConnKinds [i], MSG_MOTION_CONN_KIND, kConnKinds [i]
		  )
		);
	XtkLocalId toggle
	  = ui_button(&b->txn, modes, m->motion_conn_detail ? "Collapse" : "Expand detail", NULL, XTK_BUTTON_ACCENT);
	xtk_button_oninvoke(&b->txn, xtk_ref_local(toggle), page_post, page_bind(b, MSG_MOTION_CONN_TOGGLE));
	page_keepx(b, EX_CONN_TOGGLE, toggle);

	XtkLocalId host = ui_column(&b->txn, col, 0.0f, XENT_FLEX_ALIGN_STRETCH);
	page_keepx(b, EX_CONN_HOST, host);
	XtkLocalId body = XTK_LOCAL_INVALID, hero = XTK_LOCAL_INVALID;
	create_conn_body(&b->txn, xtk_ref_local(host), m->motion_conn_kind, m->motion_conn_detail, &body, &hero);
	page_keepx(b, EX_CONN_BODY, body);
	page_keepx(b, EX_CONN_HERO, hero);
}

static char const *reduced_status_text(void) {
	return flux_animations_enabled() ? "Active: Enabled" : "Active: Reduced Motion (Disabled)";
}

static void build_reduced_section(PageBuild *b, XtkNodeRef parent, Model const *m) {
	XtkNodeRef ex  = page_example(b, parent, "Reduced Motion (UISettings.AnimationsEnabled)");
	XtkNodeRef row = xtk_ref_local(ui_row(&b->txn, ex, 8.0f, false));
	for (int i = 0; i < 3; i++)
		page_keepx(
		  b, EX_RED_KINDS + i,
		  pill_btn(
			b, row, kReducedLabels [i], m->motion_reduced_override == kReducedModes [i], MSG_MOTION_REDUCED,
			kReducedModes [i]
		  )
		);
	page_keepout(b, MSG_MOTION_REDUCED, page_output(b, row, reduced_status_text()));
}

static bool is_tail_added(MotionPageState const *st, Model const *m) {
	if (m->motion_item_count != st->item_count + 1) return false;
	for (int i = 0; i < st->item_count; i++)
		if (st->item_ids [i] != m->motion_item_ids [i]) return false;
	return true;
}

static bool is_tail_removed(MotionPageState const *st, Model const *m) {
	if (m->motion_item_count + 1 != st->item_count) return false;
	for (int i = 0; i < m->motion_item_count; i++)
		if (st->item_ids [i] != m->motion_item_ids [i]) return false;
	return true;
}

static bool is_shifted_left(MotionPageState const *st, Model const *m) {
	if (m->motion_item_count != st->item_count || st->item_count < 2) return false;
	for (int i = 0; i < st->item_count - 1; i++)
		if (st->item_ids [i + 1] != m->motion_item_ids [i]) return false;
	return st->item_ids [0] == m->motion_item_ids [m->motion_item_count - 1];
}

static void sync_implicit_section(GalleryApp *g, XtkTxn *t, MotionPageState *st, Model const *m) {
	if (st->imp_mask != m->motion_imp_mask) {
		for (int i = 0; i < 6; i++) {
			bool active = (m->motion_imp_mask & kImpMaskChecks [i]) != 0u;
			txn_enum(
			  t, xtk_ref_existing(g->live_extra [EX_IMP_MASKS + i]), XTK_PROP_BUTTON_STYLE,
			  active ? XTK_BUTTON_ACCENT : XTK_BUTTON_STANDARD
			);
		}
		apply_implicit_cfg(t, xtk_ref_existing(g->live_extra [EX_IMP_TARGET]), m->motion_imp_mask);
		st->imp_mask = m->motion_imp_mask;
	}
	if (st->imp_step != m->motion_imp_step) {
		sync_pills(t, &g->live_extra [EX_IMP_STATES], 3, m->motion_imp_step);
		apply_implicit_style(t, xtk_ref_existing(g->live_extra [EX_IMP_TARGET]), m->motion_imp_step);
		st->imp_step = m->motion_imp_step;
	}
}

static void sync_items_delta(
  GalleryApp *g, XtkTxn *t, MotionPageState *st, Model const *m, XtkLocalId new_items [8], bool *rebuilt
) {
	for (int i = 0; i < 8; i++) new_items [i] = XTK_LOCAL_INVALID;
	*rebuilt = false;
	if (is_tail_added(st, m)) {
		int idx         = st->item_count;
		new_items [idx] = create_item_card(
		  t, xtk_ref_existing(g->live_extra [EX_ITEMS_ROW]), m->motion_item_ids [idx], m->motion_theme_kind
		);
		return;
	}
	if (is_tail_removed(st, m)) {
		xtk_txn_drop(t, xtk_ref_existing(g->live_extra [EX_ITEMS + st->item_count - 1]));
		return;
	}
	if (is_shifted_left(st, m)) {
		xtk_txn_move(
		  t, xtk_ref_existing(g->live_extra [EX_ITEMS_ROW]), xtk_ref_existing(g->live_extra [EX_ITEMS]), xtk_ref_none()
		);
		XtkCtrl first = g->live_extra [EX_ITEMS];
		for (int i = 0; i < st->item_count - 1; i++) g->live_extra [EX_ITEMS + i] = g->live_extra [EX_ITEMS + i + 1];
		g->live_extra [EX_ITEMS + st->item_count - 1] = first;
		return;
	}
	for (int i = 0; i < st->item_count; i++) xtk_txn_drop(t, xtk_ref_existing(g->live_extra [EX_ITEMS + i]));
	for (int i = 0; i < m->motion_item_count; i++)
		new_items [i] = create_item_card(
		  t, xtk_ref_existing(g->live_extra [EX_ITEMS_ROW]), m->motion_item_ids [i], m->motion_theme_kind
		);
	*rebuilt = true;
}

static void sync_enter_exit_section(
  GalleryApp *g, XtkTxn *t, MotionPageState *st, Model const *m, XtkLocalId new_items [8], bool *rebuilt
) {
	if (st->theme_kind != m->motion_theme_kind) {
		sync_pills(t, &g->live_extra [EX_THEME_KINDS], 5, index_of_i(kThemeKinds, 5, m->motion_theme_kind));
		for (int i = 0; i < st->item_count; i++)
			txn_enum(
			  t, xtk_ref_existing(g->live_extra [EX_ITEMS + i]), XTK_PROP_MOTION_ENTER_EXIT,
			  ( uint32_t ) m->motion_theme_kind
			);
		st->theme_kind = m->motion_theme_kind;
	}
	bool same = st->item_count == m->motion_item_count
	         && memcmp(st->item_ids, m->motion_item_ids, sizeof(int) * ( size_t ) st->item_count) == 0;
	if (!same) sync_items_delta(g, t, st, m, new_items, rebuilt);
}

static XtkLocalId sync_frame_section(GalleryApp *g, XtkTxn *t, MotionPageState *st, Model const *m) {
	if (st->frame_kind != m->motion_frame_kind) {
		sync_pills(t, &g->live_extra [EX_FRAME_KINDS], 6, index_of_i(kFrameKinds, 6, m->motion_frame_kind));
		txn_enum(
		  t, xtk_ref_existing(g->live_extra [EX_FRAME]), XTK_PROP_FRAME_TRANSITION, ( uint32_t ) m->motion_frame_kind
		);
		st->frame_kind = m->motion_frame_kind;
	}
	if (st->frame_page == m->motion_frame_page && st->frame_back == m->motion_frame_back) return XTK_LOCAL_INVALID;
	XtkNodeRef fref = xtk_ref_existing(g->live_extra [EX_FRAME]);
	txn_enum(t, fref, XTK_PROP_FRAME_TRANSITION, ( uint32_t ) m->motion_frame_kind);
	txn_bool(t, fref, XTK_PROP_FRAME_BACK, m->motion_frame_back);
	txn_i32(t, fref, XTK_PROP_FRAME_PAGE_KEY, (m->motion_frame_page & 3) + 1);
	xtk_txn_drop(t, xtk_ref_existing(g->live_extra [EX_SUBPAGE]));
	st->frame_page = m->motion_frame_page;
	st->frame_back = m->motion_frame_back;
	return create_subpage_card(t, fref, m->motion_frame_page & 3, m->motion_frame_back);
}

static void sync_connected_section(
  GalleryApp *g, XtkTxn *t, MotionPageState *st, Model const *m, XtkLocalId *out_body, XtkLocalId *out_hero
) {
	*out_body = XTK_LOCAL_INVALID;
	*out_hero = XTK_LOCAL_INVALID;
	if (st->conn_kind != m->motion_conn_kind) {
		sync_pills(t, &g->live_extra [EX_CONN_KINDS], 3, index_of_i(kConnKinds, 3, m->motion_conn_kind));
		ConnectedCfgWire cw = {.kind = ( uint32_t ) m->motion_conn_kind, .shadow = 1u};
		txn_bytes(
		  t, xtk_ref_existing(g->live_extra [EX_CONN_HERO]), XTK_PROP_CONNECTED_CONFIG, &cw, ( uint32_t ) sizeof(cw)
		);
		st->conn_kind = m->motion_conn_kind;
	}
	if (st->conn_detail == m->motion_conn_detail) return;
	txn_str(
	  t, xtk_ref_existing(g->live_extra [EX_CONN_TOGGLE]), XTK_PROP_TEXT,
	  m->motion_conn_detail ? "Collapse" : "Expand detail"
	);
	xtk_txn_drop(t, xtk_ref_existing(g->live_extra [EX_CONN_BODY]));
	create_conn_body(
	  t, xtk_ref_existing(g->live_extra [EX_CONN_HOST]), m->motion_conn_kind, m->motion_conn_detail, out_body, out_hero
	);
	st->conn_detail = m->motion_conn_detail;
}

static void adopt_sync_ctrls(
  GalleryApp *g, MotionPageState *st, Model const *m, XtkBatchResult const *res, XtkLocalId const new_items [8],
  bool items_rebuilt, XtkLocalId new_sub, XtkLocalId new_body, XtkLocalId new_hero
) {
	if (items_rebuilt) {
		for (int i = 0; i < 8; i++)
			g->live_extra [EX_ITEMS + i]
			  = i < m->motion_item_count ? find_result_ctrl(res, new_items [i]) : XTK_CTRL_INVALID;
	}
	else if (m->motion_item_count > st->item_count) {
		int idx                        = st->item_count;
		g->live_extra [EX_ITEMS + idx] = find_result_ctrl(res, new_items [idx]);
	}
	else if (m->motion_item_count < st->item_count) {
		g->live_extra [EX_ITEMS + m->motion_item_count] = XTK_CTRL_INVALID;
	}
	st->item_count = m->motion_item_count;
	memcpy(st->item_ids, m->motion_item_ids, sizeof(st->item_ids));

	if (new_sub != XTK_LOCAL_INVALID) g->live_extra [EX_SUBPAGE] = find_result_ctrl(res, new_sub);
	if (new_body != XTK_LOCAL_INVALID) g->live_extra [EX_CONN_BODY] = find_result_ctrl(res, new_body);
	if (new_hero != XTK_LOCAL_INVALID) g->live_extra [EX_CONN_HERO] = find_result_ctrl(res, new_hero);
}

static void motion_sync(GalleryApp *g) {
	MotionPageState *st = ( MotionPageState * ) (g->page_allocs ? g->page_allocs->data : NULL);
	Model const     *m  = &g->model;
	if (!st) return;

	XtkTxn txn;
	if (!xtk_txn_begin(&g->scene, &txn)) return;

	sync_implicit_section(g, &txn, st, m);
	XtkLocalId new_items [8];
	bool       items_rebuilt = false;
	sync_enter_exit_section(g, &txn, st, m, new_items, &items_rebuilt);
	XtkLocalId new_sub  = sync_frame_section(g, &txn, st, m);
	XtkLocalId new_body = XTK_LOCAL_INVALID, new_hero = XTK_LOCAL_INVALID;
	sync_connected_section(g, &txn, st, m, &new_body, &new_hero);

	if (st->reduced_override != m->motion_reduced_override) {
		sync_pills(&txn, &g->live_extra [EX_RED_KINDS], 3, index_of_i(kReducedModes, 3, m->motion_reduced_override));
		st->reduced_override = m->motion_reduced_override;
	}

	XtkBatchResult *res = xtk_txn_commit(&txn, NULL);
	if (res) {
		adopt_sync_ctrls(g, st, m, res, new_items, items_rebuilt, new_sub, new_body, new_hero);
		xtk_batchresult_destroy(res);
	}
	gallery_settext(g, g->live_output [MSG_MOTION_REDUCED], reduced_status_text());
}

bool page_motion_build(PageBuild *b) {
	MotionPageState *st = ( MotionPageState * ) page_alloc(b, sizeof(*st));
	if (!st) return false;
	Model const *m = &b->app->model;
	*st            = (MotionPageState) {
	  .imp_step         = m->motion_imp_step,
	  .imp_mask         = m->motion_imp_mask,
	  .theme_kind       = m->motion_theme_kind,
	  .item_count       = m->motion_item_count,
	  .frame_page       = m->motion_frame_page,
	  .frame_kind       = m->motion_frame_kind,
	  .frame_back       = m->motion_frame_back,
	  .conn_kind        = m->motion_conn_kind,
	  .conn_detail      = m->motion_conn_detail,
	  .reduced_override = m->motion_reduced_override,
	};
	memcpy(st->item_ids, m->motion_item_ids, sizeof(st->item_ids));

	page_sync(b, motion_sync);
	XtkNodeRef parent = xtk_ref_local(b->content);
	page_header(
	  b, parent, "Motion",
	  "Implicit property transitions, enter/exit/reorder theme transitions, Frame NavigationTransitionInfo, "
	  "ConnectedAnimation, and UISettings.AnimationsEnabled."
	);
	build_implicit_section(b, parent, m);
	build_enter_exit_section(b, parent, m);
	build_frame_section(b, parent, m);
	build_connected_section(b, parent, m);
	build_reduced_section(b, parent, m);
	return true;
}
