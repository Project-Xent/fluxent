/**
 * @file flux_info_bar_data.h
 * @brief Data structure for the FluxInfoBar control (WinUI 3 InfoBar).
 */
#ifndef FLUX_INFO_BAR_DATA_H
#define FLUX_INFO_BAR_DATA_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Severity level selecting the InfoBar icon and color scheme. */
typedef enum FluxInfoBarSeverity
{
	FLUX_INFOBAR_INFORMATIONAL = 0, /**< Accent icon, attention background. */
	FLUX_INFOBAR_SUCCESS,           /**< Green icon, success background. */
	FLUX_INFOBAR_WARNING,           /**< Caution icon, caution background. */
	FLUX_INFOBAR_ERROR,             /**< Critical icon, critical background. */
} FluxInfoBarSeverity;

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
} FluxInfoBarData;

#ifdef __cplusplus
}
#endif

#endif
