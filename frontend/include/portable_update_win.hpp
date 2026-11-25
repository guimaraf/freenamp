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
    void open_menu();
    bool update(SDL_Window* window, backend::YtResolver& resolver);
    void restore_result(SDL_Window* window);
    void shutdown() { m_cancelled.store(true); }
    bool is_busy() const { return m_pending.valid(); }
    int progress() const { return m_progress.load(); }
    std::string progress_message() const { return m_progress_message; }

private:
    struct Status {
        std::string state;
        std::string message;
        std::string version;
    };

    struct Result : Status {
        Status app;
        Status extractor;
    };

    std::filesystem::path m_install_dir = std::filesystem::current_path();
    std::atomic<bool> m_cancelled{false};
    std::atomic<int> m_progress{0};
    std::string m_progress_message;
    std::future<Result> m_pending;

    static Result read_result(const std::filesystem::path& file);
    bool show_menu(SDL_Window* window, backend::YtResolver& resolver, const Result& result);
    Result run(const std::filesystem::path& install, const std::wstring& action,
               const std::atomic<bool>* cancelled = nullptr, const std::wstring& release_tag = {});
    static void show_message(SDL_Window* window, const std::string& message, bool error = false);
};

} // namespace freenamp::frontend
