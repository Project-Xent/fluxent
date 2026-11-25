#include "controls/draw/flux_control_record_internal.h"
#include "render/flux_anim.h"

#include <math.h>
#include <windows.h>

typedef enum ChevronState
{
	NORMAL_OFF,
	NORMAL_ON,
	POINTER_OVER_OFF,
	POINTER_OVER_ON,
	PRESSED_OFF,
	PRESSED_ON,
} ChevronState;

static FluxIconSegment const segments [] = {
  {NORMAL_ON,        NORMAL_OFF,       0.0f,    29.05f },
  {NORMAL_ON,        POINTER_OVER_ON,  30.05f,  39.05f },
  {NORMAL_ON,        PRESSED_ON,       40.05f,  49.05f },
  {NORMAL_OFF,       NORMAL_ON,        50.05f,  79.05f },
  {NORMAL_OFF,       POINTER_OVER_OFF, 80.05f,  89.05f },
  {NORMAL_OFF,       PRESSED_OFF,      90.05f,  99.05f },
  {POINTER_OVER_ON,  POINTER_OVER_OFF, 100.05f, 109.05f},
  {POINTER_OVER_ON,  NORMAL_ON,        110.05f, 119.05f},
  {POINTER_OVER_ON,  PRESSED_ON,       120.05f, 129.05f},
  {POINTER_OVER_OFF, POINTER_OVER_ON,  130.05f, 139.05f},
  {POINTER_OVER_OFF, NORMAL_OFF,       140.05f, 149.05f},
  {POINTER_OVER_OFF, PRESSED_OFF,      150.05f, 159.05f},
  {PRESSED_ON,       PRESSED_OFF,      160.05f, 169.05f},
  {PRESSED_ON,       POINTER_OVER_OFF, 170.05f, 189.05f},
  {PRESSED_ON,       NORMAL_OFF,       190.05f, 209.05f},
  {PRESSED_OFF,      PRESSED_ON,       210.05f, 219.05f},
  {PRESSED_OFF,      POINTER_OVER_ON,  220.05f, 239.05f},
  {PRESSED_OFF,      NORMAL_ON,        240.05f, 259.05f},
};

typedef enum ChevronEase
{
	HOLD,
	ARRIVE,
	ANTICIPATE_UP,
	SETTLE,
	ANTICIPATE_DOWN,
} ChevronEase;

typedef struct ChevronKey {
	float       frame;
	float       x, y;
	ChevronEase ease;
} ChevronKey;

static ChevronKey const rotation [] = {
  {0,   180, 0, HOLD  },
  {15,  180, 0, HOLD  },
  {25,  0,   0, ARRIVE},
  {30,  180, 0, HOLD  },
  {40,  180, 0, HOLD  },
  {50,  0,   0, HOLD  },
  {65,  0,   0, HOLD  },
  {75,  180, 0, ARRIVE},
  {80,  0,   0, HOLD  },
  {90,  0,   0, HOLD  },
  {100, 180, 0, HOLD  },
  {109, 0,   0, ARRIVE},
  {110, 180, 0, HOLD  },
  {120, 180, 0, HOLD  },
  {130, 0,   0, HOLD  },
  {139, 180, 0, ARRIVE},
  {140, 0,   0, HOLD  },
  {150, 0,   0, HOLD  },
  {160, 180, 0, HOLD  },
  {169, 0,   0, ARRIVE},
  {170, 180, 0, HOLD  },
  {175, 180, 0, HOLD  },
  {185, 0,   0, ARRIVE},
  {190, 180, 0, HOLD  },
  {195, 180, 0, HOLD  },
  {205, 0,   0, ARRIVE},
  {210, 0,   0, HOLD  },
  {219, 180, 0, ARRIVE},
  {220, 0,   0, HOLD  },
  {225, 0,   0, HOLD  },
  {235, 180, 0, ARRIVE},
  {240, 0,   0, HOLD  },
  {245, 0,   0, HOLD  },
  {255, 180, 0, ARRIVE},
};

