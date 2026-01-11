#pragma once

// FluXent input handler

#include "types.hpp"
#include <xent/view.hpp>
#include <memory>
#include <functional>

namespace fluxent {

// HitTestResult

struct HitTestResult {
    std::shared_ptr<xent::ViewData> view_data;
    Rect bounds;
    Point local_position;
    
    bool hit() const { return view_data != nullptr; }
};


// InputHandler

class InputHandler {
public:
    InputHandler();
    ~InputHandler();

    InputHandler(const InputHandler&) = delete;
    InputHandler& operator=(const InputHandler&) = delete;

    HitTestResult hit_test(const xent::View& root, const Point& position);

    void handle_mouse_event(const xent::View& root, const MouseEvent& event);
    void handle_key_event(const xent::View& root, const KeyEvent& event);

    std::shared_ptr<xent::ViewData> get_hovered_view() const { return hovered_view_.lock(); }
    std::shared_ptr<xent::ViewData> get_pressed_view() const { return pressed_view_.lock(); }
    std::shared_ptr<xent::ViewData> get_focused_view() const { return focused_view_.lock(); }

    // WinUI-like behavior: focus visuals are generally shown for keyboard focus, not pointer clicks.
    bool should_show_focus_visuals() const { return show_focus_visuals_; }

    // Callbacks
    
    using HoverChangedCallback = std::function<void(
        std::shared_ptr<xent::ViewData> old_view,
        std::shared_ptr<xent::ViewData> new_view
    )>;
    void set_hover_changed_callback(HoverChangedCallback callback) {
        on_hover_changed_ = std::move(callback);
    }
    
    using InvalidateCallback = std::function<void()>;
    void set_invalidate_callback(InvalidateCallback callback) {
        on_invalidate_ = std::move(callback);
    }

private:
    bool hit_test_recursive(
        const std::shared_ptr<xent::ViewData>& view_data,
        float parent_x,
        float parent_y,
        const Point& position,
        HitTestResult& result
    );
    
    bool dispatch_to_view(
        const std::shared_ptr<xent::ViewData>& view_data,
        const MouseEvent& event
    );
    
    std::weak_ptr<xent::ViewData> hovered_view_;
    std::weak_ptr<xent::ViewData> pressed_view_;
    std::weak_ptr<xent::ViewData> focused_view_;
    Point last_mouse_position_;

    bool show_focus_visuals_ = false;
    
    HoverChangedCallback on_hover_changed_;
    InvalidateCallback on_invalidate_;
};

} // namespace fluxent
