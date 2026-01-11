#pragma once

// FluXent theme colors (based on WinUI 3 Common_themeresources_any.xaml)

#include "../types.hpp"

namespace fluxent::theme {

// Theme mode
enum class Mode {
    Dark,
    Light,
    HighContrast
};

// Dark theme colors
namespace dark {
    // Text
    constexpr Color TextPrimary{255, 255, 255, 255};           // #FFFFFF
    constexpr Color TextSecondary{255, 255, 255, 197};         // #C5FFFFFF
    constexpr Color TextTertiary{255, 255, 255, 135};          // #87FFFFFF
    constexpr Color TextDisabled{255, 255, 255, 93};           // #5DFFFFFF
    constexpr Color TextInverse{0, 0, 0, 228};                 // #E4000000
    
    // On-accent text
    constexpr Color TextOnAccentPrimary{0, 0, 0, 255};         // #000000
    constexpr Color TextOnAccentSecondary{0, 0, 0, 128};       // #80000000
    constexpr Color TextOnAccentDisabled{255, 255, 255, 135};  // #87FFFFFF
    
    // Control fill
    constexpr Color ControlFillDefault{255, 255, 255, 15};     // #0FFFFFFF
    constexpr Color ControlFillSecondary{255, 255, 255, 21};   // #15FFFFFF
    constexpr Color ControlFillTertiary{255, 255, 255, 8};     // #08FFFFFF
    constexpr Color ControlFillQuarternary{255, 255, 255, 15}; // #0FFFFFFF
    constexpr Color ControlFillDisabled{255, 255, 255, 11};    // #0BFFFFFF
    constexpr Color ControlFillTransparent{255, 255, 255, 0};  // #00FFFFFF
    constexpr Color ControlFillInputActive{30, 30, 30, 179};   // #B31E1E1E
    
    // Strong fill
    constexpr Color ControlStrongFillDefault{255, 255, 255, 139};  // #8BFFFFFF
    constexpr Color ControlStrongFillDisabled{255, 255, 255, 63};  // #3FFFFFFF
    
    // Solid fill
    constexpr Color ControlSolidFillDefault{69, 69, 69, 255};  // #454545
    
    // Subtle fill
    constexpr Color SubtleFillTransparent{255, 255, 255, 0};   // #00FFFFFF
    constexpr Color SubtleFillSecondary{255, 255, 255, 15};    // #0FFFFFFF
    constexpr Color SubtleFillTertiary{255, 255, 255, 10};     // #0AFFFFFF
    constexpr Color SubtleFillDisabled{255, 255, 255, 0};      // #00FFFFFF
    
    // Alt fill
    constexpr Color ControlAltFillTransparent{255, 255, 255, 0};   // #00FFFFFF
    constexpr Color ControlAltFillSecondary{0, 0, 0, 25};          // #19000000
    constexpr Color ControlAltFillTertiary{255, 255, 255, 11};     // #0BFFFFFF
    constexpr Color ControlAltFillQuarternary{255, 255, 255, 18};  // #12FFFFFF
    constexpr Color ControlAltFillDisabled{255, 255, 255, 0};       // #00FFFFFF

    // On-image fill
    constexpr Color ControlOnImageFillDefault{28, 28, 28, 179};     // #B31C1C1C
    constexpr Color ControlOnImageFillSecondary{26, 26, 26, 255};   // #1A1A1A
    constexpr Color ControlOnImageFillTertiary{19, 19, 19, 255};    // #131313
    constexpr Color ControlOnImageFillDisabled{30, 30, 30, 255};    // #1E1E1E
    
    // Accent fill
    constexpr Color AccentFillDisabled{255, 255, 255, 40};     // #28FFFFFF
    
    // Strokes
    constexpr Color ControlStrokeDefault{255, 255, 255, 18};   // #12FFFFFF
    constexpr Color ControlStrokeSecondary{255, 255, 255, 24}; // #18FFFFFF
    constexpr Color ControlStrokeOnAccentDefault{255, 255, 255, 20};   // #14FFFFFF
    constexpr Color ControlStrokeOnAccentSecondary{0, 0, 0, 35};       // #23000000
    constexpr Color ControlStrokeOnAccentTertiary{0, 0, 0, 55};        // #37000000
    constexpr Color ControlStrokeOnAccentDisabled{0, 0, 0, 51};        // #33000000
    constexpr Color ControlStrokeForStrongFillWhenOnImage{0, 0, 0, 107}; // #6B000000
    
