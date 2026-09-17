#include "views/info_view.hpp"
#include <iostream>

namespace freenamp::frontend {

InfoView::InfoView(int x, int y) {
    m_bounds.x = x;
    m_bounds.y = y;
}

void InfoView::render(SDL_Renderer* renderer, backend::CoreController& core) {
    if (!m_visible) return;

    // 1. Window Frame with title FREENAMP INFO
    RetroWidgets::draw_window_panel(renderer, m_bounds, "FREENAMP INFO", true);

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // 2. LCD recessed box
    Rect lcd_box = { bx + 11, by + 20, 253, 42 };
    RetroWidgets::draw_recessed_box(renderer, lcd_box);

    if (core.is_loading()) {
        // Line 1: Status message
        std::string status = core.get_status_text();
        if (status.empty()) status = "Carregando...";
        RetroFont::draw_text(renderer, status, bx + 16, by + 24, Palette::TextActive, 1);

        // Line 2: Progress bar + Percentage
        int prog = core.get_loading_progress();
        Rect pb_r = { bx + 16, by + 37, 180, 14 };
        RetroWidgets::draw_progress_bar(renderer, pb_r, prog / 100.0f);

        std::string pct_str = std::to_string(prog) + "%";
        RetroFont::draw_text(renderer, pct_str, bx + 206, by + 40, Palette::LedGreen, 1);
    } else {
        // Ready / Playing Technical Info
        // Line 1: Bitrate, Sample Rate, Mode
        std::string kbps_val = std::to_string(core.get_bitrate_kbps());
        RetroFont::draw_text(renderer, kbps_val, bx + 16, by + 25, Palette::LedGreen, 1);
        RetroFont::draw_text(renderer, "kbps", bx + 16 + static_cast<int>(kbps_val.size()) * 8 + 4, by + 25, Palette::TextDim, 1);

        std::string khz_val = std::to_string(core.get_samplerate_khz());
        RetroFont::draw_text(renderer, khz_val, bx + 85, by + 25, Palette::LedGreen, 1);
        RetroFont::draw_text(renderer, "kHz", bx + 85 + static_cast<int>(khz_val.size()) * 8 + 4, by + 25, Palette::TextDim, 1);

        RetroFont::draw_text(renderer, core.get_channels(), bx + 160, by + 25, Palette::LedGreen, 1);

        // Line 2: Codec and Status
        RetroFont::draw_text(renderer, core.get_audio_codec(), bx + 16, by + 39, Palette::TextDim, 1);

        std::string st = core.get_status_text();
        if (st.empty()) st = "Pronto";
        RetroFont::draw_text(renderer, st, bx + 120, by + 39, Palette::TextNormal, 1);
    }
}

bool InfoView::handle_mouse_down(int mx, int my, backend::CoreController& /*core*/, bool& close_requested) {
    if (!m_visible || !m_bounds.contains(mx, my)) return false;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // Check title bar (height 16)
    if (my >= by && my <= by + 16) {
        if (mx >= bx + m_bounds.w - 15) {
            close_requested = true;
            m_visible = false;
            return true;
        }
        m_dragging_window = true;
        m_drag_off_x = mx - bx;
        m_drag_off_y = my - by;
        return true;
    }

    return true;
}

void InfoView::handle_mouse_up(int /*mx*/, int /*my*/) {
    m_dragging_window = false;
}

void InfoView::handle_mouse_move(int mx, int my) {
    if (m_dragging_window) {
        m_bounds.x = mx - m_drag_off_x;
        m_bounds.y = my - m_drag_off_y;
    }
}

} // namespace freenamp::frontend
