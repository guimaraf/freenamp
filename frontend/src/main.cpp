#define SDL_MAIN_HANDLED
#include "core_controller.hpp"
#include "gui_engine.hpp"
#include <iostream>

int main(int argc, char* argv[]) {
    std::cout << "===========================================\n";
    std::cout << " Freenamp - YouTube Retro Audio Player     \n";
    std::cout << "===========================================\n";

    freenamp::backend::CoreController core("compile/bin/yt-dlp.exe");

    // If an argument was passed on the command line (e.g. YouTube video or playlist URL)
    if (argc > 1) {
        std::string initial_url = argv[1];
        std::cout << "[Freenamp] Carregando URL inicial: " << initial_url << "\n";
        core.add_url(initial_url, true);
    }

    freenamp::frontend::GuiEngine gui(680, 480);
    if (!gui.init()) {
        std::cerr << "[Freenamp] Erro ao inicializar interface grafica.\n";
        return 1;
    }

    std::cout << "[Freenamp] Interface grafica SDL2 iniciada.\n";
    gui.run(core);

    std::cout << "[Freenamp] Encerrando Freenamp.\n";
    return 0;
}
