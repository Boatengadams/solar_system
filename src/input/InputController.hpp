#pragma once

#include "../rendering/Renderer.hpp"
#include "../simulation/Simulation.hpp"

namespace bag {

class InputController {
public:
    void update(Simulation& simulation, Renderer& renderer) const;
};

} // namespace bag
