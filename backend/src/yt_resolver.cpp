#include "yt_resolver.hpp"
#include "session_file.hpp"
#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <array>
#include <fstream>
#include <chrono>
#include <ctime>
#include <cctype>
#include <cerrno>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wininet.h>
#include "windows_command_line.hpp"
#else
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
extern char** environ;
#endif

#ifndef YTDLP_COMPILED_VERSION
#define YTDLP_COMPILED_VERSION "0000.00.00"
#endif

#ifndef YTDLP_COMPILED_HASH
#define YTDLP_COMPILED_HASH "unknown"
#endif

namespace freenamp::backend {

using json = nlohmann::json;

namespace {

thread_local const char* last_resolver_error = nullptr;

bool is_video_id(const std::string& value) {
    return value.size() == 11 && value.find_first_not_of(
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-") == std::string::npos;
}

std::string trim(const std::string& str) {
    auto start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    auto end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
}

#ifdef _WIN32
std::wstring to_wide_string(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), NULL, 0);
    std::wstring wstr(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &str[0], (int)str.size(), &wstr[0], size_needed);
    return wstr;
}
#endif

} // namespace

YtResolver::YtResolver(std::string ytdlp_path)
    : m_ytdlp_path(std::move(ytdlp_path)) {
    if (std::filesystem::path(std::u8string(m_ytdlp_path.begin(), m_ytdlp_path.end())).is_absolute()) return;
    // Check executable directory first on Windows
#ifdef _WIN32
    wchar_t exePath[MAX_PATH];
    if (GetModuleFileNameW(NULL, exePath, MAX_PATH) > 0) {
        std::filesystem::path exeDir = std::filesystem::path(exePath).parent_path();
        std::vector<std::filesystem::path> win_paths = {
            exeDir / "bin" / "yt-dlp.exe",
            exeDir / "yt-dlp.exe",
            exeDir / "compile" / "bin" / "yt-dlp.exe",
            exeDir / ".." / "compile" / "bin" / "yt-dlp.exe",
            exeDir / ".." / ".." / "compile" / "bin" / "yt-dlp.exe",
            exeDir / ".." / "bin" / "yt-dlp.exe"
        };
        for (const auto& p : win_paths) {
            if (std::filesystem::exists(p)) {
                const auto utf8 = p.u8string();
                m_ytdlp_path.assign(utf8.begin(), utf8.end());
                return;
            }
        }
    }
#else
    // On Linux / POSIX, resolve via /proc/self/exe
    std::error_code ec;
    auto exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
    if (!ec) {
        std::filesystem::path exeDir = exePath.parent_path();
        std::vector<std::filesystem::path> posix_paths = {
            exeDir / "bin" / "yt-dlp",
            exeDir / "yt-dlp",
            exeDir / "compile" / "bin" / "yt-dlp",
            exeDir / ".." / "compile" / "bin" / "yt-dlp",
            exeDir / ".." / ".." / "compile" / "bin" / "yt-dlp",
            exeDir / ".." / "bin" / "yt-dlp"
        };
        for (const auto& p : posix_paths) {
            if (std::filesystem::exists(p)) {
                m_ytdlp_path = p.string();
                std::error_code pec;
                std::filesystem::permissions(p,
                    std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
                    std::filesystem::perm_options::add, pec);
                return;
            }
        }
    }
#endif

    // Fallback search paths relative to CWD
    bool need_search = !std::filesystem::exists(m_ytdlp_path);
#ifndef _WIN32
    if (!need_search && m_ytdlp_path.size() >= 4 &&
        m_ytdlp_path.substr(m_ytdlp_path.size() - 4) == ".exe") {
        need_search = true;
    }
#endif

    if (need_search) {
        std::vector<std::string> search_paths = {
#ifdef _WIN32
            "bin/yt-dlp.exe",
            "yt-dlp.exe",
            "compile/bin/yt-dlp.exe",
            "../compile/bin/yt-dlp.exe",
            "../bin/yt-dlp.exe",
            "yt-dlp"
#else
            "bin/yt-dlp",
            "yt-dlp",
            "compile/bin/yt-dlp",
            "../compile/bin/yt-dlp",
            "../bin/yt-dlp",
            "/usr/local/bin/yt-dlp",
            "/usr/bin/yt-dlp"
#endif
        };
        for (const auto& path : search_paths) {
            if (std::filesystem::exists(path)) {
                m_ytdlp_path = path;
#ifndef _WIN32
                std::error_code pec;
                std::filesystem::permissions(path,
                    std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
                    std::filesystem::perm_options::add, pec);
#endif
                break;
            }
        }
    } else {
#ifndef _WIN32
        std::error_code pec;
        std::filesystem::permissions(m_ytdlp_path,
            std::filesystem::perms::owner_exec | std::filesystem::perms::group_exec | std::filesystem::perms::others_exec,
            std::filesystem::perm_options::add, pec);
#endif
    }
}

