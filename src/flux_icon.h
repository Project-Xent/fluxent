/**
 * @file flux_icon.h
 * @brief Segoe Fluent Icons / MDL2 Assets codepoint lookup.
 *
 * Provides a sorted table mapping icon names to Unicode codepoints for use
 * with the "Segoe Fluent Icons" font. Includes binary search lookup and
 * UTF-8 conversion utilities.
 *
 * @code
 * const wchar_t *glyph = flux_icon_lookup("Settings");
 * char utf8[8];
 * flux_icon_to_utf8(glyph, utf8, sizeof(utf8));
 * // Draw utf8 with "Segoe Fluent Icons" font
 * @endcode
 */
#ifndef FLUX_ICON_H
#define FLUX_ICON_H

#include <stddef.h>
#include <string.h>

/** @brief Icon table entry mapping name to codepoint. */
typedef struct FluxIconEntry {
	char const    *name;      /**< ASCII icon name */
	wchar_t const *codepoint; /**< Unicode codepoint string */
} FluxIconEntry;

/*
 * Master table — MUST be sorted by name (case-sensitive, strcmp order).
 * Keep alphabetical when adding new entries.
 */
static FluxIconEntry const kFluxIconTable [] = {
  {"Accept",          L"\xE8FB"},
  {"Add",			 L"\xE710"},
  {"Back",			L"\xE72B"},
  {"Camera",          L"\xE722"},
  {"Cancel",          L"\xE711"},
  {"Check",           L"\xE8FB"},
  {"CheckMark",       L"\xE73E"},
  {"ChevronDown",     L"\xE70D"},
  {"ChevronLeft",     L"\xE76B"},
  {"ChevronRight",    L"\xE76C"},
  {"ChevronUp",       L"\xE70E"},
  {"Close",           L"\xE711"},
  {"Copy",			L"\xE8C8"},
  {"Cut",			 L"\xE8C6"},
  {"Delete",          L"\xE74D"},
  {"Download",        L"\xE896"},
  {"Edit",			L"\xE70F"},
  {"Emoji",           L"\xE76E"},
  {"Favorite",        L"\xE734"},
  {"Filter",          L"\xE71C"},
  {"Flag",			L"\xE7C1"},
  {"Folder",          L"\xE8B7"},
  {"Font",			L"\xE8D2"},
  {"Forward",         L"\xE72A"},
  {"Gear",			L"\xE713"},
  {"GlobalNavButton", L"\xE700"},
  {"Heart",           L"\xEB51"},
  {"Help",			L"\xE897"},
  {"Hide",			L"\xED1A"},
  {"Home",			L"\xE80F"},
  {"Import",          L"\xE8B5"},
  {"Info",			L"\xE946"},
  {"Keyboard",        L"\xE765"},
  {"Like",			L"\xEB51"},
  {"Link",			L"\xE71B"},
  {"List",			L"\xEA37"},
  {"Location",        L"\xE81D"},
  {"Lock",			L"\xE72E"},
  {"Mail",			L"\xE715"},
  {"Mic",			 L"\xE720"},
  {"Minus",           L"\xE738"},
  {"More",			L"\xE712"},
  {"MoveToFolder",    L"\xE8DE"},
  {"Mute",			L"\xE74F"},
  {"NewFolder",       L"\xE8F4"},
  {"Next",			L"\xE893"},
  {"OpenFile",        L"\xE8E5"},
  {"Paste",           L"\xE77F"},
  {"Pause",           L"\xE769"},
  {"People",          L"\xE716"},
  {"Phone",           L"\xE717"},
  {"Pin",			 L"\xE718"},
  {"Play",			L"\xE768"},
  {"Plus",			L"\xE710"},
  {"Previous",        L"\xE892"},
  {"Print",           L"\xE749"},
  {"RedEye",          L"\xE7B3"},
  {"Redo",			L"\xE7A6"},
  {"Refresh",         L"\xE72C"},
  {"Rename",          L"\xE8AC"},
  {"Repair",          L"\xE90F"},
  {"Save",			L"\xE74E"},
  {"Search",          L"\xE721"},
  {"Send",			L"\xE724"},
  {"Settings",        L"\xE713"},
  {"Share",           L"\xE72D"},
  {"Sort",			L"\xE8CB"},
  {"Sound",           L"\xE767"},
  {"Star",			L"\xE734"},
  {"Stop",			L"\xE71A"},
  {"Sync",			L"\xE895"},
  {"Trash",           L"\xE74D"},
  {"Undo",			L"\xE7A7"},
  {"Unlock",          L"\xE785"},
  {"Upload",          L"\xE898"},
  {"Video",           L"\xE714"},
  {"View",			L"\xE890"},
  {"Volume",          L"\xE767"},
  {"Warning",         L"\xE7BA"},
  {"ZoomIn",          L"\xE8A3"},
  {"ZoomOut",         L"\xE71F"},
};

/** @brief Number of entries in kFluxIconTable. */
#define FLUX_ICON_COUNT (sizeof(kFluxIconTable) / sizeof(kFluxIconTable [0]))

/**
 * @brief Look up an icon by name using binary search.
 * @param name Icon name (case-sensitive, e.g. "Settings").
 * @return Wide-string codepoint, or NULL if not found.
 */
static wchar_t const inline *flux_icon_lookup(char const *name) {
	if (!name || !name [0]) return NULL;

	int lo = 0;
	int hi = ( int ) FLUX_ICON_COUNT - 1;

		while (lo <= hi) {
			int mid = lo + (hi - lo) / 2;
			int cmp = strcmp(name, kFluxIconTable [mid].name);
			if (cmp == 0) return kFluxIconTable [mid].codepoint;
			if (cmp < 0) hi = mid - 1;
			else lo = mid + 1;
		}

	return NULL;
}

/*
 * Convenience: well-known glyphs used directly by controls
 * without going through the name lookup.
 */
#define FLUX_ICON_CHECKMARK  L"\xE73E"
#define FLUX_ICON_CHEVRON_DN L"\xE70D"
#define FLUX_ICON_CLOSE      L"\xE711"
#define FLUX_ICON_SEARCH     L"\xE721"
#define FLUX_ICON_SETTINGS   L"\xE713"
#define FLUX_ICON_ADD        L"\xE710"

/**
 * @brief Convert a wide-char icon codepoint to UTF-8.
 * @param wc  Wide-char string (BMP codepoint).
 * @param out Output buffer (must be at least 4 bytes).
 * @param cap Capacity of output buffer.
 * @return Number of bytes written (excluding NUL), or 0 on error.
 */
static int inline flux_icon_to_utf8(wchar_t const *wc, char *out, int cap) {
		if (!wc || !wc [0] || cap < 4) {
			if (out && cap > 0) out [0] = '\0';
			return 0;
		}
	unsigned int cp = ( unsigned int ) wc [0];
	int          n  = 0;
		if (cp < 0x80) { out [n++] = ( char ) cp; }
		else if (cp < 0x800) {
			out [n++] = ( char ) (0xc0 | (cp >> 6));
			out [n++] = ( char ) (0x80 | (cp & 0x3f));
		}
		else {
			out [n++] = ( char ) (0xe0 | (cp >> 12));
			out [n++] = ( char ) (0x80 | ((cp >> 6) & 0x3f));
			out [n++] = ( char ) (0x80 | (cp & 0x3f));
		}
	out [n] = '\0';
	return n;
}

#endif /* FLUX_ICON_H */
