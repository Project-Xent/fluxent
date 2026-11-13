#include "runtime/flux_date.h"

#include <stdint.h>
#include <string.h>
#include <windows.h>
#include <winstring.h>
#include <stdbool.h>

#include <cwinrt/factory.h>
#include <cwinrt/hstring.h>
#include <cwinrt/Windows.Globalization.DateTimeFormatting.h>

static int date_leap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

int        flux_date_daysin(int year, int month) {
	static int const days [12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
	if (month < 1 || month > 12) return 0;
	return days [month - 1] + (month == 2 && date_leap(year) ? 1 : 0);
}

bool flux_date_valid(int date) {
	int y = flux_date_year(date), m = flux_date_month(date), d = flux_date_day(date);
	return y >= 1 && m >= 1 && m <= 12 && d >= 1 && d <= flux_date_daysin(y, m);
}

static int64_t date_serial(int date) {
	int y = flux_date_year(date), m = flux_date_month(date), d = flux_date_day(date);
	int adj = y - (m <= 2 ? 1 : 0);
	int era = (adj >= 0 ? adj : adj - 399) / 400;
	int yoe = adj - era * 400;
	int doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
	int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
	return ( int64_t ) era * 146097 + doe - 719468;
}

static int date_from_serial(int64_t serial) {
	int64_t z   = serial + 719468;
	int64_t era = (z >= 0 ? z : z - 146096) / 146097;
	int     doe = ( int ) (z - era * 146097);
	int     yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
	int     doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
	int     mp  = (5 * doy + 2) / 153;
	int     d   = doy - (153 * mp + 2) / 5 + 1;
	int     m   = mp + (mp < 10 ? 3 : -9);
	int     y   = yoe + ( int ) era * 400 + (m <= 2 ? 1 : 0);
	return flux_date_pack(y, m, d);
}

int flux_date_weekday(int date) {
	int64_t w = (date_serial(date) + 4) % 7;
	return ( int ) (w < 0 ? w + 7 : w);
}

int flux_date_shift(int date, int days) { return date_from_serial(date_serial(date) + days); }

int flux_date_between(int from, int to) { return ( int ) (date_serial(to) - date_serial(from)); }

int flux_date_shiftmonth(int date, int months) {
	int index = flux_date_year(date) * 12 + flux_date_month(date) - 1 + months;
	int year = index / 12, month = index % 12 + 1;
	int day  = flux_date_day(date);
	int last = flux_date_daysin(year, month);
	return flux_date_pack(year, month, day > last ? last : day);
}

int flux_date_today(void) {
	SYSTEMTIME st;
	GetLocalTime(&st);
	return flux_date_pack(st.wYear, st.wMonth, st.wDay);
}

int flux_date_now(void) {
	SYSTEMTIME st;
	GetLocalTime(&st);
	return st.wHour * 60 + st.wMinute;
}

static void locale_utf8(wchar_t const *wide, char *out, size_t size) {
	if (size == 0) return;
	int n = WideCharToMultiByte(CP_UTF8, 0, wide, -1, out, ( int ) size, NULL, NULL);
	if (n <= 0) out [0] = '\0';
}

static int locale_info(LCTYPE type, wchar_t *buffer, int count) {
	return GetLocaleInfoEx(LOCALE_NAME_USER_DEFAULT, type, buffer, count);
}

int flux_locale_firstday(void) {
	wchar_t text [8] = {0};
	if (locale_info(LOCALE_IFIRSTDAYOFWEEK, text, 8) <= 0) return 0;
	int monday_first = text [0] - L'0';
	return (monday_first + 1) % 7;
}

static bool locale_pattern(wchar_t const *tmpl, wchar_t *out, size_t cap) {
	WGLDA_IDateTimeFormatterFactory *factory = NULL;
	WGLDA_DateTimeFormatter         *fmt     = NULL;
	WFOCO_IVectorView_HSTRING       *list    = NULL;
	cwinrt_hstring                   name = NULL, first = NULL;
	bool                             ok = false;
	if (FAILED(cwinrt_factory_get_statics(
		  L"Windows.Globalization.DateTimeFormatting.DateTimeFormatter", &CWINRT_IID_WGLDA_IDateTimeFormatterFactory,
		  ( void ** ) &factory
		)))
		return false;
	if (SUCCEEDED(cwinrt_hstring_from(tmpl, &name))
	    && SUCCEEDED(wglda_date_time_formatter_factory_create_date_time_formatter(factory, name, &fmt))
	    && SUCCEEDED(wglda_date_time_formatter_get__patterns(fmt, &list)))
	{
		typedef HRESULT(STDMETHODCALLTYPE * GetAt)(void *, UINT32, HSTRING *);
		GetAt get_at = ( GetAt ) ((*( void *** ) list) [6]);
		ok           = SUCCEEDED(get_at(list, 0, ( HSTRING * ) &first)) && first;
	}
	if (ok) wcsncpy_s(out, cap, WindowsGetStringRawBuffer(( HSTRING ) first, NULL), _TRUNCATE);
	if (first) cwinrt_hstring_free(first);
	if (name) cwinrt_hstring_free(name);
	if (list) (( IUnknown * ) list)->lpVtbl->Release(( IUnknown * ) list);
	if (fmt) (( IUnknown * ) fmt)->lpVtbl->Release(( IUnknown * ) fmt);
	(( IUnknown * ) factory)->lpVtbl->Release(( IUnknown * ) factory);
	return ok;
}

static int locale_before(wchar_t const *const at [3], int k) {
	int n = 0;
	for (int j = 0; j < 3; ++j) n += j != k && (at [j] < at [k] || (at [j] == at [k] && j < k));
	return n;
}

static void locale_rank(wchar_t const *pattern, wchar_t const *const keys [3], int rank [3]) {
	wchar_t const *at [3];
	for (int k = 0; k < 3; ++k) at [k] = wcsstr(pattern, keys [k]);
	for (int k = 0; k < 3; ++k) rank [k] = locale_before(at, k);
}

static void locale_columns(int const rank [3], bool rtl, int order [3]) {
	for (int k = 0; k < 3; ++k) order [rtl ? 2 - rank [k] : rank [k]] = k;
}

#define LOCALE_RTL_MARK 8207

void flux_locale_dateorder(int order [3]) {
	static wchar_t const *const keys [3] = {L"{day", L"{month", L"{year"};
	wchar_t                     pattern [128];
	int                         rank [3] = {1, 0, 2};
	bool                        rtl      = false;
	if (locale_pattern(L"day month.full year", pattern, 128)) {
		rtl = pattern [0] == LOCALE_RTL_MARK;
		locale_rank(pattern, keys, rank);
	}
	locale_columns(rank, rtl, order);
}

void flux_locale_timeorder(int order [3]) {
	static wchar_t const *const keys [3] = {L"{hour", L"{minute", L"{period"};
	wchar_t                     pattern [128];
	int                         rank [3] = {0, 1, 2};
	bool                        rtl = locale_pattern(L"month.full", pattern, 128) && pattern [0] == LOCALE_RTL_MARK;
	if (locale_pattern(L"hour minute", pattern, 128)) locale_rank(pattern, keys, rank);
	if (rtl) {
		int t    = rank [0];
		rank [0] = rank [1];
		rank [1] = t;
	}
	locale_columns(rank, rtl, order);
}

bool flux_locale_isclock24(void) {
	wchar_t text [8] = {0};
	return locale_info(LOCALE_ITIME, text, 8) > 0 && text [0] == L'1';
}

void flux_locale_ampm(char *am, char *pm, size_t size) {
	wchar_t a [16] = {0}, p [16] = {0};
	locale_info(LOCALE_S1159, a, 16);
	locale_info(LOCALE_S2359, p, 16);
	locale_utf8(a [0] ? a : L"AM", am, size);
	locale_utf8(p [0] ? p : L"PM", pm, size);
}

void flux_locale_formatdate(int date, wchar_t const *picture, char *out, size_t size) {
	SYSTEMTIME st     = {0};
	st.wYear          = ( WORD ) flux_date_year(date);
	st.wMonth         = ( WORD ) flux_date_month(date);
	st.wDay           = ( WORD ) flux_date_day(date);
	wchar_t text [96] = {0};
	if (GetDateFormatEx(LOCALE_NAME_USER_DEFAULT, 0, &st, picture, text, 96, NULL) <= 0) text [0] = L'\0';
	locale_utf8(text, out, size);
}

static bool locale_winrt_date(int date, WF_DateTime *out) {
	SYSTEMTIME local
	  = {( WORD ) flux_date_year(date), ( WORD ) flux_date_month(date), 0, ( WORD ) flux_date_day(date), 12};
	SYSTEMTIME utc;
	FILETIME   ft;
	if (!TzSpecificLocalTimeToSystemTime(NULL, &local, &utc) || !SystemTimeToFileTime(&utc, &ft)) return false;
	out->UniversalTime = ( INT64 ) ((( UINT64 ) ft.dwHighDateTime << 32) | ft.dwLowDateTime);
	return true;
}

void flux_locale_formattemplate(int date, wchar_t const *tmpl, char *out, size_t size) {
	WGLDA_IDateTimeFormatterFactory *factory = NULL;
	WGLDA_DateTimeFormatter         *fmt     = NULL;
	cwinrt_hstring                   name = NULL, text = NULL;
	WF_DateTime                      when = {0};
	if (size) out [0] = '\0';
	if (!locale_winrt_date(date, &when)
	    || FAILED(cwinrt_factory_get_statics(
		  L"Windows.Globalization.DateTimeFormatting.DateTimeFormatter", &CWINRT_IID_WGLDA_IDateTimeFormatterFactory,
		  ( void ** ) &factory
		)))
		return;
	if (SUCCEEDED(WindowsCreateString(tmpl, ( UINT32 ) wcslen(tmpl), ( HSTRING * ) &name))
	    && SUCCEEDED(wglda_date_time_formatter_factory_create_date_time_formatter(factory, name, &fmt))
	    && SUCCEEDED(wglda_date_time_formatter_format(fmt, when, &text))
	    && text)
		locale_utf8(WindowsGetStringRawBuffer(( HSTRING ) text, NULL), out, size);
	if (text) WindowsDeleteString(( HSTRING ) text);
	if (name) WindowsDeleteString(( HSTRING ) name);
	if (fmt) (( IUnknown * ) fmt)->lpVtbl->Release(( IUnknown * ) fmt);
	(( IUnknown * ) factory)->lpVtbl->Release(( IUnknown * ) factory);
}

void flux_locale_shortestday(int weekday, char *out, size_t size) {
	static LCTYPE const names [7] = {
	  LOCALE_SSHORTESTDAYNAME7, LOCALE_SSHORTESTDAYNAME1, LOCALE_SSHORTESTDAYNAME2, LOCALE_SSHORTESTDAYNAME3,
	  LOCALE_SSHORTESTDAYNAME4, LOCALE_SSHORTESTDAYNAME5, LOCALE_SSHORTESTDAYNAME6};
	wchar_t text [32] = {0};
	locale_info(names [weekday % 7], text, 32);
	locale_utf8(text, out, size);
}
