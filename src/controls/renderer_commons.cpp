#include "common_drawing.hpp"
#include <d2d1_3.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace fluxent::controls {

static ComPtr<ID2D1SolidColorBrush> CreateBrush(ID2D1DeviceContext *d2d,
                                                const Color &color) {
  ComPtr<ID2D1SolidColorBrush> brush;
  d2d->CreateSolidColorBrush(D2D1::ColorF(color.r / 255.0f, color.g / 255.0f,
                                          color.b / 255.0f, color.a / 255.0f),
                             &brush);
  return brush;
}

void DrawFocusRect(const RenderContext &ctx, const Rect &bounds,
                   float corner_radius) {
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  const float outer_pad = res.Focus.OuterPadding;
  const float outer_thick = res.Focus.OuterThickness;
  const float inner_thick = res.Focus.InnerThickness;
  const float inner_pad = res.Focus.InnerPadding;

  D2D1_ROUNDED_RECT outer = D2D1::RoundedRect(
      D2D1::RectF(bounds.x - outer_pad, bounds.y - outer_pad,
                  bounds.x + bounds.width + outer_pad,
                  bounds.y + bounds.height + outer_pad),
      corner_radius + outer_pad - 1.0f, corner_radius + outer_pad - 1.0f);

  if (auto brush = CreateBrush(d2d, res.FocusStrokeOuter)) {
    d2d->DrawRoundedRectangle(outer, brush.Get(), outer_thick);
  }

  D2D1_ROUNDED_RECT inner =
      D2D1::RoundedRect(D2D1::RectF(bounds.x - inner_pad, bounds.y - inner_pad,
                                    bounds.x + bounds.width + inner_pad,
                                    bounds.y + bounds.height + inner_pad),
                        corner_radius + inner_pad, corner_radius + inner_pad);

  if (auto brush = CreateBrush(d2d, res.FocusStrokeInner)) {
    d2d->DrawRoundedRectangle(inner, brush.Get(), inner_thick);
  }
}

} // namespace fluxent::controls
