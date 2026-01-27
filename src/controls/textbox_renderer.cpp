#include "fluxent/controls/textbox_renderer.hpp"
#include "fluxent/controls/common_drawing.hpp"
#include "fluxent/controls/renderer_utils.hpp"
#include "fluxent/text.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include <d2d1_3.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace fluxent::controls {

static ComPtr<ID2D1SolidColorBrush> CreateBrush(ID2D1DeviceContext *d2d,
                                                const Color &color) {
  ComPtr<ID2D1SolidColorBrush> brush;
  d2d->CreateSolidColorBrush(color.to_d2d(), &brush);
  return brush;
}

void TextBoxRenderer::BeginFrame() {
  needs_redraw_ = false;
}

bool TextBoxRenderer::EndFrame() {
  return needs_redraw_;
}

static Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush>
CreateTextBoxElevationBrush(ID2D1DeviceContext *d2d,
                            const theme::ThemeResources &res,
                            bool is_focused, float height) {
  D2D1_GRADIENT_STOP stops[2];

  if (is_focused) {
    stops[0] = {1.0f, res.AccentDefault.to_d2d()};
    stops[1] = {1.0f, res.ControlStrokeDefault.to_d2d()};
  } else {
    stops[0] = {0.5f, res.ControlStrongStrokeDefault.to_d2d()};
    stops[1] = {1.0f, res.ControlStrokeDefault.to_d2d()};
  }

  Microsoft::WRL::ComPtr<ID2D1GradientStopCollection> collection;
  d2d->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2,
                                    D2D1_EXTEND_MODE_CLAMP, &collection);
  if (!collection)
    return nullptr;

  Microsoft::WRL::ComPtr<ID2D1LinearGradientBrush> brush;
  d2d->CreateLinearGradientBrush(
      D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 0),
                                          D2D1::Point2F(0, height)),
      collection.Get(), &brush);

  return brush;
}

void TextBoxRenderer::DrawElevationBorder(const RenderContext &ctx,
                                          const Rect &bounds,
                                          float corner_radius, bool is_focused,
                                          bool is_hovered) {
  auto d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  D2D1_ROUNDED_RECT rr =
      D2D1::RoundedRect(bounds.to_d2d(), corner_radius, corner_radius);

  Color bottom_color = is_focused ? res.AccentDefault : res.ControlStrongStrokeDefault;
  Color top_color = res.ControlStrokeDefault;

  D2D1_GRADIENT_STOP stops[2];
  stops[0] = {0.0f, bottom_color.to_d2d()};
  stops[1] = {1.0f, top_color.to_d2d()};

  ComPtr<ID2D1GradientStopCollection> collection;
  d2d->CreateGradientStopCollection(stops, 2, D2D1_GAMMA_2_2,
                                    D2D1_EXTEND_MODE_CLAMP, &collection);
  if (!collection) return;

  ComPtr<ID2D1LinearGradientBrush> gradient_brush;
  d2d->CreateLinearGradientBrush(
      D2D1::LinearGradientBrushProperties(
          D2D1::Point2F(0, bounds.y + bounds.height),
          D2D1::Point2F(0, bounds.y + bounds.height - 2)),
      collection.Get(), &gradient_brush);
  if (!gradient_brush) return;

  d2d->DrawRoundedRectangle(rr, gradient_brush.Get(), 1.0f);

  if (is_focused) {
    auto factory = ctx.graphics->GetD2DFactory();
    if (!factory) return;

    ComPtr<ID2D1RoundedRectangleGeometry> clip_geom;
    factory->CreateRoundedRectangleGeometry(rr, &clip_geom);
    if (!clip_geom) return;

    auto accent_brush = CreateBrush(d2d, res.AccentDefault);
    if (!accent_brush) return;

    d2d->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), clip_geom.Get()),
                   nullptr);

    float line_y = bounds.y + bounds.height - 1.0f;
    d2d->DrawLine(
        D2D1::Point2F(bounds.x, line_y),
        D2D1::Point2F(bounds.x + bounds.width, line_y),
        accent_brush.Get(),
        2.0f);

    d2d->PopLayer();
  }
}


