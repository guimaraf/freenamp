#include "portable_update_win.hpp"
#include "windows_command_line.hpp"
#include "yt_resolver.hpp"

#include <SDL.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace freenamp::frontend {
namespace {

constexpr auto last_update_file = L".freenamp-last-update.json";

HANDLE launch_updater(const std::filesystem::path& install, const std::wstring& action,
                       const std::filesystem::path& status, DWORD parent = 0) {
    const auto script = install / L"freenamp-update.ps1";
    if (!std::filesystem::is_regular_file(script)) {
        throw std::runtime_error("Copie o pacote Windows completo para habilitar as atualizacoes.");
    }

    wchar_t system_dir[MAX_PATH]{};
    const auto length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) throw std::runtime_error("PowerShell do Windows nao encontrado.");
    const auto powershell = std::filesystem::path(system_dir) / L"WindowsPowerShell/v1.0/powershell.exe";
    const std::vector<std::wstring> options{
        powershell.wstring(), L"-NoProfile", L"-NonInteractive", L"-ExecutionPolicy", L"Bypass",
        L"-File", script.wstring(), L"-Action", action, L"-InstallDir", install.wstring(),
        L"-StatusFile", status.wstring()
    };
    std::wstring command;
    for (const auto& option : options) {
        if (!command.empty()) command += L' ';
        command += quote_windows_argument(option);
    }
    if (parent) command += L" -ParentPid " + std::to_wstring(parent);

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    if (!CreateProcessW(powershell.c_str(), command.data(), nullptr, nullptr, FALSE,
                       CREATE_NO_WINDOW, nullptr, install.c_str(), &startup, &process)) {
        throw std::runtime_error("Nao foi possivel iniciar o atualizador PowerShell.");
    }
    CloseHandle(process.hThread);
    return process.hProcess;
}

} // namespace

PortableUpdate::Result PortableUpdate::read_result(const std::filesystem::path& file) {
    try {
        std::ifstream input(file, std::ios::binary);
        auto data = nlohmann::json::parse(input);
        return {data.value("state", "error"), data.value("message", ""), data.value("version", "")};
    } catch (...) {
        return {"error", "O atualizador terminou sem informar o resultado.", {}};
    }
}

PortableUpdate::Result PortableUpdate::run(const std::filesystem::path& install, const std::wstring& action,
                                           const std::atomic<bool>* cancelled) {
    wchar_t temp[MAX_PATH]{}, result_file[MAX_PATH]{};
    const auto length = GetTempPathW(MAX_PATH, temp);
    if (length == 0 || length >= MAX_PATH || !GetTempFileNameW(temp, L"frn", 0, result_file)) {
        return {"error", "Nao foi possivel criar o arquivo de status da atualizacao.", {}};
    }
    const std::filesystem::path status(result_file);
    Result result;
    try {
        HANDLE process = launch_updater(install, action, status);
        DWORD wait;
        for (;;) {
            wait = WaitForSingleObject(process, 100);
            if (wait != WAIT_TIMEOUT) break;
            if (cancelled && cancelled->load()) {
                TerminateProcess(process, 1);
                WaitForSingleObject(process, 2000);
                break;
            }
        }
        DWORD code = 1;
        GetExitCodeProcess(process, &code);
        CloseHandle(process);
        result = read_result(status);
        if ((wait != WAIT_OBJECT_0 || code != 0) && result.state != "error") {
            result = {"error", "O processo do atualizador falhou.", {}};
        }
    } catch (const std::exception& error) {
        result = {"error", error.what(), {}};
    }
    std::error_code ignored;
    std::filesystem::remove(status, ignored);
    return result;
}

void PortableUpdate::show_message(SDL_Window* window, const std::string& message, bool error) {
    SDL_ShowSimpleMessageBox(error ? SDL_MESSAGEBOX_ERROR : SDL_MESSAGEBOX_INFORMATION,
                             "Atualizacoes do Freenamp", message.c_str(), window);
}

bool PortableUpdate::open_menu(SDL_Window* window, backend::YtResolver& resolver) {
    if (m_pending.valid()) {
        show_message(window, "O atualizador esta trabalhando em segundo plano.");
        return false;
    }

    constexpr SDL_MessageBoxButtonData buttons[] = {
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Fechar"},
        {0, 1, "Atualizar YouTube"},
        {0, 2, "Atualizar Freenamp"},
        {0, 3, "Reiniciar e aplicar"},
    };
    const std::string description = "Freenamp " FREENAMP_VERSION
        "  |  Atualizacoes para esta pasta portatil.\n"
        "O YouTube pode ser atualizado durante a reproducao.\n"
        "A instalacao de uma nova versao do Freenamp requer reiniciar."
        + (m_message.empty() ? std::string{} : "\n\n" + m_message);
    const SDL_MessageBoxData dialog{
        SDL_MESSAGEBOX_INFORMATION, window, "Atualizacoes", description.c_str(), m_ready ? 4 : 3, buttons, nullptr
    };
    int clicked = 0;
    if (SDL_ShowMessageBox(&dialog, &clicked) != 0) return false;
    if (clicked == 3 && m_ready) {
        try {
            const auto result_file = m_install_dir / last_update_file;
            HANDLE process = launch_updater(m_install_dir, L"ApplyApp", result_file, GetCurrentProcessId());
            CloseHandle(process);
            return true;
        } catch (const std::exception& error) {
            show_message(window, error.what(), true);
        }
    } else if (clicked == 1 || clicked == 2) {
        if (clicked == 2) m_ready = false;
        SDL_SetWindowTitle(window, clicked == 1 ? "Freenamp - Atualizando YouTube..." : "Freenamp - Buscando atualizacao...");
        m_pending = std::async(std::launch::async, [this, install = m_install_dir, clicked, &resolver] {
            if (clicked == 2) return run(install, L"StageApp", &m_cancelled);
            auto result = run(install, L"StageExtractor", &m_cancelled);
            if (result.state != "ready") return result;
            resolver.run_while_idle([&] {
                if (!m_cancelled.load()) result = run(install, L"ApplyExtractor");
            });
            return result;
        });
    }
    return false;
}

void PortableUpdate::update(SDL_Window* window) {
    if (!m_pending.valid() ||
        m_pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return;
    try {
        auto result = m_pending.get();
        if (result.state == "ready") m_ready = true;
        m_message = result.message + (result.version.empty() ? "" : "\nVersao: " + result.version);
        SDL_SetWindowTitle(window, m_ready ? "Freenamp - Atualizacao pronta (U)" : "Freenamp");
        show_message(window, m_message, result.state == "error");
    } catch (const std::exception& error) {
        SDL_SetWindowTitle(window, "Freenamp");
        show_message(window, error.what(), true);
    }
}

void PortableUpdate::restore_result(SDL_Window* window) {
    const auto file = m_install_dir / last_update_file;
    std::error_code ignored;
    if (!std::filesystem::is_regular_file(file, ignored)) return;
    const auto result = read_result(file);
    if (result.state != "success" && result.state != "error") return;
    std::filesystem::remove(file, ignored);
    show_message(window, result.message, result.state == "error");
}

} // namespace freenamp::frontend
