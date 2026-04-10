#include "fluxent/flux_theme.h"
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

struct FluxThemeManager {
    FluxThemeMode   mode;
    FluxThemeColors light;
    FluxThemeColors dark;
    FluxThemeColors current;  /* resolved copy */
    uint32_t        version;
};

static void init_light_palette(FluxThemeColors *c) {
    /* Control fill */
    c->ctrl_fill_default       = flux_color_rgba(255, 255, 255, 0xB3);
    c->ctrl_fill_secondary     = flux_color_rgba(249, 249, 249, 0x80);
    c->ctrl_fill_tertiary      = flux_color_rgba(249, 249, 249, 0x4D);
    c->ctrl_fill_disabled      = flux_color_rgba(249, 249, 249, 0x4D);
    c->ctrl_fill_input_active  = flux_color_rgba(255, 255, 255, 0xFF);

    /* Control alt fill */
    c->ctrl_alt_fill_secondary   = flux_color_rgba(0, 0, 0, 0x06);
    c->ctrl_alt_fill_tertiary    = flux_color_rgba(0, 0, 0, 0x0F);
    c->ctrl_alt_fill_quarternary = flux_color_rgba(0, 0, 0, 0x12);
    c->ctrl_alt_fill_disabled    = flux_color_rgba(0, 0, 0, 0x00);

    /* Control stroke */
    c->ctrl_stroke_default              = flux_color_rgba(0, 0, 0, 0x0F);
    c->ctrl_stroke_secondary            = flux_color_rgba(0, 0, 0, 0x29);
    c->ctrl_stroke_on_accent_default    = flux_color_rgba(255, 255, 255, 0x14);
    c->ctrl_stroke_on_accent_secondary  = flux_color_rgba(0, 0, 0, 0x66);
    c->ctrl_strong_stroke_default       = flux_color_rgba(0, 0, 0, 0x9C);
    c->ctrl_strong_stroke_disabled      = flux_color_rgba(0, 0, 0, 0x37);

    /* Solid fill */
    c->ctrl_solid_fill_default = flux_color_rgba(255, 255, 255, 0xFF);

    /* Strong fill */
    c->ctrl_strong_fill_default  = flux_color_rgba(0, 0, 0, 0x9C);
    c->ctrl_strong_fill_disabled = flux_color_rgba(0, 0, 0, 0x51);

    /* Accent */
    c->accent_default   = flux_color_rgb(0, 120, 212);
    c->accent_secondary = flux_color_rgba(0, 120, 212, 0xE6);
    c->accent_tertiary  = flux_color_rgba(0, 120, 212, 0xCC);
    c->accent_disabled  = flux_color_rgba(0, 0, 0, 0x37);

    /* Text */
    c->text_primary            = flux_color_rgba(0, 0, 0, 0xE4);
    c->text_secondary          = flux_color_rgba(0, 0, 0, 0x9E);
    c->text_disabled           = flux_color_rgba(0, 0, 0, 0x5C);
    c->text_on_accent_primary  = flux_color_rgb(255, 255, 255);
    c->text_on_accent_secondary = flux_color_rgba(255, 255, 255, 0xB3);
    c->text_on_accent_disabled = flux_color_rgb(255, 255, 255);

    /* Card & divider */
    c->card_bg_default       = flux_color_rgba(255, 255, 255, 0xB3);
    c->card_stroke_default   = flux_color_rgba(0, 0, 0, 0x0F);
    c->divider_stroke_default = flux_color_rgba(0, 0, 0, 0x14);

    /* Focus */
    c->focus_stroke_outer = flux_color_rgba(0, 0, 0, 0xE4);
    c->focus_stroke_inner = flux_color_rgb(255, 255, 255);

    /* Layer / background */
    c->layer_default    = flux_color_rgba(255, 255, 255, 0x80);
    c->layer_alt        = flux_color_rgb(255, 255, 255);
    c->solid_background = flux_color_rgb(243, 243, 243);
}

