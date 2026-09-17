#pragma once

#include <cstdint>
#include <string>
#include <algorithm>

namespace freenamp::frontend {

struct Color {
    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    uint8_t a = 255;
};

// Retro Classic Palette
namespace Palette {
    constexpr Color BgDark        = { 26,  24,  36, 255 }; // #1a1824
    constexpr Color PanelBg       = { 38,  36,  52, 255 }; // #262434
    constexpr Color PanelBorderHi = { 72,  70,  96, 255 }; // #484660
    constexpr Color PanelBorderLo = { 16,  14,  22, 255 }; // #100e16

    constexpr Color TitleBarBg    = { 52,  48,  72, 255 }; // #343048
    constexpr Color TitleText     = { 180, 190, 220, 255 };

    constexpr Color DisplayBg     = { 0,   0,   0, 255 }; // Pure black recessed area
    constexpr Color DisplayBorder = { 50,  50,  60, 255 };

    constexpr Color LedGreen      = { 0, 255,   0, 255 }; // Bright 7-segment / spectrum green
    constexpr Color LedGreenDim   = { 0,  50,   0, 255 };
    constexpr Color LedYellow     = { 255, 230, 0, 255 };
    constexpr Color LedRed        = { 255,  40, 0, 255 };

    constexpr Color ButtonFace    = { 54,  52,  70, 255 };
    constexpr Color ButtonHi      = { 90,  88, 116, 255 };
    constexpr Color ButtonLo      = { 22,  20,  32, 255 };
    constexpr Color ButtonText    = { 210, 215, 230, 255 };

    constexpr Color SliderTrack   = { 14,  12,  20, 255 };
    constexpr Color SliderThumb   = { 80,  78, 104, 255 };
    constexpr Color SliderThumbHi = { 120, 118, 150, 255 };

    constexpr Color TextActive    = { 0, 240,  80, 255 }; // Green active track
    constexpr Color TextNormal    = { 200, 205, 220, 255 };
    constexpr Color TextDim       = { 110, 115, 130, 255 };
    constexpr Color SelectionBg   = { 20,  50, 120, 255 }; // Classic blue selection
}

struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    bool contains(int px, int py) const {
        return px >= x && px < x + w && py >= y && py < y + h;
    }
};

} // namespace freenamp::frontend