static ChevronKey const offset [] = {
  {0,   24,      24,      HOLD           },
  {10,  24,      26,      ARRIVE         },
  {15,  24,      18,      ANTICIPATE_UP  },
  {29,  24.171f, 23.923f, SETTLE         },
  {30,  24,      24,      HOLD           },
  {40,  24,      24,      HOLD           },
  {49,  24,      26,      ARRIVE         },
  {50,  24,      24,      HOLD           },
  {60,  24,      22,      ARRIVE         },
  {65,  24,      30,      ANTICIPATE_DOWN},
  {79,  24,      24,      SETTLE         },
  {90,  24,      24,      HOLD           },
  {99,  24,      22,      ARRIVE         },
  {100, 24,      24,      HOLD           },
  {120, 24,      24,      HOLD           },
  {129, 24,      26,      ARRIVE         },
  {130, 24,      24,      HOLD           },
  {150, 24,      24,      HOLD           },
  {159, 24,      22,      ARRIVE         },
  {160, 24,      26,      HOLD           },
  {169, 24,      22,      ARRIVE         },
  {170, 24,      26,      HOLD           },
  {175, 24,      18,      ANTICIPATE_UP  },
  {189, 24.171f, 23.923f, SETTLE         },
  {190, 24,      26,      HOLD           },
  {195, 24,      18,      ANTICIPATE_UP  },
  {209, 24.171f, 23.923f, SETTLE         },
  {210, 24,      22,      HOLD           },
  {219, 24,      26,      ARRIVE         },
  {220, 24,      22,      HOLD           },
  {225, 24,      30,      ANTICIPATE_DOWN},
  {239, 24,      24,      SETTLE         },
  {240, 24,      22,      HOLD           },
  {245, 24,      30,      ANTICIPATE_DOWN},
  {259, 24,      24,      SETTLE         },
};

static float chevron_ease(ChevronEase ease, float t) {
	switch (ease) {
	case ARRIVE          : return flux_cubic_bezier(t, 0.167f, 0.167f, 0.0f, 1.0f);
	case ANTICIPATE_UP   : return flux_cubic_bezier(t, 0.85f, 0.0f, 0.75f, 1.0f);
	case SETTLE          : return flux_cubic_bezier(t, 0.35f, 0.0f, 0.0f, 1.0f);
	case ANTICIPATE_DOWN : return flux_cubic_bezier(t, 0.85f, 0.0f, 0.45f, 1.0f);
	default              : return 0.0f;
	}
}

static FluxPoint chevron_sample(ChevronKey const *keys, size_t count, float frame) {
	size_t i = 1;
	while (i < count && frame >= keys [i].frame) ++i;
	ChevronKey a = keys [i - 1];
	if (i == count) return (FluxPoint) {a.x, a.y};
	ChevronKey b = keys [i];
	float      t = chevron_ease(b.ease, (frame - a.frame) / (b.frame - a.frame));
	return (FluxPoint) {flux_anim_mixf(a.x, b.x, t), flux_anim_mixf(a.y, b.y, t)};
}

static uint8_t chevron_state(FluxControlState const *state, bool expanded) {
	if (state->enabled && state->pressed) return expanded ? PRESSED_ON : PRESSED_OFF;
	if (state->enabled && state->hovered) return expanded ? POINTER_OVER_ON : POINTER_OVER_OFF;
	return expanded ? NORMAL_ON : NORMAL_OFF;
}

static bool chevron_draw(XentDlBuilder *b, FluxRect const *box, float frame, bool rtl, FluxColor fg) {
	static FluxPoint const points [] = {
	  {-3.543f, -1.793f},
      {0.02f,   1.707f },
      {3.489f,  -1.715f}
    };
	FluxPoint pos = chevron_sample(offset, sizeof(offset) / sizeof(offset [0]), frame);
	float angle   = chevron_sample(rotation, sizeof(rotation) / sizeof(rotation [0]), frame).x * (0.0174532925199433f);
	float cs = cosf(angle), sn = sinf(angle);
	float cx = box->x + box->w * 0.5f, cy = box->y + box->h * 0.5f;
	float direction = rtl ? -1.0f : 1.0f;
	FluxPoint path [3];
	for (size_t i = 0; i < 3; ++i) {
		float x  = points [i].x * cs - points [i].y * sn;
		float y  = points [i].x * sn + points [i].y * cs;
		path [i] = (FluxPoint) {cx + direction * (x + (pos.x - 24.0f) * 0.25f), cy + y + (pos.y - 24.0f) * 0.25f};
	}
	float alpha = flux_color_af(fg);
	if (alpha < 1.0f && !xent_dl_pushopacity(b, alpha)) return false;
	XentColor color = flux_rec_xcolor((FluxColor) {fg.rgba | 0xffu});
	bool ok = xent_dl_strokeline(b, path [0].x, path [0].y, path [1].x, path [1].y, color, 1.0f, XENT_STROKE_CAP_ROUND)
	       && xent_dl_strokeline(b, path [1].x, path [1].y, path [2].x, path [2].y, color, 1.0f, XENT_STROKE_CAP_ROUND);
	return (alpha >= 1.0f || xent_dl_popopacity(b)) && ok;
}

