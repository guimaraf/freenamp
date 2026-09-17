#include "audio_engine.hpp"
#include <mpv/client.h>

#include <iostream>
#include <cmath>
#include <random>
#include <algorithm>

namespace freenamp::backend {

AudioEngine::AudioEngine() {
    m_last_spectrum_time = std::chrono::steady_clock::now();
    m_spectrum_levels.fill(0.0f);
    m_spectrum_peaks.fill(0.0f);
    init_mpv();
}

AudioEngine::~AudioEngine() {
    if (m_mpv) {
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
    }
}

void AudioEngine::init_mpv() {
    m_mpv = mpv_create();
    if (!m_mpv) {
        std::cerr << "[AudioEngine] Falha ao criar contexto mpv.\n";
        return;
    }

    // Configure purely headless audio mode
    mpv_set_option_string(m_mpv, "vo", "null");
    mpv_set_option_string(m_mpv, "video", "no");
    mpv_set_option_string(m_mpv, "audio-display", "no");
    mpv_set_option_string(m_mpv, "audio-client-name", "Freenamp");

    // Network resilience & caching buffers
    mpv_set_option_string(m_mpv, "demuxer-max-bytes", "33554432"); // 32MB buffer
    mpv_set_option_string(m_mpv, "demuxer-readahead-secs", "30");
    mpv_set_option_string(m_mpv, "network-timeout", "10");

    // Disable built-in mpv ytdl hook since YtResolver already resolves direct stream URLs
    mpv_set_option_string(m_mpv, "ytdl", "no");

    if (mpv_initialize(m_mpv) < 0) {
        std::cerr << "[AudioEngine] Falha ao inicializar mpv.\n";
        mpv_terminate_destroy(m_mpv);
        m_mpv = nullptr;
        return;
    }

    // Set initial volume
    mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &m_volume);
}

bool AudioEngine::load_url(const std::string& stream_url, bool autoplay) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv || stream_url.empty()) return false;

    m_track_ended = false;

    const char* cmd[] = { "loadfile", stream_url.c_str(), "replace", nullptr };
    int err = mpv_command(m_mpv, cmd);
    if (err < 0) {
        std::cerr << "[AudioEngine] Erro ao carregar URL no mpv: " << mpv_error_string(err) << "\n";
        m_current_state = PlaybackState::Stopped;
        return false;
    }

    int pause_val = autoplay ? 0 : 1;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pause_val);

    m_current_state = autoplay ? PlaybackState::Playing : PlaybackState::Paused;
    apply_equalizer();

    return true;
}

void AudioEngine::play() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return;
    int pause_val = 0;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pause_val);
    m_current_state = PlaybackState::Playing;
}

void AudioEngine::pause() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return;
    int pause_val = 1;
    mpv_set_property(m_mpv, "pause", MPV_FORMAT_FLAG, &pause_val);
    m_current_state = PlaybackState::Paused;
}

void AudioEngine::toggle_pause() {
    if (get_state() == PlaybackState::Playing) {
        pause();
    } else if (get_state() == PlaybackState::Paused) {
        play();
    }
}

void AudioEngine::stop() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return;
    const char* cmd[] = { "stop", nullptr };
    mpv_command(m_mpv, cmd);
    m_current_state = PlaybackState::Stopped;
    m_track_ended = false;
}

void AudioEngine::seek(double seconds_absolute) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return;
    std::string sec_str = std::to_string(seconds_absolute);
    const char* cmd[] = { "seek", sec_str.c_str(), "absolute", nullptr };
    mpv_command(m_mpv, cmd);
}

void AudioEngine::seek_relative(double seconds_offset) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return;
    std::string sec_str = std::to_string(seconds_offset);
    const char* cmd[] = { "seek", sec_str.c_str(), "relative", nullptr };
    mpv_command(m_mpv, cmd);
}

void AudioEngine::set_volume(double volume) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (volume < 0.0) volume = 0.0;
    if (volume > 100.0) volume = 100.0;
    m_volume = volume;
    if (m_mpv) {
        mpv_set_property(m_mpv, "volume", MPV_FORMAT_DOUBLE, &m_volume);
    }
}

double AudioEngine::get_volume() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_volume;
}

void AudioEngine::set_pan(double pan) {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (pan < -1.0) pan = -1.0;
    if (pan > 1.0) pan = 1.0;
    m_pan = pan;
    // Pan can be adjusted via mpv audio balance property
    if (m_mpv) {
        mpv_set_property(m_mpv, "balance", MPV_FORMAT_DOUBLE, &m_pan);
    }
}

