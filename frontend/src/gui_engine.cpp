#include "gui_engine.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL_syswm.h>
#endif

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

    // Restore saved application window size if available
    try {
        std::ifstream ifs("cache/settings.json");
        if (ifs.is_open()) {
            nlohmann::json settings;
            ifs >> settings;
            if (settings.contains("app_window") && settings["app_window"].is_object()) {
                m_width = settings["app_window"].value("w", m_width);
                m_height = settings["app_window"].value("h", m_height);
            }
        }
    } catch (...) {}

    m_window = SDL_CreateWindow(
        "Freenamp - YouTube Retro Audio Player",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        m_width,
        m_height,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!m_window) {
        std::cerr << "[GuiEngine] Falha ao criar janela SDL2: " << SDL_GetError() << "\n";
        return false;
    }

#ifdef _WIN32
    // Apply high quality 256x256 / 32x32 / 16x16 icon to window and taskbar
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(m_window, &wmInfo)) {
        HWND hwnd = wmInfo.info.win.window;
        HICON hIconBig = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(101), IMAGE_ICON, 256, 256, LR_DEFAULTCOLOR);
        if (!hIconBig) {
            hIconBig = (HICON)LoadImageW(NULL, L"assets/freenamp.ico", IMAGE_ICON, 256, 256, LR_LOADFROMFILE);
        }
        HICON hIconSmall = (HICON)LoadImageW(GetModuleHandleW(NULL), MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTCOLOR);
        if (!hIconSmall) {
            hIconSmall = (HICON)LoadImageW(NULL, L"assets/freenamp.ico", IMAGE_ICON, 32, 32, LR_LOADFROMFILE);
        }
        if (hIconBig) {
            SendMessageW(hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIconBig);
        }
        if (hIconSmall) {
            SendMessageW(hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
        }
    }
#endif

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

        if (event.type == SDL_WINDOWEVENT) {
            if (event.window.event == SDL_WINDOWEVENT_RESIZED || event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                m_width = event.window.data1;
                m_height = event.window.data2;
                save_window_layout();
            }
            continue;
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
                if (toggle_eq) {
                    m_eq_view.toggle_visible();
                    save_window_layout();
                }
                if (toggle_pl) {
                    m_playlist_view.toggle_visible();
                    save_window_layout();
                }
                continue;
            }

            bool eq_close = false;
            if (m_eq_view.is_visible() && m_eq_view.handle_mouse_down(mx, my, core, eq_close)) {
                if (eq_close) save_window_layout();
                continue;
            }

            bool info_close = false;
            if (m_info_view.is_visible() && m_info_view.handle_mouse_down(mx, my, core, info_close)) {
                if (info_close) save_window_layout();
                continue;
            }

            bool pl_add = false;
            bool pl_close = false;
            if (m_playlist_view.is_visible() && m_playlist_view.handle_mouse_down(mx, my, core, pl_add, pl_close)) {
                if (pl_add) m_input_modal.open();
                if (pl_close) save_window_layout();
                continue;
            }
        }

        // Mouse Button Up
        if (event.type == SDL_MOUSEBUTTONUP) {
            int mx = event.button.x;
            int my = event.button.y;

            bool was_dragging_or_resizing = m_main_view.is_dragging_window() ||
                                            m_info_view.is_dragging_window() ||
                                            m_eq_view.is_dragging_window() ||
                                            m_playlist_view.is_dragging_window() ||
                                            m_playlist_view.is_resizing();

            // Apply magnetic snapping when dragging finishes
            if (m_main_view.is_dragging_window()) {
                Rect mb = m_main_view.get_bounds();
                std::vector<Rect> others;
                if (m_info_view.is_visible()) others.push_back(m_info_view.get_bounds());
                if (m_eq_view.is_visible()) others.push_back(m_eq_view.get_bounds());
                if (m_playlist_view.is_visible()) others.push_back(m_playlist_view.get_bounds());
                WindowDock::snap(mb, others, m_width, m_height);
                m_main_view.set_position(mb.x, mb.y);
            }

            if (m_info_view.is_dragging_window() && m_info_view.is_visible()) {
                Rect ib = m_info_view.get_bounds();
                std::vector<Rect> others = { m_main_view.get_bounds() };
                if (m_eq_view.is_visible()) others.push_back(m_eq_view.get_bounds());
                if (m_playlist_view.is_visible()) others.push_back(m_playlist_view.get_bounds());
                WindowDock::snap(ib, others, m_width, m_height);
                m_info_view.set_position(ib.x, ib.y);
            }

            if (m_eq_view.is_dragging_window() && m_eq_view.is_visible()) {
                Rect eb = m_eq_view.get_bounds();
                std::vector<Rect> others = { m_main_view.get_bounds() };
                if (m_info_view.is_visible()) others.push_back(m_info_view.get_bounds());
                if (m_playlist_view.is_visible()) others.push_back(m_playlist_view.get_bounds());
                WindowDock::snap(eb, others, m_width, m_height);
                m_eq_view.set_position(eb.x, eb.y);
            }

            if (m_playlist_view.is_dragging_window() && m_playlist_view.is_visible()) {
                Rect pb = m_playlist_view.get_bounds();
                std::vector<Rect> others = { m_main_view.get_bounds() };
                if (m_info_view.is_visible()) others.push_back(m_info_view.get_bounds());
                if (m_eq_view.is_visible()) others.push_back(m_eq_view.get_bounds());
                WindowDock::snap(pb, others, m_width, m_height);
                m_playlist_view.set_position(pb.x, pb.y);
            }

            m_main_view.handle_mouse_up(mx, my);
            m_info_view.handle_mouse_up(mx, my);
            m_eq_view.handle_mouse_up(mx, my);
            m_playlist_view.handle_mouse_up(mx, my);

            if (was_dragging_or_resizing) {
                save_window_layout();
            }
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

                // If Info View is docked to Main, move it too
                if (m_info_view.is_visible() && WindowDock::are_docked(old_mb, m_info_view.get_bounds())) {
                    Rect ib = m_info_view.get_bounds();
                    m_info_view.set_position(ib.x + dx, ib.y + dy);
                }

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
                m_info_view.handle_mouse_move(mx, my);
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
            } else if (key == SDLK_DELETE) {
                if (m_playlist_view.is_visible()) {
                    m_playlist_view.remove_selected(core);
                }
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

    // 2. Render Info Window
    m_info_view.render(m_renderer, core);

    // 3. Render Equalizer Window
    m_eq_view.render(m_renderer, core);

    // 4. Render Playlist Window
    m_playlist_view.render(m_renderer, core);

    // 5. Render Input Modal (if open)
    m_input_modal.render(m_renderer, m_width, m_height);

    SDL_RenderPresent(m_renderer);
}

