#pragma once

// FluXent input handler

#include "types.hpp"
#include <functional>

#include <xent/view.hpp>

namespace fluxent {

// HitTestResult

struct HitTestResult {
  xent::View *view_data;
  Rect bounds;
  Point local_position;

  bool hit() const { return view_data != nullptr; }
};

// InputHandler

class InputHandler {
public:
  InputHandler();
  ~InputHandler();

  InputHandler(const InputHandler &) = delete;
  InputHandler &operator=(const InputHandler &) = delete;

  HitTestResult HitTest(const xent::View &root, const Point &position);

  void HandleMouseEvent(const xent::View &root, const MouseEvent &event);
  void HandleKeyEvent(const xent::View &root, const KeyEvent &event);

  void HandleDirectManipulation(xent::View &root, float x, float y, float scale,
                                bool centering);

  void CancelInteraction();

  xent::View *GetHoveredView() const { return hovered_view_; }
  xent::View *GetPressedView() const { return pressed_view_; }
  xent::View *GetFocusedView() const { return focused_view_; }

  // WinUI-like behavior: focus visuals are generally shown for keyboard focus,
  // not pointer clicks.
  bool ShouldShowFocusVisuals() const { return show_focus_visuals_; }

  // Callbacks

  using HoverChangedCallback =
      std::function<void(xent::View *old_view, xent::View *new_view)>;
  void SetHoverChangedCallback(HoverChangedCallback callback) {
    on_hover_changed_ = std::move(callback);
  }

  using InvalidateCallback = std::function<void()>;
  void SetInvalidateCallback(InvalidateCallback callback) {
    on_invalidate_ = std::move(callback);
  }

private:
  bool HitTestRecursive(xent::View *view_data, float parent_x, float parent_y,
                        const Point &position, HitTestResult &result);

  bool DispatchToView(xent::View *view_data, const MouseEvent &event);

  xent::View *hovered_view_ = nullptr;
  xent::View *pressed_view_ = nullptr;
  Rect pressed_view_bounds_;
  xent::View *focused_view_ = nullptr;
  Point last_mouse_position_;

  bool show_focus_visuals_ = false;

  // ScrollBar Dragging State
  bool is_dragging_h_scrollbar_ = false;
  bool is_dragging_v_scrollbar_ = false;
  Point drag_start_mouse_pos_;
  float drag_start_scroll_offset_ = 0.0f;

  // Slider Dragging State
  bool is_dragging_slider_thumb_ = false;
  float slider_thumb_drag_offset_ = 0.0f;

  // Direct Manipulation
  float dm_last_x_ = 0.0f;
  float dm_last_y_ = 0.0f;
  bool dm_active_ = false;

  HoverChangedCallback on_hover_changed_;
  InvalidateCallback on_invalidate_;
};

} // namespace fluxent
