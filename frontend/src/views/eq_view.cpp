#include "views/eq_view.hpp"
#include <iostream>
#include <algorithm>

namespace freenamp::frontend {

namespace {
const char* BAND_LABELS[10] = {
    "60", "170", "310", "600", "1K", "3K", "6K", "12K", "14K", "16K"
};
}

EqView::EqView(int x, int y) {
    m_bounds.x = x;
    m_bounds.y = y;

    // Initialize the 10 band slider hitboxes
    int start_x = 75;
    int spacing = 18;
    for (int i = 0; i < 10; ++i) {
        m_sliders_bands.push_back(Rect{ start_x + i * spacing, 36, 14, 68 });
    }
}

void EqView::render(SDL_Renderer* renderer, backend::CoreController& core) {
    if (!m_visible) return;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // 1. Frame & Title
    RetroWidgets::draw_window_panel(renderer, m_bounds, "FREENAMP EQUALIZER", true);

    auto& eq = core.get_equalizer();

    // 2. ON Toggle Button
    Rect on_r = { bx + m_btn_on.x, by + m_btn_on.y, m_btn_on.w, m_btn_on.h };
    RetroWidgets::draw_button(renderer, on_r, "ON", false, eq.is_enabled());

    // 3. ZERO / FLAT Reset Button
    Rect zero_r = { bx + m_btn_zero.x, by + m_btn_zero.y, m_btn_zero.w, m_btn_zero.h };
    RetroWidgets::draw_button(renderer, zero_r, "FLAT", false, false);

    // 4. EQ Response Curve Box
    Rect curve_box = { bx + 48, by + 18, 162, 14 };
    RetroWidgets::draw_recessed_box(renderer, curve_box);

    // Draw curve line
    if (eq.is_enabled()) {
        SDL_SetRenderDrawColor(renderer, Palette::LedGreen.r, Palette::LedGreen.g, Palette::LedGreen.b, 255);
        int prev_x = curve_box.x + 4;
        int mid_y = curve_box.y + curve_box.h / 2;
        int prev_y = mid_y - static_cast<int>((eq.get_band_gain(0) / 12.0f) * 5.0f);

        for (int i = 1; i < 10; ++i) {
            int cx = curve_box.x + 4 + i * (curve_box.w - 8) / 9;
            int cy = mid_y - static_cast<int>((eq.get_band_gain(i) / 12.0f) * 5.0f);
            SDL_RenderDrawLine(renderer, prev_x, prev_y, cx, cy);
            prev_x = cx;
            prev_y = cy;
        }
    }

    // 5. Preamp Slider
    Rect pre_r = { bx + m_slider_preamp.x, by + m_slider_preamp.y, m_slider_preamp.w, m_slider_preamp.h };
    RetroWidgets::draw_vertical_slider(renderer, pre_r, eq.get_preamp(), "PRE");

    // 6. 10 Frequency Band Sliders
    for (size_t i = 0; i < m_sliders_bands.size(); ++i) {
        Rect b_r = { bx + m_sliders_bands[i].x, by + m_sliders_bands[i].y, m_sliders_bands[i].w, m_sliders_bands[i].h };
        RetroWidgets::draw_vertical_slider(renderer, b_r, eq.get_band_gain(i), BAND_LABELS[i]);
    }
}

bool EqView::handle_mouse_down(int mx, int my, backend::CoreController& core, bool& close_requested) {
    if (!m_visible || !m_bounds.contains(mx, my)) return false;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // Title bar check (drag or close)
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

    auto& eq = core.get_equalizer();

    // ON button
    Rect on_r = { bx + m_btn_on.x, by + m_btn_on.y, m_btn_on.w, m_btn_on.h };
    if (on_r.contains(mx, my)) {
        eq.set_enabled(!eq.is_enabled());
        core.apply_equalizer();
        return true;
    }

    // FLAT button
    Rect zero_r = { bx + m_btn_zero.x, by + m_btn_zero.y, m_btn_zero.w, m_btn_zero.h };
    if (zero_r.contains(mx, my)) {
        eq.set_preamp(0.0f);
        for (size_t i = 0; i < 10; ++i) {
            eq.set_band_gain(i, 0.0f);
        }
        core.apply_equalizer();
        return true;
    }

    // Preamp slider
    Rect pre_r = { bx + m_slider_preamp.x, by + m_slider_preamp.y, m_slider_preamp.w, m_slider_preamp.h };
    if (pre_r.contains(mx, my)) {
        m_dragging_band = -1;
        float norm = 1.0f - (float)(my - pre_r.y) / (float)(pre_r.h - 12);
        eq.set_preamp((norm * 24.0f) - 12.0f);
        core.apply_equalizer();
        return true;
    }

    // 10 Band sliders
    for (size_t i = 0; i < m_sliders_bands.size(); ++i) {
        Rect b_r = { bx + m_sliders_bands[i].x, by + m_sliders_bands[i].y, m_sliders_bands[i].w, m_sliders_bands[i].h };
        if (b_r.contains(mx, my)) {
            m_dragging_band = static_cast<int>(i);
            float norm = 1.0f - (float)(my - b_r.y) / (float)(b_r.h - 12);
            eq.set_band_gain(i, (norm * 24.0f) - 12.0f);
            core.apply_equalizer();
            return true;
        }
    }

    return true;
}

void EqView::handle_mouse_up(int /*mx*/, int /*my*/) {
    m_dragging_window = false;
    m_dragging_band = -2;
}

void EqView::handle_mouse_move(int mx, int my, backend::CoreController& core, int canvas_w, int canvas_h) {
    if (m_dragging_window) {
        int nx = mx - m_drag_off_x;
        int ny = my - m_drag_off_y;
        if (canvas_w > 0 && canvas_h > 0) {
            nx = std::clamp(nx, 0, std::max(0, canvas_w - m_bounds.w));
            ny = std::clamp(ny, 0, std::max(0, canvas_h - m_bounds.h));
        }
        m_bounds.x = nx;
        m_bounds.y = ny;
        return;
    }

    if (m_dragging_band == -2) return;

    auto& eq = core.get_equalizer();
    int bx = m_bounds.x;
    int by = m_bounds.y;

    if (m_dragging_band == -1) {
        // Preamp
        Rect pre_r = { bx + m_slider_preamp.x, by + m_slider_preamp.y, m_slider_preamp.w, m_slider_preamp.h };
        float norm = 1.0f - (float)(my - pre_r.y) / (float)(pre_r.h - 12);
        norm = std::clamp(norm, 0.0f, 1.0f);
        eq.set_preamp((norm * 24.0f) - 12.0f);
        core.apply_equalizer();
    } else if (m_dragging_band >= 0 && m_dragging_band < 10) {
        // Band
        size_t idx = static_cast<size_t>(m_dragging_band);
        Rect b_r = { bx + m_sliders_bands[idx].x, by + m_sliders_bands[idx].y, m_sliders_bands[idx].w, m_sliders_bands[idx].h };
        float norm = 1.0f - (float)(my - b_r.y) / (float)(b_r.h - 12);
        norm = std::clamp(norm, 0.0f, 1.0f);
        eq.set_band_gain(idx, (norm * 24.0f) - 12.0f);
        core.apply_equalizer();
    }
}

} // namespace freenamp::frontend
