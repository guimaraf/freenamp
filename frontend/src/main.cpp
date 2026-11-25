#define SDL_MAIN_HANDLED
#include "core_controller.hpp"
#include "gui_engine.hpp"
#include <iostream>
#include <filesystem>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

int main_app(int argc, char* argv[]) {
#ifdef _WIN32
    // The portable session belongs to the running executable, including when started by a shortcut.
    std::wstring executable(32768, L'\0');
    const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
    if (length == 0 || length >= executable.size()) return 1;
    executable.resize(length);
    std::error_code error;
    std::filesystem::current_path(std::filesystem::path(executable).parent_path(), error);
    if (error) return 1;
    const auto extractor = (std::filesystem::current_path() / L"bin/yt-dlp.exe").u8string();
    freenamp::backend::CoreController core(std::string(extractor.begin(), extractor.end()));
#else
    freenamp::backend::CoreController core("bin/yt-dlp");
#endif

    freenamp::frontend::GuiEngine gui(680, 480);
    if (!gui.init()) {
        return 1;
    }

    gui.run(core, argc > 1 ? argv[1] : "");
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
