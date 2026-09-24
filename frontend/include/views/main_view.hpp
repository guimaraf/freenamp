#pragma once

#include "gui_common.hpp"
#include "retro_widgets.hpp"
#include "core_controller.hpp"
#include <SDL.h>
#include <string>

namespace freenamp::frontend {

class MainView {
public:
    MainView(int x = 20, int y = 20);

    void render(SDL_Renderer* renderer, backend::CoreController& core);

    // Event handling
    bool handle_mouse_down(int mx, int my, backend::CoreController& core, bool& request_open_url, bool& toggle_eq, bool& toggle_pl);
    void handle_mouse_up(int mx, int my);
    void handle_mouse_move(int mx, int my, backend::CoreController& core, int canvas_w = 0, int canvas_h = 0);

    Rect get_bounds() const { return m_bounds; }
    void set_position(int x, int y) { m_bounds.x = x; m_bounds.y = y; }

    bool is_time_remaining_mode() const { return m_time_remaining_mode; }
    void set_time_remaining_mode(bool remaining) { m_time_remaining_mode = remaining; }

    bool is_dragging_window() const { return m_dragging_window; }
    int get_drag_offset_x() const { return m_drag_off_x; }
    int get_drag_offset_y() const { return m_drag_off_y; }

private:
    Rect m_bounds{ 20, 20, 275, 116 };

    // Interactive element hitboxes (relative to m_bounds.x, m_bounds.y)
    Rect m_btn_prev{ 16, 88, 24, 18 };
    Rect m_btn_play{ 40, 88, 24, 18 };
    Rect m_btn_pause{ 64, 88, 24, 18 };
    Rect m_btn_stop{ 88, 88, 24, 18 };
    Rect m_btn_next{ 112, 88, 24, 18 };
    Rect m_btn_eject{ 138, 88, 22, 18 };

    Rect m_btn_eq{ 220, 56, 23, 12 };
    Rect m_btn_pl{ 244, 56, 23, 12 };
    Rect m_btn_shuf{ 166, 88, 50, 18 };
    Rect m_btn_rep{ 220, 88, 40, 18 };

    Rect m_clock_hitbox{ 20, 34, 98, 22 };

    Rect m_slider_seek{ 16, 73, 243, 10 };
    Rect m_slider_vol{ 105, 56, 68, 10 };
    Rect m_slider_pan{ 177, 56, 38, 10 };

    bool m_time_remaining_mode = false;

    // Drag states
    bool m_dragging_window = false;
    int m_drag_off_x = 0;
    int m_drag_off_y = 0;

    bool m_dragging_seek = false;
    bool m_dragging_vol = false;
    bool m_dragging_pan = false;

    // Pressed button tracking
    std::string m_pressed_button = "";

    // Scrolling marquee ticker
    int m_marquee_offset = 0;
    int m_marquee_ticks = 0;
};

} // namespace freenamp::frontend
