#pragma once

#include <raylib.h>

#include "../simulation/Simulation.hpp"

namespace bag {

class Renderer {
public:
    static constexpr int SCREEN_W = 1440;
    static constexpr int SCREEN_H = 900;
    static constexpr float SCALE_AU = 250.0f;

    Renderer();

    void update(const Simulation& simulation);
    void background(const Simulation& simulation);
    void scene(const Simulation& simulation);
    int hitTest(const Simulation& simulation, Vector2 mouse) const;

private:
    Camera2D camera{};
    float targetZoom = 0.65f;
    bool fullscreen = false;

    Vector2 worldToScreen(Vec3 position) const;
    Vector2 worldToScene(Vec3 position) const;
    void glow(Vector2 position, float radius, Color color, int layers = 8) const;
    void drawBody(const Body& body, const Simulation& simulation) const;
};

} // namespace bag
