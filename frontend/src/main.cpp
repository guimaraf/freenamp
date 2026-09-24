#define SDL_MAIN_HANDLED
#include "core_controller.hpp"
#include "gui_engine.hpp"
#include <iostream>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int main_app(int argc, char* argv[]) {
#ifdef _WIN32
    freenamp::backend::CoreController core("compile/bin/yt-dlp.exe");
#else
    freenamp::backend::CoreController core("bin/yt-dlp");
#endif

    // If an argument was passed on the command line (e.g. YouTube video or playlist URL)
    if (argc > 1) {
        std::string initial_url = argv[1];
        core.add_url(initial_url, true);
    }

    freenamp::frontend::GuiEngine gui(680, 480);
    if (!gui.init()) {
        return 1;
    }

    gui.run(core);
    return 0;
}

#if defined(_WIN32)
// Native compatibility exclusion for RivaTuner Statistics Server (RTSS)
// Tells RTSSHooks64.dll / RTSSHooks.dll not to hook this process
extern "C" {
    __declspec(dllexport) unsigned long RTSSHooksCompatibility = 0x00000000;

    __declspec(dllexport) int freenamp_run(int argc, char* argv[]) {
        return main_app(argc, argv);
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    return main_app(__argc, __argv);
}
#endif

int main(int argc, char* argv[]) {
    return main_app(argc, argv);
}
