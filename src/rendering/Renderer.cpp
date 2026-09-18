#include "Renderer.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace bag {
namespace {

Color color(ColorRGBA value) { return {value.r, value.g, value.b, value.a}; }

Color alpha(Color value, float amount) {
    value.a = static_cast<unsigned char>(std::clamp(amount * 255.0f, 0.0f, 255.0f));
    return value;
}

float studyVisualPeriodSeconds(const Body& body) {
    // Map real day lengths into a readable wall-clock band while studying, preserving
    // sign (retrograde) and relative ordering (Jupiter fast, Venus/Mercury slow).
    if (!std::isfinite(body.rotationPeriod) || body.rotationPeriod == 0.0) return 12.0f;
    const double sign = body.rotationPeriod < 0.0 ? -1.0 : 1.0;
    const double earthDay = 86400.0;
    const double ratio = std::abs(body.rotationPeriod) / earthDay;
    const double visualAbs = std::clamp(10.0 * std::pow(std::max(ratio, 1.0e-3), 0.32), 3.5, 48.0);
    return static_cast<float>(sign * visualAbs);
}

float spinDegrees(const Body& body, double simTime, bool studyMotion) {
    if (!std::isfinite(body.rotationPeriod) || body.rotationPeriod == 0.0) return 0.0f;
    const double clock = studyMotion ? GetTime() : simTime;
    if (!std::isfinite(clock)) return 0.0f;
    const double period = studyMotion ? studyVisualPeriodSeconds(body) : body.rotationPeriod;
    if (period == 0.0) return 0.0f;
    const double turns = clock / period;
    double degrees = std::fmod(turns * 360.0, 360.0);
    if (degrees < 0.0) degrees += 360.0;
    return static_cast<float>(degrees);
}

void applyBodyOrientation(const Body& body, double simTime, bool studyMotion) {
    // Orbital plane is horizontal (XZ); world up is Y. Tilt then spin around local axis.
    const float tilt = static_cast<float>(std::isfinite(body.axialTilt) ? body.axialTilt : 0.0);
    rlRotatef(tilt, 1.0f, 0.0f, 0.0f);
    rlRotatef(spinDegrees(body, simTime, studyMotion), 0.0f, 1.0f, 0.0f);
}

struct OrbitGeometry {
    float semiMajorAxis = 0.0f;
    float eccentricity = 0.0f;
    float direction = 0.0f;
    bool valid = false;
};

OrbitGeometry orbitGeometry(const Body& body, const Body& centralBody, float displayScale) {
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
    result.semiMajorAxis = static_cast<float>(semiMajorAxis / PhysicsEngine::AU * displayScale);
    result.eccentricity = static_cast<float>(std::clamp(eccentricity, 0.0, 0.999999));
    result.direction = eccentricity > 1.0e-12
        ? static_cast<float>(std::atan2(eccentricityVector.y, eccentricityVector.x))
        : static_cast<float>(std::atan2(relativePosition.y, relativePosition.x));
    result.valid = true;
    return result;
}

} // namespace

Renderer::Renderer(std::filesystem::path planetAssetDirectory) : planetAssets(std::move(planetAssetDirectory)) {
    camera.position = {0.0f, 90.0f, 150.0f};
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 48.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    desiredTarget = target;
    planetAssets.load();
}

void Renderer::unloadAssets() { planetAssets.unload(); }

bool Renderer::consumePendingBodyPick(Vector2& mouseOut) {
    if (!pendingBodyPick) return false;
    pendingBodyPick = false;
    mouseOut = pendingPickMouse;
    return true;
}

bool Renderer::consumePendingFocusPick(Vector2& mouseOut) {
    if (!pendingFocusPick) return false;
    pendingFocusPick = false;
    mouseOut = pendingPickMouse;
    return true;
}

Vector3 Renderer::cameraPosition() const {
    const float horizontal = cameraDistance * std::cos(pitch);
    return {
        target.x + horizontal * std::cos(yaw),
        target.y + cameraDistance * std::sin(pitch),
        target.z + horizontal * std::sin(yaw),
    };
}

float Renderer::focusDistanceFor(const Body& body, const Simulation& simulation) const {
    const float radius = presentationRadius(body, simulation);
    const float halfFov = camera.fovy * DEG2RAD * 0.5f;
    // Z study: sit close enough that the selected globe fills most of the screen.
    if (simulation.soloStudy) {
        const float fillDistance = radius / std::max(1.0e-3f, std::tan(halfFov * 0.78f));
        return std::clamp(fillDistance, 1.2f, 260.0f);
    }
    return std::clamp(radius * 14.0f + 8.0f, 10.0f, 220.0f);
}

