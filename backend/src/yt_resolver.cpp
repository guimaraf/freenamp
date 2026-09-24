#include "yt_resolver.hpp"
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

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>
#else
#include <cstdio>
#include <unistd.h>
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
                m_ytdlp_path = p.string();
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
    size_t second_http = clean.find("http", 4);
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
    std::string input = sanitize_url(raw_input);
    if (input.empty()) return UrlType::Unknown;

    // Any URL with a playlist parameter (list=, playlist?list=, Mix / Radio) is a Playlist
    if (input.find("list=") != std::string::npos) {
        return UrlType::Playlist;
    }

    // Single video formats
    if (input.find("watch?v=") != std::string::npos ||
        input.find("youtu.be/") != std::string::npos ||
        input.find("youtube.com/shorts/") != std::string::npos) {
        return UrlType::SingleVideo;
    }

    // 11-character video ID
    if (input.length() == 11 && input.find(' ') == std::string::npos && input.find('/') == std::string::npos) {
        return UrlType::SingleVideo;
    }

    // Fallback: If it starts with http, treat as single video
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

    HANDLE hChildStd_ERR = CreateFileW(
        L"NUL",
        GENERIC_WRITE,
        FILE_SHARE_WRITE,
        &saAttr,
        OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL,
        NULL
    );

    PROCESS_INFORMATION piProcInfo;
    STARTUPINFOW siStartInfo;
    ZeroMemory(&piProcInfo, sizeof(PROCESS_INFORMATION));
    ZeroMemory(&siStartInfo, sizeof(STARTUPINFOW));

    siStartInfo.cb = sizeof(STARTUPINFOW);
    siStartInfo.hStdError = (hChildStd_ERR != INVALID_HANDLE_VALUE) ? hChildStd_ERR : NULL;
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
    if (hChildStd_ERR != INVALID_HANDLE_VALUE && hChildStd_ERR != NULL) {
        CloseHandle(hChildStd_ERR);
    }

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
    std::string safe_cmd = cmd_line + " 2>/dev/null";
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
    std::string clean_url = sanitize_url(url_or_id);
    if (clean_url.empty()) return std::nullopt;

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
        "--no-warnings"
    };

    if (fetch_stream_url) {
        args.push_back("-f");
        args.push_back("ba/b");
    }

    args.push_back(clean_url);

    std::string cmd = build_command_line(args);
    std::string raw_json = execute_command(cmd);

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
    std::string clean_url = sanitize_url(video_id_or_url);
    if (clean_url.empty()) return std::nullopt;

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
        "--no-warnings",
        target_url
    };

    std::string cmd = build_command_line(args);
    std::string output = execute_command(cmd);

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
    std::string clean_url = sanitize_url(playlist_url);
    if (clean_url.empty()) return std::nullopt;

    std::vector<std::string> args = {
        "--flat-playlist",
        "--playlist-end", "50",
        "-J",
        "--skip-download",
        "--no-warnings",
        clean_url
    };

    std::string cmd = build_command_line(args);
    std::string raw_json = execute_command(cmd);

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
                TrackMetadata track;
                track.id = item.value("id", "");
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

        std::ofstream ofs(filepath);
        if (!ofs.is_open()) return false;
        ofs << j.dump(2);
        return true;
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
    std::string cmd = build_command_line({"--version"});
    std::string out = execute_command(cmd);
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

std::string YtResolver::fetch_remote_latest_version(bool master_channel) const {
    const char* api_url = master_channel
        ? "https://api.github.com/repos/yt-dlp/yt-dlp-master-builds/releases/latest"
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
    std::string curl_cmd = std::string("curl -sL --max-time 5 -A \"Freenamp/1.0\" \"") + api_url + "\"";
    raw_response = execute_command(curl_cmd);
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
    std::string compiled_ver = get_compiled_version();
    std::string local_ver = get_local_version();

    // Determine effective baseline version (compiled version or local binary version)
    std::string baseline = compiled_ver;
    if (baseline.empty() || baseline == "0000.00.00") {
        baseline = local_ver;
    } else if (!local_ver.empty() && is_version_newer(local_ver, baseline)) {
        baseline = local_ver;
    }

    if (baseline.empty() || baseline == "0000.00.00") {
        return false;
    }

    // Count dots: 3 dots (YYYY.MM.DD.HHMMSS) = master/nightly channel, 2 dots (YYYY.MM.DD) = stable channel
    size_t dot_count = std::count(baseline.begin(), baseline.end(), '.');
    bool is_master_channel = (dot_count >= 3);

    std::string remote_ver = fetch_remote_latest_version(is_master_channel);
    if (!remote_ver.empty() && is_version_newer(remote_ver, baseline)) {
        return true;
    }

    // If on master channel, also check stable release just in case
    if (is_master_channel) {
        std::string stable_ver = fetch_remote_latest_version(false);
        if (!stable_ver.empty() && is_version_newer(stable_ver, baseline)) {
            return true;
        }
    }

    return false;
}

bool YtResolver::update_ytdlp_binary() {
    std::string cmd = build_command_line({"-U"});
    std::string out = execute_command(cmd);
    return !out.empty();
}

} // namespace freenamp::backend

