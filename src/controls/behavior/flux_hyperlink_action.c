#include "controls/behavior/flux_hyperlink_action.h"

#include <malloc.h>
#include <windows.h>
#include <shellapi.h>

void flux_hyperlink_open_url(char const *url) {
	if (!url || !url [0]) return;

	int wlen = MultiByteToWideChar(CP_UTF8, 0, url, -1, NULL, 0);
	if (wlen <= 0) return;

	wchar_t *wurl = ( wchar_t * ) _alloca(( size_t ) wlen * sizeof(wchar_t));
	MultiByteToWideChar(CP_UTF8, 0, url, -1, wurl, wlen);
	ShellExecuteW(NULL, L"open", wurl, NULL, NULL, SW_SHOWNORMAL);
}