Rect TextBoxRenderer::DrawChrome(const RenderContext &ctx, const Rect &bounds,
                                 const ControlState &state,
                                 float corner_radius) {
  auto *d2d = ctx.graphics->GetD2DContext();
  const auto &res = ctx.Resources();

  Color fill_color = res.ControlFillDefault;
  if (state.is_disabled) {
    fill_color = res.ControlFillDisabled;
  } else if (state.is_focused) {
    fill_color = res.ControlFillInputActive;
  } else if (state.is_hovered) {
    fill_color = res.ControlFillSecondary;
  }

  auto brush = CreateBrush(d2d, fill_color);
  if (brush) {
    D2D1_ROUNDED_RECT rr =
        D2D1::RoundedRect(bounds.to_d2d(), corner_radius, corner_radius);
    d2d->FillRoundedRectangle(rr, brush.Get());
  }

  // Border
  if (state.is_disabled) {
    auto border_brush = CreateBrush(d2d, res.ControlStrokeDefault);
    if (border_brush) {
       D2D1_ROUNDED_RECT rr =
        D2D1::RoundedRect(bounds.to_d2d(), corner_radius, corner_radius);
       d2d->DrawRoundedRectangle(rr, border_brush.Get(), 1.0f);
    }
  } else {
    DrawElevationBorder(ctx, bounds, corner_radius, state.is_focused,
                        state.is_hovered);
  }

  return bounds;
}

