#include "controls/behavior/flux_list_internal.h"

#include <stdlib.h>
#include <string.h>

void        flux_ranges_clear(FluxListViewData *ld) { ld->range_count = 0; }

static bool ranges_reserve(FluxListViewData *ld, int count) {
	if (count <= ld->range_cap) return true;
	int cap = ld->range_cap ? ld->range_cap * 2 : 8;
	if (cap < count) cap = count;
	FluxListSelRange *r = ( FluxListSelRange * ) realloc(ld->ranges, sizeof(*r) * ( size_t ) cap);
	if (!r) return false;
	ld->ranges    = r;
	ld->range_cap = cap;
	return true;
}

bool flux_ranges_contains(FluxListViewData const *ld, int index) {
	for (int i = 0; i < ld->range_count; i++)
		if (index >= ld->ranges [i].first && index <= ld->ranges [i].last) return true;
	return false;
}

void flux_ranges_add(FluxListViewData *ld, int first, int last) {
	if (!ranges_reserve(ld, ld->range_count + 1)) return;

	int pos = 0;
	while (pos < ld->range_count && ld->ranges [pos].last < first - 1) pos++;

	int merged_first = first, merged_last = last;
	int span = 0;
	while (pos + span < ld->range_count && ld->ranges [pos + span].first <= last + 1) {
		if (ld->ranges [pos + span].first < merged_first) merged_first = ld->ranges [pos + span].first;
		if (ld->ranges [pos + span].last > merged_last) merged_last = ld->ranges [pos + span].last;
		span++;
	}

	memmove(
	  &ld->ranges [pos + 1], &ld->ranges [pos + span], sizeof(*ld->ranges) * ( size_t ) (ld->range_count - pos - span)
	);
	ld->ranges [pos]  = (FluxListSelRange) {merged_first, merged_last};
	ld->range_count  += 1 - span;
}

static void ranges_split(FluxListViewData *ld, int i, int index) {
	if (!ranges_reserve(ld, ld->range_count + 1)) return;
	memmove(&ld->ranges [i + 1], &ld->ranges [i], sizeof(*ld->ranges) * ( size_t ) (ld->range_count - i));
	ld->ranges [i].last      = index - 1;
	ld->ranges [i + 1].first = index + 1;
	ld->range_count++;
}

void flux_ranges_remove(FluxListViewData *ld, int index) {
	int i = 0;
	while (i < ld->range_count && (index < ld->ranges [i].first || index > ld->ranges [i].last)) i++;
	if (i == ld->range_count) return;

	FluxListSelRange *r = &ld->ranges [i];
	if (r->first == r->last) {
		memmove(&ld->ranges [i], &ld->ranges [i + 1], sizeof(*r) * ( size_t ) (ld->range_count - i - 1));
		ld->range_count--;
		return;
	}
	if (index == r->first) {
		r->first++;
		return;
	}
	if (index == r->last) {
		r->last--;
		return;
	}
	ranges_split(ld, i, index);
}

void flux_ranges_cut(FluxListViewData *ld, int lo, int hi) {
	FluxListSelRange *r = ld->ranges;
	int               n = ld->range_count, i = 0;
	while (i < n && r [i].last < lo) i++;
	if (i < n && r [i].first < lo && r [i].last > hi) {
		ranges_split(ld, i, lo);
		ld->ranges [i + 1].first = hi + 1;
		return;
	}
	if (i < n && r [i].first < lo) r [i++].last = lo - 1;
	int j = i;
	while (j < n && r [j].last <= hi) j++;
	if (j < n && r [j].first <= hi) r [j].first = hi + 1;
	memmove(&r [i], &r [j], sizeof(*r) * ( size_t ) (n - j));
	ld->range_count -= j - i;
}

void flux_ranges_xor(FluxListViewData *ld, int lo, int hi) {
	int  at = lo, count = ld->range_count;
	int *gaps = ( int * ) malloc(sizeof(int) * 2u * ( size_t ) (count + 1));
	if (!gaps) return;
	int ngap = 0;
	for (int i = 0; i < count && at <= hi; i++) {
		FluxListSelRange r = ld->ranges [i];
		if (r.last < at || r.first > hi) continue;
		if (r.first > at) {
			gaps [ngap * 2]     = at;
			gaps [ngap * 2 + 1] = r.first - 1;
			ngap++;
		}
		at = r.last + 1;
	}
	if (at <= hi) {
		gaps [ngap * 2]     = at;
		gaps [ngap * 2 + 1] = hi;
		ngap++;
	}
	flux_ranges_cut(ld, lo, hi);
	for (int g = 0; g < ngap; g++) flux_ranges_add(ld, gaps [g * 2], gaps [g * 2 + 1]);
	free(gaps);
}
