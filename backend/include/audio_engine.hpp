#pragma once

#include "equalizer_dsp.hpp"
#include <string>
#include <vector>
#include <array>
#include <atomic>
#include <mutex>
#include <chrono>

struct mpv_handle;

namespace freenamp::backend {

enum class PlaybackState {
    Stopped,
    Playing,
    Paused,
    Buffering
};

class AudioEngine {
public:
    static constexpr size_t SPECTRUM_BANDS = 16;

    AudioEngine();
    ~AudioEngine();

    // Disable copy
    AudioEngine(const AudioEngine&) = delete;
    AudioEngine& operator=(const AudioEngine&) = delete;

    // Core Controls
    bool load_url(const std::string& stream_url, bool autoplay = true);
    void play();
    void pause();
    void toggle_pause();
    void stop();
    void seek(double seconds_absolute);
    void seek_relative(double seconds_offset);

    // Volume & Pan (0.0 to 100.0)
    void set_volume(double volume);
    double get_volume() const;
    void set_pan(double pan); // -1.0 (left) to 1.0 (right)
    double get_pan() const;

    // Playback Information
    PlaybackState get_state();
    double get_position();
    double get_duration();
    bool is_track_finished();

    // Equalizer
    EqualizerDsp& get_equalizer() { return m_equalizer; }
    const EqualizerDsp& get_equalizer() const { return m_equalizer; }
    void apply_equalizer();

    // Spectrum Analyzer Data (16 bands, 0.0f to 1.0f)
    std::array<float, SPECTRUM_BANDS> get_spectrum_bands();

private:
    mpv_handle* m_mpv = nullptr;
    mutable std::mutex m_mutex;
    PlaybackState m_current_state = PlaybackState::Stopped;

    double m_volume = 100.0;
    double m_pan = 0.0;
    bool m_track_ended = false;

    EqualizerDsp m_equalizer;

    // Spectrum simulation & smoothing
    std::array<float, SPECTRUM_BANDS> m_spectrum_levels{};
    std::array<float, SPECTRUM_BANDS> m_spectrum_peaks{};
    std::chrono::steady_clock::time_point m_last_spectrum_time;

    void init_mpv();
    void update_spectrum();
};

} // namespace freenamp::backend