void Renderer::focusBodyIndex(const Simulation& simulation, int index, bool snap) {
    if (index < 0 || index >= static_cast<int>(simulation.bodies.size())) return;
    const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
    if (!body.active) return;
    focusBody = index;
    desiredTarget = bodyWorldPosition(body, simulation);
    targetDistance = focusDistanceFor(body, simulation);
    yawVelocity = 0.0f;
    pitchVelocity = 0.0f;
    zoomVelocity = 0.0f;
    if (snap) {
        target = desiredTarget;
        cameraDistance = targetDistance;
        camera.target = target;
        camera.position = cameraPosition();
    }
}

void Renderer::resetSystemView() {
    focusBody = -1;
    hasIsolateView = false;
    desiredTarget = {};
    target = {};
    targetDistance = 180.0f;
    cameraDistance = 180.0f;
    yaw = 0.72f;
    pitch = 0.42f;
    yawVelocity = 0.0f;
    pitchVelocity = 0.0f;
    zoomVelocity = 0.0f;
    camera.target = target;
    camera.position = cameraPosition();
}

void Renderer::saveViewBeforeIsolate() {
    savedCameraDistance = cameraDistance;
    savedTargetDistance = targetDistance;
    savedYaw = yaw;
    savedPitch = pitch;
    savedTarget = target;
    savedDesiredTarget = desiredTarget;
    hasIsolateView = true;
}

void Renderer::restoreViewAfterIsolate() {
    focusBody = -1;
    yawVelocity = 0.0f;
    pitchVelocity = 0.0f;
    zoomVelocity = 0.0f;
    if (!hasIsolateView) {
        resetSystemView();
        return;
    }
    cameraDistance = savedCameraDistance;
    targetDistance = savedTargetDistance;
    yaw = savedYaw;
    pitch = savedPitch;
    target = savedTarget;
    desiredTarget = savedDesiredTarget;
    hasIsolateView = false;
    camera.target = target;
    camera.position = cameraPosition();
}

void Renderer::updateCamera(const Simulation& simulation) {
    if (focusBody >= 0 && (focusBody >= static_cast<int>(simulation.bodies.size()) ||
                           !simulation.bodies[static_cast<std::size_t>(focusBody)].active)) {
        focusBody = -1;
    }
    if (focusBody >= 0 && focusBody < static_cast<int>(simulation.bodies.size()) &&
        simulation.bodies[static_cast<std::size_t>(focusBody)].active) {
        desiredTarget = bodyWorldPosition(simulation.bodies[static_cast<std::size_t>(focusBody)], simulation);
    }

    // Smooth follow for focus / pan target.
    target = Vector3Lerp(target, desiredTarget, 0.14f);
    cameraDistance += (targetDistance - cameraDistance) * 0.16f;

    // Inertia on orbit after release.
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        yaw += yawVelocity;
        pitch = std::clamp(pitch + pitchVelocity, -1.35f, 1.35f);
        yawVelocity *= 0.90f;
        pitchVelocity *= 0.90f;
        if (std::abs(yawVelocity) < 1.0e-5f) yawVelocity = 0.0f;
        if (std::abs(pitchVelocity) < 1.0e-5f) pitchVelocity = 0.0f;
    }

    // Zoom inertia. Allow a closer approach while Z-studying a single body.
    if (zoomVelocity != 0.0f) {
        const float minZoom = simulation.soloStudy ? 1.2f : 5.0f;
        targetDistance = std::clamp(targetDistance * std::exp(-zoomVelocity * 0.08f), minZoom, 1600.0f);
        zoomVelocity *= 0.84f;
        if (std::abs(zoomVelocity) < 1.0e-4f) zoomVelocity = 0.0f;
    }

    camera.target = target;
    camera.position = cameraPosition();
}