    // Strong strokes
    constexpr Color ControlStrongStrokeDefault{255, 255, 255, 139};    // #8BFFFFFF
    constexpr Color ControlStrongStrokeDisabled{255, 255, 255, 40};    // #28FFFFFF
    
    // Card strokes
    constexpr Color CardStrokeDefault{0, 0, 0, 25};            // #19000000
    constexpr Color CardStrokeDefaultSolid{28, 28, 28, 255};   // #1C1C1C
    
    // Surface strokes
    constexpr Color SurfaceStrokeDefault{117, 117, 117, 102};  // #66757575
    constexpr Color SurfaceStrokeFlyout{0, 0, 0, 51};          // #33000000
    constexpr Color SurfaceStrokeInverse{0, 0, 0, 15};          // #0F000000
    
    // Dividers
    constexpr Color DividerStrokeDefault{255, 255, 255, 21};   // #15FFFFFF
    
    // Focus
    constexpr Color FocusStrokeOuter{255, 255, 255, 255};      // #FFFFFF
    constexpr Color FocusStrokeInner{0, 0, 0, 179};            // #B3000000
    
    // Card backgrounds
    constexpr Color CardBackgroundDefault{255, 255, 255, 13};      // #0DFFFFFF
    constexpr Color CardBackgroundSecondary{255, 255, 255, 8};     // #08FFFFFF
    constexpr Color CardBackgroundTertiary{255, 255, 255, 18};     // #12FFFFFF
    
    // Overlays
    constexpr Color SmokeFillDefault{0, 0, 0, 77};             // #4D000000
    
    // Layers
    constexpr Color LayerFillDefault{58, 58, 58, 76};          // #4C3A3A3A
    constexpr Color LayerFillAlt{255, 255, 255, 13};           // #0DFFFFFF
    constexpr Color LayerOnAcrylicFillDefault{255, 255, 255, 9};   // #09FFFFFF
    constexpr Color LayerOnAccentAcrylicFillDefault{255, 255, 255, 9}; // #09FFFFFF
    constexpr Color LayerOnMicaBaseAltDefault{58, 58, 58, 115};    // #733A3A3A
    constexpr Color LayerOnMicaBaseAltSecondary{255, 255, 255, 15}; // #0FFFFFFF
    constexpr Color LayerOnMicaBaseAltTertiary{44, 44, 44, 255};   // #2C2C2C
    constexpr Color LayerOnMicaBaseAltTransparent{255, 255, 255, 0}; // #00FFFFFF
    
    // Solid backgrounds
    constexpr Color SolidBackgroundBase{32, 32, 32, 255};          // #202020
    constexpr Color SolidBackgroundSecondary{28, 28, 28, 255};     // #1C1C1C
    constexpr Color SolidBackgroundTertiary{40, 40, 40, 255};      // #282828
    constexpr Color SolidBackgroundQuarternary{44, 44, 44, 255};   // #2C2C2C
    constexpr Color SolidBackgroundQuinary{51, 51, 51, 255};        // #333333
    constexpr Color SolidBackgroundSenary{55, 55, 55, 255};         // #373737
    constexpr Color SolidBackgroundTransparent{32, 32, 32, 0};      // #00202020
    constexpr Color SolidBackgroundBaseAlt{10, 10, 10, 255};       // #0A0A0A
    
    // System colors
    constexpr Color SystemSuccess{108, 203, 95, 255};          // #6CCB5F
    constexpr Color SystemCaution{252, 225, 0, 255};           // #FCE100
    constexpr Color SystemCritical{255, 153, 164, 255};        // #FF99A4
    constexpr Color SystemNeutral{255, 255, 255, 139};         // #8BFFFFFF
    constexpr Color SystemSolidNeutral{157, 157, 157, 255};    // #9D9D9D
    
    // System backgrounds
    constexpr Color SystemAttentionBackground{255, 255, 255, 8};   // #08FFFFFF
    constexpr Color SystemSuccessBackground{57, 61, 27, 255};      // #393D1B
    constexpr Color SystemCautionBackground{67, 53, 25, 255};      // #433519
    constexpr Color SystemCriticalBackground{68, 39, 38, 255};     // #442726
    constexpr Color SystemNeutralBackground{255, 255, 255, 8};      // #08FFFFFF
    constexpr Color SystemSolidAttentionBackground{46, 46, 46, 255}; // #2E2E2E
    constexpr Color SystemSolidNeutralBackground{46, 46, 46, 255};   // #2E2E2E
}

// Light theme colors
namespace light {
    // Text
    constexpr Color TextPrimary{0, 0, 0, 228};                 // #E4000000
    constexpr Color TextSecondary{0, 0, 0, 158};               // #9E000000
    constexpr Color TextTertiary{0, 0, 0, 114};                // #72000000
    constexpr Color TextDisabled{0, 0, 0, 92};                 // #5C000000
    constexpr Color TextInverse{255, 255, 255, 255};           // #FFFFFF
    
