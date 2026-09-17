#include "window_dock.hpp"
#include <cmath>

namespace freenamp::frontend {

namespace {

bool overlaps_vertically(const Rect& a, const Rect& b) {
    return (a.y < b.y + b.h) && (a.y + a.h > b.y);
}

bool overlaps_horizontally(const Rect& a, const Rect& b) {
    return (a.x < b.x + b.w) && (a.x + a.w > b.x);
}

} // namespace

void WindowDock::snap(Rect& target, const std::vector<Rect>& others, int canvas_w, int canvas_h) {
    // 1. Snap to screen borders
    if (std::abs(target.x) < SNAP_DISTANCE) {
        target.x = 0;
    }
    if (std::abs(target.y) < SNAP_DISTANCE) {
        target.y = 0;
    }
    if (canvas_w > 0 && std::abs((target.x + target.w) - canvas_w) < SNAP_DISTANCE) {
        target.x = canvas_w - target.w;
    }
    if (canvas_h > 0 && std::abs((target.y + target.h) - canvas_h) < SNAP_DISTANCE) {
        target.y = canvas_h - target.h;
    }

    // 2. Snap to other windows
    for (const auto& other : others) {
        // Vertical snapping (Top-to-Bottom or Bottom-to-Top)
        if (overlaps_horizontally(target, other)) {
            // Target bottom attaches to other top
            if (std::abs((target.y + target.h) - other.y) < SNAP_DISTANCE) {
                target.y = other.y - target.h;
            }
            // Target top attaches to other bottom
            else if (std::abs(target.y - (other.y + other.h)) < SNAP_DISTANCE) {
                target.y = other.y + other.h;
            }

            // Align left edges
            if (std::abs(target.x - other.x) < SNAP_DISTANCE) {
                target.x = other.x;
            }
            // Align right edges
            else if (std::abs((target.x + target.w) - (other.x + other.w)) < SNAP_DISTANCE) {
                target.x = other.x + other.w - target.w;
            }
        }

        // Horizontal snapping (Left-to-Right or Right-to-Left)
        if (overlaps_vertically(target, other)) {
            // Target right attaches to other left
            if (std::abs((target.x + target.w) - other.x) < SNAP_DISTANCE) {
                target.x = other.x - target.w;
            }
            // Target left attaches to other right
            else if (std::abs(target.x - (other.x + other.w)) < SNAP_DISTANCE) {
                target.x = other.x + other.w;
            }

            // Align top edges
            if (std::abs(target.y - other.y) < SNAP_DISTANCE) {
                target.y = other.y;
            }
            // Align bottom edges
            else if (std::abs((target.y + target.h) - (other.y + other.h)) < SNAP_DISTANCE) {
                target.y = other.y + other.h - target.h;
            }
        }
    }

    // 3. Enforce strict canvas boundary clamping
    if (canvas_w > 0) {
        target.x = std::clamp(target.x, 0, std::max(0, canvas_w - target.w));
    }
    if (canvas_h > 0) {
        target.y = std::clamp(target.y, 0, std::max(0, canvas_h - target.h));
    }
}

bool WindowDock::are_docked(const Rect& a, const Rect& b) {
    // Check if touching along vertical boundary
    if ((a.x + a.w == b.x || b.x + b.w == a.x) && overlaps_vertically(a, b)) {
        return true;
    }
    // Check if touching along horizontal boundary
    if ((a.y + a.h == b.y || b.y + b.h == a.y) && overlaps_horizontally(a, b)) {
        return true;
    }
    return false;
}

} // namespace freenamp::frontend
