#ifndef FLUX_FLIP_INTERNAL_H
#define FLUX_FLIP_INTERNAL_H

#include <stdbool.h>
#include <stdint.h>
#include <xent/xent.h>

typedef struct FluxStore  FluxStore;
typedef struct FluxWindow FluxWindow;

typedef struct FluxFlipViewData {
	XentCtx      *ctx;
	FluxStore    *store;
	FluxWindow   *window;
	XentNodeId    root;
	XentNodeId    host;

	int           selected;
	bool          vertical;

	float         offset;
	float         anim_from;
	float         anim_to;
	unsigned long anim_start;
	bool          anim;

	float         extent;
	float         cross;

	unsigned long pointer_activity;
	unsigned long last_wheel;
	int           wheel_sign;
	uint8_t       pressed_btn;

	void          (*on_select)(void *ctx, int index);
	void         *on_select_ctx;
} FluxFlipViewData;

typedef struct FluxPipsPagerData {
	XentCtx   *ctx;
	FluxStore *store;
	XentNodeId node;

	int        count;
	int        selected;
	int        max_visible;
	bool       vertical;
	uint8_t    nav_vis;

	int        window_start;
	int        pressed_pip;
	uint8_t    pressed_nav;

	void       (*on_select)(void *ctx, int index);
	void      *on_select_ctx;
} FluxPipsPagerData;

#endif
