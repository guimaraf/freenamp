#include "views/input_modal.hpp"
#include "yt_resolver.hpp"
#include <iostream>

namespace freenamp::frontend {

InputModal::InputModal() {
}

void InputModal::open() {
    m_is_open = true;

    // Check if clipboard contains a YouTube link
    if (SDL_HasClipboardText()) {
        char* clip = SDL_GetClipboardText();
        if (clip) {
            std::string s(clip);
            if (s.find("youtube.com") != std::string::npos || s.find("youtu.be") != std::string::npos) {
                m_input_text = backend::YtResolver::sanitize_url(s);
            }
            SDL_free(clip);
        }
    }
}

void InputModal::close() {
    m_is_open = false;
}

void InputModal::render(SDL_Renderer* renderer, int canvas_w, int canvas_h) {
    if (!m_is_open) return;

    // Dim background
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
    SDL_Rect screen_dim = { 0, 0, canvas_w, canvas_h };
    SDL_RenderFillRect(renderer, &screen_dim);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    // Center dialog
    m_bounds.x = (canvas_w - m_bounds.w) / 2;
    m_bounds.y = (canvas_h - m_bounds.h) / 2;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // Window panel
    RetroWidgets::draw_window_panel(renderer, m_bounds, "ADD YOUTUBE URL / PLAYLIST", true);

    // Label
    RetroFont::draw_text(renderer, "Cole a URL do video ou playlist:", bx + 15, by + 24, Palette::TextNormal, 1);

    // Input text box
    Rect inp_r = { bx + m_box_input.x, by + m_box_input.y, m_box_input.w, m_box_input.h };
    RetroWidgets::draw_recessed_box(renderer, inp_r);

    // Render input text with clipping and sliding window
    std::string display_str = m_input_text;
    m_cursor_blink++;
    if ((m_cursor_blink / 20) % 2 == 0) {
        display_str += "_";
    }

    SDL_Rect clip_r = { inp_r.x + 2, inp_r.y + 2, inp_r.w - 4, inp_r.h - 4 };
    SDL_RenderSetClipRect(renderer, &clip_r);

    int max_chars = (inp_r.w - 8) / 8;
    std::string visible_text = display_str;
    if (static_cast<int>(visible_text.size()) > max_chars && max_chars > 0) {
        visible_text = visible_text.substr(visible_text.size() - max_chars);
    }
    RetroFont::draw_text(renderer, visible_text, inp_r.x + 4, inp_r.y + 6, Palette::TextActive, 1);
    SDL_RenderSetClipRect(renderer, nullptr);

    // OK and Cancel buttons
    Rect ok_r = { bx + m_btn_ok.x, by + m_btn_ok.y, m_btn_ok.w, m_btn_ok.h };
    RetroWidgets::draw_button(renderer, ok_r, "ADICIONAR", false, true);

    Rect cancel_r = { bx + m_btn_cancel.x, by + m_btn_cancel.y, m_btn_cancel.w, m_btn_cancel.h };
    RetroWidgets::draw_button(renderer, cancel_r, "CANCELAR", false, false);
}

bool InputModal::handle_mouse_down(int mx, int my, backend::CoreController& core) {
    if (!m_is_open) return false;

    int bx = m_bounds.x;
    int by = m_bounds.y;

    // Check Close button (top right)
    if (mx >= bx + m_bounds.w - 15 && mx <= bx + m_bounds.w - 3 && my >= by + 3 && my <= by + 15) {
        close();
        return true;
    }

    // OK button
    Rect ok_r = { bx + m_btn_ok.x, by + m_btn_ok.y, m_btn_ok.w, m_btn_ok.h };
    if (ok_r.contains(mx, my)) {
        std::string clean = backend::YtResolver::sanitize_url(m_input_text);
        if (!clean.empty()) {
            core.add_url(clean, false);
            m_input_text.clear();
        }
        close();
        return true;
    }

    // Cancel button
    Rect cancel_r = { bx + m_btn_cancel.x, by + m_btn_cancel.y, m_btn_cancel.w, m_btn_cancel.h };
    if (cancel_r.contains(mx, my)) {
        close();
        return true;
    }

    return true; // Modal is blocking
}

void InputModal::handle_text_input(const char* text) {
    if (!m_is_open || !text) return;
    if (SDL_GetModState() & KMOD_CTRL) return; // Prevent control codes like \x16 from Ctrl+V
    for (const char* p = text; *p; ++p) {
        if (static_cast<unsigned char>(*p) >= 32 && static_cast<unsigned char>(*p) <= 126) {
            m_input_text += *p;
        }
    }
}

void InputModal::handle_key_down(SDL_Keycode key, backend::CoreController& core) {
    if (!m_is_open) return;

    if (key == SDLK_RETURN || key == SDLK_KP_ENTER) {
        std::string clean = backend::YtResolver::sanitize_url(m_input_text);
        if (!clean.empty()) {
            core.add_url(clean, false);
            m_input_text.clear();
        }
        close();
    } else if (key == SDLK_ESCAPE) {
        close();
    } else if (key == SDLK_BACKSPACE) {
        if (!m_input_text.empty()) {
            m_input_text.pop_back();
        }
    } else if (key == SDLK_v && (SDL_GetModState() & KMOD_CTRL)) {
        if (SDL_HasClipboardText()) {
            char* clip = SDL_GetClipboardText();
            if (clip) {
                std::string clean = backend::YtResolver::sanitize_url(clip);
                if (!clean.empty()) {
                    m_input_text = clean;
                }
                SDL_free(clip);
            }
        }
    }
}

} // namespace freenamp::frontend