void Renderer::update(const Simulation& simulation) {
    const Vector2 mouse = GetMousePosition();
    const Vector2 mouseDelta = GetMouseDelta();
    const float topGuard = 64.0f;
    const float bottomGuard = static_cast<float>(GetScreenHeight()) - 44.0f;
    const bool overScene = mouse.y > topGuard && mouse.y < bottomGuard;

    cameraManipulating = false;

    // Left-drag orbit with click-vs-drag threshold for body picking.
    if (overScene && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        leftPressValid = true;
        leftDragActive = false;
        leftPressPos = mouse;
    }
    if (leftPressValid && IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        const float dragDistance = Vector2Distance(leftPressPos, mouse);
        if (!leftDragActive && dragDistance > 5.0f) {
            leftDragActive = true;
            suppressBodyPick = true;
            yawVelocity = 0.0f;
            pitchVelocity = 0.0f;
        }
        if (leftDragActive) {
            cameraManipulating = true;
            constexpr float ORBIT_SENSITIVITY = 0.0055f;
            const float dyaw = -mouseDelta.x * ORBIT_SENSITIVITY;
            const float dpitch = -mouseDelta.y * ORBIT_SENSITIVITY;
            yaw += dyaw;
            pitch = std::clamp(pitch + dpitch, -1.35f, 1.35f);
            yawVelocity = dyaw;
            pitchVelocity = dpitch;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        if (leftPressValid && !leftDragActive && overScene) {
            pendingBodyPick = true;
            pendingPickMouse = mouse;
        }
        leftPressValid = false;
        leftDragActive = false;
    }

    // Double-click focus: only the second click in a pair, never the first selection click.
    static bool haveClickAnchor = false;
    static double lastClickTime = 0.0;
    static Vector2 lastClickPos{};
    if (pendingBodyPick) {
        const double now = GetTime();
        if (haveClickAnchor && now - lastClickTime < 0.35 &&
            Vector2Distance(lastClickPos, pendingPickMouse) < 10.0f) {
            pendingFocusPick = true;
            pendingBodyPick = false;
            haveClickAnchor = false;
        } else {
            haveClickAnchor = true;
            lastClickTime = now;
            lastClickPos = pendingPickMouse;
        }
    }

    // Right or middle drag pans.
    if (overScene && (IsMouseButtonDown(MOUSE_RIGHT_BUTTON) || IsMouseButtonDown(MOUSE_MIDDLE_BUTTON))) {
        cameraManipulating = true;
        suppressBodyPick = true;
        focusBody = -1;
        const Vector3 forward = Vector3Normalize(Vector3Subtract(camera.target, camera.position));
        const Vector3 right = Vector3Normalize(Vector3CrossProduct(forward, camera.up));
        const Vector3 up = Vector3Normalize(Vector3CrossProduct(right, forward));
        const float panScale = targetDistance * 0.0016f;
        const Vector3 pan = Vector3Add(Vector3Scale(right, -mouseDelta.x * panScale),
                                       Vector3Scale(up, mouseDelta.y * panScale));
        desiredTarget = Vector3Add(desiredTarget, pan);
    }

    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON) && !IsMouseButtonDown(MOUSE_RIGHT_BUTTON) &&
        !IsMouseButtonDown(MOUSE_MIDDLE_BUTTON)) {
        if (!cameraManipulating) suppressBodyPick = false;
    } else if (cameraManipulating) {
        suppressBodyPick = true;
    }

    const float wheel = GetMouseWheelMove();
    if (overScene && wheel != 0.0f) {
        const float boost = (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) ? 2.2f : 1.0f;
        zoomVelocity += wheel * boost;
    }

    if (IsKeyPressed(KEY_F)) {
        fullscreen = !fullscreen;
        ToggleFullscreen();
    }
    if (IsKeyPressed(KEY_HOME)) {
        resetSystemView();
    }
    updateCamera(simulation);
}

float Renderer::displayRadius(const Body& body) const {
    if (body.luminous) return 7.0f;
    const double earthRatio = body.realRadius / PhysicsEngine::EARTH_RADIUS;
    return std::max(0.7f, static_cast<float>(1.55 * std::pow(std::max(earthRatio, 0.01), 0.28)));
}

bool Renderer::isolatingEarthMoon(const Simulation& simulation) const {
    if (!simulation.soloStudy || simulation.selected < 0 ||
        simulation.selected >= static_cast<int>(simulation.bodies.size())) {
        return false;
    }
    const Body& selected = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    return selected.id == "earth" || selected.id == "moon" || selected.type == "Moon";
}

