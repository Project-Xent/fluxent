#include "fluxent/controls/scroll_view_renderer.hpp"
#include "fluxent/controls/layout.hpp"
#include "fluxent/config.hpp"
#include <algorithm>

namespace fluxent::controls
{

void ScrollViewRenderer::BeginFrame() {}

bool ScrollViewRenderer::EndFrame() { return false; }

ID2D1SolidColorBrush *ScrollViewRenderer::GetBrush(ID2D1DeviceContext *d2d, const Color &color)
{
  if (!brush_)
  {
    d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
  }
  brush_->SetColor(
      D2D1::ColorF(color.r / 255.0f, color.g / 255.0f, color.b / 255.0f, color.a / 255.0f));
  return brush_.Get();
}

void ScrollViewRenderer::Render(const RenderContext &ctx, const xent::ViewData &data,
                                const Rect &bounds, const ControlState &state)
{
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources(); // Theme resources

  // 1. Calculate Content Size based on children layout
  float max_child_right = 0.0f;
  float max_child_bottom = 0.0f;

  for (const auto &child : data.children)
  {
    float right = child->LayoutX() + child->LayoutWidth();
    float bottom = child->LayoutY() + child->LayoutHeight();
    if (right > max_child_right)
      max_child_right = right;
    if (bottom > max_child_bottom)
      max_child_bottom = bottom;
  }

  // Ensure content size is at least the view size (scrollable area includes
  // empty space if content is small)
  float content_width = std::max(bounds.width, max_child_right);
  float content_height = std::max(bounds.height, max_child_bottom);

  // 2. Draw Background
  Color bg_color = Color(data.background_color.r, data.background_color.g, data.background_color.b,
                         data.background_color.a);

  if (bg_color.a > 0)
  {
    if (data.corner_radius > 0)
    {
      D2D1_ROUNDED_RECT rr =
          D2D1::RoundedRect(bounds.to_d2d(), data.corner_radius, data.corner_radius);
      d2d->FillRoundedRectangle(rr, GetBrush(d2d, bg_color));
    }
    else
    {
      d2d->FillRectangle(bounds.to_d2d(), GetBrush(d2d, bg_color));
    }
  }

  // 3. Determine ScrollBar Visibility - MOVED TO OVERLAY
}

void ScrollViewRenderer::RenderOverlay(const RenderContext &ctx, const xent::ViewData &data,
                                       const Rect &bounds, const ControlState &state)
{
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  // 1. Content Size
  float content_width = 0.0f;
  float content_height = 0.0f;

  // Calculate content size
  float max_r = 0.0f, max_b = 0.0f;
  for (const auto &child : data.children)
  {
    float r = child->LayoutX() + child->LayoutWidth();
    float b = child->LayoutY() + child->LayoutHeight();
    if (r > max_r)
      max_r = r;
    if (b > max_b)
      max_b = b;
  }
  content_width = std::max(bounds.width, max_r);
  content_height = std::max(bounds.height, max_b);

  // 2. Visibility Policy (Consolidated with InputHandler logic via layout.hpp usually,
  // but here we need to decide policy before calling layout...
  // actually layout.hpp takes bool arguments now)

  auto should_show = [](xent::ScrollBarVisibility vis, float content, float size)
  {
    if (vis == xent::ScrollBarVisibility::Hidden || vis == xent::ScrollBarVisibility::Disabled)
      return false;
    if (vis == xent::ScrollBarVisibility::Visible)
      return true;
    return content > size + fluxent::config::Input::ScrollVisibilityEpsilon;
  };

  bool show_h = should_show(data.horizontal_scrollbar_visibility, content_width, bounds.width);
  bool show_v = should_show(data.vertical_scrollbar_visibility, content_height, bounds.height);

  // 3. Calculate Layout
  auto layout =
      CalculateScrollViewLayout(bounds.width, bounds.height, content_width, content_height,
                                data.scroll_offset_x, data.scroll_offset_y, show_h, show_v);

  Color thumb_color =
      state.is_hovered ? res.ControlStrongFillDisabled : res.ControlStrongFillDefault;

  // 4. Render Vertical
  if (layout.v_bar.visible)
  {
    D2D1_ROUNDED_RECT thumb =
        D2D1::RoundedRect(layout.v_bar.thumb_rect.to_d2d(), layout.v_bar.thumb_rect.width * 0.5f,
                          layout.v_bar.thumb_rect.width * 0.5f);
    // Apply offset to bounds (since layout is 0,0 based relative to view)
    thumb.rect.left += bounds.x;
    thumb.rect.top += bounds.y;
    thumb.rect.right += bounds.x;
    thumb.rect.bottom += bounds.y;

    d2d->FillRoundedRectangle(thumb, GetBrush(d2d, thumb_color));
  }

  // 5. Render Horizontal
  if (layout.h_bar.visible)
  {
    D2D1_ROUNDED_RECT thumb =
        D2D1::RoundedRect(layout.h_bar.thumb_rect.to_d2d(), layout.h_bar.thumb_rect.height * 0.5f,
                          layout.h_bar.thumb_rect.height * 0.5f);
    // Apply offset
    thumb.rect.left += bounds.x;
    thumb.rect.top += bounds.y;
    thumb.rect.right += bounds.x;
    thumb.rect.bottom += bounds.y;

    d2d->FillRoundedRectangle(thumb, GetBrush(d2d, thumb_color));
  }
}

} // namespace fluxent::controls
