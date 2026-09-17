#include "core_controller.hpp"
#include "yt_resolver.hpp"
#include <iostream>
#include <cassert>
#include <thread>
#include <chrono>

using namespace freenamp::backend;

int main() {
    std::cout << "--- TESTANDO FLUXO DE URLs ---\n";

#ifdef _WIN32
    YtResolver resolver("compile/bin/yt-dlp.exe");
#else
    YtResolver resolver("compile/bin/yt-dlp");
#endif
    std::cout << "yt-dlp path: " << resolver.get_ytdlp_path() << "\n";

    // Test 1: URL com sujeira de Ctrl+V (\x16, espacos, newlines)
    std::string dirty_url = "\x16  https://www.youtube.com/watch?v=jNQXAC9IVRw \r\n";
    std::cout << "Test 1: URL com sujeira Ctrl+V\n";
    auto track1 = resolver.resolve_track_info(dirty_url, true);
    if (track1.has_value()) {
        std::cout << "  -> Sucesso: " << track1->title << "\n";
    } else {
        std::cout << "  -> FALHA ao resolver dirty_url!\n";
    }

    // Test 2: URL de video com parametro de playlist junto (&list=...)
    std::string mix_url = "https://www.youtube.com/watch?v=jNQXAC9IVRw&list=RDjNQXAC9IVRw";
    std::cout << "Test 2: Video com parametro &list=...\n";
    std::cout << "  -> Detectado como: " << (resolver.detect_url_type(mix_url) == UrlType::Playlist ? "Playlist" : "SingleVideo") << "\n";
    auto track2 = resolver.resolve_track_info(mix_url, true);
    if (track2.has_value()) {
        std::cout << "  -> Sucesso: " << track2->title << "\n";
    } else {
        std::cout << "  -> FALHA ao resolver mix_url via track_info!\n";
    }

    auto pl2 = resolver.resolve_playlist(mix_url);
    if (pl2.has_value()) {
        std::cout << "  -> Sucesso playlist: " << pl2->title << " (" << pl2->tracks.size() << " faixas)\n";
    } else {
        std::cout << "  -> FALHA ao resolver mix_url via resolve_playlist!\n";
    }

    // Test 3: CoreController add_url com playlist e reproducao automatica
    std::cout << "Test 3: CoreController add_url com playlist\n";
#ifdef _WIN32
    CoreController core("compile/bin/yt-dlp.exe");
#else
    CoreController core("compile/bin/yt-dlp");
#endif
    core.add_url("https://www.youtube.com/playlist?list=PLMC9KNkIncKtPzgY-5rmhvj7fax8fdxoj", true);

    for (int i = 0; i < 40; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        core.update();
        std::cout << "  Tick " << i << " | Status: " << core.get_status_text() << " | State: " << (int)core.get_state() << "\n";
        if (core.get_state() == PlaybackState::Playing) {
            std::cout << "  -> TOCOU COM SUCESSO!\n";
            break;
        }
    }

    return 0;
}
