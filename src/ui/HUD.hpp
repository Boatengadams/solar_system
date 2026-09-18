#pragma once

#include <raylib.h>

#include "../simulation/Simulation.hpp"

namespace bag {

class HUD {
public:
    void draw(const Simulation& simulation) const;

private:
    void text(const char* value, float x, float y, float size, Color color = RAYWHITE) const;
    void topBar(const Simulation& simulation) const;
    void bottomBar(const Simulation& simulation) const;
    void selectedInfo(const Simulation& simulation) const;
    void educationScreenPanel(const Simulation& simulation) const;
    void learningLabScreenPanel(const Simulation& simulation) const;
    void scenarioScreen(const Simulation& simulation) const;
    void telemetryScreen(const Simulation& simulation) const;
    void missionScreen(const Simulation& simulation) const;
    void settingsScreen(const Simulation& simulation) const;
    void helpScreen(const Simulation& simulation) const;
    void pageChrome(const Simulation& simulation, const char* title, const char* subtitle) const;
};

} // namespace bag
