#include "fluxent/text.hpp"
#include "fluxent/config.hpp"
#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>

namespace fluxent
{

namespace
{
constexpr float kUnboundedExtent = fluxent::config::Text::UnboundedExtent;
constexpr float kExtentQuantizeStep = fluxent::config::Text::ExtentQuantizeStep;
constexpr size_t kMaxCachedTextLength = fluxent::config::Text::MaxCachedTextLength;

float QuantizeExtent(float value)
{
  if (kExtentQuantizeStep <= 0.0f)
    return value;
  return std::round(value / kExtentQuantizeStep) * kExtentQuantizeStep;
}

int ClampIndex(int index, size_t length)
{
  if (index < 0)
    return 0;
  if (static_cast<size_t>(index) > length)
    return static_cast<int>(length);
  return index;
}

void ClampRange(int &start, int &end, size_t length)
{
  start = ClampIndex(start, length);
  end = ClampIndex(end, length);
  if (start > end)
    std::swap(start, end);
}
} // namespace

Result<std::unique_ptr<TextRenderer>> TextRenderer::Create(GraphicsPipeline *graphics)
{
  if (!graphics)
  {
    return tl::unexpected(E_INVALIDARG);
  }
  auto tr = std::unique_ptr<TextRenderer>(new TextRenderer(graphics));
  auto res = tr->Init();
  if (!res)
    return tl::unexpected(res.error());
  return tr;
}

TextRenderer::TextRenderer(GraphicsPipeline *graphics) : graphics_(graphics) {}

Result<void> TextRenderer::Init()
{
  d2d_context_ = graphics_->GetD2DContext();
  dwrite_factory_ = graphics_->GetDWriteFactory();
  return {};
}

TextRenderer::~TextRenderer() = default;

void TextRenderer::DrawText(const std::wstring &text, const Rect &bounds, const TextStyle &style)
{
  if (text.empty() || !d2d_context_)
    return;

  auto layout_res = GetOrCreateTextLayout(text, style, bounds.width, bounds.height);
  if (!layout_res)
    return;
  auto layout = *layout_res;

  auto *brush = GetBrush(style.color);
  if (!brush)
    return;

  d2d_context_->DrawTextLayout(D2D1::Point2F(bounds.x, bounds.y), layout.Get(), brush);
}

void TextRenderer::DrawTextAt(const std::wstring &text, const Point &position,
                              const TextStyle &style)
{
  if (text.empty() || !d2d_context_)
    return;

  auto *format = GetTextFormat(style);
  if (!format)
    return;

  auto *brush = GetBrush(style.color);
  if (!brush)
    return;

  D2D1_RECT_F rect = D2D1::RectF(position.x, position.y, position.x + kUnboundedExtent,
                                 position.y + kUnboundedExtent);

  d2d_context_->DrawText(text.c_str(), static_cast<UINT32>(text.length()), format, rect, brush);
}

Size TextRenderer::MeasureText(const std::wstring &text, const TextStyle &style, float max_width)
{
  if (text.empty())
    return Size();

  float layout_width = max_width > 0 ? max_width : kUnboundedExtent;
  auto layout_res = GetOrCreateTextLayout(text, style, layout_width, kUnboundedExtent);
  if (!layout_res)
    return Size();
  auto layout = *layout_res;

  DWRITE_TEXT_METRICS metrics;
  if (SUCCEEDED(layout->GetMetrics(&metrics)))
  {
    return Size(metrics.width, metrics.height);
  }

  return Size();
}

int TextRenderer::GetLineCount(const std::wstring &text, const TextStyle &style, float max_width)
{
  if (text.empty())
    return 0;

  float layout_width = max_width > 0 ? max_width : kUnboundedExtent;
  auto layout_res = GetOrCreateTextLayout(text, style, layout_width, kUnboundedExtent);
  if (!layout_res)
    return 0;
  auto layout = *layout_res;

  DWRITE_TEXT_METRICS metrics;
  if (SUCCEEDED(layout->GetMetrics(&metrics)))
  {
    return static_cast<int>(metrics.lineCount);
  }

  return 0;
}

int TextRenderer::HitTestPoint(const std::wstring &text, const TextStyle &style, float max_width,
                               float x, float y)
{
  if (text.empty())
    return 0;

  float layout_width = max_width > 0 ? max_width : kUnboundedExtent;
  auto layout_res = GetOrCreateTextLayout(text, style, layout_width, kUnboundedExtent);
  if (!layout_res)
    return 0;
  auto layout = *layout_res;

  BOOL is_trailing;
  BOOL is_inside;
  DWRITE_HIT_TEST_METRICS metrics;

  HRESULT hr = layout->HitTestPoint(x, y, &is_trailing, &is_inside, &metrics);
  if (FAILED(hr))
    return 0;

  return is_trailing ? metrics.textPosition + 1 : metrics.textPosition;
}

Rect TextRenderer::GetCaretRect(const std::wstring &text, const TextStyle &style, float max_width,
                                int index)
{
  float layout_width = max_width > 0 ? max_width : kUnboundedExtent;
  int safe_index = ClampIndex(index, text.size());
  auto layout_res = GetOrCreateTextLayout(text, style, layout_width, kUnboundedExtent);
  if (!layout_res)
    return Rect();
  auto layout = *layout_res;

  float x, y;
  DWRITE_HIT_TEST_METRICS metrics;
  HRESULT hr = layout->HitTestTextPosition(safe_index, FALSE, &x, &y, &metrics);
  if (FAILED(hr))
    return Rect();

  return Rect(x, y, 1.0f, metrics.height);
}

std::vector<Rect> TextRenderer::GetSelectionRects(const std::wstring &text, const TextStyle &style,
                                                  float max_width, int start, int end)
{
  int safe_start = start;
  int safe_end = end;
  ClampRange(safe_start, safe_end, text.size());
  if (safe_start >= safe_end)
    return {};

  float layout_width = max_width > 0 ? max_width : kUnboundedExtent;
  auto layout_res = GetOrCreateTextLayout(text, style, layout_width, kUnboundedExtent);
  if (!layout_res)
    return {};
  auto layout = *layout_res;

  uint32_t count = 0;
  layout->HitTestTextRange(safe_start, safe_end - safe_start, 0, 0, nullptr, 0, &count);
  if (count == 0)
    return {};

  std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
  if (FAILED(layout->HitTestTextRange(safe_start, safe_end - safe_start, 0, 0, metrics.data(),
                                      count, &count)))
  {
    return {};
  }

  std::vector<Rect> rects;
  rects.reserve(count);
  for (const auto &m : metrics)
  {
    rects.emplace_back(m.left, m.top, m.width, m.height);
  }
  return rects;
}

IDWriteTextFormat *TextRenderer::GetTextFormat(const TextStyle &style)
{
  size_t hash = ComputeStyleHash(style);

  auto it = format_cache_.find(hash);
  if (it != format_cache_.end())
  {
    return it->second.Get();
  }

  if (!dwrite_factory_)
    return nullptr;

  DWRITE_FONT_WEIGHT weight = static_cast<DWRITE_FONT_WEIGHT>(style.font_weight);

  DWRITE_FONT_STYLE dw_style = DWRITE_FONT_STYLE_NORMAL;
  switch (style.font_style)
  {
  case FontStyle::Normal:
    dw_style = DWRITE_FONT_STYLE_NORMAL;
    break;
  case FontStyle::Oblique:
    dw_style = DWRITE_FONT_STYLE_OBLIQUE;
    break;
  case FontStyle::Italic:
    dw_style = DWRITE_FONT_STYLE_ITALIC;
    break;
  }

  ComPtr<IDWriteTextFormat> format;
  HRESULT hr =
      dwrite_factory_->CreateTextFormat(style.font_family.c_str(), nullptr, weight, dw_style,
                                        DWRITE_FONT_STRETCH_NORMAL, style.font_size, L"", &format);

  if (FAILED(hr))
    return nullptr;

  DWRITE_TEXT_ALIGNMENT text_align = DWRITE_TEXT_ALIGNMENT_LEADING;
  switch (style.alignment)
  {
  case TextAlignment::Leading:
    text_align = DWRITE_TEXT_ALIGNMENT_LEADING;
    break;
  case TextAlignment::Center:
    text_align = DWRITE_TEXT_ALIGNMENT_CENTER;
    break;
  case TextAlignment::Trailing:
    text_align = DWRITE_TEXT_ALIGNMENT_TRAILING;
    break;
  case TextAlignment::Justified:
    text_align = DWRITE_TEXT_ALIGNMENT_JUSTIFIED;
    break;
  }
  format->SetTextAlignment(text_align);

  DWRITE_PARAGRAPH_ALIGNMENT para_align = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
  switch (style.paragraph_alignment)
  {
  case ParagraphAlignment::Near:
    para_align = DWRITE_PARAGRAPH_ALIGNMENT_NEAR;
    break;
  case ParagraphAlignment::Center:
    para_align = DWRITE_PARAGRAPH_ALIGNMENT_CENTER;
    break;
  case ParagraphAlignment::Far:
    para_align = DWRITE_PARAGRAPH_ALIGNMENT_FAR;
    break;
  }
  format->SetParagraphAlignment(para_align);

  if (!style.word_wrap)
  {
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  }

  format_cache_[hash] = format;
  return format.Get();
}

Result<ComPtr<IDWriteTextLayout>> TextRenderer::CreateTextLayout(const std::wstring &text,
                                                                 const TextStyle &style,
                                                                 float max_width, float max_height)
{
  auto *format = GetTextFormat(style);
  if (!format || !dwrite_factory_)
    return tl::unexpected(E_FAIL);

  ComPtr<IDWriteTextLayout> layout;
  HRESULT hr = dwrite_factory_->CreateTextLayout(text.c_str(), static_cast<UINT32>(text.length()),
                                                 format, max_width, max_height, &layout);

  if (FAILED(hr))
    return tl::unexpected(hr);

  return layout;
}

Result<ComPtr<IDWriteTextLayout>> TextRenderer::GetOrCreateTextLayout(const std::wstring &text,
                                                                      const TextStyle &style,
                                                                      float max_width,
                                                                      float max_height)
{
  float layout_width = max_width > 0 ? max_width : kUnboundedExtent;
  float layout_height = max_height > 0 ? max_height : kUnboundedExtent;

  if (text.size() > kMaxCachedTextLength)
  {
    return CreateTextLayout(text, style, layout_width, layout_height);
  }

  size_t text_hash = std::hash<std::wstring>{}(text);
  size_t text_len = text.size();

  float key_width = layout_width;
  float key_height = layout_height;
  if (!style.word_wrap)
  {
    key_width = QuantizeExtent(key_width);
    key_height = QuantizeExtent(key_height);
  }

  TextLayoutCacheKey key{text_hash, text_len, ComputeStyleHash(style), key_width, key_height};

  auto it = layout_cache_.FindExact(key, text);
  if (it != layout_cache_.map.end())
  {
    layout_cache_.Touch(it);
    return it->second.layout;
  }

  auto layout_res = CreateTextLayout(text, style, layout_width, layout_height);
  if (!layout_res)
    return tl::unexpected(layout_res.error());

  auto layout = *layout_res;

  EvictLayoutCacheIfNeeded();
  layout_cache_.Insert(key, text, layout);
  return layout;
}

void TextRenderer::EvictLayoutCacheIfNeeded() { layout_cache_.EvictIfNeeded(kMaxLayoutCacheSize); }

ID2D1SolidColorBrush *TextRenderer::GetBrush(const Color &color)
{
  uint32_t key = (static_cast<uint32_t>(color.r) << 24) | (static_cast<uint32_t>(color.g) << 16) |
                 (static_cast<uint32_t>(color.b) << 8) | static_cast<uint32_t>(color.a);

  auto it = brush_cache_.find(key);
  if (it != brush_cache_.end())
  {
    return it->second.Get();
  }

  if (!d2d_context_)
    return nullptr;

  ComPtr<ID2D1SolidColorBrush> brush;
  HRESULT hr = d2d_context_->CreateSolidColorBrush(color.to_d2d(), &brush);
  if (FAILED(hr))
    return nullptr;
  brush_cache_[key] = brush;
  return brush.Get();
}

size_t TextRenderer::ComputeStyleHash(const TextStyle &style)
{
  size_t hash = 0;

  HashCombine(hash, std::hash<std::wstring>{}(style.font_family));
  HashCombine(hash, std::hash<float>{}(style.font_size));
  HashCombine(hash, static_cast<size_t>(style.font_weight));
  HashCombine(hash, static_cast<size_t>(style.font_style));
  HashCombine(hash, static_cast<size_t>(style.alignment));
  HashCombine(hash, static_cast<size_t>(style.paragraph_alignment));
  HashCombine(hash, style.word_wrap ? 1 : 0);

  return hash;
}

void TextRenderer::HashCombine(size_t &hash, size_t value)
{
  hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
}

void TextRenderer::ClearFormatCache()
{
  format_cache_.clear();
  brush_cache_.clear();
  layout_cache_.Clear();
}

void TextRenderer::ClearLayoutCache() { layout_cache_.Clear(); }

} // namespace fluxent