float moonPresentationAngle(const Body& moon, const Body& earth, const Simulation& simulation) {
    // Astronomical XY maps to raylib XZ. Wide view follows live physics phase.
    const Vec3 relative = moon.position - earth.position;
    const float physicsAngle = static_cast<float>(std::atan2(relative.y, relative.x));
    if (!simulation.soloStudy) return physicsAngle;

    // Study mode: wall-clock satellite motion (like spin cues) so the Moon clearly
    // orbits Earth while inspecting — even if the sim is paused.
    constexpr double studyOrbitSeconds = 22.0;
    const double clock = GetTime();
    if (!std::isfinite(clock)) return physicsAngle;
    return static_cast<float>(2.0 * PI * (clock / studyOrbitSeconds));
}

Vector3 Renderer::bodyWorldPosition(const Body& body, const Simulation& simulation) const {
    const Vector3 physicsPos = worldPosition(body.position);
    if (!(body.id == "moon" || body.type == "Moon")) return physicsPos;

    const Body* earth = findBodyById(simulation, "earth");
    if (earth == nullptr || !earth->active) return physicsPos;

    // AU presentation scale buries the Moon inside Earth's display globe. Place it on
    // a readable ring outside Earth; angle follows physics (system) or study clock (Z).
    const Vector3 earthPos = worldPosition(earth->position);
    const float angle = moonPresentationAngle(body, *earth, simulation);
    const Vector3 dir = {std::cos(angle), 0.0f, std::sin(angle)};
    const float earthR = presentationRadius(*earth, simulation);
    const float moonR = presentationRadius(body, simulation);
    const float separation = earthR + moonR + std::max(earthR * 0.55f, 1.2f);
    return Vector3Add(earthPos, Vector3Scale(dir, separation));
}

float Renderer::presentationRadius(const Body& body, const Simulation& simulation) const {
    float radius = displayRadius(body);
    // Z study: grow the selected body into a large inspection globe that fills the screen.
    // Earth/Moon companions stay readable but smaller than the focused body.
    if (simulation.soloStudy && simulation.selected >= 0 &&
        simulation.selected < static_cast<int>(simulation.bodies.size())) {
        const Body& selected = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
        const bool isSelected = body.id == selected.id;
        if (isSelected) {
            radius = std::max(radius * 4.8f, body.luminous ? 18.0f : 8.5f);
        } else if (isolatingEarthMoon(simulation) &&
                   (body.id == "earth" || body.id == "moon" || body.type == "Moon")) {
            radius = std::max(radius * 1.8f, 2.4f);
        }
    }
    if (const PlanetPresentation* presentation = planetAssets.presentation(body.id)) {
        return radius * presentation->visualScale;
    }
    return radius;
}

float Renderer::selectionRadius(const Body& body, const Simulation& simulation) const {
    return presentationSelectionRadius(presentationRadius(body, simulation));
}

const Body* Renderer::findBodyById(const Simulation& simulation, const std::string& id) const {
    if (id.empty()) return nullptr;
    for (const Body& body : simulation.bodies) {
        if (body.id == id) return &body;
    }
    return nullptr;
}

void Renderer::background(const Simulation&) { ClearBackground({7, 11, 18, 255}); }

void Renderer::drawStars(const Simulation& simulation) const {
    for (const Star& star : simulation.stars) {
        const Vector3 position = {star.position.x, 0.0f, star.position.y};
        DrawSphere(position, std::max(0.08f, star.radius * 0.045f), alpha({180, 215, 255, 255}, star.brightness));
    }
}

void Renderer::drawAxes() const {
    DrawLine3D({0, 0, 0}, {28, 0, 0}, alpha(RED, 0.35f));
    DrawLine3D({0, 0, 0}, {0, 28, 0}, alpha(GREEN, 0.35f));
    DrawLine3D({0, 0, 0}, {0, 0, 28}, alpha(BLUE, 0.35f));
}

