#include "views/playlist_view.hpp"
#include <iostream>
#include <algorithm>

namespace freenamp::frontend {

void PlaylistView::set_size(int w, int h) {
    m_bounds.w = std::max(275, w);
    m_bounds.h = std::max(140, h);

    m_list_box.x = 10;
    m_list_box.y = 20;
    m_list_box.w = m_bounds.w - 30;
    m_list_box.h = m_bounds.h - 54;

    m_scrollbar.x = m_bounds.w - 18;
    m_scrollbar.y = 20;
    m_scrollbar.w = 10;
    m_scrollbar.h = m_list_box.h;

    int btn_y = m_bounds.h - 26;
    m_btn_add.y = btn_y;
    m_btn_rem.y = btn_y;
    m_btn_clear.y = btn_y;
    m_btn_up.x = m_bounds.w - 48;
    m_btn_up.y = btn_y;
    m_btn_down.x = m_bounds.w - 24;
    m_btn_down.y = btn_y;
}

PlaylistView::PlaylistView(int x, int y, int w, int h) {
    m_bounds.x = x;
    m_bounds.y = y;
    set_size(w, h);
}

void PlaylistView::set_selected_index(int idx) {
    m_selected_index = idx;
    m_selected_indices.clear();
    if (idx >= 0) {
        m_selected_indices.insert(idx);
    }
    m_selection_anchor = idx;
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
    m_total_tracks = static_cast<int>(tracks.size());
    int total_tracks = m_total_tracks;

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

        bool is_selected = (m_selected_indices.count(track_idx) > 0 || track_idx == m_selected_index);
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

        // Duration on right: "03:45"
        std::string dur_text = backend::YtResolver::format_duration(track.duration_seconds);
        int dur_w = static_cast<int>(dur_text.size()) * 8;
        int dur_x = list_r.x + list_r.w - dur_w - 6;

        // Truncate long title to keep playlist clean (~25-30 chars max, never overlapping duration)
        std::string prefix = std::to_string(track_idx + 1) + ". ";
        int avail_title_w = dur_x - (list_r.x + 4) - static_cast<int>(prefix.size()) * 8 - 4;
        int max_title_chars = std::max(6, avail_title_w / 8);

        std::string display_title = track.title;
        if (static_cast<int>(display_title.size()) > max_title_chars) {
            display_title = display_title.substr(0, std::max(1, max_title_chars - 2)) + "..";
        }

        std::string line_text = prefix + display_title;
        RetroFont::draw_text(renderer, line_text, list_r.x + 4, item_y + 2, item_color, 1);
        RetroFont::draw_text(renderer, dur_text, dur_x, item_y + 2, item_color, 1);
    }