static float chevron_frame(FluxIconTimeline const *timeline, FluxRenderCtx const *rc, uint64_t id, uint8_t state) {
	FluxIconPlayback  temporary = {0};
	FluxCacheEntry   *entry     = rc && rc->cache ? flux_rcache_ensure(rc->cache, id) : NULL;
	FluxIconPlayback *p         = entry ? &entry->icon_playback : &temporary;
	BOOL              enabled   = TRUE;
	SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &enabled, 0);
	bool  playing;
	float frame = flux_icon_timeline_frame(timeline, p, state, rc ? rc->now : 0.0, enabled, &playing);
	flux_rec_note_animating(rc, playing);
	return frame;
}

bool flux_rec_up_down_chevron(
  XentDlBuilder *b, FluxRenderCtx const *rc, FluxRenderSnap const *snap, FluxRect const *box,
  FluxControlState const *state, bool expanded, FluxColor fg
) {
	static FluxIconTimeline const timeline = {segments, sizeof(segments) / sizeof(segments [0]), 60.0f};
	float                         frame    = chevron_frame(&timeline, rc, snap->id, chevron_state(state, expanded));
	return chevron_draw(b, box, frame, snap->rtl, fg);
}

typedef enum DownState
{
	DOWN_NORMAL,
	DOWN_POINTER_OVER,
	DOWN_PRESSED,
} DownState;

static float chevron_down_offset(float frame) {
	if (frame < 10.0f) {
		float t = flux_minf(frame / 9.0f, 1.0f);
		return 7.5f * flux_cubic_bezier(t, 0.167f, 0.167f, 0.65f, 1.0f);
	}
	if (frame < 15.0f) {
		float t = (frame - 10.0f) / 5.0f;
		return 7.5f - 10.5f * flux_cubic_bezier(t, 0.55f, 0.0f, 0.75f, 1.0f);
	}
	return -3.0f + 3.0f * flux_cubic_bezier((frame - 15.0f) / 14.0f, 0.35f, 0.0f, 0.0f, 1.0f);
}

static DownState chevron_down_state(FluxControlState const *state) {
	if (!state->enabled) return DOWN_NORMAL;
	if (state->pressed) return DOWN_PRESSED;
	return state->hovered ? DOWN_POINTER_OVER : DOWN_NORMAL;
}

static FluxColor combo_chevron_color(FluxThemeColors const *theme, FluxControlState const *state) {
	if (!state->enabled) return theme->text_disabled;
	if (theme->contrast.active) return theme->contrast.button_text;
	return theme->text_secondary;
}

bool flux_rec_combo_chevron(
  XentDlBuilder *b, FluxRenderCtx const *rc, uint64_t id, FluxRect const *box, FluxControlState const *state
) {
	static FluxIconSegment const down_segments [] = {
	  {DOWN_NORMAL,       DOWN_POINTER_OVER, 0.0f,   0.0f  },
	  {DOWN_NORMAL,       DOWN_PRESSED,      0.0f,   9.05f },
	  {DOWN_POINTER_OVER, DOWN_NORMAL,       0.0f,   0.0f  },
	  {DOWN_POINTER_OVER, DOWN_PRESSED,      0.0f,   9.05f },
	  {DOWN_PRESSED,      DOWN_NORMAL,       10.05f, 28.05f},
	  {DOWN_PRESSED,      DOWN_POINTER_OVER, 10.05f, 28.05f},
	};
	static FluxIconTimeline const timeline  = {down_segments, sizeof(down_segments) / sizeof(down_segments [0]), 60.0f};
	float                         frame     = chevron_frame(&timeline, rc, id, chevron_down_state(state));
	FluxRect                      draw      = *box;
	draw.y                                 += chevron_down_offset(frame) * 0.25f;
	return flux_rec_fill_shape(b, FLUX_SHAPE_DOWN_CHEVRON, &draw, combo_chevron_color(flux_rec_theme(rc), state));
}
