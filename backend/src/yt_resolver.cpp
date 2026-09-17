#include "yt_resolver.hpp"
#include <nlohmann/json.hpp>

#include <iostream>
#include <sstream>
#include <iomanip>
#include <filesystem>
#include <algorithm>
#include <array>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <cstdio>
#include <unistd.h>
#endif

namespace freenamp::backend {

using json = nlohmann::json;

namespace {

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
    // If default path does not exist, check if yt-dlp is in common portable locations
    if (!std::filesystem::exists(m_ytdlp_path)) {
        std::vector<std::string> search_paths = {
            "bin/yt-dlp.exe",
            "yt-dlp.exe",
            "compile/bin/yt-dlp.exe",
            "../compile/bin/yt-dlp.exe",
            "../bin/yt-dlp.exe",
            "yt-dlp"
        };
        for (const auto& path : search_paths) {
            if (std::filesystem::exists(path)) {
                m_ytdlp_path = path;
                break;
            }
        }
    }
}

UrlType YtResolver::detect_url_type(const std::string& input) {
    if (input.empty()) return UrlType::Unknown;

    // Check for playlist indicators
    if (input.find("list=") != std::string::npos ||
        input.find("playlist?list=") != std::string::npos) {
        return UrlType::Playlist;
    }

    // Check for single video indicators
    if (input.find("youtube.com/watch") != std::string::npos ||
        input.find("youtu.be/") != std::string::npos ||
        input.find("youtube.com/shorts/") != std::string::npos ||
        (input.length() == 11 && input.find(' ') == std::string::npos && input.find('/') == std::string::npos)) {
        return UrlType::SingleVideo;
    }

    // Fallback: If it's a URL, treat as single video unless confirmed otherwise
    if (input.find("http://") == 0 || input.find("https://") == 0) {
        return UrlType::SingleVideo;
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

std::string YtResolver::build_command_line(const std::vector<std::string>& args) const {
    std::string cmd = "\"" + m_ytdlp_path + "\"";
    for (const auto& arg : args) {
        cmd += " \"" + arg + "\"";
    }
    return cmd;
}

std::string YtResolver::execute_command(const std::string& cmd_line) const {
    std::lock_guard<std::mutex> proc_lock(m_process_mutex);
    std::string output;

#ifdef _WIN32
    SECURITY_ATTRIBUTES saAttr;
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.bInheritHandle = TRUE;
    saAttr.lpSecurityDescriptor = NULL;

    HANDLE hChildStd_OUT_Rd = NULL;
    HANDLE hChildStd_OUT_Wr = NULL;

    if (!CreatePipe(&hChildStd_OUT_Rd, &hChildStd_OUT_Wr, &saAttr, 0)) {
        return "";
    }

    if (!SetHandleInformation(hChildStd_OUT_Rd, HANDLE_FLAG_INHERIT, 0)) {
        CloseHandle(hChildStd_OUT_Rd);
        CloseHandle(hChildStd_OUT_Wr);
        return "";
    }

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOW siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));

    siStartInfo.cb = sizeof(STARTUPINFOW);
    siStartInfo.hStdError = hChildStd_OUT_Wr;
    siStartInfo.hStdOutput = hChildStd_OUT_Wr;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    siStartInfo.wShowWindow = SW_HIDE;

    std::wstring wcmd = to_wide_string(cmd_line);
    std::vector<wchar_t> cmdBuffer(wcmd.begin(), wcmd.end());
    cmdBuffer.push_back(L'\0');

    BOOL bSuccess = CreateProcessW(
        NULL,
        cmdBuffer.data(),
        NULL,
        NULL,
        TRUE,
        CREATE_NO_WINDOW,
        NULL,
        NULL,
        &siStartInfo,
        &piProcInfo
    );

    // Close the write handle in parent so ReadFile hits EOF when child exits
    CloseHandle(hChildStd_OUT_Wr);

    if (!bSuccess) {
        CloseHandle(hChildStd_OUT_Rd);
        return "";
    }

    DWORD dwRead;
    CHAR chBuf[4096];
    bSuccess = FALSE;

    for (;;) {
        bSuccess = ReadFile(hChildStd_OUT_Rd, chBuf, sizeof(chBuf), &dwRead, NULL);
        if (!bSuccess || dwRead == 0) break;
        output.append(chBuf, dwRead);
    }

    WaitForSingleObject(piProcInfo.hProcess, INFINITE);

    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);
    CloseHandle(hChildStd_OUT_Rd);

#else
    // POSIX fallback
    std::array<char, 4096> buffer;
    std::string safe_cmd = cmd_line + " 2>&1";
    FILE* pipe = popen(safe_cmd.c_str(), "r");
    if (!pipe) return "";

    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) {
        output += buffer.data();
    }
    pclose(pipe);
