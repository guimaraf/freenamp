#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <string>

typedef int (*FreenampRunFn)(int argc, char* argv[]);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return 1;

    std::wstring path(exePath);
    size_t lastSlash = path.find_last_of(L"\\/");
    std::wstring rootDir = (lastSlash != std::wstring::npos) ? path.substr(0, lastSlash) : L".";
    std::wstring coreDir = rootDir + L"\\core";

    // Set core directory as DLL search path so OS loader resolves SDL2.dll and libmpv-2.dll from core/
    SetDllDirectoryW(coreDir.c_str());

    HMODULE hMod = LoadLibraryW((coreDir + L"\\libfreenamp_core.dll").c_str());
    if (!hMod) {
        hMod = LoadLibraryW((coreDir + L"\\freenamp_core.dll").c_str());
    }
    if (!hMod) {
        hMod = LoadLibraryW((rootDir + L"\\libfreenamp_core.dll").c_str());
    }
    if (!hMod) {
        hMod = LoadLibraryW((rootDir + L"\\freenamp_core.dll").c_str());
    }
    if (!hMod) {
        MessageBoxW(NULL, L"Nao foi possivel carregar as bibliotecas em core/.\nVerifique a instalacao do Freenamp.", L"Freenamp", MB_OK | MB_ICONERROR);
        return 1;
    }

    FreenampRunFn run_fn = (FreenampRunFn)GetProcAddress(hMod, "freenamp_run");
    if (!run_fn) {
        MessageBoxW(NULL, L"Ponto de entrada 'freenamp_run' nao encontrado na biblioteca core.", L"Freenamp", MB_OK | MB_ICONERROR);
        return 1;
    }

    return run_fn(__argc, __argv);
}

int main(int argc, char* argv[]) {
    return WinMain(GetModuleHandle(NULL), NULL, GetCommandLineA(), SW_SHOWNORMAL);
}