    // On-accent text
    constexpr Color TextOnAccentPrimary{255, 255, 255, 255};   // #FFFFFF
    constexpr Color TextOnAccentSecondary{255, 255, 255, 179}; // #B3FFFFFF
    constexpr Color TextOnAccentDisabled{255, 255, 255, 255};  // #FFFFFF
    
    // Control fill
    constexpr Color ControlFillDefault{255, 255, 255, 179};    // #B3FFFFFF
    constexpr Color ControlFillSecondary{249, 249, 249, 128};  // #80F9F9F9
    constexpr Color ControlFillTertiary{249, 249, 249, 77};    // #4DF9F9F9
    constexpr Color ControlFillQuarternary{243, 243, 243, 194}; // #C2F3F3F3
    constexpr Color ControlFillDisabled{249, 249, 249, 77};    // #4DF9F9F9
    constexpr Color ControlFillTransparent{255, 255, 255, 0};  // #00FFFFFF
    constexpr Color ControlFillInputActive{255, 255, 255, 255}; // #FFFFFF
    
    // Strong fill
    constexpr Color ControlStrongFillDefault{0, 0, 0, 114};    // #72000000
    constexpr Color ControlStrongFillDisabled{0, 0, 0, 81};    // #51000000
    
    // Solid fill
    constexpr Color ControlSolidFillDefault{255, 255, 255, 255}; // #FFFFFF
    
    // Subtle fill
    constexpr Color SubtleFillTransparent{255, 255, 255, 0};   // #00FFFFFF
    constexpr Color SubtleFillSecondary{0, 0, 0, 9};           // #09000000
    constexpr Color SubtleFillTertiary{0, 0, 0, 6};            // #06000000
    constexpr Color SubtleFillDisabled{255, 255, 255, 0};      // #00FFFFFF
    
    // Alt fill
    constexpr Color ControlAltFillTransparent{255, 255, 255, 0};   // #00FFFFFF
    constexpr Color ControlAltFillSecondary{0, 0, 0, 6};           // #06000000
    constexpr Color ControlAltFillTertiary{0, 0, 0, 15};           // #0F000000
    constexpr Color ControlAltFillQuarternary{0, 0, 0, 24};        // #18000000
    constexpr Color ControlAltFillDisabled{255, 255, 255, 0};       // #00FFFFFF

    // On-image fill
    constexpr Color ControlOnImageFillDefault{255, 255, 255, 201};  // #C9FFFFFF
    constexpr Color ControlOnImageFillSecondary{243, 243, 243, 255}; // #F3F3F3
    constexpr Color ControlOnImageFillTertiary{235, 235, 235, 255};  // #EBEBEB
    constexpr Color ControlOnImageFillDisabled{255, 255, 255, 0};    // #00FFFFFF
    
    // Accent fill
    constexpr Color AccentFillDisabled{0, 0, 0, 55};           // #37000000
    
    // Strokes
    constexpr Color ControlStrokeDefault{0, 0, 0, 15};         // #0F000000
    constexpr Color ControlStrokeSecondary{0, 0, 0, 41};       // #29000000
    constexpr Color ControlStrokeOnAccentDefault{255, 255, 255, 20};   // #14FFFFFF
    constexpr Color ControlStrokeOnAccentSecondary{0, 0, 0, 102};      // #66000000
    constexpr Color ControlStrokeOnAccentTertiary{0, 0, 0, 55};        // #37000000
    constexpr Color ControlStrokeOnAccentDisabled{0, 0, 0, 15};        // #0F000000
    constexpr Color ControlStrokeForStrongFillWhenOnImage{255, 255, 255, 89}; // #59FFFFFF
    
    // Strong strokes
    constexpr Color ControlStrongStrokeDefault{0, 0, 0, 114};  // #72000000
    constexpr Color ControlStrongStrokeDisabled{0, 0, 0, 55};  // #37000000
    
