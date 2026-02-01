#include "fluxent/text_edit.hpp"
#include "fluxent/config.hpp"
#include "fluxent/utils.hpp"

#include <algorithm>
#include <cwctype>

namespace fluxent
{

static void ClampSelectionRange(int &start, int &end, int length)
{
  if (start < 0)
    start = 0;
  if (end < 0)
    end = 0;
  if (start > length)
    start = length;
  if (end > length)
    end = length;
  if (start > end)
    std::swap(start, end);
}

static int FindWordStart(const std::wstring &text, int index)
{
  if (text.empty() || index < 0)
    return 0;
  if (index > (int)text.size())
    index = (int)text.size();
  if (index == 0)
    return 0;

  auto is_delimiter = [](wchar_t c) { return iswspace(c) || iswpunct(c); };
  bool start_type = is_delimiter(text[index - 1]);

  while (index > 0)
  {
    if (is_delimiter(text[index - 1]) != start_type)
      break;
    index--;
  }
  return index;
}

static int FindWordEnd(const std::wstring &text, int index)
{
  if (text.empty() || index < 0)
    return 0;

  auto is_delimiter = [](wchar_t c) { return iswspace(c) || iswpunct(c); };

  if (index >= (int)text.size())
    return (int)text.size();

  bool start_type = is_delimiter(text[index]);

  while (index < (int)text.size())
  {
    if (is_delimiter(text[index]) != start_type)
      break;
    index++;
  }
  return index;
}

void TextEditController::HandleKey(const KeyEvent &event)
{
  if (!event.is_down)
    return;
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  if (focused_view_->text_content.empty())
  {
    focused_view_->cursor_start = 0;
    focused_view_->cursor_end = 0;
  }

  std::wstring wtext = ToWide(focused_view_->text_content);
  ClampSelectionRange(focused_view_->cursor_start, focused_view_->cursor_end,
                      static_cast<int>(wtext.size()));

  int cursor = focused_view_->cursor_end;
  bool changed = false;

  switch (event.virtual_key)
  {
  case VK_LEFT:
    if (cursor > 0)
      cursor--;
    if (!event.shift)
      focused_view_->cursor_start = cursor;
    focused_view_->cursor_end = cursor;
    changed = true;
    break;
  case VK_RIGHT:
    if (cursor < static_cast<int>(wtext.size()))
      cursor++;
    if (!event.shift)
      focused_view_->cursor_start = cursor;
    focused_view_->cursor_end = cursor;
    changed = true;
    break;
  case VK_HOME:
    cursor = 0;
    if (!event.shift)
      focused_view_->cursor_start = cursor;
    focused_view_->cursor_end = cursor;
    changed = true;
    break;
  case VK_END:
    cursor = static_cast<int>(wtext.size());
    if (!event.shift)
      focused_view_->cursor_start = cursor;
    focused_view_->cursor_end = cursor;
    changed = true;
    break;
  case VK_BACK:
    last_op_was_typing_ = false;
    if (focused_view_->cursor_start != focused_view_->cursor_end)
    {
      PushUndoState();
      int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
      int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
      wtext.erase(start, end - start);
      cursor = start;
      focused_view_->cursor_start = cursor;
      focused_view_->cursor_end = cursor;
      changed = true;
    }
    else if (cursor > 0)
    {
      PushUndoState();
      wtext.erase(cursor - 1, 1);
      cursor--;
      focused_view_->cursor_start = cursor;
      focused_view_->cursor_end = cursor;
      changed = true;
    }
    break;
  case VK_DELETE:
    if (focused_view_->cursor_start != focused_view_->cursor_end)
    {
      PushUndoState();
      int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
      int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
      wtext.erase(start, end - start);
      cursor = start;
      focused_view_->cursor_start = cursor;
      focused_view_->cursor_end = cursor;
      changed = true;
    }
    else if (cursor < static_cast<int>(wtext.size()))
    {
      PushUndoState();
      wtext.erase(cursor, 1);
      changed = true;
    }
    break;
  case VK_RETURN:
    if (focused_view_->on_submit)
    {
      focused_view_->on_submit(focused_view_->text_content);
    }
    break;
  case 'A':
    if (event.ctrl)
    {
      PerformSelectAll();
      return;
    }
    break;
  case 'C':
    if (event.ctrl)
    {
      PerformCopy();
      return;
    }
    break;
  case 'V':
    if (event.ctrl)
    {
      PerformPaste();
      return;
    }
    break;
  case 'X':
    if (event.ctrl)
    {
      PerformCut();
      return;
    }
    break;
  case 'Z':
    if (event.ctrl)
    {
      Undo();
      return;
    }
    break;
  case 'Y':
    if (event.ctrl)
    {
      Redo();
      return;
    }
    break;
  }

  if (changed)
  {
    focused_view_->text_content = ToUtf8(wtext);

    if (focused_view_->on_text_changed)
    {
      focused_view_->on_text_changed(focused_view_->text_content);
    }
    EnsureCursorVisible();
    if (on_invalidate_)
      on_invalidate_();
  }
}

void TextEditController::HandleChar(wchar_t ch)
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  if (ch < 32 && ch != VK_TAB)
    return;