void TextBoxRenderer::Render(const RenderContext &ctx, const xent::View &data,
                             const Rect &bounds, const ControlState &state) {
  const auto &res = ctx.Resources();
  float corner_radius =
      data.corner_radius > 0 ? data.corner_radius : res.ControlCornerRadius;

  Rect content_bounds = DrawChrome(ctx, bounds, state, corner_radius);

  float pad_l = data.padding_left > 0 ? data.padding_left : res.TextBox.PaddingLeft;
  float pad_t = data.padding_top > 0 ? data.padding_top : res.TextBox.PaddingTop;
  float pad_r = data.padding_right > 0 ? data.padding_right : res.TextBox.PaddingRight;
  float pad_b = data.padding_bottom > 0 ? data.padding_bottom : res.TextBox.PaddingBottom;

  Rect text_bounds(bounds.x + pad_l, bounds.y + pad_t,
                   std::max(0.f, bounds.width - pad_l - pad_r),
                   std::max(0.f, bounds.height - pad_t - pad_b));

  auto d2d = ctx.graphics->GetD2DContext();
  if (d2d)
      d2d->PushAxisAlignedClip(text_bounds.to_d2d(), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

  auto *text_renderer = ctx.text;
  if (!text_renderer) {
    if (d2d) d2d->PopAxisAlignedClip();
    return;
  }

  std::wstring wtext;
  if (!data.text_content.empty()) {
    std::string utf8 = data.text_content;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len > 0) {
      wtext.resize(len);
      MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wtext[0], len);
      wtext.resize(len - 1);
    }
  }

  std::wstring wcomposition;
  if (!data.composition_text.empty()) {
    std::string utf8 = data.composition_text;
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    if (len > 0) {
      wcomposition.resize(len);
      MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, &wcomposition[0], len);
      wcomposition.resize(len - 1);
    }
  }

  std::wstring display_text = wtext;
  int composition_start = std::min(data.cursor_start, data.cursor_end);

  if (composition_start > static_cast<int>(display_text.length())) 
      composition_start = static_cast<int>(display_text.length());

  if (!wcomposition.empty()) {
      display_text.insert(composition_start, wcomposition);
  }

  TextStyle style;
  style.font_size =
      data.font_size > 0 ? data.font_size : kControlContentThemeFontSize;
  style.color = res.TextPrimary;
  if (state.is_disabled)
    style.color = res.TextDisabled;
  style.word_wrap = false;

  if (display_text.empty() && !data.placeholder_text.empty()) {
    TextStyle ph_style = style;
    ph_style.color = res.TextSecondary;
    
    std::string ph_utf8 = data.placeholder_text;
    std::wstring wph;
    int len = MultiByteToWideChar(CP_UTF8, 0, ph_utf8.c_str(), -1, nullptr, 0);
    if (len > 0) {
      wph.resize(len);
      MultiByteToWideChar(CP_UTF8, 0, ph_utf8.c_str(), -1, &wph[0], len);
      wph.resize(len - 1);
    }
    
    Rect ph_bounds = text_bounds;
    ph_bounds.x -= data.scroll_offset_x;
    text_renderer->DrawText(wph, ph_bounds, ph_style);
  }

  if (wcomposition.empty() && !data.text_content.empty() && data.cursor_start != data.cursor_end) {
    int start = std::min(data.cursor_start, data.cursor_end);
    int end = std::max(data.cursor_start, data.cursor_end);
    auto rects = text_renderer->GetSelectionRects(display_text, style, text_bounds.width,
                                                  start, end);

    auto sel_brush = CreateBrush(ctx.graphics->GetD2DContext(),
                                 res.AccentDefault);
    if (sel_brush)
      sel_brush->SetOpacity(0.4f);

    for (const auto &r : rects) {
      Rect abs_r(text_bounds.x + r.x - data.scroll_offset_x, text_bounds.y + r.y, r.width, r.height);
      if (sel_brush) {
        ctx.graphics->GetD2DContext()->FillRectangle(abs_r.to_d2d(),
                                                     sel_brush.Get());
      }
    }
  }
  
  if (!wcomposition.empty()) {
      auto rects = text_renderer->GetSelectionRects(display_text, style, text_bounds.width,
                                                    composition_start, composition_start + static_cast<int>(wcomposition.length()));
      
      auto underline_brush = CreateBrush(ctx.graphics->GetD2DContext(), res.TextPrimary);
      
      for (const auto &r : rects) {
          D2D1_POINT_2F p1 = D2D1::Point2F(text_bounds.x + r.x - data.scroll_offset_x, text_bounds.y + r.y + r.height);
          D2D1_POINT_2F p2 = D2D1::Point2F(text_bounds.x + r.x + r.width - data.scroll_offset_x, text_bounds.y + r.y + r.height);
          if (underline_brush)
             ctx.graphics->GetD2DContext()->DrawLine(p1, p2, underline_brush.Get(), 1.0f);
      }
  }

  if (!display_text.empty()) {
      Rect draw_bounds = text_bounds;
      draw_bounds.x -= data.scroll_offset_x;
      text_renderer->DrawText(display_text, draw_bounds, style);
  }

  // Draw Caret (if focused and not composing)
  if (state.is_focused && wcomposition.empty()) {
     Rect r = text_renderer->GetCaretRect(display_text, style, text_bounds.width, data.cursor_end);
     
     float caret_x = text_bounds.x + r.x - data.scroll_offset_x;
     float caret_y = text_bounds.y + r.y;
     
     auto caret_brush = CreateBrush(ctx.graphics->GetD2DContext(), res.TextPrimary);
     if (caret_brush) {
         ctx.graphics->GetD2DContext()->DrawLine(
             D2D1::Point2F(caret_x, caret_y),
             D2D1::Point2F(caret_x, caret_y + r.height),
             caret_brush.Get(),
             1.0f 
         );
     }
  }

  if (state.is_focused && (wcomposition.empty() ? (data.cursor_start == data.cursor_end) : true)) {
    auto now = std::chrono::steady_clock::now();
    auto &cs = caret_states_[&data];
    
    if (cs.last_toggle.time_since_epoch().count() == 0) {
      cs.last_toggle = now;
      cs.visible = true;
      needs_redraw_ = true;
    }
    
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - cs.last_toggle);
    if (elapsed.count() >= 500) {
      cs.visible = !cs.visible;
      cs.last_toggle = now;
      needs_redraw_ = true;
    }
    
    if (cs.visible) {
      int caret_pos_idx = wcomposition.empty() ? data.cursor_end : (composition_start + data.composition_cursor);
      
      Rect caret = text_renderer->GetCaretRect(display_text, style, text_bounds.width, caret_pos_idx);
      float h = caret.height > 0 ? caret.height : style.font_size * 1.3f;
      Rect abs_caret(text_bounds.x + caret.x, text_bounds.y + caret.y, 1.0f, h);
      
      auto caret_brush = CreateBrush(ctx.graphics->GetD2DContext(), res.TextPrimary);
      if (caret_brush) {
        ctx.graphics->GetD2DContext()->FillRectangle(abs_caret.to_d2d(), caret_brush.Get());
      }
    }
  } else {
    caret_states_.erase(&data);
  }

  if (d2d) d2d->PopAxisAlignedClip();
}

} // namespace fluxent::controls
