#include "core_controller.hpp"
#include "session_file.hpp"
#include <iostream>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace freenamp::backend {

CoreController::CoreController(std::string ytdlp_path)
    : m_resolver(std::move(ytdlp_path)) {
}

CoreController::~CoreController() {
    shutdown();
}

void CoreController::shutdown() {
    if (m_shutdown.exchange(true)) return;
    ++m_current_resolve_id;
    m_resolver.cancel_pending();
    m_playlist.shutdown_prefetch();
    for (auto& worker : m_bg_threads) {
        if (worker.thread.joinable()) worker.thread.join();
    }
    m_bg_threads.clear();
    // Preserve completed imports, but never start playback or call GUI listeners here.
    consume_completions(false);
    m_resolving_track = false;
    m_is_loading = false;
}

bool CoreController::launch_bg_thread(std::function<void()> fn) {
    if (m_shutdown.load()) return false;
    m_bg_threads.erase(std::remove_if(m_bg_threads.begin(), m_bg_threads.end(),
        [](BackgroundWorker& worker) {
            if (!worker.done->load()) return false;
            if (worker.thread.joinable()) worker.thread.join();
            return true;
        }), m_bg_threads.end());
    try {
        // Reserve before starting: a failing allocation must not destroy a joinable thread.
        m_bg_threads.reserve(m_bg_threads.size() + 1);
        auto done = std::make_shared<std::atomic<bool>>(false);
        m_bg_threads.push_back({std::thread([fn = std::move(fn), done]() {
            try { fn(); } catch (...) { /* Each operation reports its own failure. */ }
            done->store(true);
        }), done});
        return true;
    } catch (...) {
        return false;
    }
}

void CoreController::post_completion(std::function<void(bool)> completion) {
    std::lock_guard<std::mutex> lock(m_completion_mutex);
    m_completions.push_back(std::move(completion));
}

void CoreController::consume_completions(bool allow_playback) {
    std::deque<std::function<void(bool)>> completions;
    {
        std::lock_guard<std::mutex> lock(m_completion_mutex);
        completions.swap(m_completions);
    }
    for (auto& completion : completions) completion(allow_playback);
    while (true) {
        auto it = m_import_results.find(m_next_import_to_apply);
        if (it == m_import_results.end()) break;
        auto apply = std::move(it->second);
        m_import_results.erase(it);
        ++m_next_import_to_apply;
        --m_pending_imports;
        apply(allow_playback);
    }
    refresh_loading();
}

void CoreController::refresh_loading() {
    m_is_loading = m_resolving_track || m_pending_imports != 0;
}

