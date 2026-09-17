#include "core_controller.hpp"
#include <iostream>
#include <thread>

namespace freenamp::backend {

CoreController::CoreController(std::string ytdlp_path)
    : m_resolver(std::move(ytdlp_path)) {
}

void CoreController::notify_event(const std::string& event_name) {
    if (m_event_cb) {
        m_event_cb(event_name);
    }
}

void CoreController::add_url(const std::string& url_or_id, bool play_immediately) {
    if (url_or_id.empty()) return;

    UrlType type = YtResolver::detect_url_type(url_or_id);
    m_is_loading = true;
    m_status_message = "Resolvendo YouTube...";
    notify_event("loading_start");

    std::thread([this, url_or_id, type, play_immediately]() {
        if (type == UrlType::Playlist) {
            m_status_message = "Carregando Playlist...";
            auto pl = m_resolver.resolve_playlist(url_or_id);
            if (pl.has_value() && !pl->tracks.empty()) {
                bool was_empty = m_playlist.empty();
                m_playlist.add_playlist(pl.value());
                m_status_message = "Playlist adicionada: " + std::to_string(pl->tracks.size()) + " faixas";
                notify_event("playlist_updated");

                m_is_loading = false;
                notify_event("loading_end");

                if (play_immediately || was_empty) {
                    play_track_index(m_playlist.get_current_index() < 0 ? 0 : m_playlist.get_current_index());
                }
                return;
            } else {
                m_status_message = "Falha ao carregar playlist";
            }
        } else {
            m_status_message = "Buscando metadados do video...";
            auto track = m_resolver.resolve_track_info(url_or_id, true);
            if (track.has_value()) {
                bool was_empty = m_playlist.empty();
                m_playlist.add_track(track.value());
                m_status_message = "Faixa adicionada: " + track->title;
                notify_event("playlist_updated");

                m_is_loading = false;
                notify_event("loading_end");

                if (play_immediately || was_empty) {
                    play_track_index(m_playlist.size() - 1);
                }
                return;
            } else {
                m_status_message = "Falha ao resolver video";
            }
        }
        m_is_loading = false;
        notify_event("loading_end");
    }).detach();
}

void CoreController::play_current_playlist_track() {
    auto current = m_playlist.get_current_track();
    if (!current.has_value()) return;

    auto track = current.value();
    m_current_title = track.title;

    if (track.is_resolved && !track.stream_url.empty()) {
        m_status_message = "Tocando: " + track.title;
        m_audio.load_url(track.stream_url, true);
        notify_event("track_changed");
    } else {
        if (m_is_loading.exchange(true)) {
            return; // Já existe uma resolução em andamento
        }
        m_status_message = "Resolvendo stream de audio...";
        notify_event("track_loading");

        std::thread([this, track_id = track.id, idx = m_playlist.get_current_index()]() {
            auto stream_url = m_resolver.resolve_stream_url(track_id);
            if (stream_url.has_value() && !stream_url->empty()) {
                if (idx >= 0) {
                    m_playlist.set_track_stream_url(static_cast<size_t>(idx), stream_url.value());
                }
                // Verify we are still on the same index
                if (m_playlist.get_current_index() == idx) {
                    auto tr = m_playlist.get_track(idx);
                    m_status_message = tr ? ("Tocando: " + tr->title) : "Reproduzindo";
                    m_audio.load_url(stream_url.value(), true);
                    notify_event("track_changed");
                }
            } else {
                m_status_message = "Erro ao reproduzir stream";
            }
            m_is_loading = false;
            notify_event("loading_end");
        }).detach();
    }
}

void CoreController::play_track_index(size_t index) {
    if (m_playlist.set_current_index(static_cast<int>(index))) {
        play_current_playlist_track();
    }
}

void CoreController::play() {
    if (m_audio.get_state() == PlaybackState::Paused) {
        m_audio.play();
    } else if (m_playlist.get_current_index() >= 0) {
        play_current_playlist_track();
    } else if (!m_playlist.empty()) {
        play_track_index(0);
    }
}

void CoreController::pause() {
    m_audio.pause();
}

void CoreController::toggle_pause() {
    if (m_audio.get_state() == PlaybackState::Stopped && !m_playlist.empty()) {
        play();
    } else {
        m_audio.toggle_pause();
    }
}

void CoreController::stop() {
    m_audio.stop();
    m_status_message = "Parado";
    notify_event("playback_stopped");
}

void CoreController::next() {
    auto next_tr = m_playlist.next();
    if (next_tr.has_value()) {
        play_current_playlist_track();
    } else {
        stop();
    }
}

void CoreController::previous() {
    auto prev_tr = m_playlist.previous();
    if (prev_tr.has_value()) {
        play_current_playlist_track();
    } else {
        stop();
    }
}

void CoreController::seek(double seconds_absolute) {
    m_audio.seek(seconds_absolute);
}

void CoreController::seek_relative(double seconds_offset) {
    m_audio.seek_relative(seconds_offset);
}

void CoreController::set_volume(double volume) {
    m_audio.set_volume(volume);
}

double CoreController::get_volume() const {
    return m_audio.get_volume();
}

void CoreController::set_pan(double pan) {
    m_audio.set_pan(pan);
}

double CoreController::get_pan() const {
    return m_audio.get_pan();
}

EqualizerDsp& CoreController::get_equalizer() {
    return m_audio.get_equalizer();
}

void CoreController::apply_equalizer() {
    m_audio.apply_equalizer();
}

void CoreController::toggle_shuffle() {
    m_playlist.toggle_shuffle();
    notify_event("shuffle_toggled");
}

bool CoreController::is_shuffle() const {
    return m_playlist.is_shuffle();
}

void CoreController::cycle_repeat() {
    m_playlist.cycle_repeat();
    notify_event("repeat_cycled");
}

RepeatMode CoreController::get_repeat() const {
    return m_playlist.get_repeat();
}

PlaybackState CoreController::get_state() {
    return m_audio.get_state();
}

double CoreController::get_position() {
    return m_audio.get_position();
}

double CoreController::get_duration() {
    double dur = m_audio.get_duration();
    if (dur <= 0.0) {
        auto tr = m_playlist.get_current_track();
        if (tr.has_value()) {
            return static_cast<double>(tr->duration_seconds);
        }
    }
    return dur;
}

std::string CoreController::get_current_title() const {
    return m_current_title;
}

std::string CoreController::get_status_text() const {
    return m_status_message;
}

std::array<float, AudioEngine::SPECTRUM_BANDS> CoreController::get_spectrum_bands() {
    return m_audio.get_spectrum_bands();
}

void CoreController::update() {
    PlaybackState state = m_audio.get_state();

    if (state == PlaybackState::Playing) {
        double pos = m_audio.get_position();
        double dur = m_audio.get_duration();

        // Check if pre-fetching the next track is needed
        m_playlist.check_prefetch(m_resolver, pos, dur);
    }

    // Auto-advance when track finishes (only if not currently resolving/loading)
    if (!m_is_loading && m_audio.is_track_finished()) {
        next();
    }
}

} // namespace freenamp::backend