std::string YtResolver::sanitize_url(const std::string& input) {
    if (input.empty()) return "";

    // 1. Strip non-printable ASCII and control codes (like \x16 from Ctrl+V)
    std::string clean;
    clean.reserve(input.size());
    for (unsigned char c : input) {
        if (c >= 32 && c <= 126) {
            clean += static_cast<char>(c);
        }
    }

    // 2. Trim whitespace
    size_t start = clean.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = clean.find_last_not_of(" \t\r\n");
    clean = clean.substr(start, end - start + 1);

    // 3. Handle duplicate paste (e.g. "https://...https://...")
    size_t second_http = std::min(clean.find("https://", 8), clean.find("http://", 8));
    if (second_http != std::string::npos) {
        clean = clean.substr(0, second_http);
        end = clean.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) clean = clean.substr(0, end + 1);
    }

    return clean;
}

bool YtResolver::is_stream_url_expired(const std::string& stream_url) {
    if (stream_url.empty()) return true;

    // Check expire= parameter in googlevideo URL
    size_t pos = stream_url.find("expire=");
    if (pos != std::string::npos) {
        try {
            size_t start_val = pos + 7;
            size_t end_pos = stream_url.find('&', start_val);
            std::string expire_str = (end_pos != std::string::npos)
                ? stream_url.substr(start_val, end_pos - start_val)
                : stream_url.substr(start_val);
            long long expire_time = std::stoll(expire_str);
            auto now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            // Expired or expiring within the next 2 minutes (120 seconds)
            if (now + 120 >= expire_time) {
                return true;
            }
            return false;
        } catch (...) {
            return true;
        }
    }
    return false;
}

