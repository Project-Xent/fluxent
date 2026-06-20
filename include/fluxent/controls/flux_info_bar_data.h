/**
 * @file flux_info_bar_data.h
 * @brief Data structure for the FluxInfoBar control (WinUI 3 InfoBar).
 */
#ifndef FLUX_INFO_BAR_DATA_H
#define FLUX_INFO_BAR_DATA_H

#include <stdbool.h>
#include <xtk/xtk_types.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef XtkInfoBarSeverity FluxInfoBarSeverity;

#define FLUX_INFOBAR_INFORMATIONAL XTK_INFOBAR_INFORMATIONAL
#define FLUX_INFOBAR_SUCCESS       XTK_INFOBAR_SUCCESS
#define FLUX_INFOBAR_WARNING       XTK_INFOBAR_WARNING
#define FLUX_INFOBAR_ERROR         XTK_INFOBAR_ERROR

/**
 * @brief Configuration for an info bar (a status banner with an icon, title,
 * message, and optional close button).
 */
typedef struct FluxInfoBarData {
	FluxInfoBarSeverity severity;               /**< Selects icon glyph and palette. */
	char const         *title;                  /**< Bold leading text (may be NULL). */
	char const         *message;                /**< Body text (may be NULL). */
	bool                is_open;                /**< Whether the bar is shown. */
	bool                is_closable;            /**< Show the trailing close button. */
	void                (*on_close)(void *ctx); /**< Invoked when closed by the user. */
	void               *on_close_ctx;
	void               *layout_ctx;             /**< XentContext for layout text updates. */
	unsigned int        layout_node;            /**< XentNodeId for layout text updates. */
} FluxInfoBarData;

#ifdef __cplusplus
}
#endif

#endif