void GuiEngine::run(backend::CoreController& core) {
    core.load_session();
    load_window_layout();

    int cur_idx = core.get_playlist().get_current_index();
    if (cur_idx >= 0) {
        m_playlist_view.set_selected_index(cur_idx);
        m_playlist_view.ensure_visible(cur_idx, static_cast<int>(core.get_playlist().size()));
    }

    core.set_event_callback([this, &core](const std::string& event_name) {
        if (event_name == "track_changed") {
            int idx = core.get_playlist().get_current_index();
            if (idx >= 0) {
                m_playlist_view.set_selected_index(idx);
                m_playlist_view.ensure_visible(idx, static_cast<int>(core.get_playlist().size()));
            }
        }
    });

    while (m_running) {
        process_events(core);
        core.update();
        render(core);
        SDL_Delay(16); // Cap at ~60 FPS
    }

    core.save_session();
    save_window_layout();
}

void GuiEngine::save_window_layout() {
    try {
        std::filesystem::create_directories("cache");
        nlohmann::json settings;
        {
            std::ifstream ifs("cache/settings.json");
            if (ifs.is_open()) {
                try {
                    ifs >> settings;
                } catch (...) {}
            }
        }

        // Save application window dimensions
        settings["app_window"]["w"] = m_width;
        settings["app_window"]["h"] = m_height;

        // Save internal windows positions, sizes and visibilities
        Rect mb = m_main_view.get_bounds();
        settings["windows"]["main"]["x"] = mb.x;
        settings["windows"]["main"]["y"] = mb.y;
        settings["windows"]["main"]["w"] = mb.w;
        settings["windows"]["main"]["h"] = mb.h;

        Rect ib = m_info_view.get_bounds();
        settings["windows"]["info"]["x"] = ib.x;
        settings["windows"]["info"]["y"] = ib.y;
        settings["windows"]["info"]["w"] = ib.w;
        settings["windows"]["info"]["h"] = ib.h;
        settings["windows"]["info"]["visible"] = m_info_view.is_visible();

        Rect eb = m_eq_view.get_bounds();
        settings["windows"]["eq"]["x"] = eb.x;
        settings["windows"]["eq"]["y"] = eb.y;
        settings["windows"]["eq"]["w"] = eb.w;
        settings["windows"]["eq"]["h"] = eb.h;
        settings["windows"]["eq"]["visible"] = m_eq_view.is_visible();

        Rect pb = m_playlist_view.get_bounds();
        settings["windows"]["playlist"]["x"] = pb.x;
        settings["windows"]["playlist"]["y"] = pb.y;
        settings["windows"]["playlist"]["w"] = pb.w;
        settings["windows"]["playlist"]["h"] = pb.h;
        settings["windows"]["playlist"]["visible"] = m_playlist_view.is_visible();

        std::ofstream ofs("cache/settings.json");
        if (ofs.is_open()) {
            ofs << settings.dump(2);
        }
    } catch (...) {}
}

