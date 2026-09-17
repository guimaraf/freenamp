#pragma once

#include "core_controller.hpp"
#include "views/main_view.hpp"
#include "views/eq_view.hpp"
#include "views/playlist_view.hpp"
#include "views/input_modal.hpp"
#include "window_dock.hpp"
#include <SDL.h>
#include <memory>

namespace freenamp::frontend {

class GuiEngine {
public:
    GuiEngine(int width = 680, int height = 480);
    ~GuiEngine();

    bool init();
    void run(backend::CoreController& core);
    void shutdown();

private:
    int m_width = 680;
    int m_height = 480;
    bool m_running = false;

    SDL_Window* m_window = nullptr;
    SDL_Renderer* m_renderer = nullptr;

    MainView m_main_view{ 20, 20 };
    EqView m_eq_view{ 20, 142 };
    PlaylistView m_playlist_view{ 310, 20, 350, 238 };
    InputModal m_input_modal;

    void process_events(backend::CoreController& core);
    void render(backend::CoreController& core);
};

} // namespace freenamp::frontend