void Renderer::drawOrbit(const Body& body, const Body& centralBody) const {
    const OrbitGeometry orbit = orbitGeometry(body, centralBody, transform.astronomicalUnitScale);
    if (!orbit.valid) return;
    const Vec3 focusAU = centralBody.position / PhysicsEngine::AU;
    const float semiMinorAxis = orbit.semiMajorAxis * std::sqrt(1.0f - orbit.eccentricity * orbit.eccentricity);
    const float cosine = std::cos(orbit.direction);
    const float sine = std::sin(orbit.direction);
    const float inverseScale = 1.0f / transform.astronomicalUnitScale;
    const Vec3 centerAU = {
        focusAU.x - orbit.semiMajorAxis * inverseScale * orbit.eccentricity * cosine,
        focusAU.y - orbit.semiMajorAxis * inverseScale * orbit.eccentricity * sine,
        0.0,
    };
    const Color orbitColor = alpha(color(body.accent), 0.36f);
    constexpr int ORBIT_SEGMENTS = 160;
    Vector3 previous{};
    for (int segment = 0; segment <= ORBIT_SEGMENTS; ++segment) {
        const float angle = 2.0f * PI * static_cast<float>(segment) / ORBIT_SEGMENTS;
        const float localX = orbit.semiMajorAxis * std::cos(angle);
        const float localY = semiMinorAxis * std::sin(angle);
        const Vec3 pointAU = {
            centerAU.x + (localX * cosine - localY * sine) * inverseScale,
            centerAU.y + (localX * sine + localY * cosine) * inverseScale,
            0.0,
        };
        const Vector3 point = transform.positionAU(pointAU);
        if (segment > 0) DrawLine3D(previous, point, orbitColor);
        previous = point;
    }
}

void Renderer::drawTrail(const Body& body) const {
    // Moon uses an educational display offset; its AU trail would draw inside Earth.
    if (body.id == "moon" || body.type == "Moon") return;
    if (body.trail.size() <= 1) return;
    for (std::size_t index = 1; index < body.trail.size(); ++index) {
        const Vector3 a = transform.positionAU(body.trail[index - 1]);
        const Vector3 b = transform.positionAU(body.trail[index]);
        DrawLine3D(a, b, alpha(color(body.accent), 0.32f));
    }
}

void Renderer::drawMoonOrbit(const Simulation& simulation) const {
    const Body* earth = findBodyById(simulation, "earth");
    const Body* moon = findBodyById(simulation, "moon");
    if (earth == nullptr || moon == nullptr || !earth->active || !moon->active) return;

    int moonIndex = -1;
    for (int i = 0; i < static_cast<int>(simulation.bodies.size()); ++i) {
        if (simulation.bodies[static_cast<std::size_t>(i)].id == moon->id) {
            moonIndex = i;
            break;
        }
    }
    if (moonIndex < 0 || !simulation.bodyVisibleInView(moonIndex)) return;

    const Vector3 earthPos = bodyWorldPosition(*earth, simulation);
    const Vector3 moonPos = bodyWorldPosition(*moon, simulation);
    const float radius = Vector3Distance(earthPos, moonPos);
    if (radius < 1.0e-4f) return;
    const Color orbitColor = alpha(color(moon->accent), simulation.soloStudy ? 0.55f : 0.40f);
    constexpr int segments = 96;
    Vector3 previous{};
    for (int segment = 0; segment <= segments; ++segment) {
        const float angle = 2.0f * PI * static_cast<float>(segment) / static_cast<float>(segments);
        const Vector3 point = {
            earthPos.x + radius * std::cos(angle),
            earthPos.y,
            earthPos.z + radius * std::sin(angle),
        };
        if (segment > 0) DrawLine3D(previous, point, orbitColor);
        previous = point;
    }
}

void Renderer::drawVector(const Body& body, const Simulation& simulation) const {
    const Vec3 value = body.velocity;
    const Vector3 start = bodyWorldPosition(body, simulation);
    constexpr double scale = 1.0e-4;
    const Vector3 end = Vector3Add(start, transform.vector(value, scale));
    DrawLine3D(start, end, alpha(color(body.accent), 0.78f));
    DrawSphere(end, 0.16f, alpha(color(body.accent), 0.9f));
}

void Renderer::drawStudySpinCues(const Body& body, float radius) const {
    const Color axis = alpha({255, 214, 120, 255}, 0.92f);
    const Color equator = alpha({120, 220, 255, 255}, 0.78f);
    const Color tick = alpha({255, 160, 90, 255}, 0.95f);
    const float pole = radius * 1.55f;
    // Pole axis (tilt) — lines only, no floating “moonlet” spheres.
    DrawLine3D({0.0f, -pole, 0.0f}, {0.0f, pole, 0.0f}, axis);
    DrawLine3D({0.02f * radius, -pole, 0.0f}, {0.02f * radius, pole, 0.0f}, axis);
    DrawLine3D({-0.02f * radius, -pole, 0.0f}, {-0.02f * radius, pole, 0.0f}, axis);
    DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.03f, {1.0f, 0.0f, 0.0f}, 90.0f, equator);
    // Flat radial tick on the equator so spin reads clearly without looking like a satellite.
    DrawLine3D({radius * 0.82f, 0.0f, 0.0f}, {radius * 1.18f, 0.0f, 0.0f}, tick);
    DrawLine3D({radius * 0.82f, 0.02f * radius, 0.0f}, {radius * 1.18f, 0.02f * radius, 0.0f}, tick);
    if (body.rotationPeriod < 0.0) {
        DrawLine3D({-radius * 0.82f, 0.0f, 0.0f}, {-radius * 1.18f, 0.0f, 0.0f}, alpha(tick, 0.8f));
    }
}