void GuiEngine::load_window_layout() {
    try {
        std::ifstream ifs("cache/settings.json");
        if (!ifs.is_open()) return;

        nlohmann::json settings;
        ifs >> settings;

        if (settings.contains("app_window") && settings["app_window"].is_object()) {
            const auto& aw = settings["app_window"];
            int saved_w = aw.value("w", m_width);
            int saved_h = aw.value("h", m_height);
            if (saved_w >= 400 && saved_h >= 300 && m_window) {
                m_width = saved_w;
                m_height = saved_h;
                SDL_SetWindowSize(m_window, m_width, m_height);
            }
        }

        if (!settings.contains("windows") || !settings["windows"].is_object()) {
            return;
        }

        const auto& wins = settings["windows"];

        auto clamp_pos = [this](int x, int y, int w, int h) -> std::pair<int, int> {
            int cx = std::clamp(x, 0, std::max(0, m_width - w));
            int cy = std::clamp(y, 0, std::max(0, m_height - h));
            return { cx, cy };
        };

        if (wins.contains("main") && wins["main"].is_object()) {
            const auto& m = wins["main"];
            int x = m.value("x", m_main_view.get_bounds().x);
            int y = m.value("y", m_main_view.get_bounds().y);
            auto [cx, cy] = clamp_pos(x, y, m_main_view.get_bounds().w, m_main_view.get_bounds().h);
            m_main_view.set_position(cx, cy);
        }

        if (wins.contains("info") && wins["info"].is_object()) {
            const auto& i = wins["info"];
            int x = i.value("x", m_info_view.get_bounds().x);
            int y = i.value("y", m_info_view.get_bounds().y);
            auto [cx, cy] = clamp_pos(x, y, m_info_view.get_bounds().w, m_info_view.get_bounds().h);
            m_info_view.set_position(cx, cy);
            if (i.contains("visible") && i["visible"].is_boolean()) {
                m_info_view.set_visible(i["visible"].get<bool>());
            }
        }

        if (wins.contains("eq") && wins["eq"].is_object()) {
            const auto& e = wins["eq"];
            int x = e.value("x", m_eq_view.get_bounds().x);
            int y = e.value("y", m_eq_view.get_bounds().y);
            auto [cx, cy] = clamp_pos(x, y, m_eq_view.get_bounds().w, m_eq_view.get_bounds().h);
            m_eq_view.set_position(cx, cy);
            if (e.contains("visible") && e["visible"].is_boolean()) {
                m_eq_view.set_visible(e["visible"].get<bool>());
            }
        }

        if (wins.contains("playlist") && wins["playlist"].is_object()) {
            const auto& p = wins["playlist"];
            int w = p.value("w", m_playlist_view.get_bounds().w);
            int h = p.value("h", m_playlist_view.get_bounds().h);
            w = std::clamp(w, 275, std::max(275, m_width));
            h = std::clamp(h, 140, std::max(140, m_height));
            m_playlist_view.set_size(w, h);

            int x = p.value("x", m_playlist_view.get_bounds().x);
            int y = p.value("y", m_playlist_view.get_bounds().y);
            auto [cx, cy] = clamp_pos(x, y, m_playlist_view.get_bounds().w, m_playlist_view.get_bounds().h);
            m_playlist_view.set_position(cx, cy);

            if (p.contains("visible") && p["visible"].is_boolean()) {
                m_playlist_view.set_visible(p["visible"].get<bool>());
            }
        }
    } catch (...) {}
}

} // namespace freenamp::frontend
