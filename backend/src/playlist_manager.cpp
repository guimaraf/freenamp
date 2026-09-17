#include "playlist_manager.hpp"
#include <algorithm>
#include <numeric>
#include <iostream>

namespace freenamp::backend {

PlaylistManager::PlaylistManager()
    : m_rng(std::random_device{}()) {
}

void PlaylistManager::add_track(const TrackMetadata& track) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracks.push_back(track);
    rebuild_shuffle_indices();
}

void PlaylistManager::add_playlist(const PlaylistMetadata& playlist) {
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& track : playlist.tracks) {
        m_tracks.push_back(track);
    }
    rebuild_shuffle_indices();
}

bool PlaylistManager::remove_track(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= m_tracks.size()) return false;

    m_tracks.erase(m_tracks.begin() + index);

    if (m_tracks.empty()) {
        m_current_index = -1;
    } else if (static_cast<int>(index) < m_current_index) {
        m_current_index--;
    } else if (m_current_index >= static_cast<int>(m_tracks.size())) {
        m_current_index = static_cast<int>(m_tracks.size()) - 1;
    }

    rebuild_shuffle_indices();
    return true;
}

void PlaylistManager::clear() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_tracks.clear();
    m_current_index = -1;
    m_shuffle_indices.clear();
    m_prefetch_in_progress = false;
    m_prefetched_index = -1;
}

void PlaylistManager::move_track(size_t from_idx, size_t to_idx) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (from_idx >= m_tracks.size() || to_idx >= m_tracks.size() || from_idx == to_idx) {
        return;
    }

    auto track = m_tracks[from_idx];
    m_tracks.erase(m_tracks.begin() + from_idx);
    m_tracks.insert(m_tracks.begin() + to_idx, track);

    if (m_current_index == static_cast<int>(from_idx)) {
        m_current_index = static_cast<int>(to_idx);
    } else if (from_idx < to_idx && m_current_index > static_cast<int>(from_idx) && m_current_index <= static_cast<int>(to_idx)) {
        m_current_index--;
    } else if (from_idx > to_idx && m_current_index >= static_cast<int>(to_idx) && m_current_index < static_cast<int>(from_idx)) {
        m_current_index++;
    }

    rebuild_shuffle_indices();
}

size_t PlaylistManager::size() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tracks.size();
}

bool PlaylistManager::empty() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tracks.empty();
}

std::vector<TrackMetadata> PlaylistManager::get_all_tracks() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_tracks;
}

std::optional<TrackMetadata> PlaylistManager::get_track(size_t index) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index < m_tracks.size()) {
        return m_tracks[index];
    }
    return std::nullopt;
}

std::optional<TrackMetadata> PlaylistManager::get_current_track() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_current_index >= 0 && m_current_index < static_cast<int>(m_tracks.size())) {
        return m_tracks[m_current_index];
    }
    return std::nullopt;
}

int PlaylistManager::get_current_index() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_current_index;
}

int PlaylistManager::get_total_duration() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    int total = 0;
    for (const auto& t : m_tracks) {
        total += t.duration_seconds;
    }
    return total;
}

bool PlaylistManager::set_current_index(int index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index >= 0 && index < static_cast<int>(m_tracks.size())) {
        m_current_index = index;
        return true;
    }
    return false;
}

std::optional<TrackMetadata> PlaylistManager::play_track_at(size_t index) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index < m_tracks.size()) {
        m_current_index = static_cast<int>(index);
        return m_tracks[m_current_index];
    }
    return std::nullopt;
}

int PlaylistManager::peek_next_index() const {
    if (m_tracks.empty()) return -1;

    if (m_repeat == RepeatMode::One && m_current_index >= 0) {
        return m_current_index;
    }

    if (m_shuffle) {
        if (m_shuffle_indices.empty()) return -1;
        size_t next_shuffle_pos = m_shuffle_pos + 1;
        if (next_shuffle_pos >= m_shuffle_indices.size()) {
            if (m_repeat == RepeatMode::All) return static_cast<int>(m_shuffle_indices[0]);
            return -1;
        }
        return static_cast<int>(m_shuffle_indices[next_shuffle_pos]);
    }

    int next_idx = m_current_index + 1;
    if (next_idx >= static_cast<int>(m_tracks.size())) {
        if (m_repeat == RepeatMode::All) return 0;
        return -1;
    }
    return next_idx;
}

