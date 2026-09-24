#pragma once

#include <memory>
#include <string>
#include <SDL.h>
#include "core_controller.hpp"

namespace freenamp::frontend {

class ISystemMediaKeys {
public:
    virtual ~ISystemMediaKeys() = default;

    // Initializes global media key capture attached to the native window and CoreController
    virtual bool init(SDL_Window* window, backend::CoreController& core) = 0;

    // Periodic tick called in the GuiEngine loop (~60 FPS, needed for event loop / D-Bus dispatch)
    virtual void update() = 0;

    // Metadata & state update notification for the OS
    virtual void update_metadata(const std::string& title,
                                 const std::string& artist,
                                 int duration_sec,
                                 backend::PlaybackState state) = 0;

    // Releases global shortcuts and detaches listeners
    virtual void shutdown() = 0;
};

// Factory to instantiate the platform-specific implementation
std::unique_ptr<ISystemMediaKeys> create_system_media_keys();

} // namespace freenamp::frontend