  std::wstring wtext = ToWide(focused_view_->text_content);
  ClampSelectionRange(focused_view_->cursor_start, focused_view_->cursor_end,
                      static_cast<int>(wtext.size()));

  ULONGLONG now = GetTickCount64();
  bool is_space = (ch == ' ' || ch == VK_RETURN || ch == VK_TAB);

  if (last_op_was_typing_ && !is_space &&
      (now - last_typing_time_ < fluxent::config::Input::TypingMergeMs))
  {
    last_typing_time_ = now;
  }
  else
  {
    PushUndoState();
    last_typing_time_ = now;
    last_op_was_typing_ = true;
  }

  if (focused_view_->cursor_start != focused_view_->cursor_end)
  {
    int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
    int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
    wtext.erase(start, end - start);
    focused_view_->cursor_end = start;
    focused_view_->cursor_start = start;
  }

  int cursor = focused_view_->cursor_end;
  if (cursor > static_cast<int>(wtext.size()))
    cursor = static_cast<int>(wtext.size());

  wtext.insert(cursor, 1, ch);
  cursor++;
  focused_view_->cursor_start = cursor;
  focused_view_->cursor_end = cursor;

  focused_view_->text_content = ToUtf8(wtext);

  if (focused_view_->on_text_changed)
  {
    focused_view_->on_text_changed(focused_view_->text_content);
  }

  EnsureCursorVisible();

  if (on_invalidate_)
    on_invalidate_();
}

bool TextEditController::HandleSelectionAt(int index, int click_count, bool extend_selection)
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return false;

  std::wstring wtext = ToWide(focused_view_->text_content);
  int length = static_cast<int>(wtext.size());
  int clamped = index;
  ClampSelectionRange(clamped, clamped, length);

  int start = focused_view_->cursor_start;
  int end = focused_view_->cursor_end;

  if (click_count == 2)
  {
    start = FindWordStart(wtext, clamped);
    end = FindWordEnd(wtext, clamped);
  }
  else if (click_count >= 3)
  {
    start = 0;
    end = length;
  }
  else
  {
    end = clamped;
    if (!extend_selection)
      start = clamped;
  }

  if (start == focused_view_->cursor_start && end == focused_view_->cursor_end)
    return false;

  focused_view_->cursor_start = start;
  focused_view_->cursor_end = end;
  EnsureCursorVisible();
  if (on_invalidate_)
    on_invalidate_();
  return true;
}

bool TextEditController::HandleSelectionDrag(int index)
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return false;

  std::wstring wtext = ToWide(focused_view_->text_content);
  int length = static_cast<int>(wtext.size());
  int clamped = index;
  ClampSelectionRange(clamped, clamped, length);

  if (focused_view_->cursor_end == clamped)
    return false;

  focused_view_->cursor_end = clamped;
  EnsureCursorVisible();
  if (on_invalidate_)
    on_invalidate_();
  return true;
}

void TextEditController::SetCompositionText(const std::wstring &text)
{
  if (focused_view_ && focused_view_->type == xent::ComponentType::TextBox)
  {
    if (text.empty())
    {
      focused_view_->composition_text.clear();
      focused_view_->composition_cursor = 0;
      focused_view_->composition_length = 0;
    }
    else
    {
      if (focused_view_->cursor_start != focused_view_->cursor_end)
      {
        std::wstring wtext = ToWide(focused_view_->text_content);

        int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
        int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);

        if (start < static_cast<int>(wtext.size()))
        {
          if (end > static_cast<int>(wtext.size()))
            end = static_cast<int>(wtext.size());
          wtext.erase(start, end - start);

          focused_view_->text_content = ToUtf8(wtext);

          focused_view_->cursor_start = start;
          focused_view_->cursor_end = start;
        }
      }

      focused_view_->composition_text = ToUtf8(text);
      focused_view_->composition_cursor = static_cast<int>(text.length());
      focused_view_->composition_length = static_cast<int>(text.length());
    }
    if (on_invalidate_)
      on_invalidate_();
  }
}

void TextEditController::EnsureCursorVisible()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  auto func = xent::GetTextCaretRectFunc();
  if (!func)
    return;

  float content_width =
      focused_view_->LayoutWidth() - focused_view_->padding_left - focused_view_->padding_right;
  float measure_width = fluxent::config::Text::HitTestExtent;

  auto [rx, ry, rw, rh] = func(focused_view_->text_content, focused_view_->font_size, measure_width,
                               focused_view_->cursor_end);

  float caret_x = rx;
  float visual_x = caret_x - focused_view_->scroll_offset_x;

  if (visual_x < 0)
  {
    focused_view_->scroll_offset_x = caret_x;
  }
  else if (visual_x > content_width)
  {
    focused_view_->scroll_offset_x =
        caret_x - content_width + fluxent::config::Input::CaretEdgePadding;
  }

  if (focused_view_->scroll_offset_x < 0)
    focused_view_->scroll_offset_x = 0;
}

