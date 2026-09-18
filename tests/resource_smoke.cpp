#include <filesystem>
#include <iostream>
#include <random>

#include "data/ResourceRoot.hpp"
#include "data/ScenarioLoader.hpp"

int main() {
    using namespace bag;

    const auto resources = ResourceRoot::resolve();
    if (!resources) {
        std::cerr << resources.error << '\n';
        return 1;
    }

    std::mt19937 rng(20260908);
    ScenarioLoader loader(resources.dataRoot);
    const auto scenario = loader.loadScenario("default_solar_system", rng);
    if (!scenario) {
        std::cerr << "runtime data was found but the default scenario could not be loaded: "
                  << scenario.error << '\n';
        return 1;
    }
    if (scenario.value->bodies.empty()) {
        std::cerr << "default scenario loaded without bodies\n";
        return 1;
    }

    std::cout << "BAGS_LAB runtime data: " << resources.dataRoot.string() << '\n';
    std::cout << "BAGS_LAB default scenario bodies: " << scenario.value->bodies.size() << '\n';
    std::cout << "BAGS_LAB assets root: " << resources.assetsRoot().string() << '\n';
    return 0;
}
