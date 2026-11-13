#ifndef FLUX_PICKER_DATA_H
#define FLUX_PICKER_DATA_H

#include <fluxent/flux_types.h>
#include "store/flux_node_data.h"

#include <stdbool.h>
#include <xent/xent.h>

typedef struct FluxInput        FluxInput;
typedef struct FluxWindow       FluxWindow;
typedef struct FluxPickerFlyout FluxPickerFlyout;
typedef struct FluxPickerOps    FluxPickerOps;

#define FLUX_PICKER_COLUMNS  3

#define FLUX_DATE_PICKER_W   296.0f
#define FLUX_TIME_PICKER_W   242.0f

#define FLUX_PICKER_BUTTON_H 32.0f
#define FLUX_PICKER_HEADER_H 24.0f

typedef struct FluxPickerData {
	XentCtx             *ctx;
	FluxStore           *store;
	XentNodeId           root;
	FluxInput           *input;
	FluxWindow          *window;
	XentNodeId           overlay;

	FluxPickerOps const *ops;
	FluxPickerFlyout    *flyout;

	char const          *header;

	char                 text [FLUX_PICKER_COLUMNS][64];
	float                weight [FLUX_PICKER_COLUMNS];
	bool                 left [FLUX_PICKER_COLUMNS];
	int                  columns;
	float                header_h;
	float                measured_width;
	float                button_h;
	bool                 has_value;
	int                  value;

	bool                 enabled;
	bool                 open;
	bool                 press_valid;
	bool                 press_on_button;

	void                 (*on_change)(void *ctx, int value);
	void                *on_change_ctx;
} FluxPickerData;

typedef struct FluxDatePickerData {
	FluxPickerData base;
	int            date;
	int            min_year;
	int            max_year;
	int            order [FLUX_PICKER_COLUMNS];
	int            year, month, day;
	bool           loading;
} FluxDatePickerData;

typedef struct FluxTimePickerData {
	FluxPickerData base;
	int            minutes;
	int            clock;
	int            step;
	bool           h12;
	int            order [3];
	int            columns;
	int            hour, minute;
	bool           loading;
} FluxTimePickerData;

typedef struct FluxPickerCardData {
	int   columns;
	float weight [FLUX_PICKER_COLUMNS];
	float body_h;
	bool  time;
} FluxPickerCardData;

typedef FluxPickerCardData FluxPickerCardSnapshot;

typedef struct FluxPickerSnapshot {
	char const *header;
	char const *text [FLUX_PICKER_COLUMNS];
	float       weight [FLUX_PICKER_COLUMNS];
	int         columns;
	float       header_h;
	float       button_h;
	bool        has_value;
	bool        open;
	bool        pointer_on_button;
	bool        press_on_button;
	bool        left [FLUX_PICKER_COLUMNS];
} FluxPickerSnapshot;

#endif
