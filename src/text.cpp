// FluXent Text Renderer - DirectWrite implementation
#include "fluxent/text.hpp"
#include <functional>
#include <stdexcept>


namespace fluxent {

TextRenderer::TextRenderer(GraphicsPipeline *graphics) : graphics_(graphics) {
  if (!graphics_) {
    throw std::invalid_argument("Graphics pipeline cannot be null");
  }
  d2d_context_ = graphics_->GetD2DContext();
  dwrite_factory_ = graphics_->GetDWriteFactory();
}

TextRenderer::~TextRenderer() = default;

// Text drawing

void TextRenderer::DrawText(const std::wstring &text, const Rect &bounds,
                            const TextStyle &style) {
  if (text.empty() || !d2d_context_)
    return;

  auto layout = CreateTextLayout(text, style, bounds.width, bounds.height);
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

// Text measurement

Size TextRenderer::MeasureText(const std::wstring &text, const TextStyle &style,
                               float max_width) {
  if (text.empty())
    return Size();

  float layout_width = max_width > 0 ? max_width : 100000.0f;
  auto layout = CreateTextLayout(text, style, layout_width, 100000.0f);
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

  auto layout = CreateTextLayout(text, style, max_width, 100000.0f);
  if (!layout)
    return 0;

  DWRITE_TEXT_METRICS metrics;
  if (SUCCEEDED(layout->GetMetrics(&metrics))) {
    return static_cast<int>(metrics.lineCount);
  }

  return 0;
}

// Resource management

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
}

} // namespace fluxent
