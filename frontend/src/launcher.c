#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wchar.h>

typedef int (*FreenampRunFn)(int argc, char* argv[]);

// Native compatibility exclusion for RivaTuner Statistics Server (RTSS)
// Tells RTSSHooks64.dll / RTSSHooks.dll not to hook this process
__declspec(dllexport) DWORD RTSSHooksCompatibility = 0x00000000;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) == 0) return 1;

    wchar_t* lastSlash = wcsrchr(exePath, L'\\');
    if (!lastSlash) {
        lastSlash = wcsrchr(exePath, L'/');
    }
    if (lastSlash) {
        *lastSlash = L'\0';
    } else {
        wcscpy(exePath, L".");
    }

    wchar_t coreDir[MAX_PATH];
    wsprintfW(coreDir, L"%s\\core", exePath);

    // Set core directory as DLL search path so OS loader resolves SDL2.dll, libmpv-2.dll and dependencies from core/
    SetDllDirectoryW(coreDir);

    wchar_t libPath[MAX_PATH];
    wsprintfW(libPath, L"%s\\libfreenamp_core.dll", coreDir);
    HMODULE hMod = LoadLibraryW(libPath);
    if (!hMod) {
        wsprintfW(libPath, L"%s\\freenamp_core.dll", coreDir);
        hMod = LoadLibraryW(libPath);
    }
    if (!hMod) {
        wsprintfW(libPath, L"%s\\libfreenamp_core.dll", exePath);
        hMod = LoadLibraryW(libPath);
    }
    if (!hMod) {
        wsprintfW(libPath, L"%s\\freenamp_core.dll", exePath);
        hMod = LoadLibraryW(libPath);
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
