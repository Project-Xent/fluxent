#include "fluxent/controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "renderer_utils.hpp"

// DirectComposition
#include <dcomp.h>

namespace fluxent::controls {

ControlRenderer::ControlRenderer(GraphicsPipeline *graphics, TextRenderer *text)
    : graphics_(graphics), text_renderer_(text) {
  auto d2d = graphics_->get_d2d_context();
  d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
}

void ControlRenderer::begin_frame() {
  frame_hovered_button_ = nullptr;
  hover_corner_radius_ = 0.0f;
  has_active_bg_transitions_ = false;
  frame_buttons_seen_.clear();
}

void ControlRenderer::end_frame() {
  update_hover_overlay();

  // Clean up transition entries for buttons that no longer exist.
  if (frame_buttons_seen_.empty()) {
    button_bg_transitions_.clear();
  } else {
    for (auto it = button_bg_transitions_.begin();
         it != button_bg_transitions_.end();) {
      if (frame_buttons_seen_.find(it->first) == frame_buttons_seen_.end()) {
        it = button_bg_transitions_.erase(it);
      } else {
        ++it;
      }
    }
  }

  // Drive short BrushTransition animations without a dedicated timer.
  if (has_active_bg_transitions_ && graphics_) {
    graphics_->request_redraw();
  }
}

void ControlRenderer::render(const xent::ViewData &data, const Rect &bounds,
                             const ControlState &state) {
  const auto &type = data.component_type;

  if (type == "Button") {
    frame_buttons_seen_.insert(&data);
    render_button(data, bounds, state);
  } else if (type == "ToggleSwitch") {
    render_toggle_switch(data, bounds, state);
  } else if (type == "ToggleButton") {
    frame_buttons_seen_.insert(&data);
    render_toggle_button(data, bounds, state);
  } else if (type == "Text") {
    render_text(data, bounds);
  } else if (type == "Card") {
    render_card(data, bounds);
  } else if (type == "Divider") {
    render_divider(bounds, true);
  } else {
    render_view(data, bounds);
  }
}

ID2D1SolidColorBrush *ControlRenderer::get_brush(const Color &color) {
  brush_->SetColor(D2D1::ColorF(color.r / 255.0f, color.g / 255.0f,
                                color.b / 255.0f, color.a / 255.0f));
  return brush_.Get();
}

void ControlRenderer::ensure_hover_overlay_surface(int width, int height,
                                                   const Color &color) {
  if (!graphics_)
    return;

  width = std::max(1, width);
  height = std::max(1, height);

  auto *dcomp = graphics_->get_dcomp_device();
  auto *d2d = graphics_->get_d2d_overlay_context();
  if (!dcomp || !d2d)
    return;

  bool need_recreate = !hover_surface_ || hover_surface_width_ != width ||
                       hover_surface_height_ != height;

  if (need_recreate) {
    hover_surface_.Reset();
    throw_if_failed(
        dcomp->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                             DXGI_ALPHA_MODE_PREMULTIPLIED, &hover_surface_),
        "Failed to create DComp hover surface");

    hover_surface_width_ = width;
    hover_surface_height_ = height;
    hover_surface_color_ = Color::transparent();
  }

  if (!need_recreate && hover_surface_color_.r == color.r &&
      hover_surface_color_.g == color.g && hover_surface_color_.b == color.b &&
      hover_surface_color_.a == color.a) {
    return;
  }

  RECT update = {0, 0, width, height};
  POINT offset = {};
  ComPtr<IDXGISurface> dxgi_surface;
  throw_if_failed(
      hover_surface_->BeginDraw(&update, IID_PPV_ARGS(&dxgi_surface), &offset),
      "Failed to BeginDraw on DComp surface");

