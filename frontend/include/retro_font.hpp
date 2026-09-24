#pragma once

#include "gui_common.hpp"
#include <SDL.h>
#include <string>
#include <array>

namespace freenamp::frontend {

class RetroFont {
public:
    // Renders standard 8x8 bitmap text with Latin/Unicode support
    static void draw_text(SDL_Renderer* renderer, const std::string& text, int x, int y, Color color, int scale = 1);

    // UTF-8 string helpers for playlist and marquee rendering
    static size_t utf8_length(const std::string& text);
    static std::string utf8_substr(const std::string& text, size_t max_glyphs);
    static uint32_t decode_utf8(const std::string& text, size_t& index);

    // Renders scrolling marquee text clipped to a bounding box
    static void draw_marquee_text(SDL_Renderer* renderer, const std::string& text, int x, int y, int max_w, int scroll_offset, Color color);

    // Renders large 7-segment green digital clock digits (e.g. "04:15")
    static void draw_led_clock(SDL_Renderer* renderer, int minutes, int seconds, bool is_negative, int x, int y, Color color);

    // Renders small 7-segment track number (e.g. "01")
    static void draw_led_track_num(SDL_Renderer* renderer, int track_num, int x, int y, Color color);

private:
    static void draw_glyph(SDL_Renderer* renderer, uint32_t codepoint, int x, int y, Color color, int scale = 1);
    static void draw_7seg_digit(SDL_Renderer* renderer, int digit, int x, int y, int w, int h, Color color);
};

} // namespace freenamp::frontend
