#include "fluxent/text.hpp"
#include <functional>
#include <stdexcept>

namespace fluxent {

Result<std::unique_ptr<TextRenderer>>
TextRenderer::Create(GraphicsPipeline *graphics) {
  if (!graphics) {
    return tl::unexpected(E_INVALIDARG);
  }
  auto tr = std::unique_ptr<TextRenderer>(new TextRenderer(graphics));
  auto res = tr->Init();
  if (!res)
    return tl::unexpected(res.error());
  return tr;
}

TextRenderer::TextRenderer(GraphicsPipeline *graphics) : graphics_(graphics) {}

Result<void> TextRenderer::Init() {
  d2d_context_ = graphics_->GetD2DContext();
  dwrite_factory_ = graphics_->GetDWriteFactory();
  return {};
}

TextRenderer::~TextRenderer() = default;

void TextRenderer::DrawText(const std::wstring &text, const Rect &bounds,
                            const TextStyle &style) {
  if (text.empty() || !d2d_context_)
    return;

  auto layout = GetOrCreateTextLayout(text, style, bounds.width, bounds.height);
  if (!layout)
    return;

  auto *brush = GetBrush(style.color);
  if (!brush)
    return;

  d2d_context_->DrawTextLayout(D2D1::Point2F(bounds.x, bounds.y), layout.Get(),
                               brush);
}

void TextRenderer::DrawTextAt(const std::wstring &text, const Point &position,
                              const TextStyle &style) {
  if (text.empty() || !d2d_context_)
    return;

  auto *format = GetTextFormat(style);
  if (!format)
    return;

  auto *brush = GetBrush(style.color);
  if (!brush)
    return;

  D2D1_RECT_F rect = D2D1::RectF(position.x, position.y, position.x + 100000.0f,
                                 position.y + 100000.0f);

  d2d_context_->DrawText(text.c_str(), static_cast<UINT32>(text.length()),
                         format, rect, brush);
}

Size TextRenderer::MeasureText(const std::wstring &text, const TextStyle &style,
                               float max_width) {
  if (text.empty())
    return Size();

  float layout_width = max_width > 0 ? max_width : 100000.0f;
  auto layout = GetOrCreateTextLayout(text, style, layout_width, 100000.0f);
  if (!layout)
    return Size();

  DWRITE_TEXT_METRICS metrics;
  if (SUCCEEDED(layout->GetMetrics(&metrics))) {
    return Size(metrics.width, metrics.height);
  }

  return Size();
}

int TextRenderer::GetLineCount(const std::wstring &text, const TextStyle &style,
                               float max_width) {
  if (text.empty())
    return 0;

  auto layout = GetOrCreateTextLayout(text, style, max_width, 100000.0f);
  if (!layout)
    return 0;

  DWRITE_TEXT_METRICS metrics;
  if (SUCCEEDED(layout->GetMetrics(&metrics))) {
    return static_cast<int>(metrics.lineCount);
  }

  return 0;
}

int TextRenderer::HitTestPoint(const std::wstring &text, const TextStyle &style,
                               float max_width, float x, float y) {
  if (text.empty())
    return 0;

  auto layout = GetOrCreateTextLayout(text, style, max_width > 0 ? max_width : 100000.0f,
                                       100000.0f);
  if (!layout)
    return 0;

  BOOL is_trailing;
  BOOL is_inside;
  DWRITE_HIT_TEST_METRICS metrics;

  HRESULT hr = layout->HitTestPoint(x, y, &is_trailing, &is_inside, &metrics);
  if (FAILED(hr))
    return 0;

  return is_trailing ? metrics.textPosition + 1 : metrics.textPosition;
}

Rect TextRenderer::GetCaretRect(const std::wstring &text,
                                const TextStyle &style, float max_width,
                                int index) {
  auto layout = GetOrCreateTextLayout(text, style, max_width > 0 ? max_width : 100000.0f,
                                       100000.0f);
  if (!layout)
    return Rect();

  float x, y;
  DWRITE_HIT_TEST_METRICS metrics;
  HRESULT hr = layout->HitTestTextPosition(index, FALSE, &x, &y, &metrics);
  if (FAILED(hr))
    return Rect();
  
  return Rect(x, y, 1.0f, metrics.height);
}

std::vector<Rect> TextRenderer::GetSelectionRects(const std::wstring &text,
                                                  const TextStyle &style,
                                                  float max_width, int start,
                                                  int end) {
  if (start >= end) return {};
  
  auto layout = GetOrCreateTextLayout(text, style, max_width > 0 ? max_width : 100000.0f,
                                       100000.0f);
  if (!layout) return {};

  uint32_t count = 0;
  layout->HitTestTextRange(start, end - start, 0, 0, nullptr, 0, &count);
  if (count == 0) return {};

  std::vector<DWRITE_HIT_TEST_METRICS> metrics(count);
  if (FAILED(layout->HitTestTextRange(start, end - start, 0, 0, metrics.data(),
                                      count, &count))) {
    return {};
  }

  std::vector<Rect> rects;
  rects.reserve(count);
  for (const auto &m : metrics) {
    rects.emplace_back(m.left, m.top, m.width, m.height);
  }
  return rects;
}


IDWriteTextFormat *TextRenderer::GetTextFormat(const TextStyle &style) {
  size_t hash = ComputeStyleHash(style);

  auto it = format_cache_.find(hash);
  if (it != format_cache_.end()) {
    return it->second.Get();
  }

  if (!dwrite_factory_)
    return nullptr;

  DWRITE_FONT_WEIGHT weight =
      static_cast<DWRITE_FONT_WEIGHT>(style.font_weight);

  DWRITE_FONT_STYLE dw_style = DWRITE_FONT_STYLE_NORMAL;
  switch (style.font_style) {
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
  HRESULT hr = dwrite_factory_->CreateTextFormat(
      style.font_family.c_str(), nullptr, weight, dw_style,
      DWRITE_FONT_STRETCH_NORMAL, style.font_size, L"", &format);

  if (FAILED(hr))
    return nullptr;

  DWRITE_TEXT_ALIGNMENT text_align = DWRITE_TEXT_ALIGNMENT_LEADING;
  switch (style.alignment) {
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
  switch (style.paragraph_alignment) {
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

  if (!style.word_wrap) {
    format->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
  }

  format_cache_[hash] = format;
  return format.Get();
}

ComPtr<IDWriteTextLayout>
TextRenderer::CreateTextLayout(const std::wstring &text, const TextStyle &style,
                               float max_width, float max_height) {
  auto *format = GetTextFormat(style);
  if (!format || !dwrite_factory_)
    return nullptr;

  ComPtr<IDWriteTextLayout> layout;
  HRESULT hr = dwrite_factory_->CreateTextLayout(
      text.c_str(), static_cast<UINT32>(text.length()), format, max_width,
      max_height, &layout);

  if (FAILED(hr))
    return nullptr;

  return layout;
}

ComPtr<IDWriteTextLayout>
TextRenderer::GetOrCreateTextLayout(const std::wstring &text,
                                    const TextStyle &style, float max_width,
                                    float max_height) {
  TextLayoutCacheKey key{text, ComputeStyleHash(style), max_width, max_height};

  auto it = layout_cache_.find(key);
  if (it != layout_cache_.end()) {
    return it->second;
  }

  auto layout = CreateTextLayout(text, style, max_width, max_height);
  if (!layout)
    return nullptr;

  EvictLayoutCacheIfNeeded();
  layout_cache_[key] = layout;
  return layout;
}

void TextRenderer::EvictLayoutCacheIfNeeded() {
  if (layout_cache_.size() >= kMaxLayoutCacheSize) {
    layout_cache_.clear();
  }
}

ID2D1SolidColorBrush *TextRenderer::GetBrush(const Color &color) {
  uint32_t key = (static_cast<uint32_t>(color.r) << 24) |
                 (static_cast<uint32_t>(color.g) << 16) |
                 (static_cast<uint32_t>(color.b) << 8) |
                 static_cast<uint32_t>(color.a);

  auto it = brush_cache_.find(key);
  if (it != brush_cache_.end()) {
    return it->second.Get();
  }

  if (!d2d_context_)
    return nullptr;

  ComPtr<ID2D1SolidColorBrush> brush;
  d2d_context_->CreateSolidColorBrush(color.to_d2d(), &brush);
  brush_cache_[key] = brush;
  return brush.Get();
}

size_t TextRenderer::ComputeStyleHash(const TextStyle &style) {
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

void TextRenderer::HashCombine(size_t &hash, size_t value) {
  hash ^= value + 0x9e3779b9 + (hash << 6) + (hash >> 2);
}

void TextRenderer::ClearFormatCache() {
  format_cache_.clear();
  brush_cache_.clear();
  layout_cache_.clear();
}

void TextRenderer::ClearLayoutCache() { layout_cache_.clear(); }

} // namespace fluxent
