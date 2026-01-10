#pragma once

// FluXent Window (Win32)

#include "types.hpp"
#include "graphics.hpp"
#include <memory>

namespace fluxent {

class Window {
public:
    explicit Window(const WindowConfig& config = {});
    ~Window();
    
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    HWND get_hwnd() const { return hwnd_; }
    bool is_valid() const { return hwnd_ != nullptr; }

    Size get_client_size() const;

    DpiInfo get_dpi() const { return dpi_; }

    void run();
    bool process_messages();
    void close();

    void request_render();

    GraphicsPipeline* get_graphics() const { return graphics_.get(); }

    void set_render_callback(RenderCallback callback) { on_render_ = std::move(callback); }
    void set_resize_callback(ResizeCallback callback) { on_resize_ = std::move(callback); }
    void set_dpi_changed_callback(DpiChangedCallback callback) { on_dpi_changed_ = std::move(callback); }
    void set_mouse_callback(MouseEventCallback callback) { on_mouse_ = std::move(callback); }
    void set_key_callback(KeyEventCallback callback) { on_key_ = std::move(callback); }

    void set_backdrop(BackdropType type);
    void set_dark_mode(bool enabled);
    void set_title(const std::wstring& title);

    void set_cursor(HCURSOR cursor);
    void set_cursor_arrow();
    void set_cursor_hand();
    void set_cursor_ibeam();

private:
    static LRESULT CALLBACK window_proc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
    LRESULT handle_message(UINT msg, WPARAM wparam, LPARAM lparam);

    // Message handlers
    void on_create();
    void on_destroy();
    void on_size(int width, int height);
    void on_dpi_changed(UINT dpi, const RECT* suggested_rect);
    void on_paint();

    // Input handlers
    void on_mouse_move(int x, int y);
    void on_mouse_button(MouseButton button, bool is_down, int x, int y);
    void on_key(UINT vk, bool is_down);

    void setup_dwm_backdrop();
    
    HWND hwnd_ = nullptr;
    DpiInfo dpi_;
    WindowConfig config_;
    HCURSOR current_cursor_ = nullptr;
    
    std::unique_ptr<GraphicsPipeline> graphics_;

    RenderCallback on_render_;
    ResizeCallback on_resize_;
    DpiChangedCallback on_dpi_changed_;
    MouseEventCallback on_mouse_;
    KeyEventCallback on_key_;

    static constexpr const wchar_t* WINDOW_CLASS_NAME = L"FluXentWindowClass";
    static bool class_registered_;
};

} // namespace fluxent