void Renderer::drawBody(const Body& body, const Simulation& simulation) const {
    const Vector3 position = bodyWorldPosition(body, simulation);
    const float radius = presentationRadius(body, simulation);
    const Color bodyColor = color(body.color);
    const Color accentColor = color(body.accent);
    const Model* model = planetAssets.model(body.id);
    const PlanetPresentation* presentation = planetAssets.presentation(body.id);
    const bool studyMotion = simulation.soloStudy;
    const bool showStudyAxis = simulation.soloStudy &&
        simulation.selected >= 0 &&
        simulation.selected < static_cast<int>(simulation.bodies.size()) &&
        simulation.bodies[static_cast<std::size_t>(simulation.selected)].id == body.id;
    if (model != nullptr && presentation != nullptr) {
        const float localRadius = planetAssets.modelRadius(body.id);
        const float scale = radius / localRadius;
        const Vector3 center = planetAssets.modelCenter(body.id);
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, simulation.simTime, studyMotion);
        if (showStudyAxis) drawStudySpinCues(body, radius);
        rlTranslatef(presentation->localTranslation.x * scale,
                     presentation->localTranslation.y * scale,
                     presentation->localTranslation.z * scale);
        rlRotatef(presentation->localRotationDegrees.z, 0.0f, 0.0f, 1.0f);
        rlRotatef(presentation->localRotationDegrees.y, 0.0f, 1.0f, 0.0f);
        rlRotatef(presentation->localRotationDegrees.x, 1.0f, 0.0f, 0.0f);
        rlTranslatef(-center.x * scale, -center.y * scale, -center.z * scale);
        rlScalef(scale, scale, scale);
        DrawModel(*model, {0.0f, 0.0f, 0.0f}, 1.0f, WHITE);
        rlPopMatrix();
    } else if (body.luminous) {
        DrawSphere(position, radius * 1.8f, alpha(accentColor, 0.08f));
        DrawSphere(position, radius * 1.35f, alpha(accentColor, 0.2f));
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, simulation.simTime, studyMotion);
        DrawSphereEx({0.0f, 0.0f, 0.0f}, radius, 24, 32, bodyColor);
        // Surface tint only — no detached sphere that reads as another body.
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 0.98f, {0.0f, 1.0f, 0.0f}, 90.0f, alpha(accentColor, 0.35f));
        if (showStudyAxis) drawStudySpinCues(body, radius);
        rlPopMatrix();
    } else {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, simulation.simTime, studyMotion);
        DrawSphereEx({0.0f, 0.0f, 0.0f}, radius, 16, 24, bodyColor);
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.02f, {1.0f, 0.0f, 0.0f}, 90.0f, alpha(accentColor, 0.55f));
        if (showStudyAxis) drawStudySpinCues(body, radius);
        rlPopMatrix();
    }
    if (body.ringed && (presentation == nullptr || !presentation->modelIncludesRings)) {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, simulation.simTime, studyMotion);
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.75f, {1.0f, 0.0f, 0.0f}, 90.0f, alpha(accentColor, 0.72f));
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.35f, {1.0f, 0.0f, 0.0f}, 90.0f, alpha(bodyColor, 0.55f));
        rlPopMatrix();
    }
}

