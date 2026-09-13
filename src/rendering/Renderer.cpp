#include "Renderer.hpp"

#include <raymath.h>

#include <algorithm>
#include <cmath>

namespace bag {
namespace {

Color color(ColorRGBA value) { return {value.r, value.g, value.b, value.a}; }
Color alpha(Color value, float amount) {
    value.a = static_cast<unsigned char>(std::clamp(amount * 255.0f, 0.0f, 255.0f));
    return value;
}

} // namespace

Renderer::Renderer() {
    camera.target = {0.0f, 0.0f};
    camera.offset = {SCREEN_W / 2.0f, SCREEN_H / 2.0f};
    camera.zoom = targetZoom;
}

Vector2 Renderer::worldToScreen(Vec3 position) const {
    return GetWorldToScreen2D(worldToScene(position), camera);
}

Vector2 Renderer::worldToScene(Vec3 position) const {
    return {static_cast<float>(position.x / PhysicsEngine::AU * SCALE_AU),
            static_cast<float>(position.y / PhysicsEngine::AU * SCALE_AU)};
}

void Renderer::update(const Simulation& simulation) {
    const float wheel = GetMouseWheelMove();
    if (wheel != 0.0f) {
        const Vector2 before = GetScreenToWorld2D(GetMousePosition(), camera);
        targetZoom = std::clamp(targetZoom * std::pow(1.12f, wheel), 0.08f, 8.0f);
        camera.zoom = targetZoom;
        const Vector2 after = GetScreenToWorld2D(GetMousePosition(), camera);
        camera.target = Vector2Add(camera.target, Vector2Subtract(before, after));
    }
    if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
        const Vector2 delta = GetMouseDelta();
        camera.target = Vector2Subtract(camera.target, Vector2Scale(delta, 1.0f / camera.zoom));
    }
    if (IsKeyPressed(KEY_F)) {
        fullscreen = !fullscreen;
        ToggleFullscreen();
    }
    if (IsKeyPressed(KEY_HOME)) {
        camera.target = {0.0f, 0.0f};
        targetZoom = 0.65f;
        camera.zoom = targetZoom;
    }
    if (simulation.selected >= 0 && simulation.selected < static_cast<int>(simulation.bodies.size()) &&
        IsKeyPressed(KEY_C)) {
        camera.target = worldToScene(simulation.bodies[simulation.selected].position);
        targetZoom = 1.8f;
        camera.zoom = targetZoom;
    }
}

void Renderer::background(const Simulation& simulation) {
    ClearBackground({3, 6, 14, 255});
    BeginMode2D(camera);
    if (simulation.showGrid) {
        for (int i = -20; i <= 20; ++i) {
            DrawLine(static_cast<int>(i * SCALE_AU), static_cast<int>(-20 * SCALE_AU),
                     static_cast<int>(i * SCALE_AU), static_cast<int>(20 * SCALE_AU),
                     alpha({80, 100, 130, 255}, 0.08f));
            DrawLine(static_cast<int>(-20 * SCALE_AU), static_cast<int>(i * SCALE_AU),
                     static_cast<int>(20 * SCALE_AU), static_cast<int>(i * SCALE_AU),
                     alpha({80, 100, 130, 255}, 0.08f));
        }
    }
    for (const Star& star : simulation.stars) {
        const Vector2 position = Vector2Scale({star.position.x, star.position.y}, 0.45f);
        const float brightness = 0.25f + 0.75f * star.brightness;
        DrawCircleV(position, star.radius, alpha({190, 215, 255, 255}, brightness));
    }
    DrawCircleGradient({0.0f, 0.0f}, 4300.0f, alpha({50, 65, 100, 255}, 0.035f), alpha({3, 6, 14, 255}, 0.0f));
    EndMode2D();
}

void Renderer::glow(Vector2 position, float radius, Color drawColor, int layers) const {
    for (int i = layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / layers;
        DrawCircleV(position, radius * (1.0f + 1.8f * t), alpha(drawColor, 0.012f + 0.03f * (1.0f - t)));
    }
}

