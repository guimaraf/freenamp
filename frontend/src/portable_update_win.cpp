#include "portable_update_win.hpp"
#include "windows_command_line.hpp"
#include "yt_resolver.hpp"

#include <SDL.h>
#include <nlohmann/json.hpp>
#include <chrono>
#include <algorithm>
#include <fstream>
#include <stdexcept>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace freenamp::frontend {
namespace {

constexpr auto last_update_file = L".freenamp-last-update.json";

HANDLE launch_updater(const std::filesystem::path& install, const std::wstring& action,
                       const std::filesystem::path& status, const std::wstring& release_tag, DWORD parent = 0) {
    const auto script = install / L"freenamp-update.ps1";
    if (!std::filesystem::is_regular_file(script)) {
        throw std::runtime_error("Copie o pacote Windows completo para habilitar as atualizacoes.");
    }

    wchar_t system_dir[MAX_PATH]{};
    const auto length = GetSystemDirectoryW(system_dir, MAX_PATH);
    if (length == 0 || length >= MAX_PATH) throw std::runtime_error("PowerShell do Windows nao encontrado.");
    const auto powershell = std::filesystem::path(system_dir) / L"WindowsPowerShell/v1.0/powershell.exe";
    std::vector<std::wstring> options{
        powershell.wstring(), L"-NoProfile", L"-NonInteractive", L"-ExecutionPolicy", L"Bypass",
        L"-File", script.wstring(), L"-Action", action, L"-InstallDir", install.wstring(),
        L"-StatusFile", status.wstring(), L"-RunningVersion", L"" FREENAMP_VERSION
    };
    if (!release_tag.empty()) {
        options.emplace_back(L"-ReleaseTag");
        options.push_back(release_tag);
    }
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
        auto status = [](const nlohmann::json& value) -> Status {
            return {value.value("state", "error"), value.value("message", ""), value.value("version", "")};
        };
        Result result{status(data)};
        if (result.state == "checked") {
            result.app = status(data.at("app"));
            result.extractor = status(data.at("extractor"));
        }
        return result;
    } catch (...) {
        return {"error", "O atualizador terminou sem informar o resultado.", {}};
    }
}

