#ifndef FLUX_ICON_H
#define FLUX_ICON_H

#include <stddef.h>
#include <string.h>

/*
 * Segoe Fluent Icons / MDL2 Assets codepoint map.
 *
 * Sorted by name for binary search lookup. Each entry maps a short
 * ASCII name to a wide-character string containing the Unicode
 * codepoint. Names support common aliases via duplicate entries.
 *
 * Usage:
 *   const wchar_t *glyph = flux_icon_lookup("Settings");
 *   if (glyph) { ... draw with font "Segoe Fluent Icons" ... }
 */

typedef struct FluxIconEntry {
    const char    *name;
    const wchar_t *codepoint;
} FluxIconEntry;

/*
 * Master table — MUST be sorted by name (case-sensitive, strcmp order).
 * Keep alphabetical when adding new entries.
 */
static const FluxIconEntry kFluxIconTable[] = {
    { "Accept",         L"\xE8FB" },
    { "Add",            L"\xE710" },
    { "Back",           L"\xE72B" },
    { "Camera",         L"\xE722" },
    { "Cancel",         L"\xE711" },
    { "Check",          L"\xE8FB" },
    { "CheckMark",      L"\xE73E" },
    { "ChevronDown",    L"\xE70D" },
    { "ChevronLeft",    L"\xE76B" },
    { "ChevronRight",   L"\xE76C" },
    { "ChevronUp",      L"\xE70E" },
    { "Close",          L"\xE711" },
    { "Copy",           L"\xE8C8" },
    { "Cut",            L"\xE8C6" },
    { "Delete",         L"\xE74D" },
    { "Download",       L"\xE896" },
    { "Edit",           L"\xE70F" },
    { "Emoji",          L"\xE76E" },
    { "Favorite",       L"\xE734" },
    { "Filter",         L"\xE71C" },
    { "Flag",           L"\xE7C1" },
    { "Folder",         L"\xE8B7" },
    { "Font",           L"\xE8D2" },
    { "Forward",        L"\xE72A" },
    { "Gear",           L"\xE713" },
    { "GlobalNavButton",L"\xE700" },
    { "Heart",          L"\xEB51" },
    { "Help",           L"\xE897" },
    { "Hide",           L"\xED1A" },
    { "Home",           L"\xE80F" },
    { "Import",         L"\xE8B5" },
    { "Info",           L"\xE946" },
    { "Keyboard",       L"\xE765" },
    { "Like",           L"\xEB51" },
    { "Link",           L"\xE71B" },
    { "List",           L"\xEA37" },
    { "Location",       L"\xE81D" },
    { "Lock",           L"\xE72E" },
    { "Mail",           L"\xE715" },
    { "Mic",            L"\xE720" },
    { "Minus",          L"\xE738" },
    { "More",           L"\xE712" },
    { "MoveToFolder",   L"\xE8DE" },
    { "Mute",           L"\xE74F" },
    { "NewFolder",      L"\xE8F4" },
    { "Next",           L"\xE893" },
    { "OpenFile",       L"\xE8E5" },
    { "Paste",          L"\xE77F" },
    { "Pause",          L"\xE769" },
    { "People",         L"\xE716" },
    { "Phone",          L"\xE717" },
    { "Pin",            L"\xE718" },
    { "Play",           L"\xE768" },
    { "Plus",           L"\xE710" },
    { "Previous",       L"\xE892" },
    { "Print",          L"\xE749" },
    { "RedEye",         L"\xE7B3" },
    { "Redo",           L"\xE7A6" },
    { "Refresh",        L"\xE72C" },
    { "Rename",         L"\xE8AC" },
    { "Repair",         L"\xE90F" },
    { "Save",           L"\xE74E" },
    { "Search",         L"\xE721" },
    { "Send",           L"\xE724" },
    { "Settings",       L"\xE713" },
    { "Share",          L"\xE72D" },
    { "Sort",           L"\xE8CB" },
    { "Sound",          L"\xE767" },
    { "Star",           L"\xE734" },
    { "Stop",           L"\xE71A" },
    { "Sync",           L"\xE895" },
    { "Trash",          L"\xE74D" },
    { "Undo",           L"\xE7A7" },
    { "Unlock",         L"\xE785" },
    { "Upload",         L"\xE898" },
    { "Video",          L"\xE714" },
    { "View",           L"\xE890" },
    { "Volume",         L"\xE767" },
    { "Warning",        L"\xE7BA" },
    { "ZoomIn",         L"\xE8A3" },
    { "ZoomOut",        L"\xE71F" },
};

#define FLUX_ICON_COUNT (sizeof(kFluxIconTable) / sizeof(kFluxIconTable[0]))

/*
 * Binary search lookup by name.
 * Returns the wide-string codepoint, or NULL if not found.
 */
static inline const wchar_t *flux_icon_lookup(const char *name) {
    if (!name || !name[0])
        return NULL;

    int lo = 0;
    int hi = (int)FLUX_ICON_COUNT - 1;

    while (lo <= hi) {
        int mid = lo + (hi - lo) / 2;
        int cmp = strcmp(name, kFluxIconTable[mid].name);
        if (cmp == 0)
            return kFluxIconTable[mid].codepoint;
        if (cmp < 0)
            hi = mid - 1;
        else
            lo = mid + 1;
    }

    return NULL;
}

/*
 * Convenience: well-known glyphs used directly by controls
 * without going through the name lookup.
 */
#define FLUX_ICON_CHECKMARK   L"\xE73E"
#define FLUX_ICON_CHEVRON_DN  L"\xE70D"
#define FLUX_ICON_CLOSE       L"\xE711"
#define FLUX_ICON_SEARCH      L"\xE721"
#define FLUX_ICON_SETTINGS    L"\xE713"
#define FLUX_ICON_ADD         L"\xE710"

#endif /* FLUX_ICON_H */