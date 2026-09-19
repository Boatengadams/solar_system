#pragma once

#include <raylib.h>

#include "../simulation/Simulation.hpp"

namespace bag {

class HUD {
public:
    void draw(const Simulation& simulation, float cameraYaw = 0.0f) const;
    static void triggerScreenshotFeedback();

private:
    void text(const char* value, float x, float y, float size, Color color = RAYWHITE) const;
    void topBar(const Simulation& simulation) const;
    void bottomBar(const Simulation& simulation) const;
    void selectedInfo(const Simulation& simulation) const;
    void toolRail(const Simulation& simulation) const;
    void classificationOverlay(const Simulation& simulation) const;
    void forcesMotionOverlay(const Simulation& simulation) const;
    void gravityModeOverlay(const Simulation& simulation) const;
    void earthMoonOverlay(const Simulation& simulation) const;
    void scaleModeBadge(const Simulation& simulation) const;
    void viewControls(const Simulation& simulation) const;
    void visualizationControls(const Simulation& simulation) const;
    void orientationIndicator(float cameraYaw) const;
    void screenshotFeedback() const;
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
