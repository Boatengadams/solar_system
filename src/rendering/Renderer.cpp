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

struct OrbitGeometry {
    float semiMajorAxis = 0.0f;
    float eccentricity = 0.0f;
    float direction = 0.0f;
    bool valid = false;
};

OrbitGeometry orbitGeometry(const Body& body, const Body& centralBody) {
    OrbitGeometry result;
    const Vec3 relativePosition = body.position - centralBody.position;
    const Vec3 relativeVelocity = body.velocity - centralBody.velocity;
    const double radius = length(relativePosition);
    const double mu = PhysicsEngine::G * centralBody.mass;
    if (radius <= 0.0 || mu <= 0.0) return result;

    const double speedSquared = dot(relativeVelocity, relativeVelocity);
    const double radialVelocity = dot(relativePosition, relativeVelocity);
    const Vec3 eccentricityVector =
        (relativePosition * (speedSquared - mu / radius) - relativeVelocity * radialVelocity) / mu;
    const double eccentricity = length(eccentricityVector);
    const double specificEnergy = 0.5 * speedSquared - mu / radius;
    if (!std::isfinite(eccentricity) || !std::isfinite(specificEnergy) || specificEnergy >= 0.0 || eccentricity >= 1.0) {
        return result;
    }

    const double semiMajorAxis = -mu / (2.0 * specificEnergy);
    if (!std::isfinite(semiMajorAxis) || semiMajorAxis <= 0.0) return result;
    result.semiMajorAxis = static_cast<float>(semiMajorAxis / PhysicsEngine::AU * Renderer::SCALE_AU);
    result.eccentricity = static_cast<float>(std::clamp(eccentricity, 0.0, 0.999999));
    result.direction = eccentricity > 1.0e-12
        ? static_cast<float>(std::atan2(eccentricityVector.y, eccentricityVector.x))
        : static_cast<float>(std::atan2(relativePosition.y, relativePosition.x));
    result.valid = true;
    return result;
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
    // Keep the camera and hit testing aligned with the actual drawable size,
    // including fullscreen and high-DPI window changes.
    camera.offset = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
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
        // ToggleFullscreen can change the drawable size immediately. Keep
        // the camera used by the next click and the next frame in sync.
        camera.offset = {GetScreenWidth() / 2.0f, GetScreenHeight() / 2.0f};
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

float Renderer::displayRadius(const Body& body) const {
    if (body.luminous) return std::max(12.0f, static_cast<float>(body.radius * camera.zoom));
    // Body::radius is an existing presentation value. Scale it by the
    // authoritative physical radius ratio so the visual hierarchy remains
    // scientifically honest without changing physics data.
    const double earthRatio = body.realRadius / PhysicsEngine::EARTH_RADIUS;
    const float relativeSize = static_cast<float>(std::pow(std::max(earthRatio, 0.01), 0.28));
    return std::max(2.0f, 5.5f * relativeSize * camera.zoom);
}

float Renderer::selectionRadius(const Body& body) const {
    return std::max(9.0f, displayRadius(body) + 8.0f);
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
#if RAYLIB_VERSION_MAJOR >= 6
    DrawCircleGradient(Vector2{0.0f, 0.0f}, 4300.0f,
                       alpha({50, 65, 100, 255}, 0.035f), alpha({3, 6, 14, 255}, 0.0f));
#else
    DrawCircleGradient(0, 0, 4300.0f,
                       alpha({50, 65, 100, 255}, 0.035f), alpha({3, 6, 14, 255}, 0.0f));
#endif
    EndMode2D();
}

void Renderer::glow(Vector2 position, float radius, Color drawColor, int layers) const {
    for (int i = layers; i >= 1; --i) {
        const float t = static_cast<float>(i) / layers;
        DrawCircleV(position, radius * (1.0f + 1.8f * t), alpha(drawColor, 0.012f + 0.03f * (1.0f - t)));
    }
}

void Renderer::drawBody(const Body& body, const Simulation& simulation) const {
    // Bodies share the world/scene coordinate system with orbit paths, trails,
    // and vectors. BeginMode2D applies the camera transform exactly once.
    const Vector2 position = worldToScene(body.position);
    const float radius = displayRadius(body) / camera.zoom;
    const Color bodyColor = color(body.color);
    const Color accentColor = color(body.accent);
    if (body.luminous) {
        glow(position, radius * 1.8f, bodyColor, 14);
#if RAYLIB_VERSION_MAJOR >= 6
        DrawCircleGradient(position, radius * 2.0f, accentColor, alpha(bodyColor, 0.0f));
#else
        DrawCircleGradient(static_cast<int>(position.x), static_cast<int>(position.y),
                           radius * 2.0f, accentColor, alpha(bodyColor, 0.0f));
#endif
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
    const Vector2 sun = worldToScene(simulation.bodies[0].position);
    const Vector2 direction = Vector2Normalize(Vector2Subtract(sun, position));
    DrawCircleV(Vector2Add(position, Vector2Scale(direction, radius * 0.30f)), radius * 0.48f, alpha(WHITE, 0.16f));
    DrawCircleV(Vector2Add(position, Vector2Scale(direction, -radius * 0.35f)), radius * 0.62f, alpha(BLACK, 0.18f));
    if (body.name == "Earth" || body.name == "Venus" || body.name == "Mars") {
        DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius + 2.0f, alpha(accentColor, 0.7f));
    }
}

void Renderer::scene(const Simulation& simulation) {
    BeginMode2D(camera);
    if (simulation.showOrbits && !simulation.bodies.empty()) {
        const Body& centralBody = simulation.bodies.front();
        const Vector2 focus = worldToScene(centralBody.position);
        for (std::size_t i = 1; i < simulation.bodies.size(); ++i) {
            const Body& body = simulation.bodies[i];
            if (!body.active || body.type == "Spacecraft") {
                continue;
            }
            const OrbitGeometry orbit = orbitGeometry(body, centralBody);
            if (!orbit.valid) continue;
            const float semiMajorAxis = orbit.semiMajorAxis;
            const float eccentricity = orbit.eccentricity;
            const float direction = orbit.direction;
            const float cosine = std::cos(direction);
            const float sine = std::sin(direction);
            const float semiMinorAxis = semiMajorAxis * std::sqrt(1.0f - eccentricity * eccentricity);
            const Vector2 center = {
                focus.x - semiMajorAxis * eccentricity * cosine,
                focus.y - semiMajorAxis * eccentricity * sine,
            };
            const Color orbitColor = alpha(color(body.accent), 0.13f);
            constexpr int ORBIT_SEGMENTS = 128;
            Vector2 previous = {};
            for (int segment = 0; segment <= ORBIT_SEGMENTS; ++segment) {
                const float angle = 2.0f * PI * static_cast<float>(segment) / ORBIT_SEGMENTS;
                const float localX = semiMajorAxis * std::cos(angle);
                const float localY = semiMinorAxis * std::sin(angle);
                const Vector2 point = {
                    center.x + localX * cosine - localY * sine,
                    center.y + localX * sine + localY * cosine,
                };
                if (segment > 0) DrawLineV(previous, point, orbitColor);
                previous = point;
            }
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
        if (body.active) drawBody(body, simulation);
    }

    // The selection ring follows the selected body's current world position in
    // the same camera pass as the body itself.
    if (simulation.selected >= 0 && simulation.selected < static_cast<int>(simulation.bodies.size()) &&
        simulation.bodies[simulation.selected].active) {
        const Body& selectedBody = simulation.bodies[simulation.selected];
        const Vector2 position = worldToScene(selectedBody.position);
        const float radius = selectionRadius(selectedBody) / camera.zoom;
        DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius, {90, 230, 255, 242});
        DrawCircleLines(static_cast<int>(position.x), static_cast<int>(position.y), radius + 4.0f / camera.zoom, {90, 230, 255, 64});
    }
    EndMode2D();
}

int Renderer::hitTest(const Simulation& simulation, Vector2 mouse) const {
    std::vector<SelectionCandidate> candidates;
    candidates.reserve(simulation.bodies.size());
    for (int i = 0; i < static_cast<int>(simulation.bodies.size()); ++i) {
        if (!simulation.bodies[i].active) {
            continue;
        }
        const Vector2 position = worldToScreen(simulation.bodies[i].position);
        candidates.push_back({i, {position.x, position.y}, selectionRadius(simulation.bodies[i])});
    }
    return selectNearestBody({mouse.x, mouse.y}, candidates);
}

} // namespace bag
