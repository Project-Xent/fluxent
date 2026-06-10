/**
 * @file flux_content_dialog_data.h
 * @brief Result codes for the FluxContentDialog control (WinUI 3 ContentDialog).
 */
#ifndef FLUX_CONTENT_DIALOG_DATA_H
#define FLUX_CONTENT_DIALOG_DATA_H

#ifdef __cplusplus
extern "C"
{
#endif

/** @brief Which button closed the dialog (mirrors WinUI ContentDialogResult). */
typedef enum FluxDialogResult
{
	FLUX_DIALOG_NONE = 0,  /**< Closed via the close button or Escape. */
	FLUX_DIALOG_PRIMARY,   /**< Primary button. */
	FLUX_DIALOG_SECONDARY, /**< Secondary button. */
} FluxDialogResult;

#ifdef __cplusplus
}
#endif

#endif
