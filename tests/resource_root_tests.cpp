#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>

#include "data/ResourceRoot.hpp"

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
    const auto sourceResult = ResourceRoot::resolve(sourceTree / "build" / "planets");
    assert(sourceResult);
    assert(sourceResult.dataRoot == sourceTree / "data");

    const auto installTree = root / "install";
    createRequiredData(installTree / "share/bagsolar/data");
    const auto installResult = ResourceRoot::resolve(installTree / "bin" / "bagsolar_resource_smoke");
    assert(installResult);
    assert(installResult.dataRoot == installTree / "share/bagsolar/data");

    std::string error;
    assert(ResourceRoot::isDataRoot(sourceTree / "data", error));
    assert(!ResourceRoot::isDataRoot(root / "missing", error));
    assert(!error.empty());

    const auto repeatedResult = ResourceRoot::resolve(installTree / "bin" / "bagsolar_resource_smoke");
    assert(repeatedResult);
    assert(repeatedResult.dataRoot == installResult.dataRoot);

    std::filesystem::remove_all(root);
    return 0;
}