  const auto dpi = graphics_->get_dpi();
  const float scale_x = dpi.dpi_x / 96.0f;
  const float scale_y = dpi.dpi_y / 96.0f;
  D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED),
      dpi.dpi_x, dpi.dpi_y);

  ComPtr<ID2D1Bitmap1> bitmap;
  throw_if_failed(
      d2d->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &props, &bitmap),
      "Failed to create D2D bitmap for DComp surface");

  ComPtr<ID2D1Image> old_target;
  d2d->GetTarget(&old_target);
  d2d->SetTarget(bitmap.Get());

  D2D1_MATRIX_3X2_F old_transform;
  d2d->GetTransform(&old_transform);
  // BeginDraw gives the update offset in physical pixels; convert to DIPs for
  // this D2D context.
  const float offset_x_dip =
      scale_x > 0.0f ? (static_cast<float>(offset.x) / scale_x) : 0.0f;
  const float offset_y_dip =
      scale_y > 0.0f ? (static_cast<float>(offset.y) / scale_y) : 0.0f;
  d2d->SetTransform(D2D1::Matrix3x2F::Translation(offset_x_dip, offset_y_dip));

  d2d->BeginDraw();

  // Draw rounded overlay matching the hovered control's corner radius.
  // Use transparent clear so we don't tint outside the rounded rect.
  d2d->Clear(D2D1::ColorF(0, 0, 0, 0));

  const float width_dip = scale_x > 0.0f ? (static_cast<float>(width) / scale_x)
                                         : static_cast<float>(width);
  const float height_dip = scale_y > 0.0f
                               ? (static_cast<float>(height) / scale_y)
                               : static_cast<float>(height);

  float radius = std::max(0.0f, hover_corner_radius_);
  radius = std::min(radius, std::min(width_dip, height_dip) * 0.5f);

  ComPtr<ID2D1SolidColorBrush> overlay_brush;
  if (SUCCEEDED(d2d->CreateSolidColorBrush(color.to_d2d(), &overlay_brush)) &&
      overlay_brush) {
    D2D1_ROUNDED_RECT rr = D2D1::RoundedRect(
        D2D1::RectF(0.0f, 0.0f, width_dip, height_dip), radius, radius);
    d2d->FillRoundedRectangle(rr, overlay_brush.Get());
  }

  HRESULT hr = d2d->EndDraw();

  d2d->SetTransform(old_transform);

  d2d->SetTarget(old_target.Get());
  if (FAILED(hr)) {
    hover_surface_->EndDraw();
    return;
  }
  hover_surface_->EndDraw();
  hover_surface_color_ = color;
}

static void animate_opacity(GraphicsPipeline *graphics,
                            IDCompositionVisual *visual, float from, float to) {
  if (!graphics || !visual)
    return;
  (void)from;

#if defined(__IDCompositionVisual3_INTERFACE_DEFINED__)
  auto *dcomp = graphics->get_dcomp_device();
  if (!dcomp)
    return;

  ComPtr<IDCompositionVisual3> v3;
  if (FAILED(visual->QueryInterface(IID_PPV_ARGS(&v3))) || !v3) {
    visual->SetOpacity(to);
    graphics->commit();
    return;
  }

  ComPtr<IDCompositionAnimation> anim;
  if (FAILED(dcomp->CreateAnimation(&anim)) || !anim) {
    v3->SetOpacity(to);
    graphics->commit();
    return;
  }

  const double duration = kButtonBrushTransitionSeconds;
  const float slope = static_cast<float>((to - from) / duration);

  anim->AddCubic(0.0, from, slope, 0.0f, 0.0f);
  anim->End(duration, to);

  v3->SetOpacity(anim.Get());
  graphics->commit();
#else
  (void)to;
  graphics->commit();
#endif
}

