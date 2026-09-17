#pragma once

#include "gui_common.hpp"
#include "retro_widgets.hpp"
#include "core_controller.hpp"
#include <SDL.h>
#include <vector>

namespace freenamp::frontend {

class EqView {
public:
    EqView(int x = 20, int y = 136);

    void render(SDL_Renderer* renderer, backend::CoreController& core);

    bool handle_mouse_down(int mx, int my, backend::CoreController& core, bool& close_requested);
    void handle_mouse_up(int mx, int my);
    void handle_mouse_move(int mx, int my, backend::CoreController& core);

    Rect get_bounds() const { return m_bounds; }
    void set_position(int x, int y) { m_bounds.x = x; m_bounds.y = y; }

    bool is_visible() const { return m_visible; }
    void set_visible(bool v) { m_visible = v; }
    void toggle_visible() { m_visible = !m_visible; }

    bool is_dragging_window() const { return m_dragging_window; }
    int get_drag_offset_x() const { return m_drag_off_x; }
    int get_drag_offset_y() const { return m_drag_off_y; }

private:
    Rect m_bounds{ 20, 136, 275, 116 };
    bool m_visible = true;

    bool m_dragging_window = false;
    int m_drag_off_x = 0;
    int m_drag_off_y = 0;

    int m_dragging_band = -2; // -2: none, -1: preamp, 0..9: bands

    Rect m_btn_on{ 14, 18, 26, 12 };
    Rect m_btn_zero{ 218, 18, 44, 12 };

    Rect m_slider_preamp{ 20, 36, 16, 68 };
    std::vector<Rect> m_sliders_bands; // 10 sliders
};

} // namespace freenamp::frontend
