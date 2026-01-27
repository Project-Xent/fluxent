#pragma once

#include "../types.hpp"
#include "theme_manager.hpp"
#include "../controls/render_constants.hpp"

namespace fluxent::theme {


constexpr ThemeResources kDarkPalette = {
    .Elevation = {3.0f, 0.33f, 1.0f},
    .Focus = {2.0f, 1.0f, 3.0f, 1.0f},
    .CheckBox = {20.0f, 8.0f, 4.0f, 12.0f},
    .RadioButton = {20.0f, 8.0f, 12.0f, 14.0f, 10.0f},
    .TextBox = {controls::constants::kTextBoxMinHeight,
                controls::constants::kTextBoxMinWidth,
                controls::constants::kTextBoxPaddingLeft,
                controls::constants::kTextBoxPaddingTop,
                controls::constants::kTextBoxPaddingRight,
                controls::constants::kTextBoxPaddingBottom},
    .ControlCornerRadius = 4.0f,



    .TextPrimary = {255, 255, 255, 255},   // #FFFFFF
    .TextSecondary = {255, 255, 255, 197}, // #C5FFFFFF
    .TextTertiary = {255, 255, 255, 135},  // #87FFFFFF
    .TextDisabled = {255, 255, 255, 93},   // #5DFFFFFF
    .TextInverse = {0, 0, 0, 228},         // #E4000000


    .TextOnAccentPrimary = {0, 0, 0, 255},        // #000000
    .TextOnAccentSecondary = {0, 0, 0, 128},      // #80000000
    .TextOnAccentDisabled = {255, 255, 255, 135}, // #87FFFFFF


    .ControlFillDefault = {255, 255, 255, 15},     // #0FFFFFFF
    .ControlFillSecondary = {255, 255, 255, 21},   // #15FFFFFF
    .ControlFillTertiary = {255, 255, 255, 8},     // #08FFFFFF
    .ControlFillQuarternary = {255, 255, 255, 15}, // #0FFFFFFF
    .ControlFillDisabled = {255, 255, 255, 11},    // #0BFFFFFF
    .ControlFillTransparent = {255, 255, 255, 0},  // #00FFFFFF
    .ControlFillInputActive = {30, 30, 30, 179},   // #B31E1E1E


    .ControlStrongFillDefault = {255, 255, 255, 139}, // #8BFFFFFF
    .ControlStrongFillDisabled = {255, 255, 255, 63}, // #3FFFFFFF


    .ControlSolidFillDefault = {69, 69, 69, 255}, // #454545


    .SubtleFillTransparent = {255, 255, 255, 0}, // #00FFFFFF
    .SubtleFillSecondary = {255, 255, 255, 15},  // #0FFFFFFF
    .SubtleFillTertiary = {255, 255, 255, 10},   // #0AFFFFFF
    .SubtleFillDisabled = {255, 255, 255, 0},    // #00FFFFFF


    .ControlAltFillTransparent = {255, 255, 255, 0},  // #00FFFFFF
    .ControlAltFillSecondary = {0, 0, 0, 25},         // #19000000
    .ControlAltFillTertiary = {255, 255, 255, 11},    // #0BFFFFFF
    .ControlAltFillQuarternary = {255, 255, 255, 18}, // #12FFFFFF
    .ControlAltFillDisabled = {255, 255, 255, 0},     // #00FFFFFF


    .ControlOnImageFillDefault = {28, 28, 28, 179},   // #B31C1C1C
    .ControlOnImageFillSecondary = {26, 26, 26, 255}, // #1A1A1A
    .ControlOnImageFillTertiary = {19, 19, 19, 255},  // #131313
    .ControlOnImageFillDisabled = {30, 30, 30, 255},  // #1E1E1E


    .AccentDefault = {0, 0, 0, 0},
    .AccentSecondary = {0, 0, 0, 0},
    .AccentTertiary = {0, 0, 0, 0},
    .AccentDisabled = {255, 255, 255, 40}, // #28FFFFFF


    .ControlStrokeDefault = {255, 255, 255, 18},             // #12FFFFFF
    .ControlStrokeSecondary = {255, 255, 255, 24},           // #18FFFFFF
    .ControlStrokeOnAccentDefault = {255, 255, 255, 20},     // #14FFFFFF
    .ControlStrokeOnAccentSecondary = {0, 0, 0, 35},         // #23000000
    .ControlStrokeOnAccentTertiary = {0, 0, 0, 55},          // #37000000
    .ControlStrokeOnAccentDisabled = {0, 0, 0, 51},          // #33000000
    .ControlStrokeForStrongFillWhenOnImage = {0, 0, 0, 107}, // #6B000000


    .ControlStrongStrokeDefault = {255, 255, 255, 139}, // #8BFFFFFF
    .ControlStrongStrokeDisabled = {255, 255, 255, 40}, // #28FFFFFF


    .CardStrokeDefault = {0, 0, 0, 25},            // #19000000
    .CardStrokeDefaultSolid = {28, 28, 28, 255},   // #1C1C1C
    .CardBackgroundDefault = {255, 255, 255, 13},  // #0DFFFFFF
    .CardBackgroundSecondary = {255, 255, 255, 8}, // #08FFFFFF
    .CardBackgroundTertiary = {255, 255, 255, 18}, // #12FFFFFF


    .SurfaceStrokeDefault = {117, 117, 117, 102}, // #66757575
    .SurfaceStrokeFlyout = {0, 0, 0, 51},         // #33000000
    .SurfaceStrokeInverse = {0, 0, 0, 15},        // #0F000000


    .FocusStrokeOuter = {255, 255, 255, 255}, // #FFFFFF
    .FocusStrokeInner = {0, 0, 0, 179},       // #B3000000


    .DividerStrokeDefault = {255, 255, 255, 21}, // #15FFFFFF


    .SolidBackgroundBase = {32, 32, 32, 255},              // #202020
    .SolidBackgroundSecondary = {28, 28, 28, 255},         // #1C1C1C
    .SolidBackgroundTertiary = {40, 40, 40, 255},          // #282828
    .SolidBackgroundQuarternary = {44, 44, 44, 255},       // #2C2C2C
    .SolidBackgroundQuinary = {51, 51, 51, 255},           // #333333
    .SolidBackgroundSenary = {55, 55, 55, 255},            // #373737
    .SolidBackgroundTransparent = {32, 32, 32, 0},         // #00202020
    .SolidBackgroundBaseAlt = {10, 10, 10, 255},           // #0A0A0A
    .LayerFillDefault = {58, 58, 58, 76},                  // #4C3A3A3A
    .LayerFillAlt = {255, 255, 255, 13},                   // #0DFFFFFF
    .LayerOnAcrylicFillDefault = {255, 255, 255, 9},       // #09FFFFFF
    .LayerOnAccentAcrylicFillDefault = {255, 255, 255, 9}, // #09FFFFFF
    .LayerOnMicaBaseAltDefault = {58, 58, 58, 115},        // #733A3A3A
    .LayerOnMicaBaseAltSecondary = {255, 255, 255, 15},    // #0FFFFFFF
    .LayerOnMicaBaseAltTertiary = {44, 44, 44, 255},       // #2C2C2C
    .LayerOnMicaBaseAltTransparent = {255, 255, 255, 0},   // #00FFFFFF
    .SmokeFillDefault = {0, 0, 0, 77},                     // #4D000000


    .SystemSuccess = {108, 203, 95, 255},                // #6CCB5F
    .SystemCaution = {252, 225, 0, 255},                 // #FCE100
    .SystemCritical = {255, 153, 164, 255},              // #FF99A4
    .SystemNeutral = {255, 255, 255, 139},               // #8BFFFFFF
    .SystemSolidNeutral = {157, 157, 157, 255},          // #9D9D9D
    .SystemAttentionBackground = {255, 255, 255, 8},     // #08FFFFFF
    .SystemSuccessBackground = {57, 61, 27, 255},        // #393D1B
    .SystemCautionBackground = {67, 53, 25, 255},        // #433519
    .SystemCriticalBackground = {68, 39, 38, 255},       // #442726
    .SystemNeutralBackground = {255, 255, 255, 8},       // #08FFFFFF
    .SystemSolidAttentionBackground = {46, 46, 46, 255}, // #2E2E2E
    .SystemSolidNeutralBackground = {46, 46, 46, 255},   // #2E2E2E
};

constexpr ThemeResources kLightPalette = {
    .Elevation = {3.0f, 0.33f, 1.0f},
    .Focus = {2.0f, 1.0f, 3.0f, 1.0f},
    .CheckBox = {20.0f, 8.0f, 4.0f, 12.0f},
    .RadioButton = {20.0f, 8.0f, 12.0f, 14.0f, 10.0f},
    .TextBox = {controls::constants::kTextBoxMinHeight,
                controls::constants::kTextBoxMinWidth,
                controls::constants::kTextBoxPaddingLeft,
                controls::constants::kTextBoxPaddingTop,
                controls::constants::kTextBoxPaddingRight,
                controls::constants::kTextBoxPaddingBottom},
    .ControlCornerRadius = 4.0f,



    .TextPrimary = {0, 0, 0, 228},       // #E4000000
    .TextSecondary = {0, 0, 0, 158},     // #9E000000
    .TextTertiary = {0, 0, 0, 114},      // #72000000
    .TextDisabled = {0, 0, 0, 92},       // #5C000000
    .TextInverse = {255, 255, 255, 255}, // #FFFFFF


    .TextOnAccentPrimary = {255, 255, 255, 255},   // #FFFFFF
    .TextOnAccentSecondary = {255, 255, 255, 179}, // #B3FFFFFF
    .TextOnAccentDisabled = {255, 255, 255, 255},  // #FFFFFF


    .ControlFillDefault = {255, 255, 255, 179},     // #B3FFFFFF
    .ControlFillSecondary = {249, 249, 249, 128},   // #80F9F9F9
    .ControlFillTertiary = {249, 249, 249, 77},     // #4DF9F9F9
    .ControlFillQuarternary = {243, 243, 243, 194}, // #C2F3F3F3
    .ControlFillDisabled = {249, 249, 249, 77},     // #4DF9F9F9
    .ControlFillTransparent = {255, 255, 255, 0},   // #00FFFFFF
    .ControlFillInputActive = {255, 255, 255, 255}, // #FFFFFF


    .ControlStrongFillDefault = {0, 0, 0, 114}, // #72000000
    .ControlStrongFillDisabled = {0, 0, 0, 81}, // #51000000


    .ControlSolidFillDefault = {255, 255, 255, 255}, // #FFFFFF


    .SubtleFillTransparent = {255, 255, 255, 0}, // #00FFFFFF
    .SubtleFillSecondary = {0, 0, 0, 15},        // #0F000000
    .SubtleFillTertiary = {0, 0, 0, 11},         // #0B000000
    .SubtleFillDisabled = {255, 255, 255, 0},    // #00FFFFFF


    .ControlAltFillTransparent = {255, 255, 255, 0}, // #00FFFFFF
    .ControlAltFillSecondary = {0, 0, 0, 15},        // #0F000000
    .ControlAltFillTertiary = {0, 0, 0, 25},         // #19000000
    .ControlAltFillQuarternary = {0, 0, 0, 36},      // #24000000
    .ControlAltFillDisabled = {255, 255, 255, 0},    // #00FFFFFF


    .ControlOnImageFillDefault = {255, 255, 255, 201},   // #C9FFFFFF
    .ControlOnImageFillSecondary = {243, 243, 243, 255}, // #F3F3F3
    .ControlOnImageFillTertiary = {235, 235, 235, 255},  // #EBEBEB
    .ControlOnImageFillDisabled = {255, 255, 255, 0},    // #00FFFFFF


    .AccentDefault = {0, 0, 0, 0},
    .AccentSecondary = {0, 0, 0, 0},
    .AccentTertiary = {0, 0, 0, 0},
    .AccentDisabled = {0, 0, 0, 55}, // #37000000


    .ControlStrokeDefault = {0, 0, 0, 15},                        // #0F000000
    .ControlStrokeSecondary = {0, 0, 0, 41},                      // #29000000
    .ControlStrokeOnAccentDefault = {255, 255, 255, 20},          // #14FFFFFF
    .ControlStrokeOnAccentSecondary = {0, 0, 0, 102},             // #66000000
    .ControlStrokeOnAccentTertiary = {0, 0, 0, 55},               // #37000000
    .ControlStrokeOnAccentDisabled = {0, 0, 0, 15},               // #0F000000
    .ControlStrokeForStrongFillWhenOnImage = {255, 255, 255, 89}, // #59FFFFFF


    .ControlStrongStrokeDefault = {0, 0, 0, 114}, // #72000000
    .ControlStrongStrokeDisabled = {0, 0, 0, 55}, // #37000000


    .CardStrokeDefault = {0, 0, 0, 15},              // #0F000000
    .CardStrokeDefaultSolid = {235, 235, 235, 255},  // #EBEBEB
    .CardBackgroundDefault = {255, 255, 255, 179},   // #B3FFFFFF
    .CardBackgroundSecondary = {246, 246, 246, 128}, // #80F6F6F6
    .CardBackgroundTertiary = {255, 255, 255, 255},  // #FFFFFF


    .SurfaceStrokeDefault = {117, 117, 117, 102}, // #66757575
    .SurfaceStrokeFlyout = {0, 0, 0, 15},         // #0F000000
    .SurfaceStrokeInverse = {255, 255, 255, 21},  // #15FFFFFF


    .FocusStrokeOuter = {0, 0, 0, 228},       // #E4000000
    .FocusStrokeInner = {255, 255, 255, 179}, // #B3FFFFFF


    .DividerStrokeDefault = {0, 0, 0, 15}, // #0F000000


    .SolidBackgroundBase = {243, 243, 243, 255},            // #F3F3F3
    .SolidBackgroundSecondary = {238, 238, 238, 255},       // #EEEEEE
    .SolidBackgroundTertiary = {249, 249, 249, 255},        // #F9F9F9
    .SolidBackgroundQuarternary = {255, 255, 255, 255},     // #FFFFFF
    .SolidBackgroundQuinary = {253, 253, 253, 255},         // #FDFDFD
    .SolidBackgroundSenary = {255, 255, 255, 255},          // #FFFFFF
    .SolidBackgroundTransparent = {243, 243, 243, 0},       // #00F3F3F3
    .SolidBackgroundBaseAlt = {218, 218, 218, 255},         // #DADADA
    .LayerFillDefault = {255, 255, 255, 128},               // #80FFFFFF
    .LayerFillAlt = {255, 255, 255, 255},                   // #FFFFFF
    .LayerOnAcrylicFillDefault = {255, 255, 255, 64},       // #40FFFFFF
    .LayerOnAccentAcrylicFillDefault = {255, 255, 255, 64}, // #40FFFFFF
    .LayerOnMicaBaseAltDefault = {255, 255, 255, 179},      // #B3FFFFFF
    .LayerOnMicaBaseAltSecondary = {0, 0, 0, 10},           // #0A000000
    .LayerOnMicaBaseAltTertiary = {249, 249, 249, 255},     // #F9F9F9
    .LayerOnMicaBaseAltTransparent = {0, 0, 0, 0},          // #00000000
    .SmokeFillDefault = {0, 0, 0, 77},                      // #4D000000

    // System colors
    .SystemSuccess = {15, 123, 15, 255},                    // #0F7B0F
    .SystemCaution = {157, 93, 0, 255},                     // #9D5D00
    .SystemCritical = {196, 43, 28, 255},                   // #C42B1C
    .SystemNeutral = {0, 0, 0, 114},                        // #72000000
    .SystemSolidNeutral = {138, 138, 138, 255},             // #8A8A8A
    .SystemAttentionBackground = {246, 246, 246, 128},      // #80F6F6F6
    .SystemSuccessBackground = {223, 246, 221, 255},        // #DFF6DD
    .SystemCautionBackground = {255, 244, 206, 255},        // #FFF4CE
    .SystemCriticalBackground = {253, 231, 233, 255},       // #FDE7E9
    .SystemNeutralBackground = {0, 0, 0, 6},                // #06000000
    .SystemSolidAttentionBackground = {247, 247, 247, 255}, // #F7F7F7
    .SystemSolidNeutralBackground = {243, 243, 243, 255},   // #F3F3F3
};

} // namespace fluxent::theme
