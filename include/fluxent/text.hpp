#pragma once

#include "graphics.hpp"
#include "types.hpp"
#include <list>
#include <string>
#include <unordered_map>

namespace fluxent
{

enum class TextAlignment
{
  Leading, // Left (LTR) / Right (RTL)
  Center,
  Trailing, // Right (LTR) / Left (RTL)
  Justified
};

enum class ParagraphAlignment
{
  Near,
  Center,
  Far
};

enum class FontWeight
{
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

enum class FontStyle
{
  Normal,
  Oblique,
  Italic
};

struct TextStyle
{
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

struct TextLayoutCacheKey
{
  size_t text_hash;
  size_t text_len;
  size_t style_hash;
  float max_width;
  float max_height;

  bool operator==(const TextLayoutCacheKey &o) const
  {
    return text_hash == o.text_hash && text_len == o.text_len && style_hash == o.style_hash &&
           max_width == o.max_width && max_height == o.max_height;
  }
};

struct TextLayoutCacheKeyHash
{
  size_t operator()(const TextLayoutCacheKey &k) const
  {
    size_t h = k.text_hash;
    h ^= k.text_len + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= k.style_hash + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<float>{}(k.max_width) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= std::hash<float>{}(k.max_height) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

struct TextLayoutCacheEntry;
using TextLayoutCacheMap =
    std::unordered_multimap<TextLayoutCacheKey, TextLayoutCacheEntry, TextLayoutCacheKeyHash>;
using TextLayoutCacheLruList = std::list<TextLayoutCacheMap::iterator>;

struct TextLayoutCacheEntry
{
  std::wstring text;
  ComPtr<IDWriteTextLayout> layout;
  TextLayoutCacheLruList::iterator lru_it;
};

struct TextLayoutCache
{
  TextLayoutCacheMap map;
  TextLayoutCacheLruList lru;

  void Clear()
  {
    map.clear();
    lru.clear();
  }

  void Touch(TextLayoutCacheMap::iterator it) { lru.splice(lru.end(), lru, it->second.lru_it); }

  TextLayoutCacheMap::iterator FindExact(const TextLayoutCacheKey &key, const std::wstring &text)
  {
    auto range = map.equal_range(key);
    for (auto it = range.first; it != range.second; ++it)
    {
      if (it->second.text == text)
      {
        return it;
      }
    }
    return map.end();
  }

  void Insert(const TextLayoutCacheKey &key, const std::wstring &text,
              const ComPtr<IDWriteTextLayout> &layout)
  {
    auto it = map.emplace(key, TextLayoutCacheEntry{text, layout});
    lru.push_back(it);
    it->second.lru_it = std::prev(lru.end());
  }

  void EvictIfNeeded(size_t max_size)
  {
    while (map.size() >= max_size && !lru.empty())
    {
      auto it = lru.front();
      map.erase(it);
      lru.pop_front();
    }
  }
};

class TextRenderer
{
public:
  static Result<std::unique_ptr<TextRenderer>> Create(GraphicsPipeline *graphics);
  ~TextRenderer();

private:
  explicit TextRenderer(GraphicsPipeline *graphics);
  Result<void> Init();

public:
  TextRenderer(const TextRenderer &) = delete;
  TextRenderer &operator=(const TextRenderer &) = delete;

  void DrawText(const std::wstring &text, const Rect &bounds, const TextStyle &style);

  void DrawTextAt(const std::wstring &text, const Point &position, const TextStyle &style);

  Size MeasureText(const std::wstring &text, const TextStyle &style,
                   float max_width = 0.0f // 0 = unlimited
  );

  int GetLineCount(const std::wstring &text, const TextStyle &style, float max_width);

  int HitTestPoint(const std::wstring &text, const TextStyle &style, float max_width, float x,
                   float y);

  Rect GetCaretRect(const std::wstring &text, const TextStyle &style, float max_width, int index);

  std::vector<Rect> GetSelectionRects(const std::wstring &text, const TextStyle &style,
                                      float max_width, int start, int end);

  void ClearFormatCache();
  void ClearLayoutCache();

private:
  IDWriteTextFormat *GetTextFormat(const TextStyle &style);

  Result<ComPtr<IDWriteTextLayout>> CreateTextLayout(const std::wstring &text,
                                                     const TextStyle &style, float max_width,
                                                     float max_height);

  Result<ComPtr<IDWriteTextLayout>> GetOrCreateTextLayout(const std::wstring &text,
                                                          const TextStyle &style, float max_width,
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

  TextLayoutCache layout_cache_;
  static constexpr size_t kMaxLayoutCacheSize = fluxent::config::Text::LayoutCacheSize;
};

} // namespace fluxent
