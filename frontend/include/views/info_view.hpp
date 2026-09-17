#pragma once

#include "core_controller.hpp"
#include "gui_common.hpp"
#include "retro_widgets.hpp"
#include "retro_font.hpp"
#include <SDL.h>
#include <string>

namespace freenamp::frontend {

class InfoView {
public:
    InfoView(int x = 20, int y = 140);
    ~InfoView() = default;

    void render(SDL_Renderer* renderer, backend::CoreController& core);

    bool handle_mouse_down(int mx, int my, backend::CoreController& core, bool& close_requested);
    void handle_mouse_up(int mx, int my);
    void handle_mouse_move(int mx, int my);

    Rect get_bounds() const { return m_bounds; }
    void set_position(int x, int y) { m_bounds.x = x; m_bounds.y = y; }

    bool is_visible() const { return m_visible; }
    void set_visible(bool visible) { m_visible = visible; }
    void toggle_visible() { m_visible = !m_visible; }

    bool is_dragging_window() const { return m_dragging_window; }
    int get_drag_offset_x() const { return m_drag_off_x; }
    int get_drag_offset_y() const { return m_drag_off_y; }

private:
    Rect m_bounds{ 20, 140, 275, 70 };
    bool m_visible = true;

    bool m_dragging_window = false;
    int m_drag_off_x = 0;
    int m_drag_off_y = 0;
};

} // namespace freenamp::frontend