std::optional<TrackMetadata> PlaylistManager::next() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_tracks.empty()) return std::nullopt;

    if (m_repeat == RepeatMode::One && m_current_index >= 0) {
        return m_tracks[m_current_index];
    }

    if (m_shuffle) {
        if (m_shuffle_indices.empty()) return std::nullopt;
        m_shuffle_pos++;
        if (m_shuffle_pos >= m_shuffle_indices.size()) {
            if (m_repeat == RepeatMode::All) {
                rebuild_shuffle_indices();
                m_shuffle_pos = 0;
            } else {
                return std::nullopt;
            }
        }
        m_current_index = static_cast<int>(m_shuffle_indices[m_shuffle_pos]);
        return m_tracks[m_current_index];
    }

    m_current_index++;
    if (m_current_index >= static_cast<int>(m_tracks.size())) {
        if (m_repeat == RepeatMode::All) {
            m_current_index = 0;
        } else {
            m_current_index = static_cast<int>(m_tracks.size()) - 1;
            return std::nullopt;
        }
    }

    return m_tracks[m_current_index];
}

std::optional<TrackMetadata> PlaylistManager::previous() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_tracks.empty()) return std::nullopt;

    if (m_repeat == RepeatMode::One && m_current_index >= 0) {
        return m_tracks[m_current_index];
    }

    if (m_shuffle) {
        if (m_shuffle_indices.empty()) return std::nullopt;
        if (m_shuffle_pos > 0) {
            m_shuffle_pos--;
        } else if (m_repeat == RepeatMode::All) {
            m_shuffle_pos = m_shuffle_indices.size() - 1;
        } else {
            return std::nullopt;
        }
        m_current_index = static_cast<int>(m_shuffle_indices[m_shuffle_pos]);
        return m_tracks[m_current_index];
    }

    m_current_index--;
    if (m_current_index < 0) {
        if (m_repeat == RepeatMode::All) {
            m_current_index = static_cast<int>(m_tracks.size()) - 1;
        } else {
            m_current_index = 0;
            return std::nullopt;
        }
    }

    return m_tracks[m_current_index];
}

void PlaylistManager::set_shuffle(bool enabled) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shuffle = enabled;
    if (m_shuffle) {
        rebuild_shuffle_indices();
    }
}

void PlaylistManager::toggle_shuffle() {
    set_shuffle(!m_shuffle);
}

void PlaylistManager::set_repeat(RepeatMode mode) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_repeat = mode;
}

void PlaylistManager::cycle_repeat() {
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (m_repeat) {
        case RepeatMode::Off: m_repeat = RepeatMode::All; break;
        case RepeatMode::All: m_repeat = RepeatMode::One; break;
        case RepeatMode::One: m_repeat = RepeatMode::Off; break;
    }
}

void PlaylistManager::rebuild_shuffle_indices() {
    m_shuffle_indices.resize(m_tracks.size());
    std::iota(m_shuffle_indices.begin(), m_shuffle_indices.end(), 0);
    std::shuffle(m_shuffle_indices.begin(), m_shuffle_indices.end(), m_rng);
    m_shuffle_pos = 0;
}

void PlaylistManager::check_prefetch(YtResolver& resolver, double current_pos, double total_dur) {
    // If remaining time is less than 15 seconds, prefetch next track's audio stream URL
    if (total_dur <= 0.0 || (total_dur - current_pos) > 15.0) {
        return;
    }

    if (m_prefetch_in_progress) return;

    int next_idx = peek_next_index();
    if (next_idx < 0 || next_idx >= static_cast<int>(m_tracks.size())) {
        return;
    }

    if (m_prefetched_index == next_idx) {
        return; // Already prefetched or in cache
    }

    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (m_tracks[next_idx].is_resolved) {
            m_prefetched_index = next_idx;
            return;
        }
    }

    m_prefetch_in_progress = true;
    m_prefetched_index = next_idx;

    // Launch background thread to resolve next track's direct stream URL
    std::thread([this, &resolver, next_idx]() {
        std::string target_id;
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (next_idx < static_cast<int>(m_tracks.size())) {
                target_id = m_tracks[next_idx].id;
            }
        }

        if (!target_id.empty()) {
            auto stream_url = resolver.resolve_stream_url(target_id);
            if (stream_url.has_value() && !stream_url->empty()) {
                std::lock_guard<std::mutex> lock(m_mutex);
                if (next_idx < static_cast<int>(m_tracks.size()) && m_tracks[next_idx].id == target_id) {
                    m_tracks[next_idx].stream_url = stream_url.value();
                    m_tracks[next_idx].is_resolved = true;
                }
            }
        }
        m_prefetch_in_progress = false;
    }).detach();
}

} // namespace freenamp::backend
