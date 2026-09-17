#include "views/playlist_view.hpp"
#include <iostream>
#include <algorithm>

namespace freenamp::frontend {

PlaylistView::PlaylistView(int x, int y, int w, int h) {
    m_bounds.x = x;
    m_bounds.y = y;
    m_bounds.w = w;
    m_bounds.h = h;

    m_list_box.w = w - 30;
    m_list_box.h = h - 54;
    m_scrollbar.x = w - 18;
    m_scrollbar.h = m_list_box.h;

    int btn_y = h - 26;
    m_btn_add.y = btn_y;
    m_btn_rem.y = btn_y;
    m_btn_clear.y = btn_y;
    m_btn_up.x = w - 48;
    m_btn_up.y = btn_y;
    m_btn_down.x = w - 24;
    m_btn_down.y = btn_y;
}

void PlaylistView::render(SDL_Renderer* renderer, backend::CoreController& core) {
    if (!m_visible) return;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // 1. Window Frame & Title
    RetroWidgets::draw_window_panel(renderer, m_bounds, "FREENAMP PLAYLIST", true);

    const auto& playlist = core.get_playlist();
    auto tracks = playlist.get_all_tracks();
    int current_playing_idx = playlist.get_current_index();

    // 2. Track List Box (Pure black recessed)
    Rect list_r = { bx + m_list_box.x, by + m_list_box.y, m_list_box.w, m_list_box.h };
    RetroWidgets::draw_recessed_box(renderer, list_r);

    int line_h = 12;
    int visible_lines = list_r.h / line_h;
    int total_tracks = static_cast<int>(tracks.size());

    // Clamp scroll offset
    if (m_scroll_offset > total_tracks - visible_lines) {
        m_scroll_offset = std::max(0, total_tracks - visible_lines);
    }

    // Set clipping for list box
    SDL_Rect clip = { list_r.x + 2, list_r.y + 2, list_r.w - 4, list_r.h - 4 };
    SDL_RenderSetClipRect(renderer, &clip);

    for (int i = 0; i < visible_lines; ++i) {
        int track_idx = m_scroll_offset + i;
        if (track_idx >= total_tracks) break;

        const auto& track = tracks[track_idx];
        int item_y = list_r.y + 2 + i * line_h;

        bool is_selected = (track_idx == m_selected_index);
        bool is_playing = (track_idx == current_playing_idx);

        // Highlight selected track
        if (is_selected) {
            SDL_SetRenderDrawColor(renderer, Palette::SelectionBg.r, Palette::SelectionBg.g, Palette::SelectionBg.b, 255);
            SDL_Rect sel_rect = { list_r.x + 2, item_y, list_r.w - 4, line_h };
            SDL_RenderFillRect(renderer, &sel_rect);
        }

        // Text color
        Color item_color = Palette::TextNormal;
        if (is_playing) {
            item_color = Palette::TextActive; // Green
        } else if (is_selected) {
            item_color = { 255, 255, 255, 255 };
        }

        // Format: "1. Artist - Title"
        std::string line_text = std::to_string(track_idx + 1) + ". " + track.title;
        RetroFont::draw_text(renderer, line_text, list_r.x + 4, item_y + 2, item_color, 1);

        // Duration on right: "03:45"
        std::string dur_text = backend::YtResolver::format_duration(track.duration_seconds);
        int dur_w = static_cast<int>(dur_text.size()) * 8;
        RetroFont::draw_text(renderer, dur_text, list_r.x + list_r.w - dur_w - 6, item_y + 2, item_color, 1);
    }

    SDL_RenderSetClipRect(renderer, nullptr);

    // 3. Scrollbar
    Rect sb_r = { bx + m_scrollbar.x, by + m_scrollbar.y, m_scrollbar.w, m_scrollbar.h };
    RetroWidgets::draw_recessed_box(renderer, sb_r);

    if (total_tracks > visible_lines) {
        float ratio = static_cast<float>(visible_lines) / static_cast<float>(total_tracks);
        int thumb_h = std::max(12, static_cast<int>(sb_r.h * ratio));
        float scroll_ratio = static_cast<float>(m_scroll_offset) / static_cast<float>(total_tracks - visible_lines);
        int thumb_y = sb_r.y + static_cast<int>(scroll_ratio * (sb_r.h - thumb_h));

        SDL_SetRenderDrawColor(renderer, Palette::SliderThumb.r, Palette::SliderThumb.g, Palette::SliderThumb.b, 255);
        SDL_Rect thumb = { sb_r.x + 1, thumb_y, sb_r.w - 2, thumb_h };
        SDL_RenderFillRect(renderer, &thumb);
    }

    // 4. Buttons
    Rect add_r = { bx + m_btn_add.x, by + m_btn_add.y, m_btn_add.w, m_btn_add.h };
    RetroWidgets::draw_button(renderer, add_r, "+ URL");

    Rect rem_r = { bx + m_btn_rem.x, by + m_btn_rem.y, m_btn_rem.w, m_btn_rem.h };
    RetroWidgets::draw_button(renderer, rem_r, "- REM");

    Rect clr_r = { bx + m_btn_clear.x, by + m_btn_clear.y, m_btn_clear.w, m_btn_clear.h };
    RetroWidgets::draw_button(renderer, clr_r, "CLEAR");

    Rect up_r = { bx + m_btn_up.x, by + m_btn_up.y, m_btn_up.w, m_btn_up.h };
    RetroWidgets::draw_button(renderer, up_r, "^");

    Rect dn_r = { bx + m_btn_down.x, by + m_btn_down.y, m_btn_down.w, m_btn_down.h };
    RetroWidgets::draw_button(renderer, dn_r, "v");

    // 5. Total Duration & Counts
    std::string total_dur = backend::YtResolver::format_duration(playlist.get_total_duration());
    std::string info_text = std::to_string(total_tracks) + " faixas / " + total_dur;
    RetroFont::draw_text(renderer, info_text, bx + 165, by + m_bounds.h - 38, Palette::TextDim, 1);
}

bool PlaylistView::handle_mouse_down(int mx, int my, backend::CoreController& core, bool& open_url_dialog, bool& close_requested) {
    if (!m_visible || !m_bounds.contains(mx, my)) return false;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // Title bar check
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

    // List box click (select track or double-click to play)
    Rect list_r = { bx + m_list_box.x, by + m_list_box.y, m_list_box.w, m_list_box.h };
    if (list_r.contains(mx, my)) {
        int line_h = 12;
        int clicked_line = (my - (list_r.y + 2)) / line_h;
        int clicked_track = m_scroll_offset + clicked_line;

        if (clicked_track >= 0 && clicked_track < static_cast<int>(core.get_playlist().size())) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();

            if (clicked_track == m_last_clicked_index && elapsed_ms < 400) {
                // Double click -> PLAY!
                core.play_track_index(clicked_track);
            } else {
                // Single click -> SELECT
                m_selected_index = clicked_track;
            }

            m_last_clicked_index = clicked_track;
            m_last_click_time = now;
        }
        return true;
    }