double AudioEngine::get_pan() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_pan;
}

PlaybackState AudioEngine::get_state() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return PlaybackState::Stopped;

    // Pump mpv events non-blocking
    while (true) {
        mpv_event* event = mpv_wait_event(m_mpv, 0);
        if (!event || event->event_id == MPV_EVENT_NONE) break;

        if (event->event_id == MPV_EVENT_END_FILE) {
            auto* end_data = static_cast<mpv_event_end_file*>(event->data);
            if (end_data && end_data->reason == MPV_END_FILE_REASON_EOF) {
                if (m_current_state == PlaybackState::Playing) {
                    m_track_ended = true;
                }
                m_current_state = PlaybackState::Stopped;
            }
        } else if (event->event_id == MPV_EVENT_PLAYBACK_RESTART) {
            m_current_state = PlaybackState::Playing;
        }
    }

    int idle_flag = 0;
    if (mpv_get_property(m_mpv, "core-idle", MPV_FORMAT_FLAG, &idle_flag) >= 0 && idle_flag) {
        if (m_current_state != PlaybackState::Paused) {
            m_current_state = PlaybackState::Stopped;
        }
    }

    int paused_flag = 0;
    if (mpv_get_property(m_mpv, "pause", MPV_FORMAT_FLAG, &paused_flag) >= 0 && paused_flag) {
        if (m_current_state == PlaybackState::Playing) {
            m_current_state = PlaybackState::Paused;
        }
    }

    return m_current_state;
}

double AudioEngine::get_position() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return 0.0;
    double pos = 0.0;
    if (mpv_get_property(m_mpv, "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0) {
        return pos;
    }
    return 0.0;
}

double AudioEngine::get_duration() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_mpv) return 0.0;
    double dur = 0.0;
    if (mpv_get_property(m_mpv, "duration", MPV_FORMAT_DOUBLE, &dur) >= 0) {
        return dur;
    }
    return 0.0;
}

bool AudioEngine::is_track_finished() {
    get_state(); // Polls mpv events
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_track_ended) {
        m_track_ended = false; // Consume the event once!
        return true;
    }
    return false;
}

void AudioEngine::apply_equalizer() {
    if (!m_mpv) return;
    std::string filter = m_equalizer.build_mpv_filter_string();
    if (filter.empty()) {
        mpv_set_property_string(m_mpv, "af", "");
    } else {
        mpv_set_property_string(m_mpv, "af", filter.c_str());
    }
}

void AudioEngine::update_spectrum() {
    auto now = std::chrono::steady_clock::now();
    float dt = std::chrono::duration<float>(now - m_last_spectrum_time).count();
    m_last_spectrum_time = now;
    if (dt > 0.1f) dt = 0.1f;

    bool is_playing = (m_current_state == PlaybackState::Playing);
    double vol_scale = m_volume / 100.0;

    static thread_local std::mt19937 rng(42);
    std::uniform_real_distribution<float> noise(0.7f, 1.3f);

    for (size_t i = 0; i < SPECTRUM_BANDS; ++i) {
        float target = 0.0f;
        if (is_playing) {
            // Bass bands have higher energy, mids dance with melody, highs sparkle
            float freq_factor = 1.0f - (float)i / (float)SPECTRUM_BANDS * 0.4f;
            target = freq_factor * noise(rng) * static_cast<float>(vol_scale);
            if (target > 1.0f) target = 1.0f;
            if (target < 0.05f) target = 0.05f;
        }

        // Smoothly approach target or decay
        if (target > m_spectrum_levels[i]) {
            m_spectrum_levels[i] += (target - m_spectrum_levels[i]) * std::min(1.0f, dt * 18.0f);
        } else {
            m_spectrum_levels[i] -= dt * 1.5f; // Falloff
            if (m_spectrum_levels[i] < 0.0f) m_spectrum_levels[i] = 0.0f;
        }

        // Peaks hold and slowly drop
        if (m_spectrum_levels[i] > m_spectrum_peaks[i]) {
            m_spectrum_peaks[i] = m_spectrum_levels[i];
        } else {
            m_spectrum_peaks[i] -= dt * 0.6f;
            if (m_spectrum_peaks[i] < 0.0f) m_spectrum_peaks[i] = 0.0f;
        }
    }
}

std::array<float, AudioEngine::SPECTRUM_BANDS> AudioEngine::get_spectrum_bands() {
    std::lock_guard<std::mutex> lock(m_mutex);
    update_spectrum();
    return m_spectrum_levels;
}

} // namespace freenamp::backend