void TextEditController::PushUndoState()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  EditHistory state;
  state.text = focused_view_->text_content;
  state.start = focused_view_->cursor_start;
  state.end = focused_view_->cursor_end;

  undo_stack_.push_back(state);

  if (undo_stack_.size() > 50)
  {
    undo_stack_.erase(undo_stack_.begin());
  }

  redo_stack_.clear();
}

void TextEditController::Undo()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;
  if (undo_stack_.empty())
    return;

  EditHistory current;
  current.text = focused_view_->text_content;
  current.start = focused_view_->cursor_start;
  current.end = focused_view_->cursor_end;
  redo_stack_.push_back(current);

  EditHistory prev = undo_stack_.back();
  undo_stack_.pop_back();

  focused_view_->text_content = prev.text;
  focused_view_->cursor_start = prev.start;
  focused_view_->cursor_end = prev.end;

  if (focused_view_->on_text_changed)
    focused_view_->on_text_changed(focused_view_->text_content);

  EnsureCursorVisible();
  if (on_invalidate_)
    on_invalidate_();
}

void TextEditController::Redo()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;
  if (redo_stack_.empty())
    return;

  EditHistory current;
  current.text = focused_view_->text_content;
  current.start = focused_view_->cursor_start;
  current.end = focused_view_->cursor_end;
  undo_stack_.push_back(current);

  EditHistory next = redo_stack_.back();
  redo_stack_.pop_back();

  focused_view_->text_content = next.text;
  focused_view_->cursor_start = next.start;
  focused_view_->cursor_end = next.end;

  if (focused_view_->on_text_changed)
    focused_view_->on_text_changed(focused_view_->text_content);

  EnsureCursorVisible();
  if (on_invalidate_)
    on_invalidate_();
}

void TextEditController::PerformSelectAll()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  std::wstring wtext = ToWide(focused_view_->text_content);
  focused_view_->cursor_end = static_cast<int>(wtext.size());
  focused_view_->cursor_start = 0;
  if (on_invalidate_)
    on_invalidate_();
}

void TextEditController::PerformCopy()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;
  if (focused_view_->cursor_start == focused_view_->cursor_end)
    return;

  std::wstring wtext = ToWide(focused_view_->text_content);

  int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
  int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
  ClampSelectionRange(start, end, static_cast<int>(wtext.size()));

  if (start >= (int)wtext.size())
    return;

  std::wstring selection = wtext.substr(start, end - start);

  HWND owner = GetForegroundWindow();
  if (OpenClipboard(owner))
  {
    EmptyClipboard();
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, (selection.size() + 1) * sizeof(wchar_t));
    if (hMem)
    {
      wchar_t *pMem = static_cast<wchar_t *>(GlobalLock(hMem));
      if (pMem)
      {
        wcscpy_s(pMem, selection.size() + 1, selection.c_str());
        GlobalUnlock(hMem);
        SetClipboardData(CF_UNICODETEXT, hMem);
      }
    }
    CloseClipboard();
  }
}

void TextEditController::PerformPaste()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;

  HWND owner = GetForegroundWindow();
  if (OpenClipboard(owner))
  {
    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData)
    {
      wchar_t *pData = static_cast<wchar_t *>(GlobalLock(hData));
      if (pData)
      {
        std::wstring paste_text = pData;
        GlobalUnlock(hData);

        PushUndoState();

        std::wstring wtext = ToWide(focused_view_->text_content);

        int cursor = focused_view_->cursor_end;

        if (focused_view_->cursor_start != focused_view_->cursor_end)
        {
          int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
          int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
          ClampSelectionRange(start, end, static_cast<int>(wtext.size()));
          if (start < end)
          {
            wtext.erase(start, end - start);
            cursor = start;
          }
        }

        if (cursor > (int)wtext.size())
          cursor = (int)wtext.size();
        wtext.insert(cursor, paste_text);
        cursor += static_cast<int>(paste_text.size());

        focused_view_->text_content = ToUtf8(wtext);

        focused_view_->cursor_start = cursor;
        focused_view_->cursor_end = cursor;

        if (focused_view_->on_text_changed)
          focused_view_->on_text_changed(focused_view_->text_content);

        EnsureCursorVisible();
        if (on_invalidate_)
          on_invalidate_();
      }
    }
    CloseClipboard();
  }
}

void TextEditController::PerformCut()
{
  if (!focused_view_ || focused_view_->type != xent::ComponentType::TextBox)
    return;
  if (focused_view_->cursor_start == focused_view_->cursor_end)
    return;

  PushUndoState();

  PerformCopy();

  std::wstring wtext = ToWide(focused_view_->text_content);

  int start = std::min(focused_view_->cursor_start, focused_view_->cursor_end);
  int end = std::max(focused_view_->cursor_start, focused_view_->cursor_end);
  ClampSelectionRange(start, end, static_cast<int>(wtext.size()));

  if (start < end)
  {
    wtext.erase(start, end - start);
  }

  focused_view_->text_content = ToUtf8(wtext);

  focused_view_->cursor_start = start;
  focused_view_->cursor_end = start;

  if (focused_view_->on_text_changed)
    focused_view_->on_text_changed(focused_view_->text_content);

  EnsureCursorVisible();
  if (on_invalidate_)
    on_invalidate_();
}

}
