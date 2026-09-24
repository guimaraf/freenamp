#include "yt_resolver.hpp"
#include <iostream>
#include <cassert>
#include <chrono>

using namespace freenamp::backend;

void test_url_detection() {
    std::cout << "[TEST] 1. Testando deteccao de tipo de URL...\n";

    assert(YtResolver::detect_url_type("https://www.youtube.com/watch?v=jNQXAC9IVRw") == UrlType::SingleVideo);
    assert(YtResolver::detect_url_type("https://youtu.be/jNQXAC9IVRw") == UrlType::SingleVideo);
    assert(YtResolver::detect_url_type("jNQXAC9IVRw") == UrlType::SingleVideo);
    assert(YtResolver::detect_url_type("https://www.youtube.com/shorts/abcdef12345") == UrlType::SingleVideo);

    assert(YtResolver::detect_url_type("https://www.youtube.com/playlist?list=PLrAXtmErZgOdP_8GztsuKi9nv7kU4v58-") == UrlType::Playlist);
    assert(YtResolver::detect_url_type("https://www.youtube.com/watch?v=abc&list=PLrAXtmErZgOdP_8GztsuKi9nv7kU4v58-") == UrlType::Playlist);

    std::cout << "  -> Deteccao de URL passou com sucesso!\n\n";
}

void test_duration_formatting() {
    std::cout << "[TEST] 2. Testando formatacao de duracao...\n";

    assert(YtResolver::format_duration(0) == "00:00");
    assert(YtResolver::format_duration(9) == "00:09");
    assert(YtResolver::format_duration(65) == "01:05");
    assert(YtResolver::format_duration(215) == "03:35");
    assert(YtResolver::format_duration(3600) == "1:00:00");
    assert(YtResolver::format_duration(3665) == "1:01:05");

    std::cout << "  -> Formatacao de duracao passou com sucesso!\n\n";
}

void test_stream_url_expiration() {
    std::cout << "[TEST] 2.1 Testando verificacao de expiracao de stream URL...\n";
    assert(YtResolver::is_stream_url_expired(""));

    // An expired URL from timestamp 1000000000 (year 2001)
    std::string expired_url = "https://rr1---sn-uxa-h55e.googlevideo.com/videoplayback?expire=1000000000&ei=test";
    assert(YtResolver::is_stream_url_expired(expired_url));

    // A future URL (timestamp 2500000000, year 2049)
    std::string valid_url = "https://rr1---sn-uxa-h55e.googlevideo.com/videoplayback?expire=2500000000&ei=test";
    assert(!YtResolver::is_stream_url_expired(valid_url));

    std::cout << "  -> Verificacao de expiracao de URL passou com sucesso!\n\n";
}

void test_version_checking(YtResolver& resolver) {
    std::cout << "[TEST] 2.2 Testando comparacao de versoes e hash compilado do yt-dlp...\n";
    assert(YtResolver::is_version_newer("2026.09.20", "2026.09.16"));
    assert(!YtResolver::is_version_newer("2026.09.16", "2026.09.16"));
    assert(!YtResolver::is_version_newer("2026.08.19", "2026.09.16.074918"));
    assert(YtResolver::is_version_newer("2026.09.16.232951", "2026.09.16.074918"));

    std::string compiled_ver = YtResolver::get_compiled_version();
    std::string compiled_hash = YtResolver::get_compiled_hash();
    std::string local_ver = resolver.get_local_version();

    std::cout << "  -> Versao compilada: " << compiled_ver << "\n";
    std::cout << "  -> Hash compilado:   " << compiled_hash << "\n";
    std::cout << "  -> Versao local:     " << local_ver << "\n";

    assert(!compiled_ver.empty());
    assert(!compiled_hash.empty());
    assert(!local_ver.empty());
    std::cout << "  -> Verificacao de versao/hash passou com sucesso!\n\n";
}

