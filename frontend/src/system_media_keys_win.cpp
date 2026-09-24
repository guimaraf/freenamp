#include "system_media_keys.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <SDL_syswm.h>
#include <SDL_system.h>
#include <chrono>
#include <iostream>

#ifndef MOD_NOREPEAT
#define MOD_NOREPEAT 0x4000
#endif

#ifndef GET_APPCOMMAND_LPARAM
#define GET_APPCOMMAND_LPARAM(lParam) ((short)(HIWORD(lParam) & ~FAPPCOMMAND_MASK))
#endif

#ifndef FAPPCOMMAND_MASK
#define FAPPCOMMAND_MASK 0xF000
#endif

#ifndef APPCOMMAND_MEDIA_NEXTTRACK
#define APPCOMMAND_MEDIA_NEXTTRACK 11
#endif
#ifndef APPCOMMAND_MEDIA_PREVIOUSTRACK
#define APPCOMMAND_MEDIA_PREVIOUSTRACK 12
#endif
#ifndef APPCOMMAND_MEDIA_STOP
#define APPCOMMAND_MEDIA_STOP 13
#endif
#ifndef APPCOMMAND_MEDIA_PLAY_PAUSE
#define APPCOMMAND_MEDIA_PLAY_PAUSE 14
#endif
#ifndef APPCOMMAND_MEDIA_PLAY
#define APPCOMMAND_MEDIA_PLAY 46
#endif
#ifndef APPCOMMAND_MEDIA_PAUSE
#define APPCOMMAND_MEDIA_PAUSE 47
#endif

namespace freenamp::frontend {

class SystemMediaKeysWin final : public ISystemMediaKeys {
public:
    static constexpr int HK_PLAY_PAUSE = 1001;
    static constexpr int HK_NEXT       = 1002;
    static constexpr int HK_PREV       = 1003;
    static constexpr int HK_STOP       = 1004;

    SystemMediaKeysWin() = default;
    ~SystemMediaKeysWin() override {
        shutdown();
    }

    bool init(SDL_Window* window, backend::CoreController& core) override {
        if (!window) return false;
        m_core = &core;

        SDL_SysWMinfo wmInfo;
        SDL_VERSION(&wmInfo.version);
        if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
            std::cerr << "[SystemMediaKeysWin] Falha ao obter SDL_SysWMinfo: " << SDL_GetError() << "\n";
            return false;
        }

        m_hwnd = wmInfo.info.win.window;
        if (!m_hwnd) {
            std::cerr << "[SystemMediaKeysWin] HWND nulo retornado por SDL_GetWindowWMInfo\n";
            return false;
        }

        register_keys();
        SDL_SetWindowsMessageHook(win32_msg_hook, this);
        m_initialized = true;
        return true;
    }

    void update() override {
        // Nada periódico no Windows: os eventos são processados pelo hook nativo de mensagens
    }

    void update_metadata(const std::string& /*title*/,
                         const std::string& /*artist*/,
                         int /*duration_sec*/,
                         backend::PlaybackState /*state*/) override {
        // Reservado para futura expansão com WinRT SMTC
    }

    void shutdown() override {
        if (m_initialized) {
            SDL_SetWindowsMessageHook(nullptr, nullptr);
            unregister_keys();
            m_initialized = false;
            m_hwnd = nullptr;
            m_core = nullptr;
        }
    }

    void handle_hotkey(int hotkey_id) {
        if (!m_core || !check_debounce()) return;

        switch (hotkey_id) {
            case HK_PLAY_PAUSE:
                m_core->toggle_pause();
                break;
            case HK_NEXT:
                m_core->next();
                break;
            case HK_PREV:
                m_core->previous();
                break;
            case HK_STOP:
                m_core->stop();
                break;
            default:
                break;
        }
    }

    bool handle_app_command(int cmd) {
        if (!m_core || !check_debounce()) return false;

        switch (cmd) {
            case APPCOMMAND_MEDIA_PLAY_PAUSE:
                m_core->toggle_pause();
                return true;
            case APPCOMMAND_MEDIA_PLAY:
                m_core->play();
                return true;
            case APPCOMMAND_MEDIA_PAUSE:
                m_core->pause();
                return true;
            case APPCOMMAND_MEDIA_STOP:
                m_core->stop();
                return true;
            case APPCOMMAND_MEDIA_NEXTTRACK:
                m_core->next();
                return true;
            case APPCOMMAND_MEDIA_PREVIOUSTRACK:
                m_core->previous();
                return true;
            default:
                return false;
        }
    }

private:
    HWND m_hwnd = nullptr;
    backend::CoreController* m_core = nullptr;
    bool m_initialized = false;
    std::chrono::steady_clock::time_point m_last_action = std::chrono::steady_clock::now();

    bool check_debounce() {
        auto now = std::chrono::steady_clock::now();
        auto diff = std::chrono::duration_cast<std::chrono::milliseconds>(now - m_last_action).count();
        if (diff < 150) {
            return false;
        }
        m_last_action = now;
        return true;
    }

    void register_keys() {
        if (!m_hwnd) return;
        // Registro com MOD_NOREPEAT para evitar rajadas ao manter a tecla pressionada
        if (!RegisterHotKey(m_hwnd, HK_PLAY_PAUSE, MOD_NOREPEAT, VK_MEDIA_PLAY_PAUSE)) {
            RegisterHotKey(m_hwnd, HK_PLAY_PAUSE, 0, VK_MEDIA_PLAY_PAUSE);
        }
        if (!RegisterHotKey(m_hwnd, HK_NEXT, MOD_NOREPEAT, VK_MEDIA_NEXT_TRACK)) {
            RegisterHotKey(m_hwnd, HK_NEXT, 0, VK_MEDIA_NEXT_TRACK);
        }
        if (!RegisterHotKey(m_hwnd, HK_PREV, MOD_NOREPEAT, VK_MEDIA_PREV_TRACK)) {
            RegisterHotKey(m_hwnd, HK_PREV, 0, VK_MEDIA_PREV_TRACK);
        }
        if (!RegisterHotKey(m_hwnd, HK_STOP, MOD_NOREPEAT, VK_MEDIA_STOP)) {
            RegisterHotKey(m_hwnd, HK_STOP, 0, VK_MEDIA_STOP);
        }
    }

    void unregister_keys() {
        if (!m_hwnd) return;
        UnregisterHotKey(m_hwnd, HK_PLAY_PAUSE);
        UnregisterHotKey(m_hwnd, HK_NEXT);
        UnregisterHotKey(m_hwnd, HK_PREV);
        UnregisterHotKey(m_hwnd, HK_STOP);
    }

    static void SDLCALL win32_msg_hook(void* userdata, void* /*hWnd*/, unsigned int message, Uint64 wParam, Sint64 lParam) {
        auto* self = static_cast<SystemMediaKeysWin*>(userdata);
        if (!self) return;

        if (message == WM_HOTKEY) {
            self->handle_hotkey(static_cast<int>(wParam));
        } else if (message == WM_APPCOMMAND) {
            int cmd = GET_APPCOMMAND_LPARAM(lParam);
            self->handle_app_command(cmd);
        }
    }
};

std::unique_ptr<ISystemMediaKeys> create_system_media_keys() {
    return std::make_unique<SystemMediaKeysWin>();
}

} // namespace freenamp::frontend
#endif
