#include "retro_widgets.hpp"
#include <algorithm>
#include <cmath>

namespace freenamp::frontend {

namespace {

void fill_rect(SDL_Renderer* renderer, int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    SDL_Rect r = { x, y, w, h };
    SDL_RenderFillRect(renderer, &r);
}

void draw_bevel(SDL_Renderer* renderer, int x, int y, int w, int h, Color hi, Color lo) {
    // Top and Left
    SDL_SetRenderDrawColor(renderer, hi.r, hi.g, hi.b, hi.a);
    SDL_RenderDrawLine(renderer, x, y, x + w - 1, y);
    SDL_RenderDrawLine(renderer, x, y, x, y + h - 1);

    // Bottom and Right
    SDL_SetRenderDrawColor(renderer, lo.r, lo.g, lo.b, lo.a);
    SDL_RenderDrawLine(renderer, x + w - 1, y + 1, x + w - 1, y + h - 1);
    SDL_RenderDrawLine(renderer, x + 1, y + h - 1, x + w - 1, y + h - 1);
}

} // namespace

void RetroWidgets::draw_window_panel(SDL_Renderer* renderer, const Rect& rect, const std::string& title, bool show_close) {
    // Outer border
    draw_bevel(renderer, rect.x, rect.y, rect.w, rect.h, Palette::PanelBorderHi, Palette::PanelBorderLo);

    // Inner body
    fill_rect(renderer, rect.x + 1, rect.y + 1, rect.w - 2, rect.h - 2, Palette::PanelBg);

    // Title bar
    int title_h = 14;
    fill_rect(renderer, rect.x + 3, rect.y + 3, rect.w - 6, title_h, Palette::TitleBarBg);
    draw_bevel(renderer, rect.x + 3, rect.y + 3, rect.w - 6, title_h, Palette::PanelBorderHi, Palette::PanelBorderLo);

    // Decorative title bar grip lines
    SDL_SetRenderDrawColor(renderer, Palette::PanelBorderLo.r, Palette::PanelBorderLo.g, Palette::PanelBorderLo.b, 255);
    for (int gx = rect.x + 6; gx < rect.x + 24; gx += 3) {
        SDL_RenderDrawLine(renderer, gx, rect.y + 6, gx, rect.y + 11);
    }

    // Title text
    RetroFont::draw_text(renderer, title, rect.x + 28, rect.y + 6, Palette::TitleText, 1);

    // Close button (X)
    if (show_close) {
        int cx = rect.x + rect.w - 14;
        int cy = rect.y + 5;
        draw_bevel(renderer, cx, cy, 9, 9, Palette::ButtonHi, Palette::ButtonLo);
        RetroFont::draw_text(renderer, "X", cx + 1, cy + 1, Palette::TitleText, 1);
    }
}

void RetroWidgets::draw_recessed_box(SDL_Renderer* renderer, const Rect& rect) {
    fill_rect(renderer, rect.x, rect.y, rect.w, rect.h, Palette::DisplayBg);
    draw_bevel(renderer, rect.x, rect.y, rect.w, rect.h, Palette::PanelBorderLo, Palette::PanelBorderHi);
}

void RetroWidgets::draw_button(SDL_Renderer* renderer, const Rect& rect, const std::string& label, bool pressed, bool active) {
    Color bg = active ? Palette::SelectionBg : Palette::ButtonFace;
    fill_rect(renderer, rect.x, rect.y, rect.w, rect.h, bg);

    if (pressed) {
        draw_bevel(renderer, rect.x, rect.y, rect.w, rect.h, Palette::ButtonLo, Palette::ButtonHi);
    } else {
        draw_bevel(renderer, rect.x, rect.y, rect.w, rect.h, Palette::ButtonHi, Palette::ButtonLo);
    }

    int text_w = static_cast<int>(label.size()) * 8;
    int tx = rect.x + (rect.w - text_w) / 2 + (pressed ? 1 : 0);
    int ty = rect.y + (rect.h - 8) / 2 + (pressed ? 1 : 0);

    Color text_c = active ? Palette::TextActive : Palette::ButtonText;
    RetroFont::draw_text(renderer, label, tx, ty, text_c, 1);
}

void RetroWidgets::draw_transport_icon(SDL_Renderer* renderer, const Rect& rect, const std::string& icon_type, bool pressed) {
    fill_rect(renderer, rect.x, rect.y, rect.w, rect.h, Palette::ButtonFace);

    if (pressed) {
        draw_bevel(renderer, rect.x, rect.y, rect.w, rect.h, Palette::ButtonLo, Palette::ButtonHi);
    } else {
        draw_bevel(renderer, rect.x, rect.y, rect.w, rect.h, Palette::ButtonHi, Palette::ButtonLo);
    }

    int cx = rect.x + rect.w / 2 + (pressed ? 1 : 0);
    int cy = rect.y + rect.h / 2 + (pressed ? 1 : 0);

    SDL_SetRenderDrawColor(renderer, Palette::ButtonText.r, Palette::ButtonText.g, Palette::ButtonText.b, 255);

    if (icon_type == "prev") {
        // |<
        SDL_RenderDrawLine(renderer, cx - 4, cy - 4, cx - 4, cy + 4);
        for (int i = 0; i < 5; ++i) {
            SDL_RenderDrawLine(renderer, cx + i, cy - i, cx + i, cy + i);
        }
    } else if (icon_type == "play") {
        // >
        for (int i = 0; i < 7; ++i) {
            SDL_RenderDrawLine(renderer, cx - 3 + i, cy - i, cx - 3 + i, cy + i);
        }
    } else if (icon_type == "pause") {
        // ||
        SDL_Rect bar1 = { cx - 4, cy - 4, 3, 9 };
        SDL_Rect bar2 = { cx + 1, cy - 4, 3, 9 };
        SDL_RenderFillRect(renderer, &bar1);
        SDL_RenderFillRect(renderer, &bar2);
    } else if (icon_type == "stop") {
        // []
        SDL_Rect sq = { cx - 4, cy - 4, 8, 8 };
        SDL_RenderFillRect(renderer, &sq);
    } else if (icon_type == "next") {
        // >|
        for (int i = 0; i < 5; ++i) {
            SDL_RenderDrawLine(renderer, cx - 4 + i, cy - i, cx - 4 + i, cy + i);
        }
        SDL_RenderDrawLine(renderer, cx + 3, cy - 4, cx + 3, cy + 4);
    } else if (icon_type == "eject") {
        // ^ and _
        for (int i = 0; i < 5; ++i) {
            SDL_RenderDrawLine(renderer, cx - i, cy - 1 + i, cx + i, cy - 1 + i);
        }
        SDL_RenderDrawLine(renderer, cx - 4, cy + 5, cx + 4, cy + 5);
    }
}

void RetroWidgets::draw_horizontal_slider(SDL_Renderer* renderer, const Rect& rect, float value_0_to_1, const std::string& label, bool show_center_detent) {
    if (value_0_to_1 < 0.0f) value_0_to_1 = 0.0f;
    if (value_0_to_1 > 1.0f) value_0_to_1 = 1.0f;

    // Recessed track
    int track_y = rect.y + rect.h / 2 - 2;
    int track_h = 4;
    fill_rect(renderer, rect.x, track_y, rect.w, track_h, Palette::SliderTrack);
    draw_bevel(renderer, rect.x, track_y, rect.w, track_h, Palette::PanelBorderLo, Palette::PanelBorderHi);

    // Center detent tick
    if (show_center_detent) {
        int mid_x = rect.x + rect.w / 2;
        SDL_SetRenderDrawColor(renderer, Palette::PanelBorderHi.r, Palette::PanelBorderHi.g, Palette::PanelBorderHi.b, 255);
        SDL_RenderDrawLine(renderer, mid_x, track_y - 2, mid_x, track_y + track_h + 1);
    }

    // Slider thumb
    int thumb_w = 12;
    int thumb_h = rect.h;
    int thumb_x = rect.x + static_cast<int>(value_0_to_1 * (rect.w - thumb_w));
    int thumb_y = rect.y;

    fill_rect(renderer, thumb_x, thumb_y, thumb_w, thumb_h, Palette::SliderThumb);
    draw_bevel(renderer, thumb_x, thumb_y, thumb_w, thumb_h, Palette::SliderThumbHi, Palette::PanelBorderLo);

    // Grip ridge in thumb center
    SDL_SetRenderDrawColor(renderer, Palette::PanelBorderLo.r, Palette::PanelBorderLo.g, Palette::PanelBorderLo.b, 255);
    SDL_RenderDrawLine(renderer, thumb_x + thumb_w / 2, thumb_y + 2, thumb_x + thumb_w / 2, thumb_y + thumb_h - 3);

    // Optional label text
    if (!label.empty()) {
        RetroFont::draw_text(renderer, label, rect.x, rect.y - 9, Palette::TextDim, 1);
    }
}

void RetroWidgets::draw_vertical_slider(SDL_Renderer* renderer, const Rect& rect, float value_db, const std::string& freq_label) {
    // Value range -12.0 to +12.0
    float norm = (value_db + 12.0f) / 24.0f;
    norm = std::clamp(norm, 0.0f, 1.0f);

    // Recessed vertical track
    int track_w = 4;
    int track_x = rect.x + rect.w / 2 - track_w / 2;
    int track_y = rect.y;
    int track_h = rect.h - 12; // Leave room for label

    fill_rect(renderer, track_x, track_y, track_w, track_h, Palette::SliderTrack);
    draw_bevel(renderer, track_x, track_y, track_w, track_h, Palette::PanelBorderLo, Palette::PanelBorderHi);

    // Center 0dB mark
    int mid_y = track_y + track_h / 2;
    SDL_SetRenderDrawColor(renderer, Palette::PanelBorderHi.r, Palette::PanelBorderHi.g, Palette::PanelBorderHi.b, 255);
    SDL_RenderDrawLine(renderer, track_x - 2, mid_y, track_x + track_w + 1, mid_y);

    // Slider thumb
    int thumb_w = rect.w;
    int thumb_h = 9;
    // Invert Y so up is +12dB and down is -12dB
    int thumb_y = track_y + static_cast<int>((1.0f - norm) * (track_h - thumb_h));
    int thumb_x = rect.x;

    fill_rect(renderer, thumb_x, thumb_y, thumb_w, thumb_h, Palette::SliderThumb);
    draw_bevel(renderer, thumb_x, thumb_y, thumb_w, thumb_h, Palette::SliderThumbHi, Palette::PanelBorderLo);

    // Horizontal grip ridge
    SDL_SetRenderDrawColor(renderer, Palette::PanelBorderLo.r, Palette::PanelBorderLo.g, Palette::PanelBorderLo.b, 255);
    SDL_RenderDrawLine(renderer, thumb_x + 2, thumb_y + thumb_h / 2, thumb_x + thumb_w - 3, thumb_y + thumb_h / 2);

    // Frequency label
    if (!freq_label.empty()) {
        int tx = rect.x + (rect.w - static_cast<int>(freq_label.size()) * 8) / 2;
        RetroFont::draw_text(renderer, freq_label, tx, rect.y + rect.h - 8, Palette::TextDim, 1);
    }
}

void RetroWidgets::draw_spectrum(SDL_Renderer* renderer, const Rect& rect, const std::array<float, 16>& bands) {
    draw_recessed_box(renderer, rect);

    int inner_x = rect.x + 2;
    int inner_y = rect.y + 2;
    int inner_w = rect.w - 4;
    int inner_h = rect.h - 4;

    int num_bars = 16;
    int bar_spacing = 1;
    int bar_w = (inner_w - (num_bars - 1) * bar_spacing) / num_bars;
    if (bar_w < 2) bar_w = 2;

    int block_h = 2;
    int block_gap = 1;
    int total_blocks = inner_h / (block_h + block_gap);

    for (int i = 0; i < num_bars; ++i) {
        float level = std::clamp(bands[i], 0.0f, 1.0f);
        int active_blocks = static_cast<int>(level * total_blocks);

        int bx = inner_x + i * (bar_w + bar_spacing);

        for (int b = 0; b < active_blocks; ++b) {
            int by = inner_y + inner_h - (b + 1) * (block_h + block_gap);

            // Gradient: Bottom=Green, Mid=Yellow, Top=Red
            Color block_color = Palette::LedGreen;
            float height_ratio = static_cast<float>(b) / static_cast<float>(total_blocks);
            if (height_ratio > 0.8f) {
                block_color = Palette::LedRed;
            } else if (height_ratio > 0.55f) {
                block_color = Palette::LedYellow;
            }

            fill_rect(renderer, bx, by, bar_w, block_h, block_color);
        }

        // Peak line
        if (active_blocks > 0) {
            int peak_y = inner_y + inner_h - active_blocks * (block_h + block_gap) - 1;
            if (peak_y >= inner_y) {
                SDL_SetRenderDrawColor(renderer, 220, 255, 220, 255);
                SDL_RenderDrawLine(renderer, bx, peak_y, bx + bar_w - 1, peak_y);
            }
        }
    }
}

} // namespace freenamp::frontend
