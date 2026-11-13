#include "text/flux_clipboard.h"

#include <windows.h>

void flux_clipboard_set_utf8(char const *utf8, uint32_t len) {
	if (!utf8 || len == 0u) return;

	int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8, ( int ) len, NULL, 0);
	if (wlen <= 0 || !OpenClipboard(NULL)) return;

	EmptyClipboard();
	HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, (( size_t ) wlen + 1u) * sizeof(wchar_t));
	if (hg) {
		wchar_t *p = ( wchar_t * ) GlobalLock(hg);
		MultiByteToWideChar(CP_UTF8, 0, utf8, ( int ) len, p, wlen);
		p [wlen] = 0;
		GlobalUnlock(hg);
		SetClipboardData(CF_UNICODETEXT, hg);
	}
	CloseClipboard();
}
