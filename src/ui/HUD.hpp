#pragma once

#include <raylib.h>

#include "UiLayout.hpp"
#include "../simulation/Simulation.hpp"

namespace bag {

class Renderer;

// Contextual scientific HUD: the inspector, the contextual analysis overlays,
// the orientation compass, and the full-screen teaching pages.
//
// The shell chrome (top bar, navigation rail, tab strip, simulation control
// card) belongs to UIManager. HUD deliberately does not own those, because two
// components each drawing and hit-testing their own top bar is what produced
// tabs whose clickable rects did not match the pixels on screen.
class HUD {
public:
    // `layout` is the frame's ChromeLayout, shared with UIManager so the HUD
    // anchors to the same gutters the shell reserved and never overlaps it.
    // The reference is stored for the frame rather than passed to every helper,
    // so the whole HUD reads one geometry instead of recomputing its own.
    void setLayout(const ui::ChromeLayout& layout) { shellLayout_ = layout; }
    void draw(Simulation& simulation, Renderer& renderer, float cameraYaw = 0.0f) const;
    static void triggerScreenshotFeedback();

private:
    ui::ChromeLayout shellLayout_{};
    void text(const char* value, float x, float y, float size, Color color = RAYWHITE) const;
    void selectedInfo(const Simulation& simulation) const;
    void learningExplanation(const Simulation& simulation) const;
    void forcesMotionOverlay(const Simulation& simulation) const;
    void gravityModeOverlay(const Simulation& simulation) const;
    void learningLabScreenPanel(const Simulation& simulation) const;
    void earthMoonOverlay(const Simulation& simulation) const;
    void scaleModeBadge(const Simulation& simulation) const;
    void viewControls(Simulation& simulation, Renderer& renderer) const;
    void orientationIndicator(float cameraYaw) const;
    void screenshotFeedback() const;
    void educationScreenPanel(const Simulation& simulation) const;
    void scenarioScreen(const Simulation& simulation) const;
    void telemetryScreen(const Simulation& simulation) const;
    void missionScreen(const Simulation& simulation) const;
    void settingsScreen(const Simulation& simulation) const;
    void helpScreen(const Simulation& simulation) const;
    void pageChrome(const char* title, const char* subtitle) const;
};

} // namespace bag
