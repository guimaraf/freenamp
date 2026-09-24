#include "core_controller.hpp"
#include <iostream>
#include <thread>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace freenamp::backend {

CoreController::CoreController(std::string ytdlp_path)
    : m_resolver(std::move(ytdlp_path)) {
}

void CoreController::notify_event(const std::string& event_name) {
    if (m_event_cb) {
        m_event_cb(event_name);
    }
}

void CoreController::save_session() {
    try {
        std::filesystem::create_directories("cache");
        m_playlist.save_to_file("cache/playlist.json");
        m_resolver.save_cache_to_file("cache/yt_cache.json");

        nlohmann::json settings;
        {
            std::ifstream ifs("cache/settings.json");
            if (ifs.is_open()) {
                try {
                    ifs >> settings;
                } catch (...) {}
            }
        }

        settings["volume"] = m_audio.get_volume();
        settings["pan"] = m_audio.get_pan();
        settings["shuffle"] = is_shuffle();
        settings["repeat"] = static_cast<int>(get_repeat());
        settings["selected_index"] = m_playlist.get_current_index();
        settings["ytdlp_compiled_version"] = YtResolver::get_compiled_version();
        settings["ytdlp_compiled_hash"] = YtResolver::get_compiled_hash();

        std::ofstream ofs("cache/settings.json");
        if (ofs.is_open()) {
            ofs << settings.dump(2);
        }
    } catch (...) {}
}

void CoreController::load_session() {
    try {
        m_resolver.load_cache_from_file("cache/yt_cache.json");
        bool pl_loaded = m_playlist.load_from_file("cache/playlist.json");

        std::ifstream ifs("cache/settings.json");
        if (ifs.is_open()) {
            nlohmann::json settings;
            ifs >> settings;
            if (settings.contains("volume") && settings["volume"].is_number()) {
                m_audio.set_volume(settings["volume"].get<double>());
            }
            if (settings.contains("pan") && settings["pan"].is_number()) {
                m_audio.set_pan(settings["pan"].get<double>());
            }
            if (settings.contains("shuffle") && settings["shuffle"].is_boolean()) {
                m_playlist.set_shuffle(settings["shuffle"].get<bool>());
            }
            if (settings.contains("repeat") && settings["repeat"].is_number_integer()) {
                m_playlist.set_repeat(static_cast<RepeatMode>(settings["repeat"].get<int>()));
            }
            if (settings.contains("selected_index") && settings["selected_index"].is_number_integer()) {
                int idx = settings["selected_index"].get<int>();
                if (idx >= 0 && idx < static_cast<int>(m_playlist.size())) {
                    m_playlist.set_current_index(idx);
                }
            }
        }

        if (pl_loaded && !m_playlist.empty()) {
            if (m_playlist.get_current_index() < 0) {
                m_playlist.set_current_index(0);
            }
            auto tr = m_playlist.get_current_track();
            if (tr.has_value()) {
                m_current_title = tr->title;
            }
            m_status_message = "Pronto (" + std::to_string(m_playlist.size()) + " faixas)";
            notify_event("playlist_updated");
        } else {
            m_current_title = "Freenamp Ready";
            m_status_message = "Pronto";
        }
    } catch (...) {}
}

void CoreController::add_url(const std::string& url_or_id, bool play_immediately) {
    if (url_or_id.empty()) return;

    UrlType type = YtResolver::detect_url_type(url_or_id);
    m_is_loading = true;
    m_loading_progress = 10;
    m_status_message = "Conectando ao YouTube...";
    notify_event("loading_start");

    std::thread([this, url_or_id, type, play_immediately]() {
        if (type == UrlType::Playlist) {
            m_status_message = "Carregando Playlist...";
            m_loading_progress = 30;
            auto pl = m_resolver.resolve_playlist(url_or_id);
            if (pl.has_value() && !pl->tracks.empty()) {
                m_loading_progress = 85;
                bool was_empty = m_playlist.empty();
                size_t start_idx = m_playlist.size();
                m_playlist.add_playlist(pl.value());
                m_status_message = "Playlist adicionada: " + std::to_string(pl->tracks.size()) + " faixas";
                notify_event("playlist_updated");

                m_is_loading = false;
                m_loading_progress = 100;
                notify_event("loading_end");

                if (play_immediately || was_empty) {
                    play_track_index(start_idx);
                }
                save_session();
                return;
            } else {
                m_status_message = "Falha ao carregar playlist";
            }
        } else {
            m_status_message = "Buscando metadados do video...";
            m_loading_progress = 35;
            auto track = m_resolver.resolve_track_info(url_or_id, true);
            if (track.has_value()) {
                m_loading_progress = 90;
                bool was_empty = m_playlist.empty();
                m_playlist.add_track(track.value());
                m_status_message = "Faixa adicionada: " + track->title;
                notify_event("playlist_updated");

                m_is_loading = false;
                m_loading_progress = 100;
                notify_event("loading_end");

                if (play_immediately || was_empty) {
                    play_track_index(m_playlist.size() - 1);
                }
                save_session();
                return;
            } else {
                m_status_message = "Falha ao resolver video";
            }
        }
        m_is_loading = false;
        m_loading_progress = 0;
        notify_event("loading_end");
    }).detach();
}

