#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "CurlHttpClient.hpp"
#include "HorizonsProvider.hpp"
#include "JsonEphemerisProvider.hpp"
#include "LocalEphemerisProvider.hpp"
#include "SpiceProvider.hpp"

namespace {
void printUsage() { std::cout << "usage: bagsolar_ephemeris --local BODY JULIAN_DATE FRAME\n"
                              "       bagsolar_ephemeris --horizons BODY JULIAN_DATE FRAME\n"
                              "       bagsolar_ephemeris --json FILE BODY JULIAN_DATE FRAME\n"
                              "       bagsolar_ephemeris --spice MANIFEST BODY JULIAN_DATE FRAME\n"; }
}

int main(int argc, char** argv) {
    if (argc < 5) { printUsage(); return 2; }
    const std::string mode = argv[1];
    std::string body;
    std::string epochText;
    std::string frameText;
    std::unique_ptr<bag::EphemerisProvider> provider;
    int offset = 2;
    if (mode == "--json") {
        if (argc < 6) { printUsage(); return 2; }
        provider = std::make_unique<bag::JsonEphemerisProvider>(argv[2]);
        offset = 3;
    } else if (mode == "--local") {
        provider = std::make_unique<bag::LocalEphemerisProvider>(bag::LocalEphemerisProvider::deterministicFixture());
    } else if (mode == "--horizons") {
        provider = std::make_unique<bag::HorizonsProvider>(std::make_shared<bag::CurlHttpClient>());
    } else if (mode == "--spice") {
        if (argc < 6) { printUsage(); return 2; }
        std::cout << "spice_compiled=" << (bag::SpiceEphemerisProvider::compiledIn() ? "true" : "false") << "\n";
        const bag::SpiceManifestResult manifest = bag::loadSpiceKernelManifest(argv[2]);
        if (!manifest.success) { std::cerr << "SPICE manifest error: " << manifest.error << "\n"; return 1; }
        auto spice = std::make_unique<bag::SpiceEphemerisProvider>(manifest.manifest);
        const bag::SpiceLoadResult loaded = spice->loadKernels();
        std::cout << "manifest=" << argv[2] << "\n";
        if (!loaded.success) { std::cerr << bag::ephemerisStatusName(loaded.status) << ": " << loaded.error << "\n"; return 1; }
        std::cout << "kernels_loaded=" << loaded.loadedKernels.size() << "\n";
        provider = std::move(spice);
        offset = 3;
    } else { printUsage(); return 2; }
    body = argv[offset]; epochText = argv[offset + 1]; frameText = argv[offset + 2];
    bag::Frame frame;
    double epoch = 0.0;
    try { epoch = std::stod(epochText); } catch (...) { std::cerr << "invalid Julian Date\n"; return 2; }
    if (!bag::parseReferenceFrame(frameText, frame)) { std::cerr << "unsupported frame\n"; return 2; }
    const bag::EphemerisResult result = provider->getState({body, bag::Epoch::julianDate(epoch), frame});
    if (!result) { std::cerr << bag::ephemerisStatusName(result.status) << ": " << result.message << "\n"; return 1; }
    const bag::EphemerisState& state = result.state;
    std::cout << "body=" << state.bodyId << "\nepoch_jd=" << state.epoch.value << "\nframe_orientation=" << state.frame.orientation
              << "\norigin=" << state.frame.originBodyId << "\nposition_m=" << state.positionM.x << "," << state.positionM.y << "," << state.positionM.z
              << "\nvelocity_mps=" << state.velocityMps.x << "," << state.velocityMps.y << "," << state.velocityMps.z
              << "\nunits=" << state.units << "\nprovider=" << state.provider << "\nsource=" << state.source << "\n";
    return 0;
}
