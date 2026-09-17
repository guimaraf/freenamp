#pragma once

#include "yt_resolver.hpp"
#include <vector>
#include <optional>
#include <random>
#include <mutex>
#include <atomic>
#include <future>

namespace freenamp::backend {

enum class RepeatMode {
    Off,
    All,
    One
};

class PlaylistManager {
public:
    PlaylistManager();
    ~PlaylistManager() = default;

    // Track list management
    void add_track(const TrackMetadata& track);
    void add_playlist(const PlaylistMetadata& playlist);
    bool remove_track(size_t index);
    void clear();
    void move_track(size_t from_idx, size_t to_idx);
    void set_track_stream_url(size_t index, const std::string& stream_url);

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
    std::atomic<bool> m_prefetch_in_progress = false;
    int m_prefetched_index = -1;

    void rebuild_shuffle_indices();
    void resolve_track_async(YtResolver& resolver, size_t index);
};

} // namespace freenamp::backend
