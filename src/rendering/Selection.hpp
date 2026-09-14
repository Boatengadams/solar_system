#pragma once

#include <cmath>
#include <algorithm>
#include <limits>
#include <vector>

#include "../core/Vector2.hpp"

namespace bag {

struct SelectionCandidate {
    int index = -1;
    Vec2 screenPosition;
    float radius = 0.0f;
};

// Presentation-only hit testing. The caller supplies screen positions produced
// by the same camera transform used for rendering.
inline int selectNearestBody(Vec2 point, const std::vector<SelectionCandidate>& candidates) {
    int selected = -1;
    float bestDistance = std::numeric_limits<float>::infinity();
    for (const SelectionCandidate& candidate : candidates) {
        const float dx = point.x - candidate.screenPosition.x;
        const float dy = point.y - candidate.screenPosition.y;
        const float distanceSquared = dx * dx + dy * dy;
        const float hitRadius = std::max(0.0f, candidate.radius);
        if (distanceSquared > hitRadius * hitRadius) continue;
        if (distanceSquared < bestDistance ||
            (distanceSquared == bestDistance && candidate.index < selected)) {
            bestDistance = distanceSquared;
            selected = candidate.index;
        }
    }
    return selected;
}

// Deterministic keyboard navigation over the active body indices. A missing
// current selection starts at the first/last active body depending on direction.
inline int cycleActiveBody(int current, const std::vector<int>& activeIndices, int direction) {
    if (activeIndices.empty() || direction == 0) return -1;
    if (current < 0) return direction > 0 ? activeIndices.front() : activeIndices.back();

    auto it = std::find(activeIndices.begin(), activeIndices.end(), current);
    if (it == activeIndices.end()) return direction > 0 ? activeIndices.front() : activeIndices.back();

    const int index = static_cast<int>(std::distance(activeIndices.begin(), it));
    const int count = static_cast<int>(activeIndices.size());
    const int next = (index + (direction > 0 ? 1 : count - 1)) % count;
    return activeIndices[static_cast<std::size_t>(next)];
}

} // namespace bag
