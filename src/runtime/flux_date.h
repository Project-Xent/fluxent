#ifndef FLUX_DATE_H
#define FLUX_DATE_H

#include <stdbool.h>
#include <stddef.h>

static int inline flux_date_pack(int year, int month, int day) { return year * 10000 + month * 100 + day; }

static int inline flux_date_year(int date) { return date / 10000; }

static int inline flux_date_month(int date) { return date / 100 % 100; }

static int inline flux_date_day(int date) { return date % 100; }

bool flux_date_valid(int date);
int  flux_date_daysin(int year, int month);

int  flux_date_weekday(int date);

int  flux_date_shift(int date, int days);

int  flux_date_shiftmonth(int date, int months);

int  flux_date_between(int from, int to);

int  flux_date_today(void);

int  flux_date_now(void);

int  flux_locale_firstday(void);

void flux_locale_dateorder(int order [3]);
void flux_locale_timeorder(int order [3]);

bool flux_locale_isclock24(void);

void flux_locale_ampm(char *am, char *pm, size_t size);

void flux_locale_formatdate(int date, wchar_t const *picture, char *out, size_t size);

void flux_locale_formattemplate(int date, wchar_t const *tmpl, char *out, size_t size);

void flux_locale_shortestday(int weekday, char *out, size_t size);

#endif