UrlType YtResolver::detect_url_type(const std::string& raw_input) {
    const auto input = sanitize_url(raw_input);
    if (is_video_id(input)) return UrlType::SingleVideo;
    const size_t scheme_end = input.find("://");
    if (scheme_end == std::string::npos ||
        (input.substr(0, scheme_end) != "http" && input.substr(0, scheme_end) != "https")) return UrlType::Unknown;

    const size_t host_start = scheme_end + 3;
    const size_t path_start = input.find('/', host_start);
    if (path_start == std::string::npos) return UrlType::Unknown;
    auto host = input.substr(host_start, path_start - host_start);
    std::transform(host.begin(), host.end(), host.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    const bool short_link = host == "youtu.be" || host == "www.youtu.be";
    if (!short_link && host != "youtube.com" && host != "www.youtube.com" &&
        host != "m.youtube.com" && host != "music.youtube.com") return UrlType::Unknown;

    const size_t end = input.find('#', path_start);
    const auto path_and_query = input.substr(path_start, end - path_start);
    const size_t question = path_and_query.find('?');
    const auto path = path_and_query.substr(0, question);
    const auto query = question == std::string::npos ? std::string{} : "&" + path_and_query.substr(question + 1);
    const size_t playlist = query.find("&list=");
    if (playlist != std::string::npos && playlist + 6 < query.size() && query[playlist + 6] != '&') {
        return UrlType::Playlist;
    }
    if ((short_link && is_video_id(path.substr(1))) ||
        (!short_link && path == "/watch" && query.find("&v=") != std::string::npos)) return UrlType::SingleVideo;
    for (const std::string prefix : {"/shorts/", "/live/", "/embed/", "/v/"}) {
        if (!short_link && path.starts_with(prefix) && is_video_id(path.substr(prefix.size()))) return UrlType::SingleVideo;
    }
    return UrlType::Unknown;
}

std::string YtResolver::format_duration(int total_seconds) {
    if (total_seconds < 0) total_seconds = 0;
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    std::ostringstream oss;
    if (hours > 0) {
        oss << hours << ":"
            << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;
    } else {
        oss << std::setfill('0') << std::setw(2) << minutes << ":"
            << std::setfill('0') << std::setw(2) << seconds;
    }
    return oss.str();
}

namespace {

struct ProcessOutput {
    std::string text;
    std::string errors;
    bool started = false;
    bool timed_out = false;
    bool cancelled = false;
    bool success = false;
};

constexpr size_t output_limit = 4 * 1024 * 1024;

#ifdef _WIN32
class Handle {
public:
    HANDLE value = nullptr;
    ~Handle() { reset(); }
    void reset() {
        if (value && value != INVALID_HANDLE_VALUE) CloseHandle(value);
        value = nullptr;
    }
};

bool create_capture(Handle& reader, Handle& writer, SECURITY_ATTRIBUTES& security) {
    return CreatePipe(&reader.value, &writer.value, &security, 0) &&
        SetHandleInformation(reader.value, HANDLE_FLAG_INHERIT, 0);
}

ProcessOutput run_process(const std::vector<std::string>& args, const std::atomic<bool>& cancelled) {
    ProcessOutput result;
    SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES), nullptr, TRUE};
    Handle out_read, out_write, err_read, err_write, input, process, thread;
    if (!create_capture(out_read, out_write, security) ||
        !create_capture(err_read, err_write, security)) return result;
    input.value = CreateFileW(L"NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (input.value == INVALID_HANDLE_VALUE) return result;

    STARTUPINFOW startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESTDHANDLES;
    startup.hStdInput = input.value;
    startup.hStdOutput = out_write.value;
    startup.hStdError = err_write.value;
    PROCESS_INFORMATION info{};
    auto executable = to_wide_string(args.front());
    std::wstring command;
    for (const auto& argument : args) {
        if (!command.empty()) command += L' ';
        command += quote_windows_argument(to_wide_string(argument));
    }
    if (!CreateProcessW(executable.c_str(), command.data(), nullptr, nullptr, TRUE,
            CREATE_NO_WINDOW, nullptr, nullptr, &startup, &info)) return result;
    result.started = true;
    process.value = info.hProcess;
    thread.value = info.hThread;
    out_write.reset();
    err_write.reset();

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    auto drain = [](HANDLE handle, std::string& target) {
        char buffer[4096];
        for (int chunk = 0; chunk < 64; ++chunk) {
            DWORD pending = 0, bytes = 0;
            if (!PeekNamedPipe(handle, nullptr, 0, nullptr, &pending, nullptr) || !pending ||
                !ReadFile(handle, buffer, std::min<DWORD>(pending, sizeof(buffer)), &bytes, nullptr) || !bytes) break;
            target.append(buffer, std::min(static_cast<size_t>(bytes), output_limit - target.size()));
        }
    };
    for (;;) {
        drain(out_read.value, result.text);
        drain(err_read.value, result.errors);
        const DWORD state = WaitForSingleObject(process.value, 20);
        if (state == WAIT_OBJECT_0) {
            drain(out_read.value, result.text);
            drain(err_read.value, result.errors);
            DWORD code = 1;
            result.success = GetExitCodeProcess(process.value, &code) && code == 0;
            break;
        }
        result.cancelled = cancelled.load();
        result.timed_out = std::chrono::steady_clock::now() >= deadline;
        if (result.cancelled || result.timed_out || state == WAIT_FAILED) {
            TerminateProcess(process.value, 1);
            break;
        }
    }
    return result;
}
#else
class CapturePipe {
public:
    int fd[2]{-1, -1};
    bool open() {
        return pipe(fd) == 0 && fcntl(fd[0], F_SETFD, FD_CLOEXEC) == 0 &&
            fcntl(fd[1], F_SETFD, FD_CLOEXEC) == 0 && fcntl(fd[0], F_SETFL, O_NONBLOCK) == 0;
    }
    void close_writer() {
        if (fd[1] >= 0) close(fd[1]);
        fd[1] = -1;
    }
    ~CapturePipe() {
        if (fd[0] >= 0) close(fd[0]);
        close_writer();
    }
};

ProcessOutput run_process(const std::vector<std::string>& args, const std::atomic<bool>& cancelled) {
    ProcessOutput result;
    CapturePipe out, err;
    if (!out.open() || !err.open()) return result;

    std::vector<char*> argv;
    for (const auto& argument : args) argv.push_back(const_cast<char*>(argument.c_str()));
    argv.push_back(nullptr);

    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions) != 0) return result;
    pid_t pid = -1;
    int spawn_error = posix_spawn_file_actions_addopen(&actions, STDIN_FILENO, "/dev/null", O_RDONLY, 0);
    if (!spawn_error) spawn_error = posix_spawn_file_actions_adddup2(&actions, out.fd[1], STDOUT_FILENO);
    if (!spawn_error) spawn_error = posix_spawn_file_actions_adddup2(&actions, err.fd[1], STDERR_FILENO);
    if (!spawn_error) spawn_error = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    if (spawn_error) return result;
    out.close_writer();
    err.close_writer();
    result.started = true;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(45);
    auto drain = [](int fd, std::string& target) {
        char buffer[4096];
        for (int chunk = 0; chunk < 64; ++chunk) {
            auto bytes = read(fd, buffer, sizeof(buffer));
            if (bytes <= 0) break;
            target.append(buffer, std::min(static_cast<size_t>(bytes), output_limit - target.size()));
        }
    };
    for (;;) {
        drain(out.fd[0], result.text);
        drain(err.fd[0], result.errors);
        int code = 0;
        const auto reaped = waitpid(pid, &code, WNOHANG);
        if (reaped == pid) {
            drain(out.fd[0], result.text);
            drain(err.fd[0], result.errors);
            result.success = WIFEXITED(code) && WEXITSTATUS(code) == 0;
            break;
        }
        if (reaped < 0 && errno != EINTR) break;
        result.cancelled = cancelled.load();
        result.timed_out = std::chrono::steady_clock::now() >= deadline;
        if (result.cancelled || result.timed_out) {
            kill(pid, SIGKILL);
            while (waitpid(pid, &code, 0) < 0 && errno == EINTR) {}
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    return result;
}
#endif

const char* describe_process(const ProcessOutput& result) {
    if (result.cancelled) return "Resolucao cancelada";
    if (result.timed_out) return "Tempo limite do yt-dlp";
    if (!result.started) return "Nao foi possivel abrir o yt-dlp";
    auto errors = result.errors;
    std::transform(errors.begin(), errors.end(), errors.begin(),
        [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
    if (errors.find("failed to resolve") != std::string::npos ||
        errors.find("getaddrinfo") != std::string::npos) return "Falha de DNS ao acessar o YouTube";
    if (errors.find("no supported javascript runtime") != std::string::npos ||
        errors.find("challenge solving failed") != std::string::npos) return "Verifique o runtime JS (Deno) do yt-dlp";
    if (!result.success) {
        if (errors.find("video unavailable") != std::string::npos) return "Video indisponivel no YouTube";
        if (errors.find("sign in") != std::string::npos) return "O YouTube exige login para este video";
        if (errors.find("requested format") != std::string::npos) return "Formato de audio indisponivel";
        return "yt-dlp falhou. Verifique a versao e tente novamente";
    }
    return nullptr;
}

} // namespace

std::string YtResolver::run_extractor(const std::vector<std::string>& args) const {
    std::lock_guard<std::mutex> lock(m_process_mutex);
    if (m_cancelled.load()) {
        last_resolver_error = "Resolucao cancelada";
        return {};
    }
    std::vector<std::string> command{m_ytdlp_path, "--ignore-config"};
#ifdef _WIN32
    const auto deno = std::filesystem::path(to_wide_string(m_ytdlp_path)).parent_path() / L"deno.exe";
    std::error_code error;
    if (std::filesystem::is_regular_file(deno, error)) {
        const auto path = deno.u8string();
        command.emplace_back("--js-runtimes");
        command.emplace_back("deno:" + std::string(path.begin(), path.end()));
    }
#endif
    command.insert(command.end(), args.begin(), args.end());
    const auto result = run_process(command, m_cancelled);
    last_resolver_error = describe_process(result);
    return result.success ? result.text : std::string{};
}

std::string YtResolver::get_last_error() const {
    return last_resolver_error ? last_resolver_error : "";
}

void YtResolver::run_while_idle(const std::function<void()>& task) const {
    std::lock_guard<std::mutex> lock(m_process_mutex);
    task();
}

std::optional<TrackMetadata> YtResolver::resolve_track_info(const std::string& url_or_id, bool fetch_stream_url) {
    last_resolver_error = nullptr;
    std::string clean_url = sanitize_url(url_or_id);
    if (detect_url_type(clean_url) == UrlType::Unknown) {
        last_resolver_error = "Informe uma URL ou ID do YouTube";
        return std::nullopt;
    }

    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_metadata_cache.find(clean_url);
        if (it != m_metadata_cache.end()) {
            if (!fetch_stream_url) {
                return it->second;
            }
            if (it->second.is_resolved && !is_stream_url_expired(it->second.stream_url)) {
                return it->second;
            }
        }
    }

    std::vector<std::string> args = {
        "--dump-json",
        "--no-playlist",
        "--skip-download",
        "--socket-timeout", "10", "--retries", "1"
    };

    if (fetch_stream_url) {
        args.push_back("-f");
        args.push_back("ba/b");
    }

    args.emplace_back("--");
    args.push_back(is_video_id(clean_url) ? "https://www.youtube.com/watch?v=" + clean_url : clean_url);

    std::string raw_json = run_extractor(args);

    if (raw_json.empty()) {
        return std::nullopt;
    }

    // Isolate pure JSON string between first '{' and last '}'
    size_t first_brace = raw_json.find('{');
    if (first_brace != std::string::npos) {
        size_t last_brace = raw_json.rfind('}');
        if (last_brace != std::string::npos && last_brace >= first_brace) {
            raw_json = raw_json.substr(first_brace, last_brace - first_brace + 1);
        }
    }

    try {
        json j = json::parse(raw_json);
        TrackMetadata track;
        track.id = j.value("id", "");
        track.title = j.value("title", "Unknown Title");
        track.uploader = j.value("uploader", j.value("channel", "Unknown Artist"));
        track.duration_seconds = j.value("duration", 0);
        track.original_url = clean_url;

        if (fetch_stream_url && j.contains("url") && j["url"].is_string()) {
            track.stream_url = j["url"].get<std::string>();
            track.is_resolved = !track.stream_url.empty();
        }

        std::lock_guard<std::mutex> lock(m_cache_mutex);
        if (track.is_resolved) {
            m_stream_url_cache[track.id] = track.stream_url;
            m_stream_url_cache[clean_url] = track.stream_url;
        }
        m_metadata_cache[track.id] = track;
        m_metadata_cache[clean_url] = track;

        return track;
    } catch (const std::exception& e) {
        std::cerr << "[YtResolver] Erro ao parsear JSON de vídeo: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<std::string> YtResolver::resolve_stream_url(const std::string& video_id_or_url) {
    last_resolver_error = nullptr;
    std::string clean_url = sanitize_url(video_id_or_url);
    if (detect_url_type(clean_url) == UrlType::Unknown) {
        last_resolver_error = "Informe uma URL ou ID do YouTube";
        return std::nullopt;
    }

    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_stream_url_cache.find(clean_url);
        if (it != m_stream_url_cache.end()) {
            if (!is_stream_url_expired(it->second)) {
                return it->second;
            } else {
                m_stream_url_cache.erase(it);
            }
        }
    }

    std::string target_url = clean_url;
    // Canonicalize 11-char video ID to full URL for optimal yt-dlp extractor resolution
    if (clean_url.find("http://") != 0 && clean_url.find("https://") != 0 && clean_url.length() == 11) {
        target_url = "https://www.youtube.com/watch?v=" + clean_url;
    }

    std::vector<std::string> args = {
        "-f", "ba/b",
        "-g",
        "--no-playlist",
        "--socket-timeout", "10", "--retries", "1",
        "--", target_url
    };

    std::string output = run_extractor(args);

    std::istringstream iss(output);
    std::string line;
    std::string stream_url;
    while (std::getline(iss, line)) {
        line = trim(line);
        if (line.rfind("http://", 0) == 0 || line.rfind("https://", 0) == 0) {
            stream_url = line;
            break;
        }
    }

    if (!stream_url.empty()) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_stream_url_cache[clean_url] = stream_url;
        m_stream_url_cache[target_url] = stream_url;
        return stream_url;
    }

    return std::nullopt;
}

