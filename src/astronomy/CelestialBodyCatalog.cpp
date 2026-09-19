#include "CelestialBodyCatalog.hpp"

#include <unordered_map>

namespace bag {
namespace {

// Values rounded for education. Masses/radii live in data/bodies/*.json (SI).
// Satellite counts: major named moons commonly cited in intro astronomy.
const std::unordered_map<std::string, CelestialBodyInfo>& catalog() {
    static const std::unordered_map<std::string, CelestialBodyInfo> kCatalog = {
        {"sun",
         {"sun", BodyClassification::Star, 0, 0, false, "G2V", "Central star of the Solar System",
          nullptr, "Primarily hydrogen and helium", "Nuclear fusion",
          "The Sun's gravitational field keeps the planets and many other Solar System objects in orbit."}},
        {"mercury",
         {"mercury", BodyClassification::TerrestrialPlanet, 1, 0, false, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Mercury is the innermost planet. It is a rocky terrestrial world with no natural satellites."}},
        {"venus",
         {"venus", BodyClassification::TerrestrialPlanet, 2, 0, false, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Venus is a terrestrial planet with a dense atmosphere. It rotates retrograde relative to most planets."}},
        {"earth",
         {"earth", BodyClassification::TerrestrialPlanet, 3, 1, false, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Earth is the third planet from the Sun and the largest of the terrestrial planets. It has one natural satellite, the Moon."}},
        {"moon",
         {"moon", BodyClassification::NaturalSatellite, 0, 0, false, nullptr, nullptr, "earth",
          nullptr, nullptr,
          "The Moon is gravitationally bound to Earth. Both bodies orbit their common centre of mass."}},
        {"mars",
         {"mars", BodyClassification::TerrestrialPlanet, 4, 2, false, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Mars is a terrestrial planet. It has two small natural satellites, Phobos and Deimos."}},
        {"jupiter",
         {"jupiter", BodyClassification::GasGiant, 5, 95, true, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Jupiter is the largest planet in the Solar System — a gas giant with a strong gravitational influence and many moons."}},
        {"saturn",
         {"saturn", BodyClassification::GasGiant, 6, 146, false, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Saturn is a gas giant best known for its extensive ring system and large family of moons."}},
        {"uranus",
         {"uranus", BodyClassification::IceGiant, 7, 28, false, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Uranus is an ice giant. Its composition is richer in water, ammonia, and methane ices than Jupiter or Saturn."}},
        {"neptune",
         {"neptune", BodyClassification::IceGiant, 8, 16, false, nullptr, nullptr, "sun",
          nullptr, nullptr,
          "Neptune is the outermost ice giant of the Solar System and the farthest known major planet from the Sun."}},
    };
    return kCatalog;
}

} // namespace

const char* classificationLabel(BodyClassification classification) {
    switch (classification) {
    case BodyClassification::Star: return "Star";
    case BodyClassification::TerrestrialPlanet: return "Terrestrial Planet";
    case BodyClassification::GasGiant: return "Gas Giant";
    case BodyClassification::IceGiant: return "Ice Giant";
    case BodyClassification::NaturalSatellite: return "Natural Satellite";
    case BodyClassification::Spacecraft: return "Spacecraft";
    case BodyClassification::SmallBody: return "Small Body";
    case BodyClassification::Unknown: return "Celestial Body";
    }
    return "Celestial Body";
}

const char* visualScaleModeLabel(VisualScaleMode mode) {
    switch (mode) {
    case VisualScaleMode::Realistic: return "REALISTIC SCALE";
    case VisualScaleMode::Educational: return "EDUCATIONAL DISPLAY SCALE";
    }
    return "EDUCATIONAL DISPLAY SCALE";
}

const char* ordinalFromSun(int order) {
    switch (order) {
    case 1: return "1st";
    case 2: return "2nd";
    case 3: return "3rd";
    case 4: return "4th";
    case 5: return "5th";
    case 6: return "6th";
    case 7: return "7th";
    case 8: return "8th";
    default: return "";
    }
}

CelestialBodyInfo lookupCelestialInfo(const std::string& id, const std::string& type, bool luminous) {
    const auto& map = catalog();
    if (const auto it = map.find(id); it != map.end()) return it->second;

    CelestialBodyInfo info;
    info.id = id;
    if (luminous || type == "Star") {
        info.classification = BodyClassification::Star;
        info.role = "Star";
        info.description = "A self-luminous star whose gravity dominates nearby orbital motion.";
    } else if (type == "Moon") {
        info.classification = BodyClassification::NaturalSatellite;
        info.description = "A natural satellite gravitationally bound to a parent planet.";
    } else if (type == "Spacecraft") {
        info.classification = BodyClassification::Spacecraft;
        info.description = "An artificial spacecraft in the simulation.";
    } else if (type == "Planet") {
        info.classification = BodyClassification::TerrestrialPlanet;
        info.description = "A planetary body orbiting the central star.";
    } else {
        info.classification = BodyClassification::SmallBody;
        info.description = "A body in the Solar System simulation.";
    }
    return info;
}

} // namespace bag
