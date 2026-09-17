#pragma once

#include "gui_common.hpp"
#include "retro_font.hpp"
#include <SDL.h>
#include <array>
#include <string>

namespace freenamp::frontend {

class RetroWidgets {
public:
    // Window panel with chiseled borders and title bar
    static void draw_window_panel(SDL_Renderer* renderer, const Rect& rect, const std::string& title, bool show_close = false);

    // 3D beveled button with text or symbol
    static void draw_button(SDL_Renderer* renderer, const Rect& rect, const std::string& label, bool pressed = false, bool active = false);

    // Classic toggle button with indicator LED lamp (SHUFFLE, REPEAT)
    static void draw_button_with_led(SDL_Renderer* renderer, const Rect& rect, const std::string& label, bool pressed = false, bool active = false);

    // Small icon button (Prev, Play, Pause, Stop, Next, Eject)
    static void draw_transport_icon(SDL_Renderer* renderer, const Rect& rect, const std::string& icon_type, bool pressed = false);

    // Horizontal slider (Seek bar, Volume, Balance)
    static void draw_horizontal_slider(SDL_Renderer* renderer, const Rect& rect, float value_0_to_1, const std::string& label = "", bool show_center_detent = false);

    // Vertical slider (Equalizer band or Preamp: value from -12.0f to +12.0f)
    static void draw_vertical_slider(SDL_Renderer* renderer, const Rect& rect, float value_db, const std::string& freq_label);

    // Classic Winamp 16-band Spectrum Analyzer
    static void draw_spectrum(SDL_Renderer* renderer, const Rect& rect, const std::array<float, 16>& bands);

    // Recessed dark display box (for counters, marquee, playlist listbox)
    static void draw_recessed_box(SDL_Renderer* renderer, const Rect& rect);

    // Segmented LED progress bar for loading / buffering
    static void draw_progress_bar(SDL_Renderer* renderer, const Rect& rect, float progress_0_to_1, const std::string& text = "");

    // Diagonal resize grip (///) in bottom right corner
    static void draw_resize_grip(SDL_Renderer* renderer, int x, int y);
};

} // namespace freenamp::frontend