std::optional<PlaylistMetadata> YtResolver::resolve_playlist(const std::string& playlist_url) {
    last_resolver_error = nullptr;
    std::string clean_url = sanitize_url(playlist_url);
    if (detect_url_type(clean_url) != UrlType::Playlist) {
        last_resolver_error = "Informe uma playlist do YouTube";
        return std::nullopt;
    }

    std::vector<std::string> args = {
        "--flat-playlist",
        "--playlist-end", "50",
        "-J",
        "--skip-download",
        "--socket-timeout", "10", "--retries", "1",
        "--", clean_url
    };

    std::string raw_json = run_extractor(args);

    if (raw_json.empty()) {
        return std::nullopt;
    }

    // Isolate pure JSON string between first '{' and last '}'
    size_t first_brace = raw_json.find('{');
    if (first_brace != std::string::npos) {
        size_t last_brace = raw_json.rfind('}');
        if (last_brace != std::string::npos && last_brace >= first_brace) {
            raw_json = raw_json.substr(first_brace, last_brace - first_brace + 1);
        }
    }

    try {
        json j = json::parse(raw_json);
        PlaylistMetadata playlist;
        playlist.id = j.value("id", "");
        playlist.title = j.value("title", "Untitled Playlist");
        playlist.uploader = j.value("uploader", j.value("channel", "Unknown Channel"));

        if (j.contains("entries") && j["entries"].is_array()) {
            for (const auto& item : j["entries"]) {
                if (!item.is_object()) continue;
                TrackMetadata track;
                track.id = item.value("id", "");
                if (!is_video_id(track.id)) continue;
                track.title = item.value("title", "Unknown Title");
                track.uploader = item.value("uploader", item.value("channel", "Unknown Artist"));
                track.duration_seconds = item.value("duration", 0);
                track.original_url = "https://www.youtube.com/watch?v=" + track.id;
                track.is_resolved = false; // Resolved on-demand when about to play

                playlist.tracks.push_back(track);
                if (playlist.tracks.size() >= 50) {
                    break;
                }
            }
        }

        return playlist;
    } catch (const std::exception& e) {
        std::cerr << "[YtResolver] Erro ao parsear JSON de playlist: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::future<std::optional<TrackMetadata>> YtResolver::resolve_track_info_async(std::string url_or_id, bool fetch_stream_url) {
    return std::async(std::launch::async, [this, u = std::move(url_or_id), fetch_stream_url]() {
        return this->resolve_track_info(u, fetch_stream_url);
    });
}

std::future<std::optional<std::string>> YtResolver::resolve_stream_url_async(std::string video_id_or_url) {
    return std::async(std::launch::async, [this, u = std::move(video_id_or_url)]() {
        return this->resolve_stream_url(u);
    });
}

std::future<std::optional<PlaylistMetadata>> YtResolver::resolve_playlist_async(std::string playlist_url) {
    return std::async(std::launch::async, [this, u = std::move(playlist_url)]() {
        return this->resolve_playlist(u);
    });
}

void YtResolver::clear_cache() {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    m_stream_url_cache.clear();
    m_metadata_cache.clear();
}

void YtResolver::remove_from_cache(const std::string& id_or_url) {
    if (id_or_url.empty()) return;
    std::lock_guard<std::mutex> lock(m_cache_mutex);

    std::string matched_id;
    std::string matched_url;

    auto it_meta = m_metadata_cache.find(id_or_url);
    if (it_meta != m_metadata_cache.end()) {
        matched_id = it_meta->second.id;
        matched_url = it_meta->second.original_url;
    }

    for (auto it = m_metadata_cache.begin(); it != m_metadata_cache.end(); ) {
        bool match = (it->first == id_or_url) ||
                     (it->second.id == id_or_url) ||
                     (!it->second.original_url.empty() && it->second.original_url == id_or_url) ||
                     (!matched_id.empty() && (it->first == matched_id || it->second.id == matched_id)) ||
                     (!matched_url.empty() && (it->first == matched_url || it->second.original_url == matched_url));
        if (match) {
            if (matched_id.empty()) matched_id = it->second.id;
            if (matched_url.empty()) matched_url = it->second.original_url;
            it = m_metadata_cache.erase(it);
        } else {
            ++it;
        }
    }

    for (auto it = m_stream_url_cache.begin(); it != m_stream_url_cache.end(); ) {
        bool match = (it->first == id_or_url) ||
                     (!matched_id.empty() && it->first == matched_id) ||
                     (!matched_url.empty() && it->first == matched_url);
        if (match) {
            it = m_stream_url_cache.erase(it);
        } else {
            ++it;
        }
    }
}

bool YtResolver::has_cached_stream_url(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    auto it = m_stream_url_cache.find(id);
    if (it != m_stream_url_cache.end()) {
        return !is_stream_url_expired(it->second);
    }
    return false;
}

std::optional<std::string> YtResolver::get_cached_stream_url(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    auto it = m_stream_url_cache.find(id);
    if (it != m_stream_url_cache.end()) {
        if (!is_stream_url_expired(it->second)) {
            return it->second;
        }
    }
    return std::nullopt;
}

bool YtResolver::save_cache_to_file(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    try {
        json j;
        json meta = json::object();
        for (const auto& [k, v] : m_metadata_cache) {
            json item;
            item["id"] = v.id;
            item["title"] = v.title;
            item["uploader"] = v.uploader;
            item["duration"] = v.duration_seconds;
            item["original_url"] = v.original_url;
            if (!v.stream_url.empty() && !is_stream_url_expired(v.stream_url)) {
                item["stream_url"] = v.stream_url;
                item["is_resolved"] = v.is_resolved;
            } else {
                item["stream_url"] = "";
                item["is_resolved"] = false;
            }
            meta[k] = item;
        }
        j["metadata"] = meta;

        json streams = json::object();
        for (const auto& [k, v] : m_stream_url_cache) {
            if (!is_stream_url_expired(v)) {
                streams[k] = v;
            }
        }
        j["streams"] = streams;

        return write_session_file(filepath, j.dump(2));
    } catch (...) {
        return false;
    }
}

bool YtResolver::load_cache_from_file(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    try {
        std::ifstream ifs(filepath);
        if (!ifs.is_open()) return false;

        json j;
        ifs >> j;
        if (!j.is_object()) return false;

        if (j.contains("metadata") && j["metadata"].is_object()) {
            for (auto& el : j["metadata"].items()) {
                TrackMetadata tr;
                tr.id = el.value().value("id", "");
                tr.title = el.value().value("title", "Unknown Title");
                tr.uploader = el.value().value("uploader", "Unknown Artist");
                tr.duration_seconds = el.value().value("duration", 0);
                tr.original_url = el.value().value("original_url", "");
                std::string st = el.value().value("stream_url", "");
                if (!st.empty() && !is_stream_url_expired(st)) {
                    tr.stream_url = st;
                    tr.is_resolved = el.value().value("is_resolved", false);
                } else {
                    tr.stream_url = "";
                    tr.is_resolved = false;
                }
                m_metadata_cache[el.key()] = tr;
            }
        }

        if (j.contains("streams") && j["streams"].is_object()) {
            for (auto& el : j["streams"].items()) {
                if (el.value().is_string()) {
                    std::string st = el.value().get<std::string>();
                    if (!is_stream_url_expired(st)) {
                        m_stream_url_cache[el.key()] = st;
                    }
                }
            }
        }

        return true;
    } catch (...) {
        return false;
    }
}

std::string YtResolver::get_compiled_version() {
    return std::string(YTDLP_COMPILED_VERSION);
}

std::string YtResolver::get_compiled_hash() {
    return std::string(YTDLP_COMPILED_HASH);
}

std::string YtResolver::get_local_version() const {
    std::string out = run_extractor({"--version"});
    std::istringstream iss(out);
    std::string first_line;
    if (std::getline(iss, first_line)) {
        return trim(first_line);
    }
    return "";
}

bool YtResolver::is_version_newer(const std::string& candidate, const std::string& baseline) {
    auto parse_parts = [](const std::string& ver) -> std::vector<long long> {
        std::vector<long long> parts;
        std::string s = trim(ver);
        if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
            s = s.substr(1);
        }
        std::istringstream ss(s);
        std::string token;
        while (std::getline(ss, token, '.')) {
            if (token.empty()) break;
            try {
                parts.push_back(std::stoll(token));
            } catch (...) {
                break;
            }
        }
        return parts;
    };

    auto cand_parts = parse_parts(candidate);
    auto base_parts = parse_parts(baseline);
    if (cand_parts.size() < 3 || base_parts.size() < 3) {
        return false;
    }

    while (cand_parts.size() < 4) cand_parts.push_back(0);
    while (base_parts.size() < 4) base_parts.push_back(0);

    for (size_t i = 0; i < 4; ++i) {
        if (cand_parts[i] > base_parts[i]) return true;
        if (cand_parts[i] < base_parts[i]) return false;
    }
    return false;
}

