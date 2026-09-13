#pragma once

#include <raylib.h>

#include "../simulation/Simulation.hpp"

namespace bag {

class HUD {
public:
    void draw(const Simulation& simulation) const;

private:
    Rectangle panel(float x, float y, float width, float height) const;
    void text(const char* value, float x, float y, float size, Color color = RAYWHITE) const;
    void line(const char* label, const char* value, float x, float y) const;
    void top(const Simulation& simulation) const;
    void selectedInfo(const Simulation& simulation) const;
    void lessonPanel(const Simulation& simulation) const;
    void bottom(const Simulation& simulation) const;
    void experimentPanel(const Simulation& simulation) const;
    void challengePanel(const Simulation& simulation) const;
    void educationScreenPanel(const Simulation& simulation) const;
};

} // namespace bag