PortableUpdate::Result PortableUpdate::run(const std::filesystem::path& install, const std::wstring& action,
                                           const std::atomic<bool>* cancelled, const std::wstring& release_tag) {
    wchar_t temp[MAX_PATH]{}, result_file[MAX_PATH]{};
    const auto length = GetTempPathW(MAX_PATH, temp);
    if (length == 0 || length >= MAX_PATH || !GetTempFileNameW(temp, L"frn", 0, result_file)) {
        return {"error", "Nao foi possivel criar o arquivo de status da atualizacao.", {}};
    }
    const std::filesystem::path status(result_file);
    Result result;
    try {
        HANDLE process = launch_updater(install, action, status, release_tag);
        DWORD wait;
        for (;;) {
            wait = WaitForSingleObject(process, 100);
            if (wait != WAIT_TIMEOUT) break;
            try {
                std::ifstream input(status, std::ios::binary);
                auto state = nlohmann::json::parse(input);
                if (state.value("state", "") == "progress") {
                    m_progress.store(std::clamp(state.value("progress", 0), 0, 99));
                }
            } catch (...) {}
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

void PortableUpdate::open_menu() {
    if (m_pending.valid()) return;
    m_progress.store(0);
    m_progress_message = "Verificando atualizacoes...";
    m_pending = std::async(std::launch::async, [this] {
        return run(m_install_dir, L"CheckUpdates", &m_cancelled);
    });
}

bool PortableUpdate::show_menu(SDL_Window* window, backend::YtResolver& resolver, const Result& result) {
    std::vector<SDL_MessageBoxButtonData> buttons{
        {SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "Fechar"},
    };
    const auto& app = result.app;
    if (result.extractor.state == "available") buttons.push_back({0, 1, "Atualizar YouTube"});
    if (app.state == "ready") buttons.push_back({0, 3, "Reiniciar e aplicar"});
    else if (app.state == "available") buttons.push_back({0, 2, "Baixar Freenamp"});

    std::string description = "Freenamp instalado: " FREENAMP_VERSION "\n";
    if (app.state == "ready") description += "Freenamp " + app.version + " pronto para instalar.";
    else if (app.state == "available") description += "Disponivel no GitHub: " + app.version;
    else description += app.message;
    if (!result.extractor.message.empty()) description += "\n" + result.extractor.message;
    const SDL_MessageBoxData dialog{
        SDL_MESSAGEBOX_INFORMATION, window, "Atualizacoes", description.c_str(),
        static_cast<int>(buttons.size()), buttons.data(), nullptr
    };
    int clicked = 0;
    if (SDL_ShowMessageBox(&dialog, &clicked) != 0) return false;
    if (clicked == 3 && app.state == "ready") {
        try {
            const auto result_file = m_install_dir / last_update_file;
            const auto release_tag = L"v" + std::wstring(app.version.begin(), app.version.end());
            HANDLE process = launch_updater(m_install_dir, L"ApplyApp", result_file, release_tag, GetCurrentProcessId());
            CloseHandle(process);
            return true;
        } catch (const std::exception& error) {
            show_message(window, error.what(), true);
        }
    } else if (clicked == 1 || clicked == 2) {
        m_progress.store(0);
        m_progress_message = clicked == 1 ? "Atualizando YouTube..." : "Atualizando Freenamp...";
        SDL_SetWindowTitle(window, clicked == 1 ? "Freenamp - Atualizando YouTube..." : "Freenamp - Buscando atualizacao...");
        m_pending = std::async(std::launch::async, [this, install = m_install_dir, clicked, version = app.version, &resolver] {
            if (clicked == 2) {
                const auto release_tag = L"v" + std::wstring(version.begin(), version.end());
                return run(install, L"StageApp", &m_cancelled, release_tag);
            }
            auto result = run(install, L"StageExtractor", &m_cancelled);
            if (result.state != "ready") return result;
            m_progress.store(90);
            resolver.run_while_idle([&] {
                if (m_cancelled.load()) {
                    result = {"error", "Atualizacao do YouTube cancelada.", {}};
                } else {
                    result = run(install, L"ApplyExtractor");
                }
            });
            return result;
        });
    }
    return false;
}

bool PortableUpdate::update(SDL_Window* window, backend::YtResolver& resolver) {
    if (!m_pending.valid() ||
        m_pending.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return false;
    try {
        auto result = m_pending.get();
        SDL_SetWindowTitle(window, "Freenamp");
        if (result.state == "checked") return show_menu(window, resolver, result);
        if (result.state == "ready") {
            result.app = result;
            return show_menu(window, resolver, result);
        }
        const auto message = result.message + (result.version.empty() ? "" : "\nVersao: " + result.version);
        show_message(window, message, result.state == "error");
    } catch (const std::exception& error) {
        SDL_SetWindowTitle(window, "Freenamp");
        show_message(window, error.what(), true);
    }
    return false;
}

void PortableUpdate::restore_result(SDL_Window* window) {
    const auto file = m_install_dir / last_update_file;
    std::error_code ignored;
    if (!std::filesystem::is_regular_file(file, ignored)) return;
    const auto result = read_result(file);
    if (result.state != "success" && result.state != "error") return;
    std::filesystem::remove(file, ignored);
    if (result.state == "success" && result.version == FREENAMP_VERSION) {
        show_message(window, "Freenamp atualizado para a versao " FREENAMP_VERSION ".");
    } else if (result.state == "success") {
        show_message(window, "O atualizador esperava a versao " + result.version +
                             ", mas o Freenamp iniciou na versao " FREENAMP_VERSION ".", true);
    } else {
        show_message(window, result.message, true);
    }
}

} // namespace freenamp::frontend