void CoreController::play_current_playlist_track(bool force_re_resolve) {
    auto current = m_playlist.get_current_track();
    if (!current.has_value()) return;

    auto track = current.value();
    m_current_title = track.title;

    bool url_valid = !force_re_resolve &&
                     track.is_resolved &&
                     !track.stream_url.empty() &&
                     !YtResolver::is_stream_url_expired(track.stream_url);

    if (url_valid) {
        m_status_message = "Tocando: " + track.title;
        m_loading_progress = 100;
        m_audio.load_url(track.stream_url, true);
        notify_event("track_changed");
    } else {
        uint64_t req_id = ++m_current_resolve_id;
        m_is_loading = true;
        m_loading_progress = 25;
        m_status_message = force_re_resolve ? "Renovando link de áudio..." : "Carregando stream de audio...";
        notify_event("track_loading");

        m_resolver.remove_from_cache(track.id);
        if (!track.original_url.empty()) {
            m_resolver.remove_from_cache(track.original_url);
        }

        std::string resolve_target = !track.original_url.empty() ? track.original_url : track.id;

        std::thread([this, target = resolve_target, idx = m_playlist.get_current_index(), req_id]() {
            m_loading_progress = 50;
            auto stream_url = m_resolver.resolve_stream_url(target);

            if (m_current_resolve_id != req_id) {
                return; // Requisicao substituida ou cancelada
            }

            if (stream_url.has_value() && !stream_url->empty()) {
                m_loading_progress = 85;
                if (idx >= 0) {
                    m_playlist.set_track_stream_url(static_cast<size_t>(idx), stream_url.value());
                }
                // Verify we are still on the same index and same request
                if (m_playlist.get_current_index() == idx && m_current_resolve_id == req_id) {
                    auto tr = m_playlist.get_track(idx);
                    m_status_message = tr ? ("Tocando: " + tr->title) : "Reproduzindo";
                    m_loading_progress = 100;
                    m_audio.load_url(stream_url.value(), true);
                    notify_event("track_changed");
                }
            } else {
                if (m_current_resolve_id == req_id) {
                    m_status_message = "Erro ao reproduzir stream";
                    m_loading_progress = 0;
                }
            }
            if (m_current_resolve_id == req_id) {
                m_is_loading = false;
                notify_event("loading_end");
            }
        }).detach();
    }
}

void CoreController::play_track_index(size_t index) {
    m_track_retry_count = 0;
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
    m_is_loading = false;
    m_current_resolve_id++;
    m_track_retry_count = 0;
    m_status_message = "Parado";
    notify_event("playback_stopped");
}

void CoreController::next() {
    m_track_retry_count = 0;
    auto next_tr = m_playlist.next();
    if (next_tr.has_value()) {
        play_current_playlist_track();
    } else {
        stop();
    }
}

void CoreController::previous() {
    m_track_retry_count = 0;
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

void CoreController::handle_playback_error() {
    auto current = m_playlist.get_current_track();
    if (!current.has_value()) return;

    if (m_track_retry_count < 1) {
        m_track_retry_count++;
        std::cerr << "[CoreController] URL expirada ou inválida ao tocar (" << current->title
                  << "). Invalidando cache e re-resolvendo link fresco...\n";
        m_resolver.remove_from_cache(current->id);
        if (!current->original_url.empty()) {
            m_resolver.remove_from_cache(current->original_url);
        }
        int cur_idx = m_playlist.get_current_index();
        if (cur_idx >= 0) {
            m_playlist.set_track_stream_url(static_cast<size_t>(cur_idx), "");
        }
        play_current_playlist_track(true);
    } else {
        std::cerr << "[CoreController] Falha permanente ao carregar faixa (" << current->title << ").\n";
        m_track_retry_count = 0;
        m_status_message = "Erro ao reproduzir faixa";
        m_is_loading = false;
        m_loading_progress = 0;
        notify_event("playback_stopped");
    }
}

void CoreController::update() {
    if (m_audio.has_playback_error()) {
        m_audio.clear_playback_error();
        handle_playback_error();
        return;
    }

    PlaybackState state = m_audio.get_state();

    if (state == PlaybackState::Playing) {
        m_track_retry_count = 0;
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

void CoreController::check_ytdlp_update_once() {
    if (m_ytdlp_update_checked.exchange(true)) {
        return; // Guarantee strictly 1 check per session
    }

    std::thread([this]() {
        try {
            bool has_update = m_resolver.check_for_update();
            if (has_update) {
                m_ytdlp_update_available.store(true);
            }
        } catch (...) {}
    }).detach();
}

void CoreController::trigger_ytdlp_update() {
    if (!m_ytdlp_update_available.load()) return;
    m_ytdlp_update_available.store(false);
    m_status_message = "Atualizando yt-dlp...";

    std::thread([this]() {
        try {
            bool ok = m_resolver.update_ytdlp_binary();
            if (ok) {
                m_status_message = "yt-dlp atualizado!";
            } else {
                m_status_message = "Pronto";
            }
        } catch (...) {
            m_status_message = "Pronto";
        }
    }).detach();
}

} // namespace freenamp::backend

