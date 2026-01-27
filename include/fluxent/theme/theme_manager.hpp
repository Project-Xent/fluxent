#pragma once

#include "../types.hpp"
#include <functional>
#include <vector>

namespace fluxent::theme {

struct ElevationTheme {
  float Height;
  float GradientStop1;
  float GradientStop2;
};

struct FocusTheme {
  float OuterThickness;
  float InnerThickness;
  float OuterPadding;
  float InnerPadding;
};

struct CheckBoxTheme {
  float Size;
  float Gap;
  float CornerRadius;
  float GlyphFontSize;
};

struct RadioButtonTheme {
  float Size;
  float Gap;
  float GlyphSize;
  float GlyphSizePointerOver;
  float GlyphSizePressed;
};

struct TextBoxTheme {
  float MinHeight;
  float MinWidth;
  float PaddingLeft;
  float PaddingTop;
  float PaddingRight;
  float PaddingBottom;
};

struct ThemeResources {
  ElevationTheme Elevation;

  FocusTheme Focus;

  CheckBoxTheme CheckBox;
  RadioButtonTheme RadioButton;
  TextBoxTheme TextBox;

  float ControlCornerRadius;

  Color TextPrimary;
  Color TextSecondary;
  Color TextTertiary;
  Color TextDisabled;
  Color TextInverse;

  Color TextOnAccentPrimary;
  Color TextOnAccentSecondary;
  Color TextOnAccentDisabled;

  Color ControlFillDefault;
  Color ControlFillSecondary;
  Color ControlFillTertiary;
  Color ControlFillQuarternary;
  Color ControlFillDisabled;
  Color ControlFillTransparent;
  Color ControlFillInputActive;

  Color ControlStrongFillDefault;
  Color ControlStrongFillDisabled;

  Color ControlSolidFillDefault;

  Color SubtleFillTransparent;
  Color SubtleFillSecondary;
  Color SubtleFillTertiary;
  Color SubtleFillDisabled;

  Color ControlAltFillTransparent;
  Color ControlAltFillSecondary;
  Color ControlAltFillTertiary;
  Color ControlAltFillQuarternary;
  Color ControlAltFillDisabled;

  Color ControlOnImageFillDefault;
  Color ControlOnImageFillSecondary;
  Color ControlOnImageFillTertiary;
  Color ControlOnImageFillDisabled;

  Color AccentDefault;
  Color AccentSecondary;
  Color AccentTertiary;
  Color AccentDisabled;

  Color ControlStrokeDefault;
  Color ControlStrokeSecondary;
  Color ControlStrokeOnAccentDefault;
  Color ControlStrokeOnAccentSecondary;
  Color ControlStrokeOnAccentTertiary;
  Color ControlStrokeOnAccentDisabled;
  Color ControlStrokeForStrongFillWhenOnImage;

  Color ControlStrongStrokeDefault;
  Color ControlStrongStrokeDisabled;

  Color CardStrokeDefault;
  Color CardStrokeDefaultSolid;
  Color CardBackgroundDefault;
  Color CardBackgroundSecondary;
  Color CardBackgroundTertiary;

  Color SurfaceStrokeDefault;
  Color SurfaceStrokeFlyout;
  Color SurfaceStrokeInverse;

  Color FocusStrokeOuter;
  Color FocusStrokeInner;

  Color DividerStrokeDefault;

  Color SolidBackgroundBase;
  Color SolidBackgroundSecondary;
  Color SolidBackgroundTertiary;
  Color SolidBackgroundQuarternary;
  Color SolidBackgroundQuinary;
  Color SolidBackgroundSenary;
  Color SolidBackgroundTransparent;
  Color SolidBackgroundBaseAlt;
  Color LayerFillDefault;
  Color LayerFillAlt;
  Color LayerOnAcrylicFillDefault;
  Color LayerOnAccentAcrylicFillDefault;
  Color LayerOnMicaBaseAltDefault;
  Color LayerOnMicaBaseAltSecondary;
  Color LayerOnMicaBaseAltTertiary;
  Color LayerOnMicaBaseAltTransparent;
  Color SmokeFillDefault;

  Color SystemSuccess;
  Color SystemCaution;
  Color SystemCritical;
  Color SystemNeutral;
  Color SystemSolidNeutral;
  Color SystemAttentionBackground;
  Color SystemSuccessBackground;
  Color SystemCautionBackground;
  Color SystemCriticalBackground;
  Color SystemNeutralBackground;
  Color SystemSolidAttentionBackground;
  Color SystemSolidNeutralBackground;
};

enum class Mode { Dark, Light, HighContrast };

struct AccentPalette {
  Color base;
  Color light1;
  Color light2;
  Color light3;
  Color dark1;
  Color dark2;
  Color dark3;

  static AccentPalette Default() {
    return AccentPalette{
        .base = Color{0, 120, 212, 255},     // #0078D4
        .light1 = Color{26, 137, 228, 255},  // #1A89E4
        .light2 = Color{51, 154, 244, 255},  // #339AF4
        .light3 = Color{102, 178, 255, 255}, // #66B2FF
        .dark1 = Color{0, 102, 180, 255},    // #0066B4
        .dark2 = Color{0, 84, 148, 255},     // #005494
        .dark3 = Color{0, 66, 116, 255},     // #004274
    };
  }
};

class ThemeManager {
public:
  ThemeManager();

  Mode GetMode() const { return mode_; }
  void SetMode(Mode mode);

  const ThemeResources &Resources() const { return resources_; }

  const AccentPalette &GetAccent() const { return accent_; }
  void SetAccent(const AccentPalette &palette);
  void SetAccentColor(const Color &base);

  uint64_t Version() const { return version_; }

  using ThemeChangedCallback = std::function<void(Mode)>;
  size_t AddThemeChangedListener(ThemeChangedCallback callback);
  void RemoveThemeChangedListener(size_t id);

private:
  void ApplyDarkTheme();
  void ApplyLightTheme();
  void UpdateAccentResources();

  static Color AdjustBrightness(const Color &c, float factor);
  static uint8_t ClampColor(int v);

  Mode mode_ = Mode::Dark;
  ThemeResources resources_;
  AccentPalette accent_ = AccentPalette::Default();
  std::vector<std::pair<size_t, ThemeChangedCallback>> theme_changed_listeners_;
  size_t next_listener_id_ = 1;
  uint64_t version_ = 0;
};

} // namespace fluxent::theme
