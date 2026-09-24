#pragma once

#include "core_controller.hpp"
#include "views/main_view.hpp"
#include "views/info_view.hpp"
#include "views/eq_view.hpp"
#include "views/playlist_view.hpp"
#include "views/input_modal.hpp"
#include "window_dock.hpp"
#include "system_media_keys.hpp"
#include <SDL.h>
#include <memory>

namespace freenamp::frontend {

class GuiEngine {
public:
    GuiEngine(int width = 680, int height = 500);
    ~GuiEngine();

    bool init();
    void run(backend::CoreController& core);
    void shutdown();

    void save_window_layout();
    void load_window_layout();

private:
    int m_width = 680;
    int m_height = 500;
    bool m_running = false;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    MainView m_main_view{ 20, 20 };
    InfoView m_info_view{ 20, 140 };
    EqView m_eq_view{ 20, 214 };
    PlaylistView m_playlist_view{ 305, 20, 355, 310 };
    InputModal m_input_modal;

    std::unique_ptr<ISystemMediaKeys> m_system_media_keys;

    void process_events(backend::CoreController& core);
    void render(backend::CoreController& core);
    void trigger_play(backend::CoreController& core);
};

} // namespace freenamp::frontend