    // Draw drop insertion line indicator if dragging tracks
    if (m_is_dragging_track && m_drop_target_index >= 0) {
        if (m_drop_target_index >= m_scroll_offset && m_drop_target_index <= m_scroll_offset + visible_lines) {
            int line_idx = m_drop_target_index - m_scroll_offset;
            int indicator_y = list_r.y + 2 + line_idx * line_h;
            SDL_SetRenderDrawColor(renderer, Palette::LedYellow.r, Palette::LedYellow.g, Palette::LedYellow.b, 255);
            SDL_Rect ind_r = { list_r.x + 2, indicator_y - 1, list_r.w - 4, 3 };
            SDL_RenderFillRect(renderer, &ind_r);
        }
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
    int info_w = static_cast<int>(info_text.size()) * 8;
    int info_x = bx + m_btn_up.x - info_w - 6;
    if (info_x < bx + 165) info_x = bx + 165;
    RetroFont::draw_text(renderer, info_text, info_x, by + m_bounds.h - 21, Palette::TextDim, 1);

    // 6. Resize grip (///) in bottom right corner
    RetroWidgets::draw_resize_grip(renderer, bx + m_bounds.w - 3, by + m_bounds.h - 3);
}

bool PlaylistView::handle_mouse_down(int mx, int my, int clicks, backend::CoreController& core, bool& open_url_dialog, bool& close_requested) {
    if (!m_visible || !m_bounds.contains(mx, my)) return false;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // Check resize grip (bottom right corner 16x16)
    if (mx >= bx + m_bounds.w - 16 && my >= by + m_bounds.h - 16) {
        m_is_resizing = true;
        m_resize_start_w = m_bounds.w;
        m_resize_start_h = m_bounds.h;
        m_resize_start_mx = mx;
        m_resize_start_my = my;
        return true;
    }

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

    // List box click (select track, double-click to play, or start drag reorder)
    Rect list_r = { bx + m_list_box.x, by + m_list_box.y, m_list_box.w, m_list_box.h };
    if (list_r.contains(mx, my)) {
        int line_h = 12;
        int clicked_line = (my - (list_r.y + 2)) / line_h;
        int clicked_track = m_scroll_offset + clicked_line;
        int total = static_cast<int>(core.get_playlist().size());

        if (clicked_track >= 0 && clicked_track < total) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_click_time).count();

            // Double-click: native clicks >= 2 or software threshold < 600ms
            if (clicks >= 2 || (clicked_track == m_last_clicked_index && elapsed_ms < 600)) {
                m_selected_indices.clear();
                m_selected_indices.insert(clicked_track);
                m_selected_index = clicked_track;
                m_selection_anchor = clicked_track;
                m_last_clicked_index = -1;
                m_last_click_time = std::chrono::steady_clock::time_point{};
                core.play_track_index(clicked_track);
                return true;
            }

            SDL_Keymod mod = SDL_GetModState();
            bool is_shift = (mod & KMOD_SHIFT) != 0;
            bool is_ctrl = (mod & KMOD_CTRL) != 0;

            if (is_shift) {
                if (m_selection_anchor < 0 || m_selection_anchor >= total) {
                    m_selection_anchor = m_selected_index;
                }
                if (!is_ctrl) {
                    m_selected_indices.clear();
                }
                int start = std::min(m_selection_anchor, clicked_track);
                int end = std::max(m_selection_anchor, clicked_track);
                for (int i = start; i <= end; ++i) {
                    m_selected_indices.insert(i);
                }
                m_selected_index = clicked_track;
            } else if (is_ctrl) {
                if (m_selected_indices.count(clicked_track)) {
                    m_selected_indices.erase(clicked_track);
                    if (m_selected_indices.empty()) {
                        m_selected_indices.insert(clicked_track);
                    }
                } else {
                    m_selected_indices.insert(clicked_track);
                }
                m_selection_anchor = clicked_track;
                m_selected_index = clicked_track;
            } else {
                // If clicked track is not in current multi-selection, select it exclusively
                if (m_selected_indices.count(clicked_track) == 0) {
                    m_selected_indices.clear();
                    m_selected_indices.insert(clicked_track);
                    m_selection_anchor = clicked_track;
                    m_selected_index = clicked_track;
                }
            }

            // Set up drag initiation
            m_drag_track_index = clicked_track;
            m_drag_start_x = mx;
            m_drag_start_y = my;
            m_is_dragging_track = false;
            m_drop_target_index = -1;

            m_last_clicked_index = clicked_track;
            m_last_click_time = now;

            if (core.get_state() == backend::PlaybackState::Stopped && !m_selected_indices.empty()) {
                int first_sel = *m_selected_indices.begin();
                core.get_playlist().set_current_index(first_sel);
                auto tr = core.get_playlist().get_track(first_sel);
                if (tr.has_value()) {
                    core.set_current_title(tr->title);
                }
            }
        }
        return true;
    }

    // Scrollbar click & drag
    Rect sb_r = { bx + m_scrollbar.x, by + m_scrollbar.y, m_scrollbar.w, m_scrollbar.h };
    if (sb_r.contains(mx, my)) {
        m_dragging_scrollbar = true;
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
        remove_selected(core);
        return true;
    }

    // CLEAR button
    Rect clr_r = { bx + m_btn_clear.x, by + m_btn_clear.y, m_btn_clear.w, m_btn_clear.h };
    if (clr_r.contains(mx, my)) {
        core.stop();
        core.get_playlist().clear();
        core.get_resolver().clear_cache();
        core.set_current_title("Freenamp Ready");
        core.set_status_text("Pronto");
        m_selected_indices.clear();
        m_selected_index = 0;
        m_scroll_offset = 0;
        core.save_session();
        return true;
    }

    // UP button
    Rect up_r = { bx + m_btn_up.x, by + m_btn_up.y, m_btn_up.w, m_btn_up.h };
    if (up_r.contains(mx, my)) {
        if (!m_selected_indices.empty()) {
            int first_idx = *m_selected_indices.begin();
            if (first_idx > 0) {
                std::vector<size_t> move_indices(m_selected_indices.begin(), m_selected_indices.end());
                auto [new_start, new_end] = core.get_playlist().move_tracks(move_indices, first_idx - 1);
                m_selected_indices.clear();
                for (size_t i = new_start; i <= new_end; ++i) {
                    m_selected_indices.insert(static_cast<int>(i));
                }
                m_selected_index = static_cast<int>(new_start);
                m_selection_anchor = static_cast<int>(new_start);
                ensure_visible(m_selected_index, static_cast<int>(core.get_playlist().size()));
                core.save_session();
            }
        }
        return true;
    }

    // DOWN button
    Rect dn_r = { bx + m_btn_down.x, by + m_btn_down.y, m_btn_down.w, m_btn_down.h };
    if (dn_r.contains(mx, my)) {
        int total = static_cast<int>(core.get_playlist().size());
        if (!m_selected_indices.empty()) {
            int last_idx = *m_selected_indices.rbegin();
            if (last_idx >= 0 && last_idx < total - 1) {
                std::vector<size_t> move_indices(m_selected_indices.begin(), m_selected_indices.end());
                size_t to_idx = static_cast<size_t>(last_idx + 1);
                auto [new_start, new_end] = core.get_playlist().move_tracks(move_indices, to_idx);
                m_selected_indices.clear();
                for (size_t i = new_start; i <= new_end; ++i) {
                    m_selected_indices.insert(static_cast<int>(i));
                }
                m_selected_index = static_cast<int>(new_start);
                m_selection_anchor = static_cast<int>(new_start);
                ensure_visible(m_selected_index, total);
                core.save_session();
            }
        }
        return true;
    }

    return true;
}

