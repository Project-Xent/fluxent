// FluXent Theme Manager implementation
#include "fluxent/theme/theme_manager.hpp"
#include "fluxent/theme/colors.hpp"

#include <xent/button.hpp>

#include <algorithm>

namespace fluxent::theme {

static xent::Color ToXentColor(const Color &c) {
  return xent::Color{
      static_cast<std::uint8_t>(c.r),
      static_cast<std::uint8_t>(c.g),
      static_cast<std::uint8_t>(c.b),
      static_cast<std::uint8_t>(c.a),
  };
}

static void SyncXentButtonPaletteFromTheme(const ThemeResources &r) {
  xent::ButtonPalette p;
  // Map semantic roles to Fluent/WinUI-ish theme tokens.
  p.primary = ToXentColor(r.AccentDefault);
  // Secondary should behave like a default WinUI3 button (i.e., no explicit
  // background override).
  p.secondary = ToXentColor(r.ControlFillTransparent);
  p.success = ToXentColor(r.SystemSuccess);
  p.warning = ToXentColor(r.SystemCaution);
  // Danger should follow WinUI system critical color:
  // Light: deep red, Dark: light pink (see
  // CommonStyles/Common_themeresources_any.xaml).
  p.danger = ToXentColor(r.SystemCritical);
  xent::SetButtonPalette(p);
}

ThemeManager::ThemeManager() { ApplyDarkTheme(); }

size_t ThemeManager::AddThemeChangedListener(ThemeChangedCallback callback) {
  const size_t id = next_listener_id_++;
  theme_changed_listeners_.push_back({id, std::move(callback)});
  return id;
}

void ThemeManager::RemoveThemeChangedListener(size_t id) {
  theme_changed_listeners_.erase(
      std::remove_if(theme_changed_listeners_.begin(),
                     theme_changed_listeners_.end(),
                     [id](const auto &p) { return p.first == id; }),
      theme_changed_listeners_.end());
}

void ThemeManager::SetMode(Mode mode) {
  if (mode_ == mode)
    return;
  mode_ = mode;

  switch (mode) {
  case Mode::Dark:
    ApplyDarkTheme();
    break;
  case Mode::Light:
    ApplyLightTheme();
    break;
  case Mode::HighContrast:
    ApplyDarkTheme();
    break;
  }

  UpdateAccentResources();
  version_++;

  for (auto &[id, cb] : theme_changed_listeners_) {
    if (cb)
      cb(mode);
  }
}

void ThemeManager::SetAccent(const AccentPalette &palette) {
  accent_ = palette;
  UpdateAccentResources();
  version_++;
}

// Helper functions

uint8_t ThemeManager::ClampColor(int v) {
  return static_cast<uint8_t>(v > 255 ? 255 : (v < 0 ? 0 : v));
}

Color ThemeManager::AdjustBrightness(const Color &c, float factor) {
  if (factor > 1.0f) {
    float t = factor - 1.0f;
    return Color{ClampColor(static_cast<int>(c.r + (255 - c.r) * t)),
                 ClampColor(static_cast<int>(c.g + (255 - c.g) * t)),
                 ClampColor(static_cast<int>(c.b + (255 - c.b) * t)), c.a};
  } else {
    return Color{ClampColor(static_cast<int>(c.r * factor)),
                 ClampColor(static_cast<int>(c.g * factor)),
                 ClampColor(static_cast<int>(c.b * factor)), c.a};
  }
}

// Theme setup

void ThemeManager::SetAccentColor(const Color &base) {
  accent_.base = base;
  accent_.light1 = AdjustBrightness(base, 1.15f);
  accent_.light2 = AdjustBrightness(base, 1.30f);
  accent_.light3 = AdjustBrightness(base, 1.50f);
  accent_.dark1 = AdjustBrightness(base, 0.85f);
  accent_.dark2 = AdjustBrightness(base, 0.70f);
  accent_.dark3 = AdjustBrightness(base, 0.55f);

  UpdateAccentResources();
  version_++;
}

void ThemeManager::UpdateAccentResources() {
  if (mode_ == Mode::Dark) {
    resources_.AccentDefault = accent_.light2;
    resources_.AccentSecondary =
        Color{accent_.light2.r, accent_.light2.g, accent_.light2.b,
              static_cast<uint8_t>(accent_.light2.a * 0.9f)};
    resources_.AccentTertiary =
        Color{accent_.light2.r, accent_.light2.g, accent_.light2.b,
              static_cast<uint8_t>(accent_.light2.a * 0.8f)};
  } else {
    resources_.AccentDefault = accent_.dark1;
    resources_.AccentSecondary =
        Color{accent_.dark1.r, accent_.dark1.g, accent_.dark1.b,
              static_cast<uint8_t>(accent_.dark1.a * 0.9f)};
    resources_.AccentTertiary =
        Color{accent_.dark1.r, accent_.dark1.g, accent_.dark1.b,
              static_cast<uint8_t>(accent_.dark1.a * 0.8f)};
  }

  // Keep xent-core semantic role colors aligned to the active theme.
  SyncXentButtonPaletteFromTheme(resources_);
}

void ThemeManager::ApplyDarkTheme() {
  resources_ = kDarkPalette;
  UpdateAccentResources();
  version_++;
}

void ThemeManager::ApplyLightTheme() {
  resources_ = kLightPalette;
  UpdateAccentResources();
  version_++;
}

} // namespace fluxent::theme
