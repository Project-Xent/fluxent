#ifndef FLUX_INFO_BAR_DATA_H
#define FLUX_INFO_BAR_DATA_H

#include <stdbool.h>
#include <xtk/xtk_types.h>

typedef XtkInfoBarSeverity FluxInfoBarSeverity;

#define FLUX_INFOBAR_INFORMATIONAL XTK_INFOBAR_INFORMATIONAL
#define FLUX_INFOBAR_SUCCESS       XTK_INFOBAR_SUCCESS
#define FLUX_INFOBAR_WARNING       XTK_INFOBAR_WARNING
#define FLUX_INFOBAR_ERROR         XTK_INFOBAR_ERROR

typedef struct FluxInfoBarData {
	FluxInfoBarSeverity severity;
	char const         *title;
	char const         *message;
	bool                is_open;
	bool                is_closable;
	void                (*on_close)(void *ctx);
	void               *on_close_ctx;
	void               *layout_ctx;
	XentNodeId          layout_node;
} FluxInfoBarData;

#endif
