#include "playlist_manager.hpp"
#include <algorithm>
#include <numeric>
#include <iostream>
#include <fstream>
#include <nlohmann/json.hpp>

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
    return remove_tracks({ index });
}

bool PlaylistManager::remove_tracks(const std::vector<size_t>& indices) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (indices.empty() || m_tracks.empty()) return false;

    // Filter valid indices
    std::vector<size_t> sorted_indices;
    sorted_indices.reserve(indices.size());
    for (size_t idx : indices) {
        if (idx < m_tracks.size()) {
            sorted_indices.push_back(idx);
        }
    }
    if (sorted_indices.empty()) return false;

    std::sort(sorted_indices.begin(), sorted_indices.end());
    sorted_indices.erase(std::unique(sorted_indices.begin(), sorted_indices.end()), sorted_indices.end());

    int orig_current = m_current_index;
    bool current_removed = false;
    int before_current_count = 0;

    for (size_t idx : sorted_indices) {
        if (static_cast<int>(idx) == orig_current) {
            current_removed = true;
        } else if (static_cast<int>(idx) < orig_current) {
            before_current_count++;
        }
    }

    // Erase in descending order
    for (auto it = sorted_indices.rbegin(); it != sorted_indices.rend(); ++it) {
        m_tracks.erase(m_tracks.begin() + *it);
    }

    if (m_tracks.empty()) {
        m_current_index = -1;
    } else if (current_removed) {
        int next_idx = orig_current - before_current_count;
        if (next_idx >= static_cast<int>(m_tracks.size())) {
            next_idx = static_cast<int>(m_tracks.size()) - 1;
        }
        m_current_index = std::max(0, next_idx);
    } else {
        m_current_index = std::max(0, orig_current - before_current_count);
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
    move_tracks({ from_idx }, to_idx);
}

std::pair<size_t, size_t> PlaylistManager::move_tracks(const std::vector<size_t>& from_indices, size_t to_idx) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (from_indices.empty() || m_tracks.empty() || to_idx >= m_tracks.size()) {
        return { 0, 0 };
    }

    // Sort unique indices
    std::vector<size_t> sorted = from_indices;
    std::sort(sorted.begin(), sorted.end());
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());

    // Extract tracks
    std::vector<TrackMetadata> extracted;
    extracted.reserve(sorted.size());
    for (size_t idx : sorted) {
        if (idx < m_tracks.size()) {
            extracted.push_back(m_tracks[idx]);
        }
    }
    if (extracted.empty()) return { 0, 0 };

    // Remember currently playing track ID if any
    std::string current_track_id = "";
    if (m_current_index >= 0 && m_current_index < static_cast<int>(m_tracks.size())) {
        current_track_id = m_tracks[m_current_index].id;
    }

    // Count how many removed tracks were strictly before to_idx
    size_t count_before_to = 0;
    for (size_t idx : sorted) {
        if (idx < to_idx) {
            count_before_to++;
        }
    }

    // Erase tracks in reverse order
    for (auto it = sorted.rbegin(); it != sorted.rend(); ++it) {
        if (*it < m_tracks.size()) {
            m_tracks.erase(m_tracks.begin() + *it);
        }
    }

    // Determine insert position
    size_t insert_pos = (to_idx >= count_before_to) ? (to_idx - count_before_to) : 0;
    if (insert_pos > m_tracks.size()) {
        insert_pos = m_tracks.size();
    }

    // Insert extracted tracks
    m_tracks.insert(m_tracks.begin() + insert_pos, extracted.begin(), extracted.end());

    // Restore m_current_index
    if (!current_track_id.empty()) {
        for (size_t i = 0; i < m_tracks.size(); ++i) {
            if (m_tracks[i].id == current_track_id) {
                m_current_index = static_cast<int>(i);
                break;
            }
        }
    }

    rebuild_shuffle_indices();

    size_t new_start = insert_pos;
    size_t new_end = insert_pos + extracted.size() - 1;
    return { new_start, new_end };
}

void PlaylistManager::set_track_stream_url(size_t index, const std::string& stream_url) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (index < m_tracks.size()) {
        m_tracks[index].stream_url = stream_url;
        m_tracks[index].is_resolved = !stream_url.empty();
    }
}

bool PlaylistManager::save_to_file(const std::string& filepath) const {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        nlohmann::json j = nlohmann::json::array();
        for (const auto& tr : m_tracks) {
            nlohmann::json item;
            item["id"] = tr.id;
            item["title"] = tr.title;
            item["uploader"] = tr.uploader;
            item["duration"] = tr.duration_seconds;
            item["original_url"] = tr.original_url;
            if (!tr.stream_url.empty() && !YtResolver::is_stream_url_expired(tr.stream_url)) {
                item["stream_url"] = tr.stream_url;
                item["is_resolved"] = tr.is_resolved;
            } else {
                item["stream_url"] = "";
                item["is_resolved"] = false;
            }
            j.push_back(item);
        }

        std::ofstream ofs(filepath);
        if (!ofs.is_open()) return false;
        ofs << j.dump(2);
        return true;
    } catch (...) {
        return false;
    }
}

bool PlaylistManager::load_from_file(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(m_mutex);
    try {
        std::ifstream ifs(filepath);
        if (!ifs.is_open()) return false;

        nlohmann::json j;
        ifs >> j;
        if (!j.is_array()) return false;

        m_tracks.clear();
        for (const auto& item : j) {
            TrackMetadata tr;
            tr.id = item.value("id", "");
            tr.title = item.value("title", "Unknown Title");
            tr.uploader = item.value("uploader", "Unknown Artist");
            tr.duration_seconds = item.value("duration", 0);
            tr.original_url = item.value("original_url", "");
            std::string loaded_stream = item.value("stream_url", "");
            if (!loaded_stream.empty() && !YtResolver::is_stream_url_expired(loaded_stream)) {
                tr.stream_url = loaded_stream;
                tr.is_resolved = item.value("is_resolved", false);
            } else {
                tr.stream_url = "";
                tr.is_resolved = false;
            }
            m_tracks.push_back(tr);
        }

        if (!m_tracks.empty()) {
            m_current_index = 0;
        } else {
            m_current_index = -1;
        }

        rebuild_shuffle_indices();
        return true;
    } catch (...) {
        return false;
    }
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
        if (m_tracks[next_idx].is_resolved && !m_tracks[next_idx].stream_url.empty() && !YtResolver::is_stream_url_expired(m_tracks[next_idx].stream_url)) {
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