std::string YtResolver::fetch_remote_latest_version(bool nightly_channel) const {
    const char* api_url = nightly_channel
        ? "https://api.github.com/repos/yt-dlp/yt-dlp-nightly-builds/releases/latest"
        : "https://api.github.com/repos/yt-dlp/yt-dlp/releases/latest";

    std::string raw_response;

#ifdef _WIN32
    HINTERNET hInternet = InternetOpenA("Freenamp/1.0", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
    if (hInternet) {
        DWORD timeout_ms = 5000;
        InternetSetOptionA(hInternet, INTERNET_OPTION_CONNECT_TIMEOUT, &timeout_ms, sizeof(timeout_ms));
        InternetSetOptionA(hInternet, INTERNET_OPTION_RECEIVE_TIMEOUT, &timeout_ms, sizeof(timeout_ms));

        const char* headers = "Accept: application/vnd.github+json\r\nUser-Agent: Freenamp/1.0\r\n";
        HINTERNET hConnect = InternetOpenUrlA(
            hInternet,
            api_url,
            headers,
            static_cast<DWORD>(-1L),
            INTERNET_FLAG_RELOAD | INTERNET_FLAG_SECURE | INTERNET_FLAG_NO_CACHE_WRITE,
            0
        );

        if (hConnect) {
            char buffer[4096];
            DWORD bytesRead = 0;
            while (InternetReadFile(hConnect, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
                raw_response.append(buffer, bytesRead);
                if (raw_response.size() > 65536) break; // Safety cap
            }
            InternetCloseHandle(hConnect);
        }
        InternetCloseHandle(hInternet);
    }
#else
    const auto response = run_process({"curl", "-fsL", "--max-time", "5", "-A", "Freenamp/1.0", api_url}, m_cancelled);
    if (response.success) raw_response = response.text;
#endif

    if (raw_response.empty()) return "";

    try {
        json j = json::parse(raw_response);
        if (j.contains("tag_name") && j["tag_name"].is_string()) {
            return trim(j["tag_name"].get<std::string>());
        }
    } catch (...) {}

    return "";
}

bool YtResolver::check_for_update() const {
    const auto installed = get_local_version();
    const auto latest = fetch_remote_latest_version(true);
    return !installed.empty() && !latest.empty() && is_version_newer(latest, installed);
}

bool YtResolver::update_ytdlp_binary() {
    run_extractor({"--update-to", "nightly"});
    return !last_resolver_error && !get_local_version().empty();
}

} // namespace freenamp::backend