    // Scrollbar click
    Rect sb_r = { bx + m_scrollbar.x, by + m_scrollbar.y, m_scrollbar.w, m_scrollbar.h };
    if (sb_r.contains(mx, my)) {
        float click_ratio = static_cast<float>(my - sb_r.y) / static_cast<float>(sb_r.h);
        int total = static_cast<int>(core.get_playlist().size());
        int visible = m_list_box.h / 12;
        m_scroll_offset = std::clamp(static_cast<int>(click_ratio * total), 0, std::max(0, total - visible));
        return true;
    }

    // + URL button
    Rect add_r = { bx + m_btn_add.x, by + m_btn_add.y, m_btn_add.w, m_btn_add.h };
    if (add_r.contains(mx, my)) {
        open_url_dialog = true;
        return true;
    }

    // - REM button
    Rect rem_r = { bx + m_btn_rem.x, by + m_btn_rem.y, m_btn_rem.w, m_btn_rem.h };
    if (rem_r.contains(mx, my)) {
        if (!core.get_playlist().empty() && m_selected_index >= 0) {
            core.get_playlist().remove_track(m_selected_index);
            if (m_selected_index >= static_cast<int>(core.get_playlist().size())) {
                m_selected_index = static_cast<int>(core.get_playlist().size()) - 1;
            }
        }
        return true;
    }

    // CLEAR button
    Rect clr_r = { bx + m_btn_clear.x, by + m_btn_clear.y, m_btn_clear.w, m_btn_clear.h };
    if (clr_r.contains(mx, my)) {
        core.get_playlist().clear();
        m_selected_index = 0;
        m_scroll_offset = 0;
        return true;
    }

    // UP button
    Rect up_r = { bx + m_btn_up.x, by + m_btn_up.y, m_btn_up.w, m_btn_up.h };
    if (up_r.contains(mx, my)) {
        if (m_selected_index > 0) {
            core.get_playlist().move_track(m_selected_index, m_selected_index - 1);
            m_selected_index--;
        }
        return true;
    }

    // DOWN button
    Rect dn_r = { bx + m_btn_down.x, by + m_btn_down.y, m_btn_down.w, m_btn_down.h };
    if (dn_r.contains(mx, my)) {
        if (m_selected_index >= 0 && m_selected_index < static_cast<int>(core.get_playlist().size()) - 1) {
            core.get_playlist().move_track(m_selected_index, m_selected_index + 1);
            m_selected_index++;
        }
        return true;
    }

    return true;
}

void PlaylistView::handle_mouse_up(int /*mx*/, int /*my*/) {
    m_dragging_window = false;
}

void PlaylistView::handle_mouse_move(int mx, int my) {
    if (m_dragging_window) {
        m_bounds.x = mx - m_drag_off_x;
        m_bounds.y = my - m_drag_off_y;
    }
}

void PlaylistView::handle_mouse_wheel(int wheel_y) {
    if (wheel_y > 0) {
        m_scroll_offset = std::max(0, m_scroll_offset - 2);
    } else if (wheel_y < 0) {
        m_scroll_offset += 2;
    }
}

} // namespace freenamp::frontend