void ControlRenderer::update_hover_overlay() {
  if (!graphics_)
    return;

  auto *dcomp = graphics_->get_dcomp_device();
  auto *root = graphics_->get_dcomp_root_visual();
  if (!dcomp || !root)
    return;

  const bool should_show = (frame_hovered_button_ != nullptr);
  const float target_opacity = should_show ? 1.0f : 0.0f;

  if (!hover_visual_) {
    throw_if_failed(dcomp->CreateVisual(&hover_visual_),
                    "Failed to create hover visual");
#if defined(__IDCompositionVisual3_INTERFACE_DEFINED__)
    if (ComPtr<IDCompositionVisual3> v3;
        SUCCEEDED(hover_visual_.As(&v3)) && v3) {
      v3->SetOpacity(0.0f);
    }
#endif
  }

  if (!hover_visual_added_) {
    graphics_->add_overlay_visual(hover_visual_.Get());
    hover_visual_added_ = true;
  }

  const auto dpi = graphics_->get_dpi();
  const float scale_x = dpi.dpi_x / 96.0f;
  const float scale_y = dpi.dpi_y / 96.0f;

  if (should_show) {
    Color overlay;
    switch (theme::current_mode()) {
    case theme::Mode::Light:
      overlay = Color(0xCD, 0xCD, 0xCD, 0xD1);
      break;
    case theme::Mode::Dark:
    case theme::Mode::HighContrast:
    default:
      overlay = Color(255, 255, 255, 0x21);
      break;
    }

    const int width_px = std::max(
        1, static_cast<int>(std::lround(hover_bounds_.width * scale_x)));
    const int height_px = std::max(
        1, static_cast<int>(std::lround(hover_bounds_.height * scale_y)));

    ensure_hover_overlay_surface(width_px, height_px, overlay);

    if (hover_surface_) {
      hover_visual_->SetContent(hover_surface_.Get());
    }

    hover_visual_->SetOffsetX(
        static_cast<float>(std::lround(hover_bounds_.x * scale_x)));
    hover_visual_->SetOffsetY(
        static_cast<float>(std::lround(hover_bounds_.y * scale_y)));
  } else {
    hover_corner_radius_ = 0.0f;
  }

  if (target_opacity != hover_opacity_) {
    const float from = hover_opacity_;
    hover_opacity_ = target_opacity;
    animate_opacity(graphics_, hover_visual_.Get(), from, target_opacity);
  }
}

void ControlRenderer::draw_focus_rect(const Rect &bounds, float corner_radius) {
  auto d2d = graphics_->get_d2d_context();
  const auto &res = theme::res();

  D2D1_ROUNDED_RECT outer =
      D2D1::RoundedRect(D2D1::RectF(bounds.x - 3.0f, bounds.y - 3.0f,
                                    bounds.x + bounds.width + 3.0f,
                                    bounds.y + bounds.height + 3.0f),
                        corner_radius + 2.0f, corner_radius + 2.0f);
  d2d->DrawRoundedRectangle(outer, get_brush(res.FocusStrokeOuter), 2.0f);

  D2D1_ROUNDED_RECT inner =
      D2D1::RoundedRect(D2D1::RectF(bounds.x - 1.0f, bounds.y - 1.0f,
                                    bounds.x + bounds.width + 1.0f,
                                    bounds.y + bounds.height + 1.0f),
                        corner_radius + 1.0f, corner_radius + 1.0f);
  d2d->DrawRoundedRectangle(inner, get_brush(res.FocusStrokeInner), 1.0f);
}

