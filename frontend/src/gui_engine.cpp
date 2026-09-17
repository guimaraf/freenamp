#include "gui_engine.hpp"
#include <iostream>

namespace freenamp::frontend {

GuiEngine::GuiEngine(int width, int height)
    : m_width(width), m_height(height) {
}

GuiEngine::~GuiEngine() {
    shutdown();
}

bool GuiEngine::init() {
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "[GuiEngine] Falha ao inicializar SDL2: " << SDL_GetError() << "\n";
        return false;
    }

    m_window = SDL_CreateWindow(
        "Freenamp - YouTube Retro Audio Player",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_SHOWN
    );

    if (!m_window) {
        std::cerr << "[GuiEngine] Falha ao criar janela SDL2: " << SDL_GetError() << "\n";
        return false;
    }

    m_renderer = SDL_CreateRenderer(
        m_window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!m_renderer) {
        // Fallback to software renderer if GPU accelerated is unavailable
        m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_SOFTWARE);
    }

    if (!m_renderer) {
        std::cerr << "[GuiEngine] Falha ao criar renderizador SDL2: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_StartTextInput();
    m_running = true;
    return true;
}

void GuiEngine::shutdown() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    SDL_Quit();
}

void GuiEngine::process_events(backend::CoreController& core) {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_QUIT) {
            m_running = false;
            return;
        }

        // Text input for modal dialog
        if (m_input_modal.is_open()) {
            if (event.type == SDL_TEXTINPUT) {
                m_input_modal.handle_text_input(event.text.text);
                continue;
            }
            if (event.type == SDL_KEYDOWN) {
                m_input_modal.handle_key_down(event.key.keysym.sym, core);
                continue;
            }
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                m_input_modal.handle_mouse_down(event.button.x, event.button.y, core);
                continue;
            }
            continue;
        }

        // Mouse Button Down
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            int mx = event.button.x;
            int my = event.button.y;

            bool request_open_url = false;
            bool toggle_eq = false;
            bool toggle_pl = false;

            if (m_main_view.handle_mouse_down(mx, my, core, request_open_url, toggle_eq, toggle_pl)) {
                if (request_open_url) m_input_modal.open();
                if (toggle_eq) m_eq_view.toggle_visible();
                if (toggle_pl) m_playlist_view.toggle_visible();
                continue;
            }

            bool eq_close = false;
            if (m_eq_view.is_visible() && m_eq_view.handle_mouse_down(mx, my, core, eq_close)) {
                continue;
            }

            bool pl_add = false;
            bool pl_close = false;
            if (m_playlist_view.is_visible() && m_playlist_view.handle_mouse_down(mx, my, core, pl_add, pl_close)) {
                if (pl_add) m_input_modal.open();
                continue;
            }
        }

        // Mouse Button Up
        if (event.type == SDL_MOUSEBUTTONUP) {
            int mx = event.button.x;
            int my = event.button.y;

            // Apply magnetic snapping when dragging finishes
            if (m_main_view.is_dragging_window()) {
                Rect mb = m_main_view.get_bounds();
                std::vector<Rect> others;
                if (m_eq_view.is_visible()) others.push_back(m_eq_view.get_bounds());
                if (m_playlist_view.is_visible()) others.push_back(m_playlist_view.get_bounds());
                WindowDock::snap(mb, others, m_width, m_height);
                m_main_view.set_position(mb.x, mb.y);
            }

            if (m_eq_view.is_dragging_window() && m_eq_view.is_visible()) {
                Rect eb = m_eq_view.get_bounds();
                std::vector<Rect> others = { m_main_view.get_bounds() };
                if (m_playlist_view.is_visible()) others.push_back(m_playlist_view.get_bounds());
                WindowDock::snap(eb, others, m_width, m_height);
                m_eq_view.set_position(eb.x, eb.y);
            }

            if (m_playlist_view.is_dragging_window() && m_playlist_view.is_visible()) {
                Rect pb = m_playlist_view.get_bounds();
                std::vector<Rect> others = { m_main_view.get_bounds() };
                if (m_eq_view.is_visible()) others.push_back(m_eq_view.get_bounds());
                WindowDock::snap(pb, others, m_width, m_height);
                m_playlist_view.set_position(pb.x, pb.y);
            }

            m_main_view.handle_mouse_up(mx, my);
            m_eq_view.handle_mouse_up(mx, my);
            m_playlist_view.handle_mouse_up(mx, my);
        }

        // Mouse Move
        if (event.type == SDL_MOUSEMOTION) {
            int mx = event.motion.x;
            int my = event.motion.y;

            // Check if dragging Main View moves docked child windows together
            if (m_main_view.is_dragging_window()) {
                Rect old_mb = m_main_view.get_bounds();
                m_main_view.handle_mouse_move(mx, my, core);
                Rect new_mb = m_main_view.get_bounds();
                int dx = new_mb.x - old_mb.x;
                int dy = new_mb.y - old_mb.y;

                // If Equalizer is docked to Main, move it too
                if (m_eq_view.is_visible() && WindowDock::are_docked(old_mb, m_eq_view.get_bounds())) {
                    Rect eb = m_eq_view.get_bounds();
                    m_eq_view.set_position(eb.x + dx, eb.y + dy);
                }

                // If Playlist is docked to Main, move it too
                if (m_playlist_view.is_visible() && WindowDock::are_docked(old_mb, m_playlist_view.get_bounds())) {
                    Rect pb = m_playlist_view.get_bounds();
                    m_playlist_view.set_position(pb.x + dx, pb.y + dy);
                }
            } else {
                m_main_view.handle_mouse_move(mx, my, core);
                m_eq_view.handle_mouse_move(mx, my, core);
                m_playlist_view.handle_mouse_move(mx, my);
            }
        }

        // Mouse Wheel
        if (event.type == SDL_MOUSEWHEEL) {
            m_playlist_view.handle_mouse_wheel(event.wheel.y);
        }

        // Global Keyboard Shortcuts (Classic Winamp style)
        if (event.type == SDL_KEYDOWN) {
            auto key = event.key.keysym.sym;
            bool ctrl = (SDL_GetModState() & KMOD_CTRL);

            if (key == SDLK_SPACE) {
                core.toggle_pause();
            } else if (key == SDLK_z) {
                core.previous();
            } else if (key == SDLK_x) {
                core.play();
            } else if (key == SDLK_c) {
                core.pause();
            } else if (key == SDLK_v) {
                core.stop();
            } else if (key == SDLK_b) {
                core.next();
            } else if (key == SDLK_l || (ctrl && key == SDLK_v) || (ctrl && key == SDLK_o)) {
                m_input_modal.open();
            } else if (key == SDLK_LEFT) {
                core.seek_relative(-5.0);
            } else if (key == SDLK_RIGHT) {
                core.seek_relative(5.0);
            } else if (key == SDLK_UP) {
                core.set_volume(core.get_volume() + 5.0);
            } else if (key == SDLK_DOWN) {
                core.set_volume(core.get_volume() - 5.0);
            }
        }
    }
}

void GuiEngine::render(backend::CoreController& core) {
    // Clear screen with dark background
    SDL_SetRenderDrawColor(m_renderer, Palette::BgDark.r, Palette::BgDark.g, Palette::BgDark.b, 255);
    SDL_RenderClear(m_renderer);

    // 1. Render Main Player Window
    m_main_view.render(m_renderer, core);

    // 2. Render Equalizer Window
    m_eq_view.render(m_renderer, core);

    // 3. Render Playlist Window
    m_playlist_view.render(m_renderer, core);

    // 4. Render Input Modal (if open)
    m_input_modal.render(m_renderer, m_width, m_height);

    SDL_RenderPresent(m_renderer);
}

void GuiEngine::run(backend::CoreController& core) {
    while (m_running) {
        process_events(core);
        core.update();
        render(core);
        SDL_Delay(16); // Cap at ~60 FPS
    }
}

} // namespace freenamp::frontend
