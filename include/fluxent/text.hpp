#pragma once

#include "graphics.hpp"
#include "types.hpp"
#include <string>
#include <unordered_map>

namespace fluxent {

enum class TextAlignment {
  Leading, // Left (LTR) / Right (RTL)
  Center,
  Trailing, // Right (LTR) / Left (RTL)
  Justified
};

enum class ParagraphAlignment { Near, Center, Far };

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

enum class FontStyle { Normal, Oblique, Italic };

struct TextStyle {
  std::wstring font_family = L"Segoe UI Variable";
  float font_size = 14.0f;
  FontWeight font_weight = FontWeight::Normal;
  FontStyle font_style = FontStyle::Normal;
  Color color = Color::white();
  TextAlignment alignment = TextAlignment::Leading;
  ParagraphAlignment paragraph_alignment = ParagraphAlignment::Near;
  bool word_wrap = true;
  float line_height = 0.0f; // 0 = auto
};

struct TextLayoutCacheKey {
  std::wstring text;
  size_t style_hash;
  float max_width;
  float max_height;

  bool operator==(const TextLayoutCacheKey &o) const {
    return style_hash == o.style_hash && max_width == o.max_width &&
           max_height == o.max_height && text == o.text;
  }
};

struct TextLayoutCacheKeyHash {
  size_t operator()(const TextLayoutCacheKey &k) const {
    size_t h = std::hash<std::wstring>{}(k.text);
    h ^= k.style_hash + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<float>{}(k.max_width) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<float>{}(k.max_height) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

class TextRenderer {
public:
  static Result<std::unique_ptr<TextRenderer>>
  Create(GraphicsPipeline *graphics);
  ~TextRenderer();

private:
  explicit TextRenderer(GraphicsPipeline *graphics);
  Result<void> Init();

public:
  TextRenderer(const TextRenderer &) = delete;
  TextRenderer &operator=(const TextRenderer &) = delete;

  void DrawText(const std::wstring &text, const Rect &bounds,
                const TextStyle &style);

  void DrawTextAt(const std::wstring &text, const Point &position,
                  const TextStyle &style);

  Size MeasureText(const std::wstring &text, const TextStyle &style,
                   float max_width = 0.0f // 0 = unlimited
  );

  int GetLineCount(const std::wstring &text, const TextStyle &style,
                   float max_width);

  int HitTestPoint(const std::wstring &text, const TextStyle &style,
                   float max_width, float x, float y);

  Rect GetCaretRect(const std::wstring &text, const TextStyle &style,
                    float max_width, int index);

  std::vector<Rect> GetSelectionRects(const std::wstring &text,
                                      const TextStyle &style, float max_width,
                                      int start, int end);

  void ClearFormatCache();
  void ClearLayoutCache();

private:
  IDWriteTextFormat *GetTextFormat(const TextStyle &style);

  ComPtr<IDWriteTextLayout> CreateTextLayout(const std::wstring &text,
                                             const TextStyle &style,
                                             float max_width, float max_height);

  ComPtr<IDWriteTextLayout> GetOrCreateTextLayout(const std::wstring &text,
                                                  const TextStyle &style,
                                                  float max_width,
                                                  float max_height);

  ID2D1SolidColorBrush *GetBrush(const Color &color);

  static size_t ComputeStyleHash(const TextStyle &style);
  static void HashCombine(size_t &hash, size_t value);

  void EvictLayoutCacheIfNeeded();

  GraphicsPipeline *graphics_;
  ID2D1DeviceContext *d2d_context_ = nullptr;
  IDWriteFactory *dwrite_factory_ = nullptr;

  std::unordered_map<size_t, ComPtr<IDWriteTextFormat>> format_cache_;
  std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> brush_cache_;

  std::unordered_map<TextLayoutCacheKey, ComPtr<IDWriteTextLayout>,
                     TextLayoutCacheKeyHash>
      layout_cache_;
  static constexpr size_t kMaxLayoutCacheSize = 512;
};

} // namespace fluxent
