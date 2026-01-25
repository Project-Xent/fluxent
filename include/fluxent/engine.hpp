#pragma once

// FluXent render engine

#include "controls/control_renderer.hpp"
#include "fluxent/theme/theme_manager.hpp"
#include "graphics.hpp"
#include "text.hpp"
#include "types.hpp"
#include <memory>
#include <unordered_map>
#include <xent/view.hpp>

namespace fluxent {

class RenderEngine {
public:
  static Result<std::unique_ptr<RenderEngine>>
  Create(GraphicsPipeline *graphics, theme::ThemeManager *theme_manager);
  ~RenderEngine();

private:
  RenderEngine(GraphicsPipeline *graphics, theme::ThemeManager *theme_manager);
  Result<void> Init();

public:
  RenderEngine(const RenderEngine &) = delete;
  RenderEngine &operator=(const RenderEngine &) = delete;

  void Render(const xent::View &root);

  void RenderFrame(xent::View &root);

  void SetInputHandler(InputHandler *input) { input_ = input; }

  void FillRect(const Rect &rect, const Color &color);
  void FillRoundedRect(const Rect &rect, float radius, const Color &color);
  void DrawRect(const Rect &rect, const Color &color,
                float stroke_width = 1.0f);
  void DrawRoundedRect(const Rect &rect, float radius, const Color &color,
                       float stroke_width = 1.0f);

  void PushClip(const Rect &rect);
  void PopClip();

  void PushTransform(const D2D1_MATRIX_3X2_F &transform);
  void PopTransform();

  void PushTranslation(float x, float y);

  void ClearBrushCache();

  void SetDebugMode(bool enabled) { debug_mode_ = enabled; }
  bool IsDebugMode() const { return debug_mode_; }

  TextRenderer *GetTextRenderer() const { return text_renderer_.get(); }

private:
  // Text measurement callback (no lambda).
  std::pair<float, float> MeasureTextCallback(const std::string &text,
                                              float font_size, float max_width);

  void DrawViewRecursive(const xent::View &view, float parent_x,
                         float parent_y);

  void DrawViewDataRecursive(const xent::View *data, float parent_x,
                             float parent_y);

  void DrawViewBackground(const xent::View &data, const Rect &bounds);

  void DrawViewBorder(const xent::View &data, const Rect &bounds);

  void DrawViewText(const xent::View &data, const Rect &bounds);

  ID2D1SolidColorBrush *GetBrush(const Color &color);

  static Color convert_color(const xent::Color &c) {
    return Color(c.r, c.g, c.b, c.a);
  }

  GraphicsPipeline *graphics_;
  ID2D1DeviceContext *d2d_context_ = nullptr;
  std::unique_ptr<TextRenderer> text_renderer_;

  std::unique_ptr<controls::ControlRenderer> control_renderer_;
  InputHandler *input_ = nullptr;

  std::unordered_map<uint32_t, ComPtr<ID2D1SolidColorBrush>> brush_cache_;

  std::vector<D2D1_MATRIX_3X2_F> transform_stack_;

  bool debug_mode_ = false;

  // Theme sync
  theme::ThemeManager *theme_manager_ = nullptr;
  size_t theme_listener_id_ = 0;
  theme::ThemeResources last_theme_resources_{};
  xent::View *last_root_data_ = nullptr;
};

} // namespace fluxent
