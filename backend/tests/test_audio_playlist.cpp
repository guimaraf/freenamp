#include "audio_engine.hpp"
#include "playlist_manager.hpp"
#include "equalizer_dsp.hpp"
#include "core_controller.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace freenamp::backend;

void test_equalizer_dsp() {
    std::cout << "[TEST] 1. Testando EqualizerDsp (10 Bandas e Preamp)...\n";

    EqualizerDsp eq;
    assert(!eq.is_enabled());
    assert(eq.build_mpv_filter_string().empty());

    eq.set_enabled(true);
    eq.set_preamp(3.5f);
    eq.set_band_gain(0, 6.0f);   // 60Hz
    eq.set_band_gain(4, -2.5f);  // 1kHz
    eq.set_band_gain(9, 4.0f);   // 16kHz

    // Test clamping
    eq.set_band_gain(1, 20.0f);
    assert(eq.get_band_gain(1) == 12.0f);
    eq.set_band_gain(2, -30.0f);
    assert(eq.get_band_gain(2) == -12.0f);

    std::string filter = eq.build_mpv_filter_string();
    assert(!filter.empty());
    assert(filter.find("firequalizer") != std::string::npos);
    assert(filter.find("entry(60,6.0)") != std::string::npos);
    std::cout << "  -> Filtro MPV gerado: " << filter.substr(0, 50) << "...\n";
    std::cout << "  -> EqualizerDsp passou com sucesso!\n\n";
}

void test_playlist_manager() {
    std::cout << "[TEST] 2. Testando PlaylistManager (Fila, Reordenacao, Shuffle e Repeat)...\n";

    PlaylistManager pm;
    assert(pm.empty());
    assert(pm.size() == 0);

    // Add sample tracks
    TrackMetadata t1{"id1", "Track 1", "Artist A", 180, "http://stream1", true, ""};
    TrackMetadata t2{"id2", "Track 2", "Artist B", 240, "http://stream2", true, ""};
    TrackMetadata t3{"id3", "Track 3", "Artist C", 200, "http://stream3", true, ""};

    pm.add_track(t1);
    pm.add_track(t2);
    pm.add_track(t3);

    assert(pm.size() == 3);
    assert(pm.get_total_duration() == 620); // 180 + 240 + 200

    // Test sequential playback
    auto current = pm.play_track_at(0);
    assert(current.has_value() && current->title == "Track 1");

    auto next = pm.next();
    assert(next.has_value() && next->title == "Track 2");

    next = pm.next();
    assert(next.has_value() && next->title == "Track 3");

    // Repeat All test (wrap around to 0)
    pm.set_repeat(RepeatMode::All);
    next = pm.next();
    assert(next.has_value() && next->title == "Track 1");

    // Repeat One test
    pm.set_repeat(RepeatMode::One);
    next = pm.next();
    assert(next.has_value() && next->title == "Track 1");

    // Move track test (move track 2 to position 0)
    pm.set_repeat(RepeatMode::All);
    pm.move_track(1, 0);
    assert(pm.get_track(0)->title == "Track 2");

    // Shuffle test
    pm.set_shuffle(true);
    assert(pm.is_shuffle());
    int peek_idx = pm.peek_next_index();
    assert(peek_idx >= 0 && peek_idx < 3);

    std::cout << "  -> PlaylistManager passou com sucesso!\n\n";
}

void test_audio_engine_lifecycle() {
    std::cout << "[TEST] 3. Testando AudioEngine (Inicializacao headless, Volume, Pan e Espectro)...\n";

    AudioEngine audio;
    assert(audio.get_state() == PlaybackState::Stopped);

    audio.set_volume(85.0);
    assert(audio.get_volume() == 85.0);

    audio.set_pan(-0.5);
    assert(audio.get_pan() == -0.5);

    // Test spectrum output array
    auto bands = audio.get_spectrum_bands();
    assert(bands.size() == 16);
    for (float b : bands) {
        assert(b >= 0.0f && b <= 1.0f);
    }

    std::cout << "  -> AudioEngine inicializado e configurado com sucesso!\n\n";
}

void test_live_headless_playback() {
    std::cout << "[TEST] 4. Testando reproducao headless em tempo real com stream do YouTube...\n";

    CoreController core("compile/bin/yt-dlp.exe");

    // Test resolving & starting live playback of sample video
    std::cout << "  -> Disparando add_url com 'jNQXAC9IVRw' (play_immediately = true)...\n";
    core.add_url("jNQXAC9IVRw", true);

    // Wait up to 6 seconds for resolution and playback start
    bool started_playing = false;
    for (int i = 0; i < 60; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        core.update();

        PlaybackState state = core.get_state();
        if (state == PlaybackState::Playing) {
            started_playing = true;
            std::cout << "  -> Playback iniciado com sucesso! Estado: PLAYING\n";
            std::cout << "  -> Titulo: " << core.get_current_title() << "\n";
            std::cout << "  -> Posicao: " << core.get_position() << "s / " << core.get_duration() << "s\n";
            break;
        }
    }

    assert(started_playing);

    // Let it play for 1.5 seconds and inspect spectrum bands
    std::this_thread::sleep_for(std::chrono::milliseconds(1500));
    core.update();

    auto bands = core.get_spectrum_bands();
    std::cout << "  -> Espectro ativo durante reproducao (16 bandas): [";
    for (size_t i = 0; i < bands.size(); ++i) {
        std::cout << (int)(bands[i] * 10);
    }
    std::cout << "]\n";

    assert(core.get_position() > 0.0);

    // Test pause / resume
    core.pause();
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    assert(core.get_state() == PlaybackState::Paused);
    std::cout << "  -> Pausa confirmada com sucesso!\n";

    core.stop();
    assert(core.get_state() == PlaybackState::Stopped);
    std::cout << "  -> Stop confirmado com sucesso!\n\n";
}

int main() {
    std::cout << "===================================================\n";
    std::cout << " Freenamp Backend - Testes das Fases 3 e 4         \n";
    std::cout << "===================================================\n\n";

    test_equalizer_dsp();
    test_playlist_manager();
    test_audio_engine_lifecycle();
    test_live_headless_playback();

    std::cout << "===================================================\n";
    std::cout << " TODOS OS TESTES DAS FASES 3 E 4 PASSARAM!         \n";
    std::cout << "===================================================\n";
    return 0;
}
