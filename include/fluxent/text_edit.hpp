#pragma once

#include "types.hpp"
#include <xent/delegate.hpp>
#include <xent/view.hpp>

namespace fluxent
{

class TextEditController
{
public:
  using InvalidateCallback = xent::Delegate<void()>;

  void SetInvalidateCallback(InvalidateCallback callback) { on_invalidate_ = callback; }
  void SetFocusedView(xent::View *view) { focused_view_ = view; }
  xent::View *GetFocusedView() const { return focused_view_; }

  void HandleKey(const KeyEvent &event);
  void HandleChar(wchar_t ch);
  bool HandleSelectionAt(int index, int click_count, bool extend_selection);
  bool HandleSelectionDrag(int index);

  void SetCompositionText(const std::wstring &text);
  void EnsureCursorVisible();

  void PerformCopy();
  void PerformCut();
  void PerformPaste();
  void PerformSelectAll();
  void ResetTypingSequence() { last_op_was_typing_ = false; }

private:
  struct EditHistory
  {
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

  xent::View *focused_view_ = nullptr;
  InvalidateCallback on_invalidate_;
};

} // namespace fluxent
