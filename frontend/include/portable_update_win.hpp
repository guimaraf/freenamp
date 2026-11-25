#pragma once

#include <filesystem>
#include <atomic>
#include <future>
#include <string>

struct SDL_Window;

namespace freenamp::backend { class YtResolver; }

namespace freenamp::frontend {

class PortableUpdate {
public:
    bool open_menu(SDL_Window* window, backend::YtResolver& resolver);
    void update(SDL_Window* window);
    void restore_result(SDL_Window* window);
    void shutdown() { m_cancelled.store(true); }

private:
    struct Result {
        std::string state;
        std::string message;
        std::string version;
    };

    std::filesystem::path m_install_dir = std::filesystem::current_path();
    std::atomic<bool> m_cancelled{false};
    std::future<Result> m_pending;
    bool m_ready = false;
    std::string m_message;

    static Result read_result(const std::filesystem::path& file);
    static Result run(const std::filesystem::path& install, const std::wstring& action,
                      const std::atomic<bool>* cancelled = nullptr);
    static void show_message(SDL_Window* window, const std::string& message, bool error = false);
};

} // namespace freenamp::frontend