void ControlRenderer::draw_elevation_border(const Rect &bounds,
                                            float corner_radius,
                                            bool is_accent) {
  if (!graphics_)
    return;
  auto *d2d = graphics_->get_d2d_context();
  if (!d2d)
    return;

  const auto &res = theme::res();

  const bool need_recreate =
      !control_elevation_border_brush_ ||
      control_elevation_border_secondary_.r != res.ControlStrokeSecondary.r ||
      control_elevation_border_secondary_.g != res.ControlStrokeSecondary.g ||
      control_elevation_border_secondary_.b != res.ControlStrokeSecondary.b ||
      control_elevation_border_secondary_.a != res.ControlStrokeSecondary.a ||
      control_elevation_border_default_.r != res.ControlStrokeDefault.r ||
      control_elevation_border_default_.g != res.ControlStrokeDefault.g ||
      control_elevation_border_default_.b != res.ControlStrokeDefault.b ||
      control_elevation_border_default_.a != res.ControlStrokeDefault.a ||
      !control_accent_elevation_border_brush_ ||
      control_accent_elevation_border_secondary_.r !=
          res.ControlStrokeOnAccentSecondary.r ||
      control_accent_elevation_border_secondary_.g !=
          res.ControlStrokeOnAccentSecondary.g ||
      control_accent_elevation_border_secondary_.b !=
          res.ControlStrokeOnAccentSecondary.b ||
      control_accent_elevation_border_secondary_.a !=
          res.ControlStrokeOnAccentSecondary.a ||
      control_accent_elevation_border_default_.r !=
          res.ControlStrokeOnAccentDefault.r ||
      control_accent_elevation_border_default_.g !=
          res.ControlStrokeOnAccentDefault.g ||
      control_accent_elevation_border_default_.b !=
          res.ControlStrokeOnAccentDefault.b ||
      control_accent_elevation_border_default_.a !=
          res.ControlStrokeOnAccentDefault.a;

  if (need_recreate) {
    control_elevation_border_brush_.Reset();

    D2D1_GRADIENT_STOP stops[2] = {
        {0.33f, res.ControlStrokeSecondary.to_d2d()},
        {1.0f, res.ControlStrokeDefault.to_d2d()},
    };

    ComPtr<ID2D1GradientStopCollection> stop_collection;
    throw_if_failed(
        d2d->CreateGradientStopCollection(
            stops, 2, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_CLAMP, &stop_collection),
        "Failed to create elevation border gradient stop collection");

    throw_if_failed(
        d2d->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f),
                                                D2D1::Point2F(0.0f, 3.0f)),
            stop_collection.Get(), &control_elevation_border_brush_),
        "Failed to create elevation border brush");

    control_elevation_border_secondary_ = res.ControlStrokeSecondary;
    control_elevation_border_default_ = res.ControlStrokeDefault;

    D2D1_GRADIENT_STOP accent_stops[2] = {
        {0.33f, res.ControlStrokeOnAccentSecondary.to_d2d()},
        {1.0f, res.ControlStrokeOnAccentDefault.to_d2d()},
    };
    ComPtr<ID2D1GradientStopCollection> accent_stop_collection;
    throw_if_failed(
        d2d->CreateGradientStopCollection(accent_stops, 2, D2D1_GAMMA_2_2,
                                          D2D1_EXTEND_MODE_CLAMP,
                                          &accent_stop_collection),
        "Failed to create accent elevation gradient stop collection");

    control_accent_elevation_border_brush_.Reset();
    throw_if_failed(
        d2d->CreateLinearGradientBrush(
            D2D1::LinearGradientBrushProperties(D2D1::Point2F(0.0f, 0.0f),
                                                D2D1::Point2F(0.0f, 3.0f)),
            accent_stop_collection.Get(),
            &control_accent_elevation_border_brush_),
        "Failed to create accent elevation border brush");

    control_accent_elevation_border_secondary_ =
        res.ControlStrokeOnAccentSecondary;
    control_accent_elevation_border_default_ = res.ControlStrokeOnAccentDefault;
  }

  const bool is_light = (theme::current_mode() == theme::Mode::Light);
  const bool flip_y = is_accent || is_light;

  ID2D1LinearGradientBrush *brush = control_elevation_border_brush_.Get();
  if (is_accent && control_accent_elevation_border_brush_) {
    brush = control_accent_elevation_border_brush_.Get();
  }

  const float band_start =
      flip_y ? (bounds.y + bounds.height - 3.0f) : bounds.y;
  const float band_end = band_start + 3.0f;

  if (flip_y) {
    brush->SetStartPoint(D2D1::Point2F(0.0f, band_end));
    brush->SetEndPoint(D2D1::Point2F(0.0f, band_start));
  } else {
    brush->SetStartPoint(D2D1::Point2F(0.0f, band_start));
    brush->SetEndPoint(D2D1::Point2F(0.0f, band_end));
  }

  const float left = bounds.x + 0.5f;
  const float top = bounds.y + 0.5f;
  const float right = bounds.x + bounds.width - 0.5f;
  const float bottom = bounds.y + bounds.height - 0.5f;

  D2D1_ROUNDED_RECT stroke_rect = D2D1::RoundedRect(
      D2D1::RectF(left, top, right, bottom), corner_radius, corner_radius);

  d2d->DrawRoundedRectangle(stroke_rect, brush, 1.0f);
}

} // namespace fluxent::controls
