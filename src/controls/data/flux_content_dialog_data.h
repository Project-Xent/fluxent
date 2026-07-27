/**
 * @file flux_dialog_data.h
 * @brief Result codes for the FluxContentDialog control (WinUI 3 ContentDialog).
 */
#ifndef FLUX_CONTENT_DIALOG_DATA_H
#define FLUX_CONTENT_DIALOG_DATA_H

/** @brief Which button closed the dialog (mirrors WinUI ContentDialogResult). */
typedef enum FluxDialogResult
{
	FLUX_DIALOG_NONE = 0,  /**< Closed via the close button or Escape. */
	FLUX_DIALOG_PRIMARY,   /**< Primary button. */
	FLUX_DIALOG_SECONDARY, /**< Secondary button. */
} FluxDialogResult;


#endif
