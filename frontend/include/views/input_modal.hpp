#pragma once

#include "gui_common.hpp"
#include "retro_widgets.hpp"
#include "core_controller.hpp"
#include <SDL.h>
#include <string>

namespace freenamp::frontend {

class InputModal {
public:
    InputModal();

    void open();
    void close();
    bool is_open() const { return m_is_open; }

    void render(SDL_Renderer* renderer, int canvas_w, int canvas_h);

    bool handle_mouse_down(int mx, int my, backend::CoreController& core);
    void handle_text_input(const char* text);
    void handle_key_down(SDL_Keycode key, backend::CoreController& core);

    const std::string& get_text() const { return m_input_text; }
    void set_text(const std::string& text) { m_input_text = text; }

private:
    bool m_is_open = false;
    std::string m_input_text;
    Rect m_bounds{ 0, 0, 480, 115 };

    Rect m_box_input{ 15, 42, 450, 20 };
    Rect m_btn_ok{ 140, 74, 90, 22 };
    Rect m_btn_cancel{ 250, 74, 90, 22 };

    int m_cursor_blink = 0;
};

} // namespace freenamp::frontend
