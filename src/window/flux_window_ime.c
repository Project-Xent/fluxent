/**
 * @file flux_window_ime.c
 * @brief IMM32 input-method protocol for FluxWindow: composition/candidate
 * window placement, result/composition string extraction, and the system
 * caret that anchors the IME UI. Dispatched from the main wndproc via
 * flux_window_ime_message.
 */
#include "flux_window_internal.h"

#include <stdlib.h>
#include <string.h>
#include <imm.h>
#ifdef _MSC_VER
  #pragma comment(lib, "imm32.lib")
#endif

static wchar_t *window_ime_read_string(HIMC hImc, DWORD kind, LONG *out_len) {
	*out_len   = 0;
	LONG bytes = ImmGetCompositionStringW(hImc, kind, NULL, 0);
	if (bytes <= 0) return NULL;

	wchar_t *buffer = ( wchar_t * ) malloc(( size_t ) bytes + sizeof(wchar_t));
	if (!buffer) return NULL;
	if (ImmGetCompositionStringW(hImc, kind, buffer, ( DWORD ) bytes) < 0) {
		free(buffer);
		return NULL;
	}
	*out_len          = bytes / ( LONG ) sizeof(wchar_t);
	buffer [*out_len] = 0;
	return buffer;
}

static void window_emit_ime_result(FluxWindow *win, wchar_t const *text, LONG len) {
	if (!win->on_char || !text || len <= 0) return;
	for (LONG i = 0; i < len; i++) win->on_char(win->char_ctx, text [i]);
}

static void window_apply_ime_position(FluxWindow *win, HIMC hImc) {
	if (!win || !win->hwnd || !hImc || !win->ime_position_valid) return;
	if (win->ime_applying_position) return;
	win->ime_applying_position = true;

	COMPOSITIONFORM cf;
	memset(&cf, 0, sizeof(cf));
	cf.dwStyle        = CFS_POINT;
	cf.ptCurrentPos.x = win->ime_x;
	cf.ptCurrentPos.y = win->ime_y;
	ImmSetCompositionWindow(hImc, &cf);

	CANDIDATEFORM cand;
	memset(&cand, 0, sizeof(cand));
	cand.dwIndex        = 0;
	cand.dwStyle        = CFS_EXCLUDE;
	cand.ptCurrentPos.x = win->ime_x;
	cand.ptCurrentPos.y = win->ime_y;
	cand.rcArea.left    = win->ime_x;
	cand.rcArea.top     = win->ime_y;
	cand.rcArea.right   = win->ime_x + 1;
	cand.rcArea.bottom  = win->ime_y + win->ime_h;
	ImmSetCandidateWindow(hImc, &cand);

	win->ime_applying_position = false;
}

static void window_update_system_caret(FluxWindow *win) {
	if (!win || !win->hwnd || !win->ime_position_valid) return;
	CreateCaret(win->hwnd, NULL, 1, win->ime_h > 0 ? win->ime_h : 1);
	SetCaretPos(win->ime_x, win->ime_y);
	HideCaret(win->hwnd);
}

static bool window_on_ime_request(FluxWindow *win, WPARAM wp, LPARAM lp, LRESULT *result) {
	if (wp != IMR_QUERYCHARPOSITION || !lp || !win || !win->ime_position_valid) return false;

	IMECHARPOSITION *pos = ( IMECHARPOSITION * ) lp;
	memset(pos, 0, sizeof(*pos));
	pos->dwSize      = sizeof(*pos);
	pos->dwCharPos   = 0;
	pos->pt.x        = win->ime_x;
	pos->pt.y        = win->ime_y;
	pos->cLineHeight = ( UINT ) (win->ime_h > 0 ? win->ime_h : 1);
	ClientToScreen(win->hwnd, &pos->pt);
	GetClientRect(win->hwnd, &pos->rcDocument);
	MapWindowPoints(win->hwnd, NULL, ( POINT * ) &pos->rcDocument, 2);
	*result = 1;
	return true;
}

static bool window_on_ime_notify(FluxWindow *win, WPARAM wp) {
	if (!win || !win->ime_position_valid) return false;
	if (wp != IMN_OPENCANDIDATE && wp != IMN_CHANGECANDIDATE) return false;

	HIMC hImc = ImmGetContext(win->hwnd);
	if (!hImc) return false;
	window_apply_ime_position(win, hImc);
	ImmReleaseContext(win->hwnd, hImc);
	return true;
}

static bool window_on_ime_composition(FluxWindow *win, HWND hwnd, LPARAM lp) {
	if (!win->on_ime_composition && !win->on_char) return false;

	HIMC hImc = ImmGetContext(hwnd);
	if (!hImc) return false;
	window_apply_ime_position(win, hImc);

	bool handled = false;
	if (lp & GCS_RESULTSTR) {
		LONG     result_len = 0;
		wchar_t *result     = window_ime_read_string(hImc, GCS_RESULTSTR, &result_len);
		if (win->on_ime_composition) win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
		window_emit_ime_result(win, result, result_len);
		free(result);
		handled = true;
	}

	if (win->on_ime_composition && (lp & (GCS_COMPSTR | GCS_CURSORPOS))) {
		LONG     comp_len = 0;
		wchar_t *comp     = window_ime_read_string(hImc, GCS_COMPSTR, &comp_len);
		int      cursor   = ( int ) ImmGetCompositionStringW(hImc, GCS_CURSORPOS, NULL, 0);
		if (comp && comp_len > 0) win->on_ime_composition(win->ime_composition_ctx, comp, cursor);
		else win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
		free(comp);
		handled = true;
	}
	else if (!handled && win->on_ime_composition) {
		win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
		handled = true;
	}

	ImmReleaseContext(hwnd, hImc);
	return handled;
}

bool flux_window_ime_message(FluxWindow *win, HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, LRESULT *result) {
	switch (msg) {
	case WM_IME_STARTCOMPOSITION : {
		HIMC hImc = ImmGetContext(hwnd);
		if (hImc) {
			window_apply_ime_position(win, hImc);
			ImmReleaseContext(hwnd, hImc);
		}
		*result = 0;
		return true;
	}
	case WM_IME_COMPOSITION : *result = 0; return window_on_ime_composition(win, hwnd, lp);
	case WM_IME_ENDCOMPOSITION :
		/* Not consumed: DefWindowProc still needs the end-of-composition. */
		if (win->on_ime_composition) win->on_ime_composition(win->ime_composition_ctx, NULL, 0);
		return false;
	case WM_IME_REQUEST : return window_on_ime_request(win, wp, lp, result);
	case WM_IME_NOTIFY :
		if (!window_on_ime_notify(win, wp)) return false;
		*result = 0;
		return true;
	default : return false;
	}
}

void flux_window_setimepos(FluxWindow *win, int x, int y, int height) {
	if (!win || !win->hwnd) return;

	win->ime_x              = x;
	win->ime_y              = y;
	win->ime_h              = height > 0 ? height : 1;
	win->ime_position_valid = true;
	window_update_system_caret(win);

	HIMC hImc = ImmGetContext(win->hwnd);
	if (!hImc) return;
	window_apply_ime_position(win, hImc);
	ImmReleaseContext(win->hwnd, hImc);
}
