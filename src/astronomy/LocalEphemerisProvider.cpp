#include "LocalEphemerisProvider.hpp"

#include <cmath>

namespace bag {
namespace {
constexpr double J2000 = 2451545.0;
constexpr double AU = 1.495978707e11;
constexpr double EARTH_ORBITAL_SPEED = 29784.6918;
constexpr double MARS_ORBITAL_SPEED = 24077.0;
}

LocalEphemerisProvider::LocalEphemerisProvider(std::vector<EphemerisState> values) {
    for (const EphemerisState& state : values) {
        if (state.valid()) states[state.bodyId] = state;
    }
}

EphemerisResult LocalEphemerisProvider::getState(const EphemerisRequest& request) {
    if (!request.epoch.valid() || !request.frame.valid() || request.bodyId.empty()) return {EphemerisStatus::INVALID_REQUEST, {}, "body, Julian date, and frame are required"};
    const auto found = states.find(request.bodyId);
    if (found == states.end()) return {EphemerisStatus::BODY_NOT_FOUND, {}, "local provider has no state for '" + request.bodyId + "'"};
    const EphemerisState& state = found->second;
    if (state.epoch.type != request.epoch.type || std::abs(state.epoch.value - request.epoch.value) > 1e-12) return {EphemerisStatus::UNSUPPORTED_EPOCH, {}, "local fixture is available only at its recorded epoch"};
    if (state.frame.type != request.frame.type || state.frame.originBodyId != request.frame.originBodyId || state.frame.orientation != request.frame.orientation) return {EphemerisStatus::UNSUPPORTED_FRAME, {}, "local fixture does not provide the requested frame"};
    return {EphemerisStatus::SUCCESS, state, {}};
}

LocalEphemerisProvider LocalEphemerisProvider::deterministicFixture() {
    const Frame frame = Frame::heliocentric();
    const Epoch epoch = Epoch::julianDate(J2000);
    return LocalEphemerisProvider({
        {"sun", epoch, frame, {0.0, 0.0, 0.0}, {0.0, 0.0, 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
        {"earth", epoch, frame, {AU, 0.0, 0.0}, {0.0, EARTH_ORBITAL_SPEED, 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
        {"mars", epoch, frame, {1.523679 * AU, 0.0, 0.0}, {0.0, MARS_ORBITAL_SPEED, 0.0}, "BAGSOLAR deterministic local fixture", "LocalEphemerisProvider", "SI", EphemerisStatus::SUCCESS, {}},
    });
}

} // namespace bag