static void init_dark_palette(FluxThemeColors *c) {
    /* Control fill */
    c->ctrl_fill_default       = flux_color_rgba(255, 255, 255, 0x0F);
    c->ctrl_fill_secondary     = flux_color_rgba(255, 255, 255, 0x15);
    c->ctrl_fill_tertiary      = flux_color_rgba(255, 255, 255, 0x08);
    c->ctrl_fill_disabled      = flux_color_rgba(255, 255, 255, 0x0B);
    c->ctrl_fill_input_active  = flux_color_rgba(30, 30, 30, 0xB3);

    /* Control alt fill */
    c->ctrl_alt_fill_secondary   = flux_color_rgba(255, 255, 255, 0x06);
    c->ctrl_alt_fill_tertiary    = flux_color_rgba(255, 255, 255, 0x0F);
    c->ctrl_alt_fill_quarternary = flux_color_rgba(255, 255, 255, 0x12);
    c->ctrl_alt_fill_disabled    = flux_color_rgba(255, 255, 255, 0x00);

    /* Control stroke */
    c->ctrl_stroke_default              = flux_color_rgba(255, 255, 255, 0x12);
    c->ctrl_stroke_secondary            = flux_color_rgba(255, 255, 255, 0x18);
    c->ctrl_stroke_on_accent_default    = flux_color_rgba(255, 255, 255, 0x14);
    c->ctrl_stroke_on_accent_secondary  = flux_color_rgba(0, 0, 0, 0x23);
    c->ctrl_strong_stroke_default       = flux_color_rgba(255, 255, 255, 0x8B);
    c->ctrl_strong_stroke_disabled      = flux_color_rgba(255, 255, 255, 0x28);

    /* Solid fill */
    c->ctrl_solid_fill_default = flux_color_rgb(69, 69, 69);

    /* Strong fill */
    c->ctrl_strong_fill_default  = flux_color_rgba(255, 255, 255, 0x9C);
    c->ctrl_strong_fill_disabled = flux_color_rgba(255, 255, 255, 0x28);

    /* Accent */
    c->accent_default   = flux_color_rgb(51, 154, 244);
    c->accent_secondary = flux_color_rgba(51, 154, 244, 0xE6);
    c->accent_tertiary  = flux_color_rgba(51, 154, 244, 0xCC);
    c->accent_disabled  = flux_color_rgba(255, 255, 255, 0x28);

    /* Text */
    c->text_primary            = flux_color_rgb(255, 255, 255);
    c->text_secondary          = flux_color_rgba(255, 255, 255, 0xC5);
    c->text_disabled           = flux_color_rgba(255, 255, 255, 0x5D);
    c->text_on_accent_primary  = flux_color_rgb(0, 0, 0);
    c->text_on_accent_secondary = flux_color_rgba(0, 0, 0, 0x80);
    c->text_on_accent_disabled = flux_color_rgba(255, 255, 255, 0x87);

    /* Card & divider */
    c->card_bg_default       = flux_color_rgba(255, 255, 255, 0x0F);
    c->card_stroke_default   = flux_color_rgba(0, 0, 0, 0x19);
    c->divider_stroke_default = flux_color_rgba(255, 255, 255, 0x14);

    /* Focus */
    c->focus_stroke_outer = flux_color_rgb(255, 255, 255);
    c->focus_stroke_inner = flux_color_rgba(0, 0, 0, 0xB3);

    /* Layer / background */
    c->layer_default    = flux_color_rgba(58, 58, 58, 0x80);
    c->layer_alt        = flux_color_rgba(255, 255, 255, 0x06);
    c->solid_background = flux_color_rgb(32, 32, 32);
}

static void resolve_current(FluxThemeManager *tm) {
    bool dark = false;
    switch (tm->mode) {
    case FLUX_THEME_LIGHT:  dark = false; break;
    case FLUX_THEME_DARK:   dark = true;  break;
    case FLUX_THEME_SYSTEM: dark = flux_theme_system_is_dark(); break;
    }
    memcpy(&tm->current, dark ? &tm->dark : &tm->light, sizeof(FluxThemeColors));
}

FluxThemeManager *flux_theme_create(void) {
    FluxThemeManager *tm = calloc(1, sizeof(FluxThemeManager));
    if (!tm) return NULL;

    tm->mode = FLUX_THEME_LIGHT;
    tm->version = 1;

    init_light_palette(&tm->light);
    init_dark_palette(&tm->dark);
    resolve_current(tm);

    return tm;
}

void flux_theme_destroy(FluxThemeManager *tm) {
    free(tm);
}

void flux_theme_set_mode(FluxThemeManager *tm, FluxThemeMode mode) {
    if (!tm) return;
    tm->mode = mode;
    resolve_current(tm);
    tm->version++;
}

FluxThemeMode flux_theme_get_mode(const FluxThemeManager *tm) {
    if (!tm) return FLUX_THEME_LIGHT;
    return tm->mode;
}

const FluxThemeColors *flux_theme_colors(const FluxThemeManager *tm) {
    if (!tm) return NULL;
    return &tm->current;
}

uint32_t flux_theme_version(const FluxThemeManager *tm) {
    if (!tm) return 0;
    return tm->version;
}

bool flux_theme_system_is_dark(void) {
#ifdef _WIN32
    DWORD value = 1; /* default: light theme */
    DWORD size = sizeof(value);
    LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        NULL,
        &value,
        &size);
    if (status == ERROR_SUCCESS)
        return value == 0;
#endif
    return false;
}