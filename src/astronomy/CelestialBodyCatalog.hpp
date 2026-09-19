#pragma once

#include <string>

namespace bag {

// Educational classification — distinct from the runtime Body::type string.
enum class BodyClassification {
    Unknown,
    Star,
    TerrestrialPlanet,
    GasGiant,
    IceGiant,
    NaturalSatellite,
    Spacecraft,
    SmallBody,
};

enum class VisualScaleMode {
    Educational, // compressed radii so small bodies stay readable
    Realistic,   // physically proportional radii (tiny at solar-system scale)
};

struct CelestialBodyInfo {
    std::string id;
    BodyClassification classification = BodyClassification::Unknown;
    int orderFromSun = 0;           // 0 = not a planet; 1 = Mercury …
    int naturalSatelliteCount = 0;  // major moons; approximate educational count
    bool largestPlanet = false;
    const char* spectralClass = nullptr; // stars only
    const char* role = nullptr;
    const char* parentBodyId = nullptr;
    const char* composition = nullptr;
    const char* energySource = nullptr;
    const char* description = nullptr;
};

const char* classificationLabel(BodyClassification classification);
const char* visualScaleModeLabel(VisualScaleMode mode);
const char* ordinalFromSun(int order); // "3rd", or empty if order <= 0

// Lookup by body id (e.g. "earth"). Falls back to type-based defaults when unknown.
CelestialBodyInfo lookupCelestialInfo(const std::string& id, const std::string& type, bool luminous);

} // namespace bag
