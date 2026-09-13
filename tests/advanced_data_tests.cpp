#include <cassert>
#include <fstream>
#include <filesystem>

#include "astronomy/SpiceProvider.hpp"
#include "astronomy/EphemerisComparison.hpp"

int main() {
    using namespace bag;
    SpiceKernelManifest manifest;
    assert(!manifest.valid());
    SpiceEphemerisProvider provider(manifest);
    const EphemerisResult result = provider.getState({"earth", Epoch::julianDate(2451545.0), Frame::heliocentric()});
    assert(result.status == EphemerisStatus::PROVIDER_UNAVAILABLE);
    assert(!result.message.empty());
    assert(julianDateToSpiceEt(Epoch::julianDate(2451545.0)) == 0.0);
    assert(julianDateToSpiceEt(Epoch::julianDate(2451545.5)) == 43200.0);
    std::string target;
    assert(spiceTargetId("earth", target) && target == "EARTH");
    assert(!spiceTargetId("unknown", target));
    std::string observer;
    assert(spiceOriginName(Frame::geocentric(), observer) && observer == "EARTH");
    EphemerisState left{"earth", Epoch::julianDate(2451545.0), Frame::heliocentric(), {1.0, 0.0, 0.0}, {0.0, 2.0, 0.0}, "left", "test", "SI", EphemerisStatus::SUCCESS, {}};
    EphemerisState right = left;
    right.positionM.x = 4.0;
    const EphemerisComparison comparison = compareEphemerisStates(left, right);
    assert(comparison.valid && !comparison.providersDiffer && comparison.positionDifferenceM == 3.0 && comparison.velocityDifferenceMps == 0.0);
    right.provider = "another-provider";
    const EphemerisComparison crossProvider = compareEphemerisStates(left, right);
    assert(crossProvider.valid && crossProvider.providersDiffer && !crossProvider.warning.empty());
    const auto root = std::filesystem::temp_directory_path() / "bagsolar_spice_manifest_test";
    std::filesystem::create_directories(root);
    std::ofstream(root / "naif0012.tls") << "fixture";
    std::ofstream(root / "de440.bsp") << "fixture";
    std::ofstream(root / "manifest.json") << R"({"schema_version":1,"kernels":[{"path":"de440.bsp","type":"SPK","load_order":20},{"path":"naif0012.tls","type":"LSK","load_order":10}]})";
    const SpiceManifestResult loaded = loadSpiceKernelManifest(root / "manifest.json");
    assert(loaded.success && loaded.manifest.entries.size() == 2);
    assert(loaded.manifest.entries[0].type == SpiceKernelType::LSK);
    std::ofstream(root / "bad.json") << R"({"schema_version":1,"kernels":[{"path":"x.bad","type":"BAD"}]})";
    assert(!loadSpiceKernelManifest(root / "bad.json").success);
    std::filesystem::remove_all(root);
    return 0;
}
