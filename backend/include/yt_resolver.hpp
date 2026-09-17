#pragma once

#include <string>
#include <vector>
#include <optional>
#include <future>
#include <unordered_map>
#include <mutex>

namespace freenamp::backend {

struct TrackMetadata {
    std::string id;
    std::string title;
    std::string uploader;
    int duration_seconds = 0;
    std::string stream_url;      // Direct Opus/AAC CDN URL
    bool is_resolved = false;     // True if stream_url is resolved and valid
    std::string original_url;   // Original video URL or YouTube ID
};

struct PlaylistMetadata {
    std::string id;
    std::string title;
    std::string uploader;
    std::vector<TrackMetadata> tracks;
};

enum class UrlType {
    SingleVideo,
    Playlist,
    Unknown
};

class YtResolver {
public:
    explicit YtResolver(std::string ytdlp_path = "compile/bin/yt-dlp.exe");
    ~YtResolver() = default;

    // Detect if input is a playlist, single video, or unknown
    static UrlType detect_url_type(const std::string& input);

    // Format duration helper (e.g. 215 seconds -> "3:35" or "03:35")
    static std::string format_duration(int total_seconds);

    // Synchronous resolution methods
    std::optional<TrackMetadata> resolve_track_info(const std::string& url_or_id, bool fetch_stream_url = true);
    std::optional<std::string> resolve_stream_url(const std::string& video_id_or_url);
    std::optional<PlaylistMetadata> resolve_playlist(const std::string& playlist_url);

    // Asynchronous resolution (non-blocking for UI)
    std::future<std::optional<TrackMetadata>> resolve_track_info_async(std::string url_or_id, bool fetch_stream_url = true);
    std::future<std::optional<std::string>> resolve_stream_url_async(std::string video_id_or_url);
    std::future<std::optional<PlaylistMetadata>> resolve_playlist_async(std::string playlist_url);

    // Cache management
    void clear_cache();
    bool has_cached_stream_url(const std::string& id) const;
    std::optional<std::string> get_cached_stream_url(const std::string& id) const;

    const std::string& get_ytdlp_path() const { return m_ytdlp_path; }
    void set_ytdlp_path(const std::string& path) { m_ytdlp_path = path; }

private:
    std::string m_ytdlp_path;

    mutable std::mutex m_cache_mutex;
    mutable std::mutex m_process_mutex; // Garantia estrita de processo único yt-dlp
    std::unordered_map<std::string, std::string> m_stream_url_cache; // id -> stream_url
    std::unordered_map<std::string, TrackMetadata> m_metadata_cache;  // id -> TrackMetadata

    // Subprocess execution helper (Win32 CREATE_NO_WINDOW / POSIX popen)
    std::string execute_command(const std::string& cmd_line) const;
    std::string build_command_line(const std::vector<std::string>& args) const;
};

} // namespace freenamp::backend
