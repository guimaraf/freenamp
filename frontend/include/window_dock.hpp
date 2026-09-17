#pragma once

#include "gui_common.hpp"
#include <vector>
#include <algorithm>

namespace freenamp::frontend {

class WindowDock {
public:
    static constexpr int SNAP_DISTANCE = 14;

    // Checks and adjusts 'target' position so it magnetically snaps to 'others' or screen borders
    static void snap(Rect& target, const std::vector<Rect>& others, int canvas_w, int canvas_h);

    // Checks if rect A is docked/attached to rect B (sharing a boundary)
    static bool are_docked(const Rect& a, const Rect& b);
};

} // namespace freenamp::frontend
