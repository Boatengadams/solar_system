#include "LocalEphemerisProvider.hpp"

#include <algorithm>
#include <cmath>

namespace bag {
namespace {
constexpr double J2000 = 2451545.0;
constexpr double AU = 1.495978707e11;
constexpr double EARTH_ORBITAL_SPEED = 29784.6918;
constexpr double MARS_ORBITAL_SPEED = 24077.0;
constexpr double DAY = 86400.0;
}

LocalEphemerisProvider::LocalEphemerisProvider(std::vector<EphemerisState> values) {
    for (const EphemerisState& state : values) if (state.valid()) states.push_back(state);
}

EphemerisResult LocalEphemerisProvider::getState(const EphemerisRequest& request) {
    if (!request.epoch.valid() || !request.frame.valid() || request.bodyId.empty()) return {EphemerisStatus::INVALID_REQUEST, {}, "body, Julian date, and frame are required"};
    const auto body = std::find_if(states.begin(), states.end(), [&](const EphemerisState& state) { return state.bodyId == request.bodyId; });
    if (body == states.end()) return {EphemerisStatus::BODY_NOT_FOUND, {}, "local provider has no state for '" + request.bodyId + "'"};
    const auto found = std::find_if(body, states.end(), [&](const EphemerisState& state) {
        return state.bodyId == request.bodyId && state.epoch.type == request.epoch.type &&
            std::abs(state.epoch.value - request.epoch.value) <= 1e-12;
    });
    if (found == states.end()) return {EphemerisStatus::UNSUPPORTED_EPOCH, {}, "local fixture has no state at the requested epoch"};
    const EphemerisState& state = *found;
    if (state.frame.type != request.frame.type || state.frame.originBodyId != request.frame.originBodyId || state.frame.orientation != request.frame.orientation) return {EphemerisStatus::UNSUPPORTED_FRAME, {}, "local fixture does not provide the requested frame"};
    return {EphemerisStatus::SUCCESS, state, {}};
}

LocalEphemerisProvider LocalEphemerisProvider::deterministicFixture() {
    const Frame frame = Frame::heliocentric();
    const Epoch epoch = Epoch::julianDate(J2000);
    const Epoch nextEpoch = Epoch::julianDate(J2000 + 1.0);
    const double earthAngle = EARTH_ORBITAL_SPEED / AU * DAY;
    const double marsRadius = 1.523679 * AU;
    const double marsAngle = MARS_ORBITAL_SPEED / marsRadius * DAY;
    return LocalEphemerisProvider({
        {"sun", epoch, frame, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
        {"earth", epoch, frame, {AU, 0.0, 0.0}, {0.0, EARTH_ORBITAL_SPEED, 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
        {"mars", epoch, frame, {1.523679 * AU, 0.0, 0.0}, {0.0, MARS_ORBITAL_SPEED, 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
        {"sun", nextEpoch, frame, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
        {"earth", nextEpoch, frame, {AU * std::cos(earthAngle), AU * std::sin(earthAngle), 0.0}, {-EARTH_ORBITAL_SPEED * std::sin(earthAngle), EARTH_ORBITAL_SPEED * std::cos(earthAngle), 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
        {"mars", nextEpoch, frame, {marsRadius * std::cos(marsAngle), marsRadius * std::sin(marsAngle), 0.0}, {-MARS_ORBITAL_SPEED * std::sin(marsAngle), MARS_ORBITAL_SPEED * std::cos(marsAngle), 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
    });
}

} // namespace bag
