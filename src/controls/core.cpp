#include <memory>

#include "fluxent/controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "fluxent/plugin_manager.hpp"
#include "fluxent/config.hpp"

#include "fluxent/controls/button_renderer.hpp"
#include "fluxent/controls/checkbox_renderer.hpp"
#include "fluxent/controls/radio_renderer.hpp"
#include "fluxent/controls/scroll_view_renderer.hpp"
#include "fluxent/controls/slider_renderer.hpp"
#include "fluxent/controls/toggle_renderer.hpp"
#include "fluxent/controls/textbox_renderer.hpp"


// Sub-renderers (implementation handled in their own cpp files)
// Headers are included in control_renderer.hpp

// DirectComposition
#include <cmath>
#include <dcomp.h>

namespace fluxent::controls {

ControlRenderer::ControlRenderer(GraphicsPipeline *graphics, TextRenderer *text,
                                 theme::ThemeManager *theme_manager,
                                 PluginManager *plugin_manager)
    : graphics_(graphics), text_renderer_(text), theme_manager_(theme_manager) {

  ctx_ = {graphics, text, theme_manager, plugin_manager};
  button_renderer_ = std::make_unique<ButtonRenderer>();
  toggle_renderer_ = std::make_unique<ToggleSwitchRenderer>();
  checkbox_renderer_ = std::make_unique<CheckBoxRenderer>();
  radio_renderer_ = std::make_unique<RadioButtonRenderer>();
  slider_renderer_ = std::make_unique<SliderRenderer>();
  textbox_renderer_ = std::make_unique<TextBoxRenderer>();
  scroll_view_renderer_ = std::make_unique<ScrollViewRenderer>();


  auto d2d = graphics_->GetD2DContext();
  d2d->CreateSolidColorBrush(D2D1::ColorF(1, 1, 1, 1), &brush_);
}

ControlRenderer::~ControlRenderer() = default;

void ControlRenderer::BeginFrame() {
  CheckResources(); // Check theme version

  button_renderer_->BeginFrame();
  toggle_renderer_->BeginFrame();
  checkbox_renderer_->BeginFrame();
  radio_renderer_->BeginFrame();
  slider_renderer_->BeginFrame();
  textbox_renderer_->BeginFrame();
  scroll_view_renderer_->BeginFrame();


  frame_hovered_button_ = nullptr;
  hover_corner_radius_ = 0.0f;
  frame_buttons_seen_.clear();
}

void ControlRenderer::CheckResources() {
  uint64_t current_version = theme_manager_->Version();
  if (current_version != last_theme_version_) {
    // Theme changed, invalidate cached resources
    brush_->SetColor(D2D1::ColorF(0, 0, 0, 0));
    control_elevation_border_brush_.Reset();
    control_accent_elevation_border_brush_.Reset();

    last_theme_version_ = current_version;
  }
}

void ControlRenderer::EndFrame() {
  UpdateHoverOverlay();

  bool active = false;
  active |= button_renderer_->EndFrame();
  active |= toggle_renderer_->EndFrame();
  active |= checkbox_renderer_->EndFrame();
  active |= radio_renderer_->EndFrame();
  active |= slider_renderer_->EndFrame();
  active |= textbox_renderer_->EndFrame();
  active |= scroll_view_renderer_->EndFrame();

  if (active && graphics_) {
    graphics_->RequestRedraw();
  }
}

void ControlRenderer::Render(const xent::ViewData &data, const Rect &bounds,
                             const ControlState &state) {
  switch (data.type) {
  case xent::ComponentType::Button:
    frame_buttons_seen_.insert(&data);
    RenderButton(data, bounds, state);
    break;
  case xent::ComponentType::ToggleSwitch:
    RenderToggleSwitch(data, bounds, state);
    break;
  case xent::ComponentType::ToggleButton:
    frame_buttons_seen_.insert(&data);
    RenderToggleButton(data, bounds, state);
    break;
  case xent::ComponentType::Text:
    RenderText(data, bounds);
    break;
  case xent::ComponentType::CheckBox:
    frame_buttons_seen_.insert(&data);
    RenderCheckBox(data, bounds, state);
    break;
  case xent::ComponentType::RadioButton:
    frame_buttons_seen_.insert(&data);
    RenderRadioButton(data, bounds, state);
    break;
  case xent::ComponentType::Slider:
    // Slider usually captures input, so maybe track it?
    // For now just render.
    RenderSlider(data, bounds, state);
    break;
  case xent::ComponentType::TextBox:
    frame_buttons_seen_.insert(&data); // Treat as interactive
    RenderTextBox(data, bounds, state);
    break;

  case xent::ComponentType::ScrollView:
    RenderScrollView(data, bounds, state);
    break;
  case xent::ComponentType::Card:
    RenderCard(data, bounds);
    break;
  case xent::ComponentType::Divider:
    RenderDivider(bounds, true);
    break;
  case xent::ComponentType::Custom:
    if (ctx_.plugins) {
      if (auto *plugin = ctx_.plugins->Get(data.extension_type)) {
        plugin->Render(data, ctx_, bounds);
      }
    }
    break;
  default:
    RenderView(data, bounds);
    break;
  }
}

void ControlRenderer::RenderOverlay(const xent::ViewData &data,
                                    const Rect &bounds,
                                    const ControlState &state) {
  if (data.type == xent::ComponentType::ScrollView) {
    scroll_view_renderer_->RenderOverlay(ctx_, data, bounds, state);
  }
}

void ControlRenderer::RenderButton(const xent::ViewData &data,
                                   const Rect &bounds,
                                   const ControlState &state) {
  if (bounds.contains(state.mouse_x, state.mouse_y)) {
    frame_hovered_button_ = &data;
    hover_bounds_ = bounds;
    hover_corner_radius_ = theme_manager_->Resources().ControlCornerRadius;
  }
  button_renderer_->Render(ctx_, data, bounds, state);
}

void ControlRenderer::RenderToggleButton(const xent::ViewData &data,
                                         const Rect &bounds,
                                         const ControlState &state) {
  if (bounds.contains(state.mouse_x, state.mouse_y)) {
    frame_hovered_button_ = &data;
    hover_bounds_ = bounds;
    hover_corner_radius_ = theme_manager_->Resources().ControlCornerRadius;
  }
  button_renderer_->RenderToggleButton(ctx_, data, bounds, state);
}

void ControlRenderer::RenderToggleSwitch(const xent::ViewData &data,
                                         const Rect &bounds,
                                         const ControlState &state) {
  toggle_renderer_->Render(ctx_, data, bounds, state);
}

void ControlRenderer::RenderCheckBox(const xent::ViewData &data,
                                     const Rect &bounds,
                                     const ControlState &state) {
  if (bounds.contains(state.mouse_x, state.mouse_y)) {
    frame_hovered_button_ = &data;
    hover_bounds_ = bounds;
    hover_corner_radius_ = theme_manager_->Resources().CheckBox.CornerRadius;
  }
  checkbox_renderer_->RenderCheckBox(ctx_, data, bounds, state);
}

void ControlRenderer::RenderRadioButton(const xent::ViewData &data,
                                        const Rect &bounds,
                                        const ControlState &state) {
  if (bounds.contains(state.mouse_x, state.mouse_y)) {
    frame_hovered_button_ = &data;
    hover_bounds_ = bounds;
    hover_corner_radius_ = theme_manager_->Resources().CheckBox.CornerRadius;
  }
  radio_renderer_->Render(ctx_, data, bounds, state);
}

void ControlRenderer::RenderSlider(const xent::ViewData &data,
                                   const Rect &bounds,
                                   const ControlState &state) {
  slider_renderer_->Render(ctx_, data, bounds, state);
}

void ControlRenderer::RenderTextBox(const xent::ViewData &data,
                                    const Rect &bounds,
                                    const ControlState &state) {
  if (bounds.contains(state.mouse_x, state.mouse_y)) {
    frame_hovered_button_ = &data;
    hover_bounds_ = bounds;
    // float corner_radius = data.corner_radius > 0 ? data.corner_radius :
    // theme_manager_->Resources().ControlCornerRadius;
    // Hover overlay might NOT be desired for TextBox (it changes border usually).
    // WinUI TextBox hover effect is border change, not overlay.
    // So we skip overlay for TextBox or handle it inside renderer.
    // Fluxent CheckBox uses overlay. Button uses overlay.
    // TextBoxRenderer handles hover state visually in DrawChrome.
    // So we DO NOT set frame_hovered_button_ here?
    // If we set it, `UpdateHoverOverlay` will draw a semi-transparent box on top.
    // This is probably WRONG for TextBox (background shouldn't be covered).
    // So I comment it out.
    // frame_hovered_button_ = &data;
  }
  textbox_renderer_->Render(ctx_, data, bounds, state);
}

void ControlRenderer::RenderScrollView(const xent::ViewData &data,
                                       const Rect &bounds,
                                       const ControlState &state) {
  scroll_view_renderer_->Render(ctx_, data, bounds, state);
}

void ControlRenderer::EnsureHoverOverlaySurface(int width, int height,
                                                const Color &color) {
  if (!graphics_)
    return;

  width = std::max(1, width);
  height = std::max(1, height);

  auto *dcomp = graphics_->GetDCompDevice();
  auto *d2d = graphics_->GetD2DOverlayContext();
  if (!dcomp || !d2d)
    return;

  bool need_recreate = !hover_surface_ || hover_surface_width_ != width ||
                       hover_surface_height_ != height;

  if (need_recreate) {
    hover_surface_.Reset();
    hover_surface_.Reset();
    HRESULT hr =
        dcomp->CreateSurface(width, height, DXGI_FORMAT_B8G8R8A8_UNORM,
                             DXGI_ALPHA_MODE_PREMULTIPLIED, &hover_surface_);
    if (FAILED(hr))
      return;

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
  HRESULT hr =
      hover_surface_->BeginDraw(&update, IID_PPV_ARGS(&dxgi_surface), &offset);
  if (FAILED(hr))
    return;

  const auto dpi = graphics_->GetDpi();
  const float scale_x = dpi.dpi_x / 96.0f;
  const float scale_y = dpi.dpi_y / 96.0f;
  D2D1_BITMAP_PROPERTIES1 props = D2D1::BitmapProperties1(
      D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW,
      D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,
                        D2D1_ALPHA_MODE_PREMULTIPLIED),
      dpi.dpi_x, dpi.dpi_y);

  ComPtr<ID2D1Bitmap1> bitmap;
  hr = d2d->CreateBitmapFromDxgiSurface(dxgi_surface.Get(), &props, &bitmap);
  if (FAILED(hr))
    return;

  ComPtr<ID2D1Image> old_target;
  d2d->GetTarget(&old_target);
  d2d->SetTarget(bitmap.Get());

  D2D1_MATRIX_3X2_F old_transform;
  d2d->GetTransform(&old_transform);
  const float offset_x_dip =
      scale_x > 0.0f ? (static_cast<float>(offset.x) / scale_x) : 0.0f;
  const float offset_y_dip =
      scale_y > 0.0f ? (static_cast<float>(offset.y) / scale_y) : 0.0f;
  d2d->SetTransform(D2D1::Matrix3x2F::Translation(offset_x_dip, offset_y_dip));

  d2d->BeginDraw();
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

  hr = d2d->EndDraw();
  d2d->SetTransform(old_transform);
  d2d->SetTarget(old_target.Get());

  hover_surface_->EndDraw();
  hover_surface_color_ = color;
}

static void AnimateOpacity(GraphicsPipeline *graphics,
                           IDCompositionVisual *visual, float from, float to) {
  if (!graphics || !visual)
    return;
  (void)from;

#if defined(__IDCompositionVisual3_INTERFACE_DEFINED__)
  auto *dcomp = graphics->GetDCompDevice();
  if (!dcomp)
    return;

  ComPtr<IDCompositionVisual3> v3;
  if (FAILED(visual->QueryInterface(IID_PPV_ARGS(&v3))) || !v3) {
    visual->SetOpacity(to);
    graphics->Commit();
    return;
  }

  ComPtr<IDCompositionAnimation> anim;
  if (FAILED(dcomp->CreateAnimation(&anim)) || !anim) {
    v3->SetOpacity(to);
    graphics->Commit();
    return;
  }

  const double duration = kButtonBrushTransitionSeconds;
  const float slope = static_cast<float>((to - from) / duration);

  anim->AddCubic(0.0, from, slope, 0.0f, 0.0f);
  anim->End(duration, to);

  v3->SetOpacity(anim.Get());
  graphics->Commit();
#else
  (void)to;
  graphics->Commit();
#endif
}

void ControlRenderer::UpdateHoverOverlay() {
  if (!graphics_)
    return;

  auto *dcomp = graphics_->GetDCompDevice();
  if (!dcomp)
    return;

  const bool should_show = (frame_hovered_button_ != nullptr);
  const float target_opacity = should_show ? 1.0f : 0.0f;

  if (!hover_visual_) {
    HRESULT hr = dcomp->CreateVisual(&hover_visual_);
    if (FAILED(hr))
      return;
#if defined(__IDCompositionVisual3_INTERFACE_DEFINED__)
    if (ComPtr<IDCompositionVisual3> v3;
        SUCCEEDED(hover_visual_.As(&v3)) && v3) {
      v3->SetOpacity(0.0f);
    }
#endif
  }

  if (!hover_visual_added_) {
    graphics_->AddOverlayVisual(hover_visual_.Get());
    hover_visual_added_ = true;
  }

  const auto dpi = graphics_->GetDpi();
  const float scale_x = dpi.dpi_x / 96.0f;
  const float scale_y = dpi.dpi_y / 96.0f;

  if (should_show) {
    Color overlay;
    switch (theme_manager_->GetMode()) {
    case theme::Mode::Light:
      overlay = Color(0xCD, 0xCD, 0xCD, 0xD1);
      break;
    default:
      overlay = Color(255, 255, 255, 0x21);
      break;
    }

    const int width_px = std::max(
        1, static_cast<int>(std::lround(hover_bounds_.width * scale_x)));
    const int height_px = std::max(
        1, static_cast<int>(std::lround(hover_bounds_.height * scale_y)));

    EnsureHoverOverlaySurface(width_px, height_px, overlay);

    if (hover_surface_) {
      hover_visual_->SetContent(hover_surface_.Get());
    }

    hover_visual_->SetOffsetX(
        static_cast<float>(std::lround(hover_bounds_.x * scale_x)));
    hover_visual_->SetOffsetY(
        static_cast<float>(std::lround(hover_bounds_.y * scale_y)));
  }

  if (target_opacity != hover_opacity_) {
    const float from = hover_opacity_;
    hover_opacity_ = target_opacity;
    AnimateOpacity(graphics_, hover_visual_.Get(), from, target_opacity);
  }
}

ID2D1SolidColorBrush *ControlRenderer::GetBrush(const Color &color) {
  if (brush_) {
    brush_->SetColor(D2D1::ColorF(color.r / 255.0f, color.g / 255.0f,
                                  color.b / 255.0f, color.a / 255.0f));
  }
  return brush_.Get();
}

} // namespace fluxent::controls
