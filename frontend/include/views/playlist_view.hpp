#pragma once

#include "gui_common.hpp"
#include "retro_widgets.hpp"
#include "core_controller.hpp"
#include <SDL.h>
#include <vector>
#include <chrono>

namespace freenamp::frontend {

class PlaylistView {
public:
    PlaylistView(int x = 20, int y = 254, int w = 275, int h = 180);

    void render(SDL_Renderer* renderer, backend::CoreController& core);

    bool handle_mouse_down(int mx, int my, backend::CoreController& core, bool& open_url_dialog, bool& close_requested);
    void handle_mouse_up(int mx, int my);
    void handle_mouse_move(int mx, int my, int canvas_w = 0, int canvas_h = 0);
    void handle_mouse_wheel(int wheel_y);

    Rect get_bounds() const { return m_bounds; }
    void set_position(int x, int y) { m_bounds.x = x; m_bounds.y = y; }
    void set_size(int w, int h);

    bool is_visible() const { return m_visible; }
    void set_visible(bool v) { m_visible = v; }
    void toggle_visible() { m_visible = !m_visible; }

    int get_selected_index() const { return m_selected_index; }
    void set_selected_index(int idx) { m_selected_index = idx; }
    void ensure_visible(int index, int total_tracks = -1);
    void remove_selected(backend::CoreController& core);

    bool is_dragging_window() const { return m_dragging_window; }
    bool is_resizing() const { return m_is_resizing; }
    int get_drag_offset_x() const { return m_drag_off_x; }
    int get_drag_offset_y() const { return m_drag_off_y; }

private:
    Rect m_bounds{ 20, 254, 275, 180 };
    bool m_visible = true;

    bool m_dragging_window = false;
    int m_drag_off_x = 0;
    int m_drag_off_y = 0;

    // Resizing state
    bool m_is_resizing = false;
    int m_resize_start_w = 0;
    int m_resize_start_h = 0;
    int m_resize_start_mx = 0;
    int m_resize_start_my = 0;

    // Scroll dragging state
    bool m_dragging_scrollbar = false;

    int m_selected_index = 0;
    int m_scroll_offset = 0; // First visible track index
    int m_total_tracks = 0;

    // Double-click detection
    std::chrono::steady_clock::time_point m_last_click_time;
    int m_last_clicked_index = -1;

    // Hitboxes
    Rect m_list_box{ 10, 20, 245, 126 };
    Rect m_scrollbar{ 257, 20, 10, 126 };

    // Buttons
    Rect m_btn_add{ 10, 152, 48, 18 };
    Rect m_btn_rem{ 62, 152, 48, 18 };
    Rect m_btn_clear{ 114, 152, 46, 18 };
    Rect m_btn_up{ 224, 152, 20, 18 };
    Rect m_btn_down{ 246, 152, 20, 18 };
};

} // namespace freenamp::frontend