    // Card strokes
    constexpr Color CardStrokeDefault{0, 0, 0, 15};            // #0F000000
    constexpr Color CardStrokeDefaultSolid{235, 235, 235, 255}; // #EBEBEB
    
    // Surface strokes
    constexpr Color SurfaceStrokeDefault{117, 117, 117, 102};  // #66757575
    constexpr Color SurfaceStrokeFlyout{0, 0, 0, 15};          // #0F000000
    constexpr Color SurfaceStrokeInverse{255, 255, 255, 21};    // #15FFFFFF
    
    // Dividers
    constexpr Color DividerStrokeDefault{0, 0, 0, 15};         // #0F000000
    
    // Focus
    constexpr Color FocusStrokeOuter{0, 0, 0, 228};            // #E4000000
    constexpr Color FocusStrokeInner{255, 255, 255, 179};      // #B3FFFFFF
    
    // Card backgrounds
    constexpr Color CardBackgroundDefault{255, 255, 255, 179};     // #B3FFFFFF
    constexpr Color CardBackgroundSecondary{246, 246, 246, 128};   // #80F6F6F6
    constexpr Color CardBackgroundTertiary{255, 255, 255, 255};    // #FFFFFF
    
    // Overlays
    constexpr Color SmokeFillDefault{0, 0, 0, 77};             // #4D000000
    
    // Layers
    constexpr Color LayerFillDefault{255, 255, 255, 128};      // #80FFFFFF
    constexpr Color LayerFillAlt{255, 255, 255, 255};          // #FFFFFF
    constexpr Color LayerOnAcrylicFillDefault{255, 255, 255, 64};  // #40FFFFFF
    constexpr Color LayerOnAccentAcrylicFillDefault{255, 255, 255, 64}; // #40FFFFFF
    constexpr Color LayerOnMicaBaseAltDefault{255, 255, 255, 179}; // #B3FFFFFF
    constexpr Color LayerOnMicaBaseAltSecondary{0, 0, 0, 10};      // #0A000000
    constexpr Color LayerOnMicaBaseAltTertiary{249, 249, 249, 255}; // #F9F9F9
    constexpr Color LayerOnMicaBaseAltTransparent{0, 0, 0, 0};       // #00000000
    
    // Solid backgrounds
    constexpr Color SolidBackgroundBase{243, 243, 243, 255};       // #F3F3F3
    constexpr Color SolidBackgroundSecondary{238, 238, 238, 255};  // #EEEEEE
    constexpr Color SolidBackgroundTertiary{249, 249, 249, 255};   // #F9F9F9
    constexpr Color SolidBackgroundQuarternary{255, 255, 255, 255}; // #FFFFFF
    constexpr Color SolidBackgroundQuinary{253, 253, 253, 255};      // #FDFDFD
    constexpr Color SolidBackgroundSenary{255, 255, 255, 255};       // #FFFFFF
    constexpr Color SolidBackgroundTransparent{243, 243, 243, 0};    // #00F3F3F3
    constexpr Color SolidBackgroundBaseAlt{218, 218, 218, 255};    // #DADADA
    
    // System colors
    constexpr Color SystemSuccess{15, 123, 15, 255};           // #0F7B0F
    constexpr Color SystemCaution{157, 93, 0, 255};            // #9D5D00
    constexpr Color SystemCritical{196, 43, 28, 255};          // #C42B1C
    constexpr Color SystemNeutral{0, 0, 0, 114};               // #72000000
    constexpr Color SystemSolidNeutral{138, 138, 138, 255};    // #8A8A8A
    
    // System backgrounds
    constexpr Color SystemAttentionBackground{246, 246, 246, 128}; // #80F6F6F6
    constexpr Color SystemSuccessBackground{223, 246, 221, 255};   // #DFF6DD
    constexpr Color SystemCautionBackground{255, 244, 206, 255};   // #FFF4CE
    constexpr Color SystemCriticalBackground{253, 231, 233, 255};  // #FDE7E9
    constexpr Color SystemNeutralBackground{0, 0, 0, 6};            // #06000000
    constexpr Color SystemSolidAttentionBackground{247, 247, 247, 255}; // #F7F7F7
    constexpr Color SystemSolidNeutralBackground{243, 243, 243, 255};   // #F3F3F3
}

// Accent palette
struct AccentPalette {
    Color base;
    Color light1;
    Color light2;
    Color light3;
    Color dark1;
    Color dark2;
    Color dark3;

    // Default Windows blue
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

} // namespace fluxent::theme