void Renderer::drawBody(const Body& body, const Simulation& simulation) const {
    const Vector2 position = worldToScreen(body.position);
    const float radius = std::max(1.0f, static_cast<float>(body.radius * camera.zoom));
    const Color bodyColor = color(body.color);
    const Color accentColor = color(body.accent);
    if (body.luminous) {
        glow(position, radius * 1.8f, bodyColor, 14);
        DrawCircleGradient(position, radius * 2.0f, accentColor, alpha(bodyColor, 0.0f));
        DrawCircleV(position, radius, bodyColor);
        DrawCircleV({position.x - radius * 0.25f, position.y - radius * 0.28f}, radius * 0.22f, alpha(WHITE, 0.35f));
        return;
    }
    if (body.ringed) {
        DrawEllipse(static_cast<int>(position.x), static_cast<int>(position.y), radius * 2.5f, radius * 0.72f, alpha(accentColor, 0.22f));
        DrawEllipseLines(static_cast<int>(position.x), static_cast<int>(position.y), radius * 2.35f, radius * 0.62f, alpha(accentColor, 0.8f));
        DrawEllipseLines(static_cast<int>(position.x), static_cast<int>(position.y), radius * 1.75f, radius * 0.47f, alpha(bodyColor, 0.7f));
    }
    glow(position, radius, accentColor, 5);
    DrawCircleV(position, radius, bodyColor);
    const Vector2 sun = worldToScreen(simulation.bodies[0].position);
    const Vector2 direction = Vector2Normalize(Vector2Subtract(sun, position));
    DrawCircleV(Vector2Add(position, Vector2Scale(direction, radius * 0.30f)), radius * 0.48f, alpha(WHITE, 0.16f));
    DrawCircleV(Vector2Add(position, Vector2Scale(direction, -radius * 0.35f)), radius * 0.62f, alpha(BLACK, 0.18f));
    if (body.name == "Earth" || body.name == "Venus" || body.name == "Mars") {
        DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius + 2.0f, alpha(accentColor, 0.7f));
    }
}

void Renderer::scene(const Simulation& simulation) {
    BeginMode2D(camera);
    if (simulation.showOrbits) {
        for (std::size_t i = 1; i < simulation.bodies.size(); ++i) {
            const Body& body = simulation.bodies[i];
            if (!body.active || body.type == "Spacecraft") {
                continue;
            }
            const float semiMajorAxis = static_cast<float>(body.semiMajorAxis / PhysicsEngine::AU * SCALE_AU);
            DrawEllipseLines(0, 0, semiMajorAxis,
                             semiMajorAxis * std::sqrt(std::max(0.001, 1.0 - body.eccentricity * body.eccentricity)),
                             alpha(color(body.accent), 0.13f));
        }
    }
    if (simulation.showTrails) {
        for (const Body& body : simulation.bodies) {
            if (!body.active || body.trail.size() <= 1) {
                continue;
            }
            for (std::size_t i = 1; i < body.trail.size(); ++i) {
                const Vector2 a = {body.trail[i - 1].x * SCALE_AU, body.trail[i - 1].y * SCALE_AU};
                const Vector2 b = {body.trail[i].x * SCALE_AU, body.trail[i].y * SCALE_AU};
                DrawLineV(a, b, alpha(color(body.accent), 0.18f));
            }
        }
    }
    if (simulation.showVectors) {
        for (const Body& body : simulation.bodies) {
            if (!body.active || body.type == "Star") {
                continue;
            }
            const Vector2 position = worldToScene(body.position);
            const Vector2 velocity = Vector2Scale({static_cast<float>(body.velocity.x / 30000.0), static_cast<float>(body.velocity.y / 30000.0)}, 40.0f);
            DrawLineV(position, Vector2Add(position, velocity), alpha(color(body.accent), 0.75f));
        }
    }
    for (const Body& body : simulation.bodies) {
        if (body.active) {
            drawBody(body, simulation);
        }
    }
    if (simulation.selected >= 0 && simulation.selected < static_cast<int>(simulation.bodies.size()) &&
        simulation.bodies[simulation.selected].active) {
        const Vector2 position = worldToScreen(simulation.bodies[simulation.selected].position);
        const float radius = std::max(8.0f, static_cast<float>(simulation.bodies[simulation.selected].radius * camera.zoom + 9));
        DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius, {90, 230, 255, 242});
        DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius + 4, {90, 230, 255, 64});
    }
    EndMode2D();
}

int Renderer::hitTest(const Simulation& simulation, Vector2 mouse) const {
    int hit = -1;
    float best = 18.0f;
    for (int i = 0; i < static_cast<int>(simulation.bodies.size()); ++i) {
        if (!simulation.bodies[i].active) {
            continue;
        }
        const Vector2 position = worldToScreen(simulation.bodies[i].position);
        const float distance = Vector2Distance(mouse, position);
        const float radius = std::max(5.0f, static_cast<float>(simulation.bodies[i].radius * camera.zoom));
        if (distance < radius + 7.0f && distance < best) {
            best = distance;
            hit = i;
        }
    }
    return hit;
}

} // namespace bag
