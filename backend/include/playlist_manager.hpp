#pragma once

#include "yt_resolver.hpp"
#include <vector>
#include <optional>
#include <random>
#include <mutex>
#include <atomic>
#include <future>
#include <thread>
#include <chrono>

namespace freenamp::backend {

enum class RepeatMode {
    Off,
    All,
    One
};

class PlaylistManager {
public:
    PlaylistManager();
    ~PlaylistManager();
    void shutdown_prefetch();
    void apply_prefetch_result();

    // Track list management
    size_t add_track(const TrackMetadata& track);
    struct ImportResult { size_t added = 0; std::optional<size_t> first_index; };
    ImportResult add_playlist(const PlaylistMetadata& playlist);
    static std::string track_identity(const TrackMetadata& track);
    int find_track(const std::string& identity) const;
    bool set_stream_url_for_identity(const std::string& identity, const std::string& stream_url);
    bool remove_track(size_t index);
    bool remove_tracks(const std::vector<size_t>& indices);
    void clear();
    void move_track(size_t from_idx, size_t to_idx);
    std::pair<size_t, size_t> move_tracks(const std::vector<size_t>& from_indices, size_t to_idx);
    void set_track_stream_url(size_t index, const std::string& stream_url);
    bool save_to_file(const std::string& filepath) const;
    bool load_from_file(const std::string& filepath);

    // Getters
    size_t size() const;
    bool empty() const;
    std::vector<TrackMetadata> get_all_tracks() const;
    std::optional<TrackMetadata> get_track(size_t index) const;
    std::optional<TrackMetadata> get_current_track() const;
    int get_current_index() const;
    int get_total_duration() const;

    // Navigation
    bool set_current_index(int index);
    std::optional<TrackMetadata> play_track_at(size_t index);
    std::optional<TrackMetadata> next();
    std::optional<TrackMetadata> previous();
    int peek_next_index() const;

    // Playback modes
    void set_shuffle(bool enabled);
    bool is_shuffle() const { return m_shuffle; }
    void toggle_shuffle();

    void set_repeat(RepeatMode mode);
    RepeatMode get_repeat() const { return m_repeat; }
    void cycle_repeat();

    // Pre-fetching (Zero-delay transition)
    // Called periodically by the playback loop
    void check_prefetch(YtResolver& resolver, double current_pos, double total_dur);

private:
    mutable std::mutex m_mutex;
    std::vector<TrackMetadata> m_tracks;
    int m_current_index = -1;

    bool m_shuffle = false;
    RepeatMode m_repeat = RepeatMode::All;

    std::vector<size_t> m_shuffle_indices;
    size_t m_shuffle_pos = 0;
    std::mt19937 m_rng;

    // Async prefetch tracking
    std::thread m_prefetch_thread;
    std::atomic<bool> m_prefetch_done{false};
    bool m_prefetch_shutdown = false;
    std::string m_prefetch_identity;
    std::optional<std::string> m_prefetch_result;
    std::chrono::steady_clock::time_point m_prefetch_retry_after{};

    void rebuild_shuffle_indices();

};

} // namespace freenamp::backend
