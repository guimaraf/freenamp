#include "core_controller.hpp"
#include <algorithm>
#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <thread>

using namespace freenamp::backend;
using namespace std::chrono_literals;

namespace {

constexpr auto first_id = "jNQXAC9IVRw";
constexpr auto next_id = "djV11Xbc914";
constexpr auto bad_id = "badExample1";
constexpr auto first_stream = "https://audio.invalid/first";
constexpr auto next_stream = "https://audio.invalid/next";

int imitate_extractor(int argc, char** argv) {
    const std::string url = argv[argc - 1];
    if (url.find(bad_id) != std::string::npos) {
        std::cerr << "ERROR: [youtube] Video unavailable\n";
        return 1;
    }
    std::this_thread::sleep_for(120ms);
    const bool first = url.find(first_id) != std::string::npos;
    if (std::any_of(argv, argv + argc, [](const std::string& item) { return item == "-g"; })) {
        std::cout << (first ? first_stream : next_stream) << '\n';
    } else {
        std::cout << "{\"id\":\"" << (first ? first_id : next_id)
                  << "\",\"title\":\"Playable\",\"duration\":120,\"url\":\""
                  << (first ? first_stream : next_stream) << "\"}\n";
    }
    return 0;
}

void settle(CoreController& core) {
    const auto deadline = std::chrono::steady_clock::now() + 5s;
    while (core.is_loading() && std::chrono::steady_clock::now() < deadline) {
        core.update();
        std::this_thread::sleep_for(10ms);
    }
    assert(!core.is_loading());
}

} // namespace

int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--ignore-config") return imitate_extractor(argc, argv);
    assert(argc == 2);
    CoreController core;
    core.get_resolver().set_ytdlp_path(argv[1]);

    const auto owner = std::this_thread::get_id();
    int events = 0;
    core.set_event_callback([&](const std::string&) {
        assert(std::this_thread::get_id() == owner);
        ++events;
    });

    core.add_url(first_id, false);
    core.add_url(next_id, false);
    settle(core);
    auto& playlist = core.get_playlist();
    assert(playlist.size() == 2 && playlist.get_track(0)->id == first_id);
    assert(playlist.get_track(1)->id == next_id);
    assert(core.get_state() == PlaybackState::Stopped);

    const auto added = playlist.add_track(*playlist.get_track(0));
    assert(added == 0 && playlist.size() == 2);
    core.add_url(bad_id, true);
    settle(core);
    assert(playlist.size() == 2 && core.get_status_text().find("indisponivel") != std::string::npos);

    playlist.set_track_stream_url(0, "");
    core.play_track_index(0);
    playlist.move_track(1, 0);
    settle(core);
    assert(playlist.get_track(0)->id == next_id && playlist.get_track(0)->stream_url == next_stream);
    assert(playlist.get_track(1)->stream_url == first_stream);
    core.stop();

    playlist.set_track_stream_url(1, "");
    core.play_track_index(1);
    core.play_track_index(0);
    std::this_thread::sleep_for(180ms);
    core.update();
    assert(playlist.get_track(0)->stream_url == next_stream);
    assert(playlist.get_track(1)->stream_url.empty());

    core.play_track_index(1);
    core.stop();
    std::this_thread::sleep_for(180ms);
    core.update();
    assert(playlist.get_track(1)->stream_url.empty());
    assert(core.get_state() == PlaybackState::Stopped);
    assert(events > 0);

    core.play_track_index(1);
    core.shutdown();
    core.shutdown();
    assert(!core.is_loading());
}
