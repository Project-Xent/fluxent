#include "fluxent/controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "fluxent/controls/renderer_utils.hpp"

namespace fluxent::controls {

void ControlRenderer::RenderText(const xent::ViewData &data,
                                 const Rect &bounds) {
  if (!text_renderer_ || data.text_content.empty())
    return;

  const auto &res = theme_manager_->Resources();

  Color user_color = ToFluXentColor(data.text_color);
  Color text_color = user_color.a > 0 ? user_color : res.TextPrimary;

  float font_size =
      data.font_size > 0 ? data.font_size : kControlContentThemeFontSize;

  std::wstring wtext(data.text_content.begin(), data.text_content.end());
  TextStyle style;
  style.font_size = font_size;
  style.color = text_color;
  style.alignment = TextAlignment::Leading;
  style.paragraph_alignment = ParagraphAlignment::Near;

  text_renderer_->DrawText(wtext, bounds, style);
}

void ControlRenderer::RenderCard(const xent::ViewData &data,
                                 const Rect &bounds) {
  auto d2d = graphics_->GetD2DContext();
  const auto &res = theme_manager_->Resources();

  float corner_radius = data.corner_radius > 0 ? data.corner_radius : fluxent::config::Layout::DefaultCornerRadius;

  D2D1_ROUNDED_RECT rounded_rect =
      D2D1::RoundedRect(D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width,
                                    bounds.y + bounds.height),
                        corner_radius, corner_radius);

  Color user_bg = ToFluXentColor(data.background_color);
  Color bg = user_bg.a > 0 ? user_bg : res.CardBackgroundDefault;
  d2d->FillRoundedRectangle(rounded_rect, GetBrush(bg));

  D2D1_ROUNDED_RECT stroke_rect =
      D2D1::RoundedRect(D2D1::RectF(bounds.x + 0.5f, bounds.y + 0.5f,
                                    bounds.x + bounds.width - 0.5f,
                                    bounds.y + bounds.height - 0.5f),
                        corner_radius, corner_radius);
  d2d->DrawRoundedRectangle(stroke_rect, GetBrush(res.CardStrokeDefault), 1.0f);
}

void ControlRenderer::RenderDivider(const Rect &bounds, bool horizontal) {
  auto d2d = graphics_->GetD2DContext();
  const auto &res = theme_manager_->Resources();

  if (horizontal) {
    float y = bounds.y + bounds.height / 2.0f;
    d2d->DrawLine(D2D1::Point2F(bounds.x, y),
                  D2D1::Point2F(bounds.x + bounds.width, y),
                  GetBrush(res.DividerStrokeDefault), 1.0f);
  } else {
    float x = bounds.x + bounds.width / 2.0f;
    d2d->DrawLine(D2D1::Point2F(x, bounds.y),
                  D2D1::Point2F(x, bounds.y + bounds.height),
                  GetBrush(res.DividerStrokeDefault), 1.0f);
  }
}

void ControlRenderer::RenderView(const xent::ViewData &data,
                                 const Rect &bounds) {
  auto d2d = graphics_->GetD2DContext();

  Color user_bg = ToFluXentColor(data.background_color);

  if (user_bg.a > 0) {
    float corner_radius = data.corner_radius;

    if (corner_radius > 0) {
      D2D1_ROUNDED_RECT rounded_rect = D2D1::RoundedRect(
          D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width,
                      bounds.y + bounds.height),
          corner_radius, corner_radius);
      d2d->FillRoundedRectangle(rounded_rect, GetBrush(user_bg));
    } else {
      D2D1_RECT_F rect =
          D2D1::RectF(bounds.x, bounds.y, bounds.x + bounds.width,
                      bounds.y + bounds.height);
      d2d->FillRectangle(rect, GetBrush(user_bg));
    }
  }

  Color user_border = ToFluXentColor(data.border_color);
  if (data.border_width > 0 && user_border.a > 0) {
    float corner_radius = data.corner_radius;
    float half_border = data.border_width / 2.0f;

    if (corner_radius > 0) {
      D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
          D2D1::RectF(bounds.x + half_border, bounds.y + half_border,
                      bounds.x + bounds.width - half_border,
                      bounds.y + bounds.height - half_border),
          corner_radius, corner_radius);
      d2d->DrawRoundedRectangle(stroke_rect, GetBrush(user_border),
                                data.border_width);
    } else {
      D2D1_RECT_F rect =
          D2D1::RectF(bounds.x + half_border, bounds.y + half_border,
                      bounds.x + bounds.width - half_border,
                      bounds.y + bounds.height - half_border);
      d2d->DrawRectangle(rect, GetBrush(user_border), data.border_width);
    }
  }

  if (!data.text_content.empty() && text_renderer_) {
    RenderText(data, bounds);
  }
}

} // namespace fluxent::controls