void test_live_resolution(YtResolver& resolver) {
    std::cout << "[TEST] 3. Testando resolucao real de video do YouTube (jNQXAC9IVRw - 'Me at the zoo')...\n";

    auto start = std::chrono::steady_clock::now();
    auto result = resolver.resolve_track_info("jNQXAC9IVRw", true);
    auto end = std::chrono::steady_clock::now();
    auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    if (!result.has_value()) {
        if (std::getenv("CI")) {
            std::cerr << "  -> [AVISO CI] Nao foi possivel resolver o video no runner (bloqueio de IP de datacenter pelo YouTube). Prosseguindo em CI.\n";
            return;
        }
        std::cerr << "  -> FALHA: Nao foi possivel resolver o video.\n";
        std::exit(1);
    }

    const auto& track = result.value();
    std::cout << "  -> Resolvido em: " << elapsed_ms << " ms\n";
    std::cout << "  -> Titulo:       " << track.title << "\n";
    std::cout << "  -> Canal:        " << track.uploader << "\n";
    std::cout << "  -> Duracao:      " << track.duration_seconds << "s (" << YtResolver::format_duration(track.duration_seconds) << ")\n";
    std::cout << "  -> Resolvido?:   " << (track.is_resolved ? "SIM" : "NAO") << "\n";
    std::cout << "  -> Stream URL:   " << (track.stream_url.empty() ? "VAZIO" : track.stream_url.substr(0, 60) + "...") << "\n";

    assert(track.id == "jNQXAC9IVRw");
    assert(!track.title.empty());
    assert(track.duration_seconds > 0);
    assert(track.is_resolved);
    assert(track.stream_url.find("http") == 0);

    // Test caching
    std::cout << "\n[TEST] 4. Testando cache em memoria...\n";
    auto cache_start = std::chrono::steady_clock::now();
    auto cached_result = resolver.resolve_track_info("jNQXAC9IVRw", true);
    auto cache_end = std::chrono::steady_clock::now();
    auto cache_ms = std::chrono::duration_cast<std::chrono::microseconds>(cache_end - cache_start).count();

    assert(cached_result.has_value());
    assert(cached_result->stream_url == track.stream_url);
    std::cout << "  -> Retorno do cache: " << cache_ms << " us (instantaneo)!\n\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << " Freenamp Backend - Teste de Integracao \n";
    std::cout << "========================================\n\n";

    test_url_detection();
    test_duration_formatting();
    test_stream_url_expiration();

#ifdef _WIN32
    YtResolver resolver("compile/bin/yt-dlp.exe");
#else
    YtResolver resolver("compile/bin/yt-dlp");
#endif
    std::cout << "[INFO] Caminho do yt-dlp: " << resolver.get_ytdlp_path() << "\n\n";

    test_version_checking(resolver);
    test_live_resolution(resolver);

    std::cout << "[TEST] 5. Testando resolucao de playlist plana...\n";
    auto pl_start = std::chrono::steady_clock::now();
    auto pl_result = resolver.resolve_playlist("https://www.youtube.com/playlist?list=PLMC9KNkIncKtPzgY-5rmhvj7fax8fdxoj");
    auto pl_end = std::chrono::steady_clock::now();
    auto pl_ms = std::chrono::duration_cast<std::chrono::milliseconds>(pl_end - pl_start).count();

    if (!pl_result.has_value()) {
        if (std::getenv("CI")) {
            std::cerr << "  -> [AVISO CI] Playlist nao retornou dados no runner CI (bloqueio de IP). Prosseguindo em CI.\n";
            return 0;
        }
        assert(pl_result.has_value());
    }
    const auto& pl = pl_result.value();
    std::cout << "  -> Playlist resolvida em: " << pl_ms << " ms\n";
    std::cout << "  -> Titulo da Playlist:   " << pl.title << "\n";
    std::cout << "  -> Total de faixas:      " << pl.tracks.size() << "\n";
    if (!pl.tracks.empty()) {
        std::cout << "  -> Faixa #1:             " << pl.tracks[0].title << " (" << YtResolver::format_duration(pl.tracks[0].duration_seconds) << ")\n";
    }
    assert(!pl.tracks.empty());
    std::cout << "  -> Resolucao de playlist passou com sucesso!\n\n";

    std::cout << "========================================\n";
    std::cout << " TODOS OS TESTES PASSARAM COM SUCESSO!  \n";
    std::cout << "========================================\n";
    return 0;
}
