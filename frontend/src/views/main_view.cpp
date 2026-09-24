#include "views/main_view.hpp"
#include <iostream>
#include <algorithm>

namespace freenamp::frontend {

MainView::MainView(int x, int y) {
    m_bounds.x = x;
    m_bounds.y = y;
}

void MainView::render(SDL_Renderer* renderer, backend::CoreController& core) {
    // 1. Window Frame & Title Bar
    RetroWidgets::draw_window_panel(renderer, m_bounds, "FREENAMP", true);

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // 2. Main LCD/LED Display Box
    Rect lcd_box = { bx + 11, by + 20, 253, 50 };
    RetroWidgets::draw_recessed_box(renderer, lcd_box);

    // Marquee Title (scrolling at ~30 chars/sec)
    m_marquee_ticks++;
    if (m_marquee_ticks % 2 == 0) {
        m_marquee_offset++;
    }
    std::string title_display = core.get_current_title();
    if (title_display.empty()) title_display = "Freenamp - YouTube Retro Audio Player";
    RetroFont::draw_marquee_text(renderer, title_display, bx + 16, by + 23, 240, m_marquee_offset, Palette::TextActive);

    // Digital LED Clock
    double pos = std::max(0.0, core.get_position());
    int min = 0;
    int sec = 0;
    bool is_neg = false;

    if (m_time_remaining_mode) {
        double dur = core.get_duration();
        double rem = (dur > 0.0) ? std::max(0.0, dur - pos) : 0.0;
        min = static_cast<int>(rem) / 60;
        sec = static_cast<int>(rem) % 60;
        is_neg = true;
    } else {
        min = static_cast<int>(pos) / 60;
        sec = static_cast<int>(pos) % 60;
        is_neg = false;
    }
    RetroFont::draw_led_clock(renderer, min, sec, is_neg, bx + 55, by + 38, Palette::LedGreen);

    // Track Number (small 7-seg)
    int cur_idx = core.get_playlist().get_current_index();
    RetroFont::draw_led_track_num(renderer, cur_idx >= 0 ? cur_idx + 1 : 0, bx + 26, by + 41, Palette::LedGreen);

    // 3. Sliders
    // Volume Slider
    float vol_norm = static_cast<float>(core.get_volume()) / 100.0f;
    Rect vol_r = { bx + m_slider_vol.x, by + m_slider_vol.y, m_slider_vol.w, m_slider_vol.h };
    RetroWidgets::draw_horizontal_slider(renderer, vol_r, vol_norm);

    // Pan / Balance Slider
    float pan_norm = (static_cast<float>(core.get_pan()) + 1.0f) / 2.0f;
    Rect pan_r = { bx + m_slider_pan.x, by + m_slider_pan.y, m_slider_pan.w, m_slider_pan.h };
    RetroWidgets::draw_horizontal_slider(renderer, pan_r, pan_norm, "", true);

    // Seek / Progress Bar
    double dur = core.get_duration();
    float seek_norm = (dur > 0.0) ? static_cast<float>(pos / dur) : 0.0f;
    Rect seek_r = { bx + m_slider_seek.x, by + m_slider_seek.y, m_slider_seek.w, m_slider_seek.h };
    RetroWidgets::draw_horizontal_slider(renderer, seek_r, seek_norm);

    // 4. Mode Toggle Buttons (EQ, PL, SHUF, REP)
    Rect eq_r = { bx + m_btn_eq.x, by + m_btn_eq.y, m_btn_eq.w, m_btn_eq.h };
    RetroWidgets::draw_button(renderer, eq_r, "EQ", m_pressed_button == "eq", core.get_equalizer().is_enabled());

    Rect pl_r = { bx + m_btn_pl.x, by + m_btn_pl.y, m_btn_pl.w, m_btn_pl.h };
    RetroWidgets::draw_button(renderer, pl_r, "PL", m_pressed_button == "pl", true);

    Rect shuf_r = { bx + m_btn_shuf.x, by + m_btn_shuf.y, m_btn_shuf.w, m_btn_shuf.h };
    RetroWidgets::draw_button_with_led(renderer, shuf_r, "SHUF", m_pressed_button == "shuf", core.is_shuffle());

    Rect rep_r = { bx + m_btn_rep.x, by + m_btn_rep.y, m_btn_rep.w, m_btn_rep.h };
    std::string rep_label = (core.get_repeat() == backend::RepeatMode::One) ? "REP 1" : "REP";
    bool rep_active = (core.get_repeat() != backend::RepeatMode::Off);
    RetroWidgets::draw_button_with_led(renderer, rep_r, rep_label, m_pressed_button == "rep", rep_active);

    // 5. Transport Buttons
    Rect prev_r = { bx + m_btn_prev.x, by + m_btn_prev.y, m_btn_prev.w, m_btn_prev.h };
    RetroWidgets::draw_transport_icon(renderer, prev_r, "prev", m_pressed_button == "prev");

    Rect play_r = { bx + m_btn_play.x, by + m_btn_play.y, m_btn_play.w, m_btn_play.h };
    RetroWidgets::draw_transport_icon(renderer, play_r, "play", m_pressed_button == "play");

    Rect pause_r = { bx + m_btn_pause.x, by + m_btn_pause.y, m_btn_pause.w, m_btn_pause.h };
    RetroWidgets::draw_transport_icon(renderer, pause_r, "pause", m_pressed_button == "pause");

    Rect stop_r = { bx + m_btn_stop.x, by + m_btn_stop.y, m_btn_stop.w, m_btn_stop.h };
    RetroWidgets::draw_transport_icon(renderer, stop_r, "stop", m_pressed_button == "stop");

    Rect next_r = { bx + m_btn_next.x, by + m_btn_next.y, m_btn_next.w, m_btn_next.h };
    RetroWidgets::draw_transport_icon(renderer, next_r, "next", m_pressed_button == "next");

    Rect eject_r = { bx + m_btn_eject.x, by + m_btn_eject.y, m_btn_eject.w, m_btn_eject.h };
    RetroWidgets::draw_transport_icon(renderer, eject_r, "eject", m_pressed_button == "eject");
}

bool MainView::handle_mouse_down(int mx, int my, backend::CoreController& core, bool& request_open_url, bool& toggle_eq, bool& toggle_pl, bool& play_requested) {
    if (!m_bounds.contains(mx, my)) return false;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // Check title bar for window dragging
    if (my >= by && my <= by + 16) {
        // Close button check
        if (mx >= bx + m_bounds.w - 15) {
            SDL_Event quit_ev;
            quit_ev.type = SDL_QUIT;
            SDL_PushEvent(&quit_ev);
            return true;
        }
        m_dragging_window = true;
        m_drag_off_x = mx - bx;
        m_drag_off_y = my - by;
        return true;
    }

    // LED Clock click (toggle elapsed / remaining countdown)
    Rect clock_r = { bx + m_clock_hitbox.x, by + m_clock_hitbox.y, m_clock_hitbox.w, m_clock_hitbox.h };
    if (clock_r.contains(mx, my)) {
        m_time_remaining_mode = !m_time_remaining_mode;
        return true;
    }

    // Transport buttons
    Rect prev_r = { bx + m_btn_prev.x, by + m_btn_prev.y, m_btn_prev.w, m_btn_prev.h };
    if (prev_r.contains(mx, my)) {
        m_pressed_button = "prev";
        core.previous();
        return true;
    }

    Rect play_r = { bx + m_btn_play.x, by + m_btn_play.y, m_btn_play.w, m_btn_play.h };
    if (play_r.contains(mx, my)) {
        m_pressed_button = "play";
        play_requested = true;
        return true;
    }

    Rect pause_r = { bx + m_btn_pause.x, by + m_btn_pause.y, m_btn_pause.w, m_btn_pause.h };
    if (pause_r.contains(mx, my)) {
        m_pressed_button = "pause";
        core.toggle_pause();
        return true;
    }

    Rect stop_r = { bx + m_btn_stop.x, by + m_btn_stop.y, m_btn_stop.w, m_btn_stop.h };
    if (stop_r.contains(mx, my)) {
        m_pressed_button = "stop";
        core.stop();
        return true;
    }

    Rect next_r = { bx + m_btn_next.x, by + m_btn_next.y, m_btn_next.w, m_btn_next.h };
    if (next_r.contains(mx, my)) {
        m_pressed_button = "next";
        core.next();
        return true;
    }

    Rect eject_r = { bx + m_btn_eject.x, by + m_btn_eject.y, m_btn_eject.w, m_btn_eject.h };
    if (eject_r.contains(mx, my)) {
        m_pressed_button = "eject";
        request_open_url = true;
        return true;
    }

    // Mode buttons
    Rect eq_r = { bx + m_btn_eq.x, by + m_btn_eq.y, m_btn_eq.w, m_btn_eq.h };
    if (eq_r.contains(mx, my)) {
        m_pressed_button = "eq";
        toggle_eq = true;
        return true;
    }

    Rect pl_r = { bx + m_btn_pl.x, by + m_btn_pl.y, m_btn_pl.w, m_btn_pl.h };
    if (pl_r.contains(mx, my)) {
        m_pressed_button = "pl";
        toggle_pl = true;
        return true;
    }

    Rect shuf_r = { bx + m_btn_shuf.x, by + m_btn_shuf.y, m_btn_shuf.w, m_btn_shuf.h };
    if (shuf_r.contains(mx, my)) {
        m_pressed_button = "shuf";
        core.toggle_shuffle();
        return true;
    }

    Rect rep_r = { bx + m_btn_rep.x, by + m_btn_rep.y, m_btn_rep.w, m_btn_rep.h };
    if (rep_r.contains(mx, my)) {
        m_pressed_button = "rep";
        core.cycle_repeat();
        return true;
    }

    // Sliders
    Rect vol_r = { bx + m_slider_vol.x, by + m_slider_vol.y, m_slider_vol.w, m_slider_vol.h };
    if (vol_r.contains(mx, my)) {
        m_dragging_vol = true;
        float v = std::clamp((float)(mx - vol_r.x) / (float)vol_r.w, 0.0f, 1.0f);
        core.set_volume(v * 100.0);
        return true;
    }

    Rect pan_r = { bx + m_slider_pan.x, by + m_slider_pan.y, m_slider_pan.w, m_slider_pan.h };
    if (pan_r.contains(mx, my)) {
        m_dragging_pan = true;
        float p = std::clamp((float)(mx - pan_r.x) / (float)pan_r.w, 0.0f, 1.0f);
        core.set_pan((p * 2.0f) - 1.0f);
        return true;
    }

    Rect seek_r = { bx + m_slider_seek.x, by + m_slider_seek.y, m_slider_seek.w, m_slider_seek.h };
    if (seek_r.contains(mx, my)) {
        m_dragging_seek = true;
        float s = std::clamp((float)(mx - seek_r.x) / (float)seek_r.w, 0.0f, 1.0f);
        double dur = core.get_duration();
        if (dur > 0.0) core.seek(s * dur);
        return true;
    }

    return true;
}

void MainView::handle_mouse_up(int /*mx*/, int /*my*/) {
    m_dragging_window = false;
    m_dragging_seek = false;
    m_dragging_vol = false;
    m_dragging_pan = false;
    m_pressed_button.clear();
}

void MainView::handle_mouse_move(int mx, int my, backend::CoreController& core, int canvas_w, int canvas_h) {
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

    int bx = m_bounds.x;
    int by = m_bounds.y;

    if (m_dragging_vol) {
        Rect vol_r = { bx + m_slider_vol.x, by + m_slider_vol.y, m_slider_vol.w, m_slider_vol.h };
        float v = std::clamp((float)(mx - vol_r.x) / (float)vol_r.w, 0.0f, 1.0f);
        core.set_volume(v * 100.0);
    } else if (m_dragging_pan) {
        Rect pan_r = { bx + m_slider_pan.x, by + m_slider_pan.y, m_slider_pan.w, m_slider_pan.h };
        float p = std::clamp((float)(mx - pan_r.x) / (float)pan_r.w, 0.0f, 1.0f);
        core.set_pan((p * 2.0f) - 1.0f);
    } else if (m_dragging_seek) {
        Rect seek_r = { bx + m_slider_seek.x, by + m_slider_seek.y, m_slider_seek.w, m_slider_seek.h };
        float s = std::clamp((float)(mx - seek_r.x) / (float)seek_r.w, 0.0f, 1.0f);
        double dur = core.get_duration();
        if (dur > 0.0) core.seek(s * dur);
    }
}

} // namespace freenamp::frontend
