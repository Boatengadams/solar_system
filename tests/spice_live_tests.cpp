#include <cassert>
#include <cstdlib>
#include <iostream>
#include <string>

#include "astronomy/SpiceProvider.hpp"
#include "simulation/Simulation.hpp"

int main() {
    const char* manifestPath = std::getenv("BAGSOLAR_SPICE_TEST_MANIFEST");
    if (manifestPath == nullptr || std::string(manifestPath).empty()) {
        std::cout << "SKIPPED: BAGSOLAR_SPICE_TEST_MANIFEST is not set\n";
        return 77;
    }
    const bag::SpiceManifestResult manifest = bag::loadSpiceKernelManifest(manifestPath);
    if (!manifest.success) { std::cerr << manifest.error << '\n'; return 1; }
    bag::SpiceEphemerisProvider provider(manifest.manifest);
    const bag::SpiceLoadResult loaded = provider.loadKernels();
    if (!loaded.success) { std::cerr << loaded.error << '\n'; return 1; }
    const bag::EphemerisResult state = provider.getState({"earth", bag::Epoch::julianDate(2451545.0), bag::Frame::heliocentric()});
    if (!state) { std::cerr << state.message << '\n'; return 1; }
    assert(state.state.provider == "SpiceEphemerisProvider");
    assert(state.state.units == "SI");
    assert(state.state.frame.originBodyId == "sun");
    assert(state.state.frame.orientation == "J2000");
    assert(state.state.epoch.value == 2451545.0);
    assert(length(state.state.positionM) > 1.0e11 && length(state.state.positionM) < 2.0e11);
    assert(length(state.state.velocityMps) > 1.0e4 && length(state.state.velocityMps) < 5.0e4);
    assert(state.state.valid());
    const bag::EphemerisResult sun = provider.getState({"sun", bag::Epoch::julianDate(2451545.0), bag::Frame::heliocentric()});
    assert(sun);
    bag::EphemerisSnapshot snapshot;
    assert(snapshot.add(sun.state));
    assert(snapshot.add(state.state));
    bag::Simulation simulation("data");
    assert(simulation.initializeFromEphemeris(snapshot));
    assert(simulation.startTelemetry(60.0, "sun"));
    assert(simulation.telemetry.metadata.ephemerisProvider == "SpiceEphemerisProvider");
    assert(simulation.telemetry.metadata.ephemerisSource.find("de440.bsp") != std::string::npos);
    assert(simulation.telemetry.metadata.ephemerisEpochJulianDate == 2451545.0);
    assert(simulation.telemetry.metadata.referenceFrame == "J2000");
    assert(simulation.telemetry.metadata.ephemerisOriginBodyId == "sun");
    assert(simulation.telemetry.metadata.ephemerisUnits == "SI");
    simulation.stopTelemetry();
    provider.unloadKernels();
    return 0;
}