void PlaylistView::remove_selected(backend::CoreController& core) {
    int total = static_cast<int>(core.get_playlist().size());
    if (total <= 0) return;

    std::vector<size_t> to_remove;
    if (!m_selected_indices.empty()) {
        for (int idx : m_selected_indices) {
            if (idx >= 0 && idx < total) {
                to_remove.push_back(static_cast<size_t>(idx));
            }
        }
    } else if (m_selected_index >= 0 && m_selected_index < total) {
        to_remove.push_back(static_cast<size_t>(m_selected_index));
    }

    if (to_remove.empty()) return;

    std::sort(to_remove.begin(), to_remove.end());
    int min_removed = static_cast<int>(to_remove.front());

    for (size_t idx : to_remove) {
        auto tr_opt = core.get_playlist().get_track(idx);
        if (tr_opt.has_value()) {
            if (!tr_opt->id.empty()) core.get_resolver().remove_from_cache(tr_opt->id);
            if (!tr_opt->original_url.empty()) core.get_resolver().remove_from_cache(tr_opt->original_url);
        }
    }

    int current_idx = core.get_playlist().get_current_index();
    bool was_current_removed = (std::find(to_remove.begin(), to_remove.end(), static_cast<size_t>(current_idx)) != to_remove.end());

    core.get_playlist().remove_tracks(to_remove);
    int new_total = static_cast<int>(core.get_playlist().size());

    if (was_current_removed) {
        core.stop();
        if (new_total == 0) {
            core.set_current_title("Freenamp Ready");
            core.set_status_text("Pronto");
        } else {
            auto next_tr = core.get_playlist().get_current_track();
            if (next_tr.has_value()) {
                core.set_current_title(next_tr->title);
            } else {
                core.set_current_title("Freenamp Ready");
            }
            core.set_status_text("Pronto");
        }
    }

    m_selected_indices.clear();
    if (new_total == 0) {
        m_selected_index = 0;
        m_scroll_offset = 0;
        core.set_current_title("Freenamp Ready");
        core.set_status_text("Pronto");
    } else {
        m_selected_index = std::clamp(min_removed, 0, new_total - 1);
        m_selected_indices.insert(m_selected_index);
        m_selection_anchor = m_selected_index;
        if (core.get_state() == backend::PlaybackState::Stopped) {
            core.get_playlist().set_current_index(m_selected_index);
            auto cur_tr = core.get_playlist().get_current_track();
            if (cur_tr.has_value()) {
                core.set_current_title(cur_tr->title);
            }
        }
    }

    core.save_session();
}

void PlaylistView::handle_mouse_up(int /*mx*/, int /*my*/, backend::CoreController& core) {
    m_dragging_window = false;
    m_is_resizing = false;
    m_dragging_scrollbar = false;

    if (m_is_dragging_track && m_drag_track_index >= 0 && m_drop_target_index >= 0) {
        int total = static_cast<int>(core.get_playlist().size());
        if (m_drop_target_index < total) {
            std::vector<size_t> move_indices;
            if (m_selected_indices.count(m_drag_track_index) > 0 && m_selected_indices.size() > 1) {
                for (int idx : m_selected_indices) {
                    if (idx >= 0 && idx < total) {
                        move_indices.push_back(static_cast<size_t>(idx));
                    }
                }
            } else {
                move_indices.push_back(static_cast<size_t>(m_drag_track_index));
            }

            auto [new_start, new_end] = core.get_playlist().move_tracks(move_indices, static_cast<size_t>(m_drop_target_index));

            m_selected_indices.clear();
            for (size_t i = new_start; i <= new_end; ++i) {
                m_selected_indices.insert(static_cast<int>(i));
            }
            m_selected_index = static_cast<int>(new_start);
            m_selection_anchor = static_cast<int>(new_start);
            ensure_visible(m_selected_index, total);
            core.save_session();
        }
    } else if (!m_is_dragging_track && m_drag_track_index >= 0) {
        SDL_Keymod mod = SDL_GetModState();
        if ((mod & KMOD_SHIFT) == 0 && (mod & KMOD_CTRL) == 0) {
            m_selected_indices.clear();
            m_selected_indices.insert(m_drag_track_index);
            m_selected_index = m_drag_track_index;
            m_selection_anchor = m_drag_track_index;
        }
    }

    m_is_dragging_track = false;
    m_drag_track_index = -1;
    m_drop_target_index = -1;
}

