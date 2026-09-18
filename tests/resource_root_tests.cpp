#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

#include "data/ResourceRoot.hpp"

#if defined(_WIN32)
#include <stdlib.h>
#endif

namespace {

void createRequiredData(const std::filesystem::path& dataRoot) {
    std::filesystem::create_directories(dataRoot / "bodies");
    std::filesystem::create_directories(dataRoot / "scenarios");
    std::ofstream(dataRoot / "bodies/sun.json") << "{}";
    std::ofstream(dataRoot / "scenarios/default_solar_system.json") << "{}";
}

} // namespace

int main() {
    using namespace bag;

    const auto root = std::filesystem::temp_directory_path() / "bagsolar-resource-root-tests";
    std::filesystem::remove_all(root);

    const auto sourceTree = root / "source";
    createRequiredData(sourceTree / "data");
    const auto sourceResult = ResourceRoot::resolve(sourceTree / "build" / "BAGS_LAB");
    assert(sourceResult);
    assert(sourceResult.dataRoot == sourceTree / "data");

    const auto installTree = root / "install";
    createRequiredData(installTree / "share/bags_lab/data");
    const auto installResult = ResourceRoot::resolve(installTree / "bin" / "bagsolar_resource_smoke");
    assert(installResult);
    assert(installResult.dataRoot == installTree / "share/bags_lab/data");

    // Legacy BAGSOLAR install layout remains resolvable for older packages.
    const auto legacyTree = root / "legacy";
    createRequiredData(legacyTree / "share/bagsolar/data");
    const auto legacyResult = ResourceRoot::resolve(legacyTree / "bin" / "BAGS_LAB");
    assert(legacyResult);
    assert(legacyResult.dataRoot == legacyTree / "share/bagsolar/data");

    std::string error;
    assert(ResourceRoot::isDataRoot(sourceTree / "data", error));
    assert(!ResourceRoot::isDataRoot(root / "missing", error));
    assert(!error.empty());

    const auto repeatedResult = ResourceRoot::resolve(installTree / "bin" / "bagsolar_resource_smoke");
    assert(repeatedResult);
    assert(repeatedResult.dataRoot == installResult.dataRoot);
    assert(repeatedResult.projectRoot() == installTree / "share/bags_lab");
    assert(repeatedResult.assetsRoot() == installTree / "share/bags_lab/assets");
    assert(repeatedResult.planetAssetsRoot() == installTree / "share/bags_lab/assets/planets");

    // VFAT launcher override: temporary ELF under /tmp must still resolve USB package data.
    const auto portableTree = root / "portable_usb";
    createRequiredData(portableTree / "data");
    std::filesystem::create_directories(portableTree / "assets/planets/prepared");
    std::ofstream(portableTree / "assets/planets/manifest.json") << "{\"format\":\"bagsolar_planet_presentation_v1\",\"models\":[]}";
    {
#if defined(_WIN32)
        _putenv_s("BAGS_LAB_RESOURCE_ROOT", portableTree.string().c_str());
#else
        setenv("BAGS_LAB_RESOURCE_ROOT", portableTree.string().c_str(), 1);
#endif
        const auto overridden = ResourceRoot::resolve();
        assert(overridden);
        assert(overridden.dataRoot == portableTree / "data");
        assert(overridden.projectRoot() == portableTree);
        assert(overridden.assetsRoot() == portableTree / "assets");
        assert(overridden.planetAssetsRoot() == portableTree / "assets/planets");
#if defined(_WIN32)
        _putenv_s("BAGS_LAB_RESOURCE_ROOT", "");
#else
        unsetenv("BAGS_LAB_RESOURCE_ROOT");
#endif
    }

    // macOS .app layout (path simulation; does not require Darwin):
    // BAGS_LAB.app/Contents/MacOS/BAGS_LAB → Contents/Resources/data
    const auto appTree = root / "BAGS_LAB.app";
    createRequiredData(appTree / "Contents" / "Resources" / "data");
    std::filesystem::create_directories(appTree / "Contents" / "Resources" / "assets" / "planets");
    const auto appResult = ResourceRoot::resolve(appTree / "Contents" / "MacOS" / "BAGS_LAB");
    assert(appResult);
    assert(appResult.dataRoot == appTree / "Contents" / "Resources" / "data");
    assert(appResult.projectRoot() == appTree / "Contents" / "Resources");
    assert(appResult.assetsRoot() == appTree / "Contents" / "Resources" / "assets");

    std::filesystem::remove_all(root);
    return 0;
}