void Renderer::scene(const Simulation& simulation) {
    const Body* selectedBody = nullptr;
    // Close near-plane while studying so large inspection globes are not clipped.
    if (simulation.soloStudy) rlSetClipPlanes(0.001, 10000.0);
    else rlSetClipPlanes(0.05, 10000.0);
    BeginMode3D(camera);
    drawStars(simulation);
    if (simulation.showGrid) DrawGrid(40, 10.0f);
    if (simulation.showGrid) drawAxes();
    if (!simulation.bodies.empty()) {
        const Body& centralBody = simulation.bodies.front();
        if (simulation.showOrbits) {
            for (std::size_t index = 1; index < simulation.bodies.size(); ++index) {
                if (!simulation.bodyVisibleInView(static_cast<int>(index))) continue;
                const Body& body = simulation.bodies[index];
                if (!body.active || body.type == "Spacecraft") continue;
                // Moon uses the educational ring from drawMoonOrbit instead of AU-scale ellipse.
                if (body.id == "moon" || body.type == "Moon") continue;
                const Body* orbitParent = &centralBody;
                if (!body.parentId.empty()) {
                    if (const Body* parent = findBodyById(simulation, body.parentId)) orbitParent = parent;
                }
                drawOrbit(body, *orbitParent);
            }
        }
        drawMoonOrbit(simulation);
        if (simulation.showTrails) {
            for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                if (!simulation.bodyVisibleInView(index)) continue;
                const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
                if (body.active) drawTrail(body);
            }
        }
        if (simulation.showVectors) {
            for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                if (!simulation.bodyVisibleInView(index)) continue;
                const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
                if (body.active && body.type != "Star") drawVector(body, simulation);
            }
        }
        for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
            if (!simulation.bodyVisibleInView(index)) continue;
            const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
            if (!body.active) continue;
            drawBody(body, simulation);
        }
        if (simulation.selected >= 0 && simulation.selected < static_cast<int>(simulation.bodies.size()) &&
            simulation.bodies[static_cast<std::size_t>(simulation.selected)].active) {
            const Body& body = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
            if (selectionIndicatorVisible(simulation.settings.selectionHighlightEnabled, simulation.selected, body.active)) {
                selectedBody = &body;
            }
        }
    }
    EndMode3D();
    if (selectedBody != nullptr) drawSelectionIndicator(*selectedBody, simulation);
}

void Renderer::drawSelectionIndicator(const Body& body, const Simulation& simulation) const {
    // Screen-space brackets only. Never DrawSphereWires / 3D shells — those were
    // previously mistaken for selection when Earth's blue ocean limb aliased
    // against the dark background. Amber avoids colliding with Earth cyan/blue.
    const Vector3 center = bodyWorldPosition(body, simulation);
    const Vector2 screenCenter = GetWorldToScreen(center, camera);
    const Vector2 screenEdge = GetWorldToScreen(Vector3Add(center, {0.0f, presentationRadius(body, simulation), 0.0f}), camera);
    const float bodyPixels = Vector2Distance(screenCenter, screenEdge);
    const float radius = std::clamp(bodyPixels + 8.0f, 14.0f, simulation.soloStudy ? 220.0f : 64.0f);
    const float corner = std::clamp(radius * 0.38f, 6.0f, 16.0f);
    const Color tick = {255, 214, 120, 230};
    const float left = screenCenter.x - radius;
    const float right = screenCenter.x + radius;
    const float top = screenCenter.y - radius;
    const float bottom = screenCenter.y + radius;
    constexpr float thickness = 2.0f;
    DrawLineEx({left, top + corner}, {left, top}, thickness, tick);
    DrawLineEx({left, top}, {left + corner, top}, thickness, tick);
    DrawLineEx({right - corner, top}, {right, top}, thickness, tick);
    DrawLineEx({right, top}, {right, top + corner}, thickness, tick);
    DrawLineEx({left, bottom - corner}, {left, bottom}, thickness, tick);
    DrawLineEx({left, bottom}, {left + corner, bottom}, thickness, tick);
    DrawLineEx({right - corner, bottom}, {right, bottom}, thickness, tick);
    DrawLineEx({right, bottom - corner}, {right, bottom}, thickness, tick);
}

int Renderer::hitTest(const Simulation& simulation, Vector2 mouse) const {
    const Ray ray = GetMouseRay(mouse, camera);
    float nearest = std::numeric_limits<float>::infinity();
    int selected = -1;
    for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
        if (!simulation.bodyVisibleInView(index)) continue;
        const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
        if (!body.active) continue;
        const RayCollision collision =
            GetRayCollisionSphere(ray, bodyWorldPosition(body, simulation), selectionRadius(body, simulation));
        if (collision.hit && collision.distance < nearest) {
            nearest = collision.distance;
            selected = index;
        }
    }
    return selected;
}

} // namespace bag