void PlaylistView::handle_mouse_move(int mx, int my, backend::CoreController& core, int canvas_w, int canvas_h) {
    if (m_drag_track_index >= 0) {
        if (!m_is_dragging_track) {
            if (std::abs(my - m_drag_start_y) > 4 || std::abs(mx - m_drag_start_x) > 4) {
                m_is_dragging_track = true;
            }
        }
        if (m_is_dragging_track) {
            int line_h = 12;
            int bx = m_bounds.x;
            int by = m_bounds.y;
            Rect list_r = { bx + m_list_box.x, by + m_list_box.y, m_list_box.w, m_list_box.h };
            int total = static_cast<int>(core.get_playlist().size());
            int visible_lines = list_r.h / line_h;

            int hover_line = (my - (list_r.y + 2)) / line_h;
            int target_idx = std::clamp(m_scroll_offset + hover_line, 0, std::max(0, total - 1));
            m_drop_target_index = target_idx;

            // Auto-scroll near top/bottom edges
            if (my < list_r.y + 6 && m_scroll_offset > 0) {
                m_scroll_offset--;
            } else if (my > list_r.y + list_r.h - 6 && m_scroll_offset < total - visible_lines) {
                m_scroll_offset++;
            }
            return;
        }
    }

    if (m_is_resizing) {
        int nw = m_resize_start_w + (mx - m_resize_start_mx);
        int nh = m_resize_start_h + (my - m_resize_start_my);
        int max_w = (canvas_w > 0) ? (canvas_w - m_bounds.x) : 800;
        int max_h = (canvas_h > 0) ? (canvas_h - m_bounds.y) : 600;
        nw = std::clamp(nw, 275, std::max(275, max_w));
        nh = std::clamp(nh, 140, std::max(140, max_h));
        set_size(nw, nh);
    } else if (m_dragging_scrollbar) {
        Rect sb_r = { m_bounds.x + m_scrollbar.x, m_bounds.y + m_scrollbar.y, m_scrollbar.w, m_scrollbar.h };
        float click_ratio = static_cast<float>(my - sb_r.y) / static_cast<float>(sb_r.h);
        int visible = m_list_box.h / 12;
        float clamped_ratio = std::clamp(click_ratio, 0.0f, 1.0f);
        m_scroll_offset = std::clamp(static_cast<int>(clamped_ratio * m_total_tracks), 0, std::max(0, m_total_tracks - visible));
    } else if (m_dragging_window) {
        int nx = mx - m_drag_off_x;
        int ny = my - m_drag_off_y;
        if (canvas_w > 0 && canvas_h > 0) {
            nx = std::clamp(nx, 0, std::max(0, canvas_w - m_bounds.w));
            ny = std::clamp(ny, 0, std::max(0, canvas_h - m_bounds.h));
        }
        m_bounds.x = nx;
        m_bounds.y = ny;
    }
}

void PlaylistView::handle_mouse_wheel(int wheel_y) {
    if (wheel_y > 0) {
        m_scroll_offset = std::max(0, m_scroll_offset - 2);
    } else if (wheel_y < 0) {
        m_scroll_offset += 2;
    }
}

void PlaylistView::ensure_visible(int index, int total_tracks) {
    if (total_tracks >= 0) {
        m_total_tracks = total_tracks;
    }
    int visible_lines = m_list_box.h / 12;
    if (visible_lines <= 0 || m_total_tracks <= 0) return;

    if (m_total_tracks <= visible_lines) {
        m_scroll_offset = 0;
        return;
    }

    if (index < m_scroll_offset) {
        m_scroll_offset = index;
    } else if (index >= m_scroll_offset + visible_lines) {
        m_scroll_offset = index - visible_lines + 1;
    }
    m_scroll_offset = std::clamp(m_scroll_offset, 0, std::max(0, m_total_tracks - visible_lines));
}

} // namespace freenamp::frontend
