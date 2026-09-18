#pragma once

#include <raylib.h>

#include "../core/Vector3.hpp"
#include "../physics/PhysicsEngine.hpp"

namespace bag {

// Rendering convention:
//   astronomical +X -> raylib +X
//   astronomical +Y -> raylib +Z (depth)
//   astronomical +Z -> raylib +Y (up)
// Physical state remains in SI metres. This adapter is presentation-only.
struct RenderTransform {
    float astronomicalUnitScale = 20.0f;

    Vector3 positionAU(Vec3 astronomicalUnits) const {
        return {
            static_cast<float>(astronomicalUnits.x * astronomicalUnitScale),
            static_cast<float>(astronomicalUnits.z * astronomicalUnitScale),
            static_cast<float>(astronomicalUnits.y * astronomicalUnitScale),
        };
    }

    Vector3 position(Vec3 physicalMetres) const {
        return positionAU(physicalMetres / PhysicsEngine::AU);
    }

    Vector3 vector(Vec3 physicalVector, double displayScale) const {
        return {
            static_cast<float>(physicalVector.x * displayScale),
            static_cast<float>(physicalVector.z * displayScale),
            static_cast<float>(physicalVector.y * displayScale),
        };
    }
};

} // namespace bag