#endif

    return output;
}

std::optional<TrackMetadata> YtResolver::resolve_track_info(const std::string& url_or_id, bool fetch_stream_url) {
    if (url_or_id.empty()) return std::nullopt;

    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_metadata_cache.find(url_or_id);
        if (it != m_metadata_cache.end()) {
            if (!fetch_stream_url || it->second.is_resolved) {
                return it->second;
            }
        }
    }

    std::vector<std::string> args = {
        "--dump-json",
        "--no-playlist",
        "--skip-download",
        "--no-warnings"
    };

    if (fetch_stream_url) {
        args.push_back("-f");
        args.push_back("bestaudio[ext=opus]/bestaudio[ext=m4a]/bestaudio");
    }

    args.push_back(url_or_id);

    std::string cmd = build_command_line(args);
    std::string raw_json = execute_command(cmd);

    if (raw_json.empty()) {
        return std::nullopt;
    }

    try {
        json j = json::parse(raw_json);
        TrackMetadata track;
        track.id = j.value("id", "");
        track.title = j.value("title", "Unknown Title");
        track.uploader = j.value("uploader", j.value("channel", "Unknown Artist"));
        track.duration_seconds = j.value("duration", 0);
        track.original_url = url_or_id;

        if (fetch_stream_url && j.contains("url") && j["url"].is_string()) {
            track.stream_url = j["url"].get<std::string>();
            track.is_resolved = !track.stream_url.empty();
        }

        std::lock_guard<std::mutex> lock(m_cache_mutex);
        if (track.is_resolved) {
            m_stream_url_cache[track.id] = track.stream_url;
            m_stream_url_cache[url_or_id] = track.stream_url;
        }
        m_metadata_cache[track.id] = track;
        m_metadata_cache[url_or_id] = track;

        return track;
    } catch (const std::exception& e) {
        std::cerr << "[YtResolver] Erro ao parsear JSON de vídeo: " << e.what() << std::endl;
        return std::nullopt;
    }
}

std::optional<std::string> YtResolver::resolve_stream_url(const std::string& video_id_or_url) {
    if (video_id_or_url.empty()) return std::nullopt;

    {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        auto it = m_stream_url_cache.find(video_id_or_url);
        if (it != m_stream_url_cache.end()) {
            return it->second;
        }
    }

    std::vector<std::string> args = {
        "-f", "bestaudio[ext=opus]/bestaudio[ext=m4a]/bestaudio",
        "-g",
        "--no-playlist",
        "--no-warnings",
        video_id_or_url
    };

    std::string cmd = build_command_line(args);
    std::string output = trim(execute_command(cmd));

    if (output.find("http://") == 0 || output.find("https://") == 0) {
        std::lock_guard<std::mutex> lock(m_cache_mutex);
        m_stream_url_cache[video_id_or_url] = output;
        return output;
    }

    return std::nullopt;
}

std::optional<PlaylistMetadata> YtResolver::resolve_playlist(const std::string& playlist_url) {
    if (playlist_url.empty()) return std::nullopt;

    std::vector<std::string> args = {
        "--flat-playlist",
        "-J",
        "--skip-download",
        "--no-warnings",
        playlist_url
    };

    std::string cmd = build_command_line(args);
    std::string raw_json = execute_command(cmd);

    if (raw_json.empty()) {
        return std::nullopt;
    }

    try {
        json j = json::parse(raw_json);
        PlaylistMetadata playlist;
        playlist.id = j.value("id", "");
        playlist.title = j.value("title", "Untitled Playlist");
        playlist.uploader = j.value("uploader", j.value("channel", "Unknown Channel"));

        if (j.contains("entries") && j["entries"].is_array()) {
            for (const auto& item : j["entries"]) {
                TrackMetadata track;
                track.id = item.value("id", "");
                track.title = item.value("title", "Unknown Title");
                track.uploader = item.value("uploader", item.value("channel", "Unknown Artist"));
                track.duration_seconds = item.value("duration", 0);
                track.original_url = "https://www.youtube.com/watch?v=" + track.id;
                track.is_resolved = false; // Resolved on-demand when about to play

                playlist.tracks.push_back(track);
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

bool YtResolver::has_cached_stream_url(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    return m_stream_url_cache.find(id) != m_stream_url_cache.end();
}

std::optional<std::string> YtResolver::get_cached_stream_url(const std::string& id) const {
    std::lock_guard<std::mutex> lock(m_cache_mutex);
    auto it = m_stream_url_cache.find(id);
    if (it != m_stream_url_cache.end()) {
        return it->second;
    }
    return std::nullopt;
}

} // namespace freenamp::backend
