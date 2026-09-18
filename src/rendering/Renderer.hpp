#pragma once

#include <raylib.h>

#include "../simulation/Simulation.hpp"
#include "RenderTransform.hpp"
#include "Selection.hpp"
#include "PlanetAssetManager.hpp"

namespace bag {

class Renderer {
public:
    static constexpr int SCREEN_W = 1440;
    static constexpr int SCREEN_H = 900;

    explicit Renderer(std::filesystem::path planetAssetDirectory = {});
    void unloadAssets();

    void update(const Simulation& simulation);
    void background(const Simulation& simulation);
    void scene(const Simulation& simulation);
    int hitTest(const Simulation& simulation, Vector2 mouse) const;
    // True while orbit/pan is active, and until all mouse buttons release afterward
    // so button transitions cannot accidentally change selection.
    bool isCameraManipulating() const { return cameraManipulating || suppressBodyPick; }
    // True once after a click (no drag) in the scene; consumed by InputController.
    bool consumePendingBodyPick(Vector2& mouseOut);
    bool consumePendingFocusPick(Vector2& mouseOut);
    void focusBodyIndex(const Simulation& simulation, int index, bool snap = false);
    void resetSystemView();
    void saveViewBeforeIsolate();
    void restoreViewAfterIsolate();

private:
    Camera3D camera{};
    RenderTransform transform{};
    float cameraDistance = 180.0f;
    float targetDistance = 180.0f;
    float yaw = 0.72f;
    float pitch = 0.42f;
    float yawVelocity = 0.0f;
    float pitchVelocity = 0.0f;
    float zoomVelocity = 0.0f;
    Vector3 target{};
    Vector3 desiredTarget{};
    int focusBody = -1;
    // Camera snapshot so Q can return to the pre-isolate screen.
    bool hasIsolateView = false;
    float savedCameraDistance = 180.0f;
    float savedTargetDistance = 180.0f;
    float savedYaw = 0.72f;
    float savedPitch = 0.42f;
    Vector3 savedTarget{};
    Vector3 savedDesiredTarget{};
    bool fullscreen = false;
    bool cameraManipulating = false;
    bool suppressBodyPick = false;
    bool pendingBodyPick = false;
    bool pendingFocusPick = false;
    Vector2 pendingPickMouse{};
    bool leftDragActive = false;
    bool leftPressValid = false;
    Vector2 leftPressPos{};
    PlanetAssetManager planetAssets;

    Vector3 worldPosition(Vec3 position) const { return transform.position(position); }
    const Body* findBodyById(const Simulation& simulation, const std::string& id) const;
    Vector3 cameraPosition() const;
    void updateCamera(const Simulation& simulation);
    void drawOrbit(const Body& body, const Body& centralBody) const;
    void drawTrail(const Body& body) const;
    void drawVector(const Body& body, const Simulation& simulation) const;
    float displayRadius(const Body& body) const;
    float selectionRadius(const Body& body, const Simulation& simulation) const;
    float presentationRadius(const Body& body, const Simulation& simulation) const;
    bool isolatingEarthMoon(const Simulation& simulation) const;
    Vector3 bodyWorldPosition(const Body& body, const Simulation& simulation) const;
    void drawMoonOrbit(const Simulation& simulation) const;
    void drawStars(const Simulation& simulation) const;
    void drawBody(const Body& body, const Simulation& simulation) const;
    void drawStudySpinCues(const Body& body, float radius) const;
    void drawAxes() const;
    void drawSelectionIndicator(const Body& body, const Simulation& simulation) const;
    float focusDistanceFor(const Body& body, const Simulation& simulation) const;
};

} // namespace bag
