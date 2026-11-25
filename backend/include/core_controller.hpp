#pragma once

#include "audio_engine.hpp"
#include "yt_resolver.hpp"
#include "playlist_manager.hpp"
#include <string>
#include <functional>
#include <memory>
#include <atomic>
#include <mutex>
#include <vector>
#include <thread>
#include <deque>
#include <map>

namespace freenamp::backend {

class CoreController {
public:
#ifdef _WIN32
    explicit CoreController(std::string ytdlp_path = "compile/bin/yt-dlp.exe");
#else
    explicit CoreController(std::string ytdlp_path = "bin/yt-dlp");
#endif
    ~CoreController();
    // Call on the UI thread before final persistence. Safe to call repeatedly.
    void shutdown();
    CoreController(const CoreController&) = delete;
    CoreController& operator=(const CoreController&) = delete;

    // High level actions
    void add_url(const std::string& url_or_id, bool play_immediately = true);
    void play_track_index(size_t index);

    // Transport controls
    void play();
    void pause();
    void toggle_pause();
    void stop();
    void next();
    void previous();
    void seek(double seconds_absolute);
    void seek_relative(double seconds_offset);

    // Volume & Pan
    void set_volume(double volume);
    double get_volume() const;
    void set_pan(double pan);
    double get_pan() const;

    // Equalizer
    EqualizerDsp& get_equalizer();
    void apply_equalizer();

    // Modes
    void toggle_shuffle();
    bool is_shuffle() const;
    void cycle_repeat();
    RepeatMode get_repeat() const;

    // Status queries for UI
    PlaybackState get_state();
    double get_position();
    double get_duration();
    std::string get_current_title() const;
    void set_current_title(const std::string& title) { m_current_title = title; }
    std::string get_status_text() const;
    bool has_status_error() const { return m_status_error; }
    void set_status_text(const std::string& status) { m_status_message = status; }
    std::array<float, AudioEngine::SPECTRUM_BANDS> get_spectrum_bands();
    bool is_loading() const { return m_is_loading.load(); }
    int get_loading_progress() const { return m_loading_progress.load(); }
    int get_bitrate_kbps() const { return m_bitrate_kbps; }
    int get_samplerate_khz() const { return m_samplerate_khz; }
    std::string get_audio_codec() const { return m_audio_codec; }
    std::string get_channels() const { return m_channels; }

    // Session and cache persistence
    void save_session();
    void load_session();

    // Access to components
    PlaylistManager& get_playlist() { return m_playlist; }
    const PlaylistManager& get_playlist() const { return m_playlist; }
    AudioEngine& get_audio() { return m_audio; }
    YtResolver& get_resolver() { return m_resolver; }

    // yt-dlp update check (once at startup)
    void check_ytdlp_update_once();
    bool is_ytdlp_update_available() const { return m_ytdlp_update_available.load(); }
    void set_ytdlp_update_available(bool avail) { m_ytdlp_update_available.store(avail); }
    void trigger_ytdlp_update();

    // Periodic tick to be called from the main loop (~60 FPS)
    // Handles gapless pre-fetching, track advancement, and status
    void update();

    // Callbacks for events (optional UI listeners)
    using EventCallback = std::function<void(const std::string& event_name)>;
    void set_event_callback(EventCallback cb) { m_event_cb = std::move(cb); }

private:
    YtResolver m_resolver;
    AudioEngine m_audio;
    PlaylistManager m_playlist;

    std::string m_current_title = "Freenamp Ready";
    std::string m_status_message = "Ready";
    bool m_status_error = false;
    std::atomic<bool> m_is_loading = false;
    std::atomic<int> m_loading_progress{0};
    std::atomic<uint64_t> m_current_resolve_id{0};
    std::atomic<bool> m_ytdlp_update_available{false};
    std::atomic<bool> m_ytdlp_update_checked{false};
    int m_bitrate_kbps = 160;
    int m_samplerate_khz = 48;
    std::string m_audio_codec = "Opus Audio";
    std::string m_channels = "STEREO";
    EventCallback m_event_cb;

    // UI state, playlist changes and events are applied only by the calling main thread.
    // Workers publish immutable results into the completion queue.
    std::atomic<bool> m_shutdown{false};
    struct BackgroundWorker {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> done;
    };
    std::vector<BackgroundWorker> m_bg_threads;
    std::mutex m_completion_mutex;
    std::deque<std::function<void(bool)>> m_completions;
    std::map<uint64_t, std::function<void(bool)>> m_import_results;
    uint64_t m_next_import_id = 0;
    uint64_t m_next_import_to_apply = 0;
    size_t m_pending_imports = 0;
    bool m_resolving_track = false;
    void post_completion(std::function<void(bool)> completion);
    void consume_completions(bool allow_playback);
    void refresh_loading();

    void notify_event(const std::string& event_name);
    void start_playlist_stream(const TrackMetadata& track);
    void play_current_playlist_track(bool force_re_resolve = false);
    void handle_playback_error();
    // Launches a background thread, registers it for clean join on shutdown
    bool launch_bg_thread(std::function<void()> fn);
    int m_track_retry_count = 0;
};

} // namespace freenamp::backend
