#pragma once

// FluXent text renderer (DirectWrite)

#include "types.hpp"
#include "graphics.hpp"
#include <string>
#include <unordered_map>

namespace fluxent {

// Alignment

enum class TextAlignment {
    Leading,    // Left (LTR) / Right (RTL)
    Center,
    Trailing,   // Right (LTR) / Left (RTL)
    Justified
};

enum class ParagraphAlignment {
    Near,
    Center,
    Far
};

// Font

enum class FontWeight {
    Thin = 100,
    ExtraLight = 200,
    Light = 300,
    SemiLight = 350,
    Normal = 400,
    Medium = 500,
    SemiBold = 600,
    Bold = 700,
    ExtraBold = 800,
    Black = 900
};

enum class FontStyle {
    Normal,
    Oblique,
    Italic
};


// TextStyle

struct TextStyle {
    std::wstring font_family = L"Segoe UI Variable";
    float font_size = 14.0f;
    FontWeight font_weight = FontWeight::Normal;
    FontStyle font_style = FontStyle::Normal;
    Color color = Color::white();
    TextAlignment alignment = TextAlignment::Leading;
    ParagraphAlignment paragraph_alignment = ParagraphAlignment::Near;
    bool word_wrap = true;
    float line_height = 0.0f;  // 0 = auto
};

// TextRenderer

class TextRenderer {
public:
    explicit TextRenderer(GraphicsPipeline* graphics);
    ~TextRenderer();
    
    TextRenderer(const TextRenderer&) = delete;
    TextRenderer& operator=(const TextRenderer&) = delete;

    void draw_text(
        const std::wstring& text,
        const Rect& bounds,
        const TextStyle& style
    );

    void draw_text_at(
        const std::wstring& text,
        const Point& position,
        const TextStyle& style
    );

    Size measure_text(
        const std::wstring& text,
        const TextStyle& style,
        float max_width = 0.0f  // 0 = unlimited
    );

    int get_line_count(
        const std::wstring& text,
        const TextStyle& style,
        float max_width
    );

    void clear_format_cache();

private:
    IDWriteTextFormat* get_text_format(const TextStyle& style);

    ComPtr<IDWriteTextLayout> create_text_layout(
        const std::wstring& text,
        const TextStyle& style,
        float max_width,
        float max_height
    );

    ID2D1SolidColorBrush* get_brush(const Color& color);

    static size_t compute_style_hash(const TextStyle& style);

    // Static helper (no lambda).
    static void hash_combine(size_t& hash, size_t value);
    
    GraphicsPipeline* graphics_;
    ID2D1DeviceContext* d2d_context_ = nullptr;
    IDWriteFactory* dwrite_factory_ = nullptr;
    
    std::unordered_map<size_t, ComPtr<IDWriteTextFormat>> format_cache_;

    std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> brush_cache_;
};

} // namespace fluxent