void CoreController::notify_event(const std::string& event_name) {
    if (!m_shutdown.load() && m_event_cb) {
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

        if (!settings.is_object()) settings = nlohmann::json::object();
        settings["volume"] = m_audio.get_volume();
        settings["pan"] = m_audio.get_pan();
        settings["shuffle"] = is_shuffle();
        settings["repeat"] = static_cast<int>(get_repeat());
        settings["selected_index"] = m_playlist.get_current_index();
        settings["ytdlp_compiled_version"] = YtResolver::get_compiled_version();
        settings["ytdlp_compiled_hash"] = YtResolver::get_compiled_hash();

        // Persist equalizer state
        const auto& eq = m_audio.get_equalizer();
        settings["eq"]["enabled"] = eq.is_enabled();
        settings["eq"]["preamp"] = eq.get_preamp();
        nlohmann::json eq_bands = nlohmann::json::array();
        for (size_t i = 0; i < EqualizerDsp::NUM_BANDS; ++i) {
            eq_bands.push_back(eq.get_band_gain(i));
        }
        settings["eq"]["bands"] = eq_bands;

        write_session_file("cache/settings.json", settings.dump(2));
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
            // Restore equalizer state
            if (settings.contains("eq") && settings["eq"].is_object()) {
                const auto& eq_node = settings["eq"];
                auto& eq = m_audio.get_equalizer();
                if (eq_node.contains("enabled") && eq_node["enabled"].is_boolean()) {
                    eq.set_enabled(eq_node["enabled"].get<bool>());
                }
                if (eq_node.contains("preamp") && eq_node["preamp"].is_number()) {
                    eq.set_preamp(eq_node["preamp"].get<float>());
                }
                if (eq_node.contains("bands") && eq_node["bands"].is_array()) {
                    const auto& bands = eq_node["bands"];
                    for (size_t i = 0; i < EqualizerDsp::NUM_BANDS && i < bands.size(); ++i) {
                        if (bands[i].is_number()) {
                            eq.set_band_gain(i, bands[i].get<float>());
                        }
                    }
                }
                m_audio.apply_equalizer();
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

void CoreController::add_url(const std::string& input, bool play_immediately) {
    if (m_shutdown.load()) return;
    const auto url = YtResolver::sanitize_url(input);
    const auto type = YtResolver::detect_url_type(url);
    if (type == UrlType::Unknown) {
        m_status_error = true;
        m_status_message = "Informe um link ou ID válido do YouTube";
        notify_event("loading_end");
        return;
    }
    m_status_error = false;
    const uint64_t import_id = m_next_import_id++;
    const uint64_t intent = play_immediately ? ++m_current_resolve_id : m_current_resolve_id.load();
    if (play_immediately) m_resolving_track = false;
    ++m_pending_imports;
    refresh_loading();
    m_loading_progress = 10;
    m_status_message = "Conectando ao YouTube...";
    notify_event("loading_start");
    if (!launch_bg_thread([this, url, type, play_immediately, import_id, intent]() {
        std::optional<PlaylistMetadata> playlist;
        std::optional<TrackMetadata> track;
        std::string error;
        try {
            if (!m_shutdown.load()) {
                if (type == UrlType::Playlist) playlist = m_resolver.resolve_playlist(url);
                else track = m_resolver.resolve_track_info(url, true);
                error = m_resolver.get_last_error();
            }
        } catch (...) { error = "Falha ao consultar o YouTube"; }
        post_completion([this, playlist = std::move(playlist), track = std::move(track),
                         error, play_immediately, import_id, intent](bool) mutable {
            m_import_results.emplace(import_id,
                [this, playlist = std::move(playlist), track = std::move(track), error,
                 play_immediately, intent](bool allow_playback) {
                    std::optional<size_t> first_index;
                    size_t added = 0;
                    if (playlist && !playlist->tracks.empty()) {
                        auto result = m_playlist.add_playlist(*playlist);
                        first_index = result.first_index;
                        added = result.added;
                    } else if (track) {
                        const auto before = m_playlist.size();
                        first_index = m_playlist.add_track(*track);
                        added = m_playlist.size() - before;
                    }
                    if (first_index) {
                        m_status_error = false;
                        m_status_message = std::to_string(added) + " faixas adicionadas";
                        notify_event("playlist_updated");
                        m_loading_progress = 100;
                        if (allow_playback && play_immediately && intent == m_current_resolve_id.load()) {
                            play_track_index(*first_index);
                        }
                        if (allow_playback) save_session();
                    } else {
                        m_status_error = true;
                        m_status_message = error.empty() ? "Falha ao carregar conteúdo do YouTube" : error;
                        m_loading_progress = 0;
                    }
                    refresh_loading();
                    notify_event("loading_end");
                });
        });
    })) {
        // Keep the import-order barrier moving even if the OS cannot create a worker.
        m_import_results.emplace(import_id, [this](bool) {
            m_status_error = true;
            m_status_message = "Falha ao iniciar consulta do YouTube";
            m_loading_progress = 0;
            refresh_loading();
            notify_event("loading_end");
        });
    }
}

void CoreController::play_current_playlist_track(bool force_re_resolve) {
    if (m_shutdown.load()) return;
    m_status_error = false;
    const uint64_t request = ++m_current_resolve_id; // Also cancels old results on a cached switch.
    m_resolving_track = false;
    refresh_loading();
    auto current = m_playlist.get_current_track();
    if (!current) return;
    const auto track = *current;
    const auto identity = PlaylistManager::track_identity(track);
    m_current_title = track.title;
    if (!force_re_resolve && track.is_resolved && !track.stream_url.empty() && !track.http_headers.empty() &&
        !YtResolver::is_stream_url_expired(track.stream_url)) {
        m_status_message = "Tocando: " + track.title;
        m_loading_progress = 100;
        if (!m_audio.load_url(track.stream_url, true, track.http_headers)) {
            m_status_error = true;
            m_status_message = "Erro ao iniciar reproducao de audio";
            notify_event("playback_stopped");
        } else {
            notify_event("track_changed");
        }
        return;
    }
    m_audio.stop();
    m_resolving_track = true;
    refresh_loading();
    m_loading_progress = 25;
    m_status_message = (force_re_resolve || !track.stream_url.empty())
        ? "Renovando link de áudio..." : "Carregando stream de audio...";
    notify_event("track_loading");
    const bool has_video_id = track.id.size() == 11 && YtResolver::detect_url_type(track.id) == UrlType::SingleVideo;
    const auto target = (has_video_id || track.original_url.empty()) ? track.id : track.original_url;
    if (!launch_bg_thread([this, target, identity, request, track, force_re_resolve]() {
        std::optional<TrackMetadata> stream;
        std::string error;
        try {
            if (!m_shutdown.load()) {
                if (force_re_resolve) {
                    m_resolver.remove_from_cache(track.id);
                    m_resolver.remove_from_cache(track.original_url);
                }
                stream = m_resolver.resolve_track_info(target, true, force_re_resolve);
                error = m_resolver.get_last_error();
            }
        } catch (...) { error = "Falha ao resolver áudio"; }
        post_completion([this, stream = std::move(stream), error, identity, request](bool allow_playback) {
            if (!allow_playback || request != m_current_resolve_id.load()) return;
            m_resolving_track = false;
            refresh_loading();
            auto selected = m_playlist.get_current_track();
            if (!selected || PlaylistManager::track_identity(*selected) != identity) return;
            if (stream && stream->is_resolved && m_playlist.set_stream_for_identity(identity, *stream)) {
                m_status_message = "Tocando: " + selected->title;
                m_loading_progress = 100;
                m_status_error = !m_audio.load_url(stream->stream_url, true, stream->http_headers);
                if (m_status_error) {
                    m_status_message = "Erro ao iniciar reproducao de audio";
                    notify_event("playback_stopped");
                } else {
                    notify_event("track_changed");
                }
            } else {
                m_status_error = true;
                m_status_message = error.empty() ? "Erro ao reproduzir stream" : error;
                m_loading_progress = 0;
            }
            notify_event("loading_end");
        });
    })) {
        m_resolving_track = false;
        refresh_loading();
        m_status_error = true;
        m_status_message = "Falha ao iniciar consulta de audio";
        m_loading_progress = 0;
        notify_event("loading_end");
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
    m_status_error = false;
    m_audio.stop();
    m_resolving_track = false;
    refresh_loading();
    m_loading_progress = 0;
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
        m_status_error = true;
        m_status_message = "Erro ao reproduzir faixa";
        m_resolving_track = false;
        refresh_loading();
        m_loading_progress = 0;
        notify_event("playback_stopped");
    }
}

void CoreController::update() {
    if (m_shutdown.load()) return;
    consume_completions(true);
    m_playlist.apply_prefetch_result();
    if (m_audio.has_playback_error()) {
        m_audio.clear_playback_error();
        handle_playback_error();
        return;
    }

    PlaybackState state = m_audio.get_state();

    if (state == PlaybackState::Playing) {
        double pos = m_audio.get_position();
        double dur = m_audio.get_duration();

        // Check if pre-fetching the next track is needed
        m_playlist.check_prefetch(m_resolver, pos, dur);
    }

    // Background imports must not stall advancement of the current queue.
    if (!m_resolving_track && m_audio.is_track_finished()) {
        next();
    }
}

void CoreController::check_ytdlp_update_once() {
    if (m_ytdlp_update_checked.exchange(true)) {
        return; // Guarantee strictly 1 check per session
    }

    launch_bg_thread([this]() {
        try {
            if (m_shutdown.load()) return;
            bool has_update = m_resolver.check_for_update();
            post_completion([this, has_update](bool allow) {
                if (allow) m_ytdlp_update_available.store(has_update);
            });
        } catch (...) {}
    });
}

void CoreController::trigger_ytdlp_update() {
    if (!m_ytdlp_update_available.load()) return;
    m_ytdlp_update_available.store(false);
    m_status_error = false;
    m_status_message = "Atualizando yt-dlp...";

    if (!launch_bg_thread([this]() {
        try {
            if (m_shutdown.load()) return;
            bool ok = m_resolver.update_ytdlp_binary();
            post_completion([this, ok](bool allow) {
                if (allow) {
                    m_status_error = !ok;
                    m_status_message = ok ? "yt-dlp atualizado!" : "Falha ao atualizar yt-dlp";
                }
            });
        } catch (...) {
            post_completion([this](bool allow) {
                if (allow) {
                    m_status_error = true;
                    m_status_message = "Falha ao atualizar yt-dlp";
                }
            });
        }
    })) {
        m_status_error = true;
        m_status_message = "Falha ao iniciar atualizacao do yt-dlp";
    }
}

} // namespace freenamp::backend

