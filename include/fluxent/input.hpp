#pragma once

#include "types.hpp"
#include <xent/delegate.hpp>

#include <xent/view.hpp>

namespace fluxent {

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
  void HandleCharEvent(const xent::View &root, wchar_t ch);


  void HandleDirectManipulation(xent::View &root, float x, float y, float scale,
                                bool centering);

  void CancelInteraction();
  
  void SetCompositionText(const std::wstring &text);

  xent::View *GetHoveredView() const { return hovered_view_; }
  xent::View *GetPressedView() const { return pressed_view_; }
  xent::View *GetFocusedView() const { return focused_view_; }
  Rect GetFocusedViewBounds() const { return focused_view_bounds_; }

  bool ShouldShowFocusVisuals() const { return show_focus_visuals_; }

  using HoverChangedCallback =
      xent::Delegate<void(xent::View *old_view, xent::View *new_view)>;
  void SetHoverChangedCallback(HoverChangedCallback callback) {
    on_hover_changed_ = callback;
  }

  using InvalidateCallback = xent::Delegate<void()>;
  void SetInvalidateCallback(InvalidateCallback callback) {
    on_invalidate_ = callback;
  }
  
  void PerformCopy();
  void PerformCut();
  void PerformPaste();
  void PerformSelectAll();
  void ShowContextMenu(int x, int y);
  
  using ImeStateChangeCallback = xent::Delegate<void(bool enable)>;
  void SetImeStateChangeCallback(ImeStateChangeCallback callback) {
    on_ime_state_change_ = callback;
  }

  using ShowTouchKeyboardCallback = xent::Delegate<void(bool show)>;
  void SetShowTouchKeyboardCallback(ShowTouchKeyboardCallback callback) {
    on_keyboard_ = callback;
  }

private:
  bool HitTestRecursive(xent::View *view_data, float parent_x, float parent_y,
                        const Point &position, HitTestResult &result);

  bool DispatchToView(xent::View *view_data, const MouseEvent &event);

  void EnsureCursorVisible();

  struct EditHistory {
    std::string text;
    int start;
    int end;
  };

  void PushUndoState();
  void Undo();
  void Redo();

  std::vector<EditHistory> undo_stack_;
  std::vector<EditHistory> redo_stack_;

  ULONGLONG last_typing_time_ = 0;
  bool last_op_was_typing_ = false;

  xent::View *hovered_view_ = nullptr;
  xent::View *pressed_view_ = nullptr;
  Rect pressed_view_bounds_;
  xent::View *focused_view_ = nullptr;
  Rect focused_view_bounds_;
  Point last_mouse_position_;

  bool show_focus_visuals_ = false;

  bool is_dragging_h_scrollbar_ = false;
  bool is_dragging_v_scrollbar_ = false;
  Point drag_start_mouse_pos_;

  bool is_dragging_slider_thumb_ = false;
  float slider_thumb_drag_offset_ = 0.0f;

  float dm_last_x_ = 0.0f;
  float dm_last_y_ = 0.0f;
  bool dm_active_ = false;

  HoverChangedCallback on_hover_changed_;
  InvalidateCallback on_invalidate_;
  ImeStateChangeCallback on_ime_state_change_;
  ShowTouchKeyboardCallback on_keyboard_;
};

} // namespace fluxent
