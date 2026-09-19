#include "Renderer.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "../core/Logger.hpp"
#include "../ui/UiTheme.hpp"

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

float spinDegrees(const Body& body, double clockSeconds, bool studyMotion) {
    if (!std::isfinite(body.rotationPeriod) || body.rotationPeriod == 0.0) return 0.0f;
    if (!std::isfinite(clockSeconds)) return 0.0f;
    const double period = studyMotion ? studyVisualPeriodSeconds(body) : body.rotationPeriod;
    if (period == 0.0) return 0.0f;
    const double turns = clockSeconds / period;
    double degrees = std::fmod(turns * 360.0, 360.0);
    if (degrees < 0.0) degrees += 360.0;
    return static_cast<float>(degrees);
}

void applyBodyOrientation(const Body& body, double clockSeconds, bool studyMotion) {
    // Orbital plane is horizontal (XZ); world up is Y. Tilt then spin around local axis.
    const float tilt = static_cast<float>(std::isfinite(body.axialTilt) ? body.axialTilt : 0.0);
    rlRotatef(tilt, 1.0f, 0.0f, 0.0f);
    rlRotatef(spinDegrees(body, clockSeconds, studyMotion), 0.0f, 1.0f, 0.0f);
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

constexpr const char* PLANET_LIGHT_VERTEX_SHADER = R"GLSL(
#version 330
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 worldPosition;
out vec3 worldNormal;
void main() {
    worldPosition = vec3(matModel * vec4(vertexPosition, 1.0));
    worldNormal = normalize(vec3(matNormal * vec4(vertexNormal, 0.0)));
    fragTexCoord = vertexTexCoord;
    fragColor = vertexColor;
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)GLSL";

constexpr const char* PLANET_LIGHT_FRAGMENT_SHADER = R"GLSL(
#version 330
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 worldPosition;
in vec3 worldNormal;
uniform sampler2D texture0;
uniform vec4 colDiffuse;
uniform vec3 sunPosition;
out vec4 finalColor;
void main() {
    vec4 surface = texture(texture0, fragTexCoord) * colDiffuse * fragColor;
    vec3 lightDirection = normalize(sunPosition - worldPosition);
    float sunFacing = dot(normalize(worldNormal), lightDirection);
    // A soft terminator reads much more naturally than a hard day/night split,
    // while still leaving a clearly visible night side for classroom discussion.
    float direct = smoothstep(-0.10, 0.24, sunFacing);
    float horizon = pow(1.0 - abs(clamp(sunFacing, -1.0, 1.0)), 3.0);
    float dayGlow = horizon * smoothstep(-0.08, 0.32, sunFacing);
    vec3 illumination = vec3(0.055) + vec3(0.945, 0.855, 0.680) * direct;
    // The thin warm band at the terminator suggests a dense, light-scattering
    // atmosphere without hiding the supplied planetary surface textures.
    vec3 atmosphere = vec3(1.0, 0.31, 0.07) * dayGlow * 0.18;
    finalColor = vec4(surface.rgb * illumination + atmosphere, surface.a);
}
)GLSL";

} // namespace

Renderer::Renderer(std::filesystem::path planetAssetDirectory) : planetAssets(planetAssetDirectory) {
    camera.target = {0.0f, 0.0f, 0.0f};
    camera.up = {0.0f, 1.0f, 0.0f};
    camera.fovy = 46.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    // Galaxy overview: the 1,200-unit solar-system span is 15% of the 8,000-unit
    // Milky Way. Scroll in or focus a body to move from this contextual view.
    cameraDistance = 6200.0f;
    targetDistance = 6200.0f;
    pitch = 0.90f;
    yaw = 0.72f;
    desiredTarget = target;
    camera.position = cameraPosition();
    const char* traceEnv = std::getenv("BAGS_LAB_STARTUP_TRACE");
    const bool trace = traceEnv != nullptr && traceEnv[0] != '\0' && traceEnv[0] != '0';
    const auto loadStart = std::chrono::steady_clock::now();
    planetAssets.load();
    loadPlanetLighting();
    loadMilkyWayAsset(planetAssetDirectory.parent_path() / "milkyway.glb");
    if (trace) {
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loadStart).count();
        logInfo("[STARTUP] Planet models loaded (" + std::to_string(ms) + " ms)");
    }
}

void Renderer::unloadAssets() {
    if (spaceBackground.id != 0) {
        UnloadRenderTexture(spaceBackground);
        spaceBackground = {};
        bgWidth = 0;
        bgHeight = 0;
    }
    milkyWayPoints.clear();
    milkyWaySourceDiameter = 0.0f;
    milkyWayCenter = {};
    planetAssets.unload();
    if (planetLightShader.id != 0) {
        UnloadShader(planetLightShader);
        planetLightShader = {};
        sunLightPositionLocation = -1;
    }
}

void Renderer::loadPlanetLighting() {
    planetLightShader = LoadShaderFromMemory(PLANET_LIGHT_VERTEX_SHADER, PLANET_LIGHT_FRAGMENT_SHADER);
    if (planetLightShader.id == 0) {
        logError("Unable to create the planet sunlight shader; using unlit planet models");
        return;
    }
    sunLightPositionLocation = GetShaderLocation(planetLightShader, "sunPosition");
}

void Renderer::setPlanetLightingEnabled(bool enabled) {
    planetLightingEnabled_ = enabled && planetLightShader.id != 0;
    const Shader defaultShader = {rlGetShaderIdDefault(), rlGetShaderLocsDefault()};
    for (const char* bodyId : {"mercury", "venus", "earth", "moon", "mars", "jupiter", "saturn", "uranus", "neptune"}) {
        planetAssets.setShader(bodyId, planetLightingEnabled_ ? planetLightShader : defaultShader);
    }
}

void Renderer::togglePlanetLighting() {
    setPlanetLightingEnabled(!planetLightingEnabled_);
}

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
    if (simulation.soloStudy && !simulation.earthMoonFocus) {
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

void Renderer::focusEarthMoonSystem(const Simulation& simulation, bool snap) {
    const Body* earth = findBodyById(simulation, "earth");
    const Body* moon = findBodyById(simulation, "moon");
    if (earth == nullptr || !earth->active) {
        resetSystemView();
        return;
    }

    int earthIndex = -1;
    for (int i = 0; i < static_cast<int>(simulation.bodies.size()); ++i) {
        if (simulation.bodies[static_cast<std::size_t>(i)].id == "earth") {
            earthIndex = i;
            break;
        }
    }
    focusBody = earthIndex;

    const Vector3 earthPos = bodyWorldPosition(*earth, simulation);
    if (moon == nullptr || !moon->active) {
        focusBodyIndex(simulation, earthIndex, snap);
        return;
    }

    // Bounding sphere around Earth + Moon presentation positions + radii.
    const Vector3 moonPos = bodyWorldPosition(*moon, simulation);
    const Vector3 mid = Vector3Scale(Vector3Add(earthPos, moonPos), 0.5f);
    const float earthR = presentationRadius(*earth, simulation);
    const float moonR = presentationRadius(*moon, simulation);
    const float separation = Vector3Distance(earthPos, moonPos);
    const float span = separation + earthR + moonR;
    const float halfFov = camera.fovy * DEG2RAD * 0.5f;
    const float fitDistance = (span * 0.55f) / std::max(1.0e-3f, std::tan(halfFov * 0.85f));

    desiredTarget = mid;
    targetDistance = std::clamp(fitDistance, 4.0f, 120.0f);
    pitch = 0.55f;
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
    targetDistance = 6200.0f;
    cameraDistance = 6200.0f;
    yaw = 0.72f;
    pitch = 0.90f;
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
        targetDistance = std::clamp(targetDistance * std::exp(-zoomVelocity * 0.08f), minZoom, 14000.0f);
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

float Renderer::displayRadius(const Body& body, const Simulation& simulation) const {
    const double earthRatio = std::max(body.realRadius / PhysicsEngine::EARTH_RADIUS, 1.0e-6);

    if (simulation.visualScaleMode == VisualScaleMode::Realistic) {
        // Physically proportional radii in presentation units (Earth ≈ 1).
        // At true AU distances planets are tiny — that is intentional.
        if (body.luminous) return static_cast<float>(earthRatio * 0.35);
        return static_cast<float>(earthRatio * 0.35);
    }

    // Educational display scale: preserve ordering (Sun ≫ Jupiter ≫ Earth ≫ Moon)
    // while keeping small bodies visible. Physics still uses realRadius / mass.
    if (body.luminous) return 7.0f;
    return std::max(0.7f, static_cast<float>(1.55 * std::pow(earthRatio, 0.28)));
}

bool Renderer::isolatingEarthMoon(const Simulation& simulation) const {
    if (simulation.earthMoonFocus) return true;
    if (!simulation.soloStudy || simulation.selected < 0 ||
        simulation.selected >= static_cast<int>(simulation.bodies.size())) {
        return false;
    }
    const Body& selected = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    return selected.id == "earth" || selected.id == "moon" || selected.type == "Moon";
}

double Renderer::presentationClock(const Simulation& simulation) const {
    // Physics time (simTime) freezes when paused. Study-mode spin previously used
    // GetTime() and kept animating during pause — that looked like fake orbital motion
    // when zooming. Hold the wall clock while paused so only the camera may change.
    if (simulation.soloStudy || simulation.earthMoonFocus) {
        if (!simulation.paused) studyPresentationClock = GetTime();
        return studyPresentationClock;
    }
    return simulation.simTime;
}

float moonPresentationAngle(const Body& moon, const Body& earth, const Simulation& simulation,
                            double studyClock) {
    // Astronomical XY maps to raylib XZ. Wide view follows live physics phase.
    const Vec3 relative = moon.position - earth.position;
    const float physicsAngle = static_cast<float>(std::atan2(relative.y, relative.x));
    if (!simulation.soloStudy && !simulation.earthMoonFocus) return physicsAngle;

    // Study / F2: readable satellite motion — frozen when paused (studyClock held).
    constexpr double studyOrbitSeconds = 22.0;
    if (!std::isfinite(studyClock)) return physicsAngle;
    return static_cast<float>(2.0 * PI * (studyClock / studyOrbitSeconds));
}

Vector3 Renderer::bodyWorldPosition(const Body& body, const Simulation& simulation) const {
    const Vector3 physicsPos = worldPosition(body.position);
    if (!(body.id == "moon" || body.type == "Moon")) return physicsPos;

    const Body* earth = findBodyById(simulation, "earth");
    if (earth == nullptr || !earth->active) return physicsPos;

    // AU presentation scale buries the Moon inside Earth's display globe. Place it on
    // a readable ring outside Earth; angle follows physics (system) or study clock (Z/F2).
    const Vector3 earthPos = worldPosition(earth->position);
    const float angle = moonPresentationAngle(body, *earth, simulation, presentationClock(simulation));
    const Vector3 dir = {std::cos(angle), 0.0f, std::sin(angle)};
    const float earthR = presentationRadius(*earth, simulation);
    const float moonR = presentationRadius(body, simulation);
    const float separation = earthR + moonR + std::max(earthR * 0.55f, 1.2f);
    return Vector3Add(earthPos, Vector3Scale(dir, separation));
}

float Renderer::presentationRadius(const Body& body, const Simulation& simulation) const {
    float radius = displayRadius(body, simulation);
    // Z study: grow the selected body into a large inspection globe that fills the screen.
    // Earth/Moon companions stay readable but smaller than the focused body.
    // F2 system focus: enlarge the pair without collapsing to a single-body fill.
    if (simulation.earthMoonFocus &&
        (body.id == "earth" || body.id == "moon" || body.type == "Moon")) {
        radius = std::max(radius * 2.6f, body.id == "earth" ? 4.5f : 2.2f);
    } else if (simulation.soloStudy && simulation.selected >= 0 &&
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

void Renderer::loadMilkyWayAsset(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        logInfo("Milky Way asset not found; using the procedural starfield");
        return;
    }

    input.seekg(0, std::ios::end);
    const std::streamoff size = input.tellg();
    input.seekg(0, std::ios::beg);
    if (size < 28) {
        logError("Milky Way asset is too small; using the procedural starfield");
        return;
    }
    std::vector<unsigned char> bytes(static_cast<std::size_t>(size));
    if (!input.read(reinterpret_cast<char*>(bytes.data()), size)) {
        logError("Unable to read Milky Way asset; using the procedural starfield");
        return;
    }
    const auto u32At = [&bytes](std::size_t offset) -> std::uint32_t {
        if (offset + 4 > bytes.size()) return 0;
        return static_cast<std::uint32_t>(bytes[offset]) |
               (static_cast<std::uint32_t>(bytes[offset + 1]) << 8U) |
               (static_cast<std::uint32_t>(bytes[offset + 2]) << 16U) |
               (static_cast<std::uint32_t>(bytes[offset + 3]) << 24U);
    };
    const std::size_t jsonLength = u32At(12);
    constexpr std::uint32_t GLB_MAGIC = 0x46546c67U;
    constexpr std::uint32_t JSON_CHUNK = 0x4e4f534aU;
    constexpr std::uint32_t BIN_CHUNK = 0x004e4942U;
    if (u32At(0) != GLB_MAGIC || u32At(4) != 2U || u32At(16) != JSON_CHUNK ||
        20 + jsonLength + 8 > bytes.size() || u32At(20 + jsonLength + 4) != BIN_CHUNK) {
        logError("Milky Way asset is not a supported GLB file; using the procedural starfield");
        return;
    }

    try {
        const auto gltf = nlohmann::json::parse(std::string(reinterpret_cast<const char*>(bytes.data() + 20), jsonLength));
        const std::size_t binaryOffset = 20 + jsonLength + 8;
        const auto& accessors = gltf.at("accessors");
        const auto& views = gltf.at("bufferViews");
        const auto& meshes = gltf.at("meshes");
        std::size_t totalPoints = 0;
        for (const auto& mesh : meshes) for (const auto& primitive : mesh.at("primitives")) {
            if (primitive.value("mode", 4) == 0 && primitive.contains("attributes") &&
                primitive.at("attributes").contains("POSITION")) {
                totalPoints += accessors.at(primitive.at("attributes").at("POSITION").get<std::size_t>()).at("count").get<std::size_t>();
            }
        }
        // The source is a 2.9-million-particle point cloud. A deterministic 40k
        // sample keeps it fluid on modest laboratory GPUs while preserving its shape.
        const std::size_t stride = std::max<std::size_t>(1, (totalPoints + 39999) / 40000);
        milkyWayPoints.reserve((totalPoints + stride - 1) / stride);
        Vector3 minimum{std::numeric_limits<float>::max(), std::numeric_limits<float>::max(), std::numeric_limits<float>::max()};
        Vector3 maximum{-minimum.x, -minimum.y, -minimum.z};
        const auto readFloat = [&bytes](std::size_t offset) {
            float value = 0.0f;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        };
        for (const auto& mesh : meshes) for (const auto& primitive : mesh.at("primitives")) {
            if (primitive.value("mode", 4) != 0 || !primitive.contains("attributes")) continue;
            const auto& attributes = primitive.at("attributes");
            if (!attributes.contains("POSITION") || !attributes.contains("COLOR_0")) continue;
            const auto& positions = accessors.at(attributes.at("POSITION").get<std::size_t>());
            const auto& colors = accessors.at(attributes.at("COLOR_0").get<std::size_t>());
            if (positions.value("componentType", 0) != 5126 || positions.value("type", "") != "VEC3" ||
                colors.value("componentType", 0) != 5126 || colors.value("type", "") != "VEC4") continue;
            const auto& positionView = views.at(positions.at("bufferView").get<std::size_t>());
            const auto& colorView = views.at(colors.at("bufferView").get<std::size_t>());
            const std::size_t count = std::min(positions.at("count").get<std::size_t>(), colors.at("count").get<std::size_t>());
            const std::size_t positionBase = binaryOffset + positionView.value("byteOffset", 0U) + positions.value("byteOffset", 0U);
            const std::size_t colorBase = binaryOffset + colorView.value("byteOffset", 0U) + colors.value("byteOffset", 0U);
            const std::size_t positionStride = positionView.value("byteStride", 12U);
            const std::size_t colorStride = colorView.value("byteStride", 16U);
            if (positionBase + (count ? (count - 1) * positionStride + 12 : 0) > bytes.size() ||
                colorBase + (count ? (count - 1) * colorStride + 16 : 0) > bytes.size()) continue;
            for (std::size_t index = 0; index < count; index += stride) {
                const Vector3 point = {readFloat(positionBase + index * positionStride),
                                       readFloat(positionBase + index * positionStride + 4),
                                       readFloat(positionBase + index * positionStride + 8)};
                const auto channel = [&readFloat, colorBase, colorStride, index](std::size_t component) {
                    return static_cast<unsigned char>(std::clamp(readFloat(colorBase + index * colorStride + component * 4) * 255.0f, 0.0f, 255.0f));
                };
                milkyWayPoints.push_back({point, {channel(0), channel(1), channel(2), channel(3)}});
                minimum.x = std::min(minimum.x, point.x); minimum.y = std::min(minimum.y, point.y); minimum.z = std::min(minimum.z, point.z);
                maximum.x = std::max(maximum.x, point.x); maximum.y = std::max(maximum.y, point.y); maximum.z = std::max(maximum.z, point.z);
            }
        }
        milkyWaySourceDiameter = std::max({maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z});
        milkyWayCenter = Vector3Scale(Vector3Add(minimum, maximum), 0.5f);
    } catch (const std::exception& error) {
        milkyWayPoints.clear();
        logError(std::string("Unable to decode Milky Way point cloud: ") + error.what());
    }
    if (milkyWayPoints.empty() || !std::isfinite(milkyWaySourceDiameter) || milkyWaySourceDiameter <= 0.0f) {
        milkyWayPoints.clear();
        milkyWaySourceDiameter = 0.0f;
        milkyWayCenter = {};
        logError("Milky Way asset contains no supported point-cloud data; using the procedural starfield");
        return;
    }
    logInfo("Loaded Milky Way point cloud (" + std::to_string(milkyWayPoints.size()) + " stars)");
}

void Renderer::drawMilkyWay() const {
    if (milkyWayPoints.empty() || milkyWaySourceDiameter <= 0.0f) return;

    // Neptune's presentation orbit is about 1,200 units across.  A galaxy 8,000
    // units across makes the complete solar-system span 15% of the galaxy width.
    constexpr float GALAXY_DIAMETER = 8000.0f;
    const float scale = GALAXY_DIAMETER / milkyWaySourceDiameter;
    for (const GalaxyPoint& point : milkyWayPoints) {
        DrawPoint3D(Vector3Scale(Vector3Subtract(point.position, milkyWayCenter), scale), point.color);
    }
}

void Renderer::drawStars(const Simulation& simulation) const {
    // Background starfield must stay cheap: DrawSphere(16×16) × N regenerates
    // immediate-mode geometry every frame. Measured on Intel HD 4400 with N=1500:
    // ~227 ms/frame in drawStars vs ~0.4 ms drawing all planet GLBs — enough to
    // starve the main thread after model load (black window / "Not Responding").
    for (const Star& star : simulation.stars) {
        const Vector3 position = {star.position.x, 0.0f, star.position.y};
        DrawPoint3D(position, alpha({180, 215, 255, 255}, star.brightness));
    }
}

void Renderer::drawAxes() const {
    DrawLine3D({0, 0, 0}, {28, 0, 0}, alpha(RED, 0.35f));
    DrawLine3D({0, 0, 0}, {0, 28, 0}, alpha(GREEN, 0.35f));
    DrawLine3D({0, 0, 0}, {0, 0, 28}, alpha(BLUE, 0.35f));
}

void Renderer::drawOrbit(const Body& body, const Body& centralBody, const Simulation&) const {
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

void Renderer::drawArrow3D(Vector3 start, Vector3 direction, float length, Color color) const {
    const float dirLen = Vector3Length(direction);
    if (dirLen < 1.0e-8f || length < 0.05f) return;
    const Vector3 unit = Vector3Scale(direction, 1.0f / dirLen);
    const Vector3 end = Vector3Add(start, Vector3Scale(unit, length));
    DrawLine3D(start, end, color);
    const Vector3 side = {unit.z, 0.0f, -unit.x};
    const float sideLen = Vector3Length(side);
    const Vector3 sideUnit = sideLen > 1.0e-6f ? Vector3Scale(side, 1.0f / sideLen) : Vector3{0.0f, 1.0f, 0.0f};
    const float arrow = std::min(length * 0.22f, 1.1f);
    const Vector3 back = Vector3Scale(unit, -1.0f);
    DrawLine3D(end, Vector3Add(end, Vector3Scale(Vector3Add(back, sideUnit), arrow)), color);
    DrawLine3D(end, Vector3Add(end, Vector3Scale(Vector3Subtract(back, sideUnit), arrow)), color);
    DrawSphere(end, std::clamp(length * 0.04f, 0.08f, 0.22f), color);
}

void Renderer::drawVector(const Body& body, const Simulation& simulation) const {
    drawForceMotionVectors(body, simulation);
}

void Renderer::drawForceMotionVectors(const Body& body, const Simulation& simulation) const {
    // Direction from physics; length is presentation-clamped for readability.
    const Vector3 start = bodyWorldPosition(body, simulation);
    const float bodyR = presentationRadius(body, simulation);

    auto visualLength = [&](double physicalMagnitude, double scale, float minLen, float maxLen) {
        const float raw = static_cast<float>(physicalMagnitude * scale);
        return std::clamp(raw, minLen, maxLen);
    };

    // Velocity — instantaneous rate of change of position.
    {
        const double speed = length(body.velocity);
        if (speed > 1.0e-6) {
            const float arrowLen = visualLength(speed, 1.0e-4, std::max(bodyR * 1.6f, 1.2f),
                                               std::max(bodyR * 7.5f, 6.0f));
            drawArrow3D(start, transform.vector(body.velocity, 1.0), arrowLen,
                        alpha({80, 220, 255, 255}, 0.92f));
        }
    }

    std::size_t bodyIndex = simulation.bodies.size();
    for (std::size_t i = 0; i < simulation.bodies.size(); ++i) {
        if (&simulation.bodies[i] == &body) {
            bodyIndex = i;
            break;
        }
    }
    if (bodyIndex >= simulation.bodies.size()) return;

    // Net gravitational acceleration (same Softened N-body field as PhysicsEngine).
    bool accelValid = false;
    const Vec3 netAccel = PhysicsEngine::acceleration(simulation.bodies, bodyIndex, {}, &accelValid);
    const double accelMag = length(netAccel);
    if (accelValid && accelMag > 1.0e-30) {
        const float arrowLen = visualLength(accelMag, 2.5e4, std::max(bodyR * 1.4f, 1.0f),
                                            std::max(bodyR * 6.5f, 5.5f));
        drawArrow3D(start, transform.vector(netAccel, 1.0), arrowLen,
                    alpha({255, 170, 70, 255}, 0.90f));
    }

    // Optional individual gravitational contributions (toward each other mass).
    // Only for the selected body in gravity mode — keep short gravity field marks only.
    // Individual contribution lines removed for cleaner visualization.
    const bool isSelected = simulation.selected >= 0 &&
        simulation.selected < static_cast<int>(simulation.bodies.size()) &&
        &simulation.bodies[static_cast<std::size_t>(simulation.selected)] == &body;
    if (isSelected && simulation.gravityMode) {
        // In gravity mode, show detailed analysis of which bodies contribute
        // Display in HUD instead of cluttering the 3D view
    }
}

void Renderer::drawGravityField(const Simulation& simulation) const {
    if (simulation.soloStudy) return;
    const Body* sun = findBodyById(simulation, "sun");
    if (sun == nullptr || !sun->active) return;

    const Vector3 sunPosition = bodyWorldPosition(*sun, simulation);
    // Nested rings make the central well legible from the galaxy overview. They
    // are intentionally subtle: gravity remains a force, not a physical shell.
    constexpr float ringRadii[] = {13.0f, 22.0f, 34.0f};
    for (float radius : ringRadii) {
        DrawCircle3D(sunPosition, radius, {1.0f, 0.0f, 0.0f}, 90.0f,
                     alpha({255, 182, 72, 255}, 0.12f));
    }

    // Short inward marks beside each planet show the direction of the Sun's
    // gravitational pull without drawing distracting full-length spokes.
    for (const Body& body : simulation.bodies) {
        if (!body.active || body.id == "sun" || body.type == "Spacecraft") continue;
        const Vector3 position = bodyWorldPosition(body, simulation);
        const Vector3 towardSun = Vector3Subtract(sunPosition, position);
        const float distance = Vector3Length(towardSun);
        if (distance < 0.01f) continue;
        const Vector3 direction = Vector3Scale(towardSun, 1.0f / distance);
        const float length = std::clamp(presentationRadius(body, simulation) * 1.5f, 0.8f, 3.5f);
        const Vector3 end = Vector3Add(position, Vector3Scale(direction, length));
        const Color gravity = alpha({255, 203, 104, 255}, 0.48f);
        DrawLine3D(position, end, gravity);
        const Vector3 side = {direction.z, 0.0f, -direction.x};
        const float arrow = std::min(length * 0.28f, 0.85f);
        const Vector3 backward = Vector3Scale(direction, -1.0f);
        DrawLine3D(end, Vector3Add(end, Vector3Scale(Vector3Add(backward, side), arrow)), gravity);
        DrawLine3D(end, Vector3Add(end, Vector3Scale(Vector3Subtract(backward, side), arrow)), gravity);
    }
}

void Renderer::drawStudySpinCues(const Body& body, float radius) const {
    const Color axis = RED;
    //const Color equator = alpha({120, 220, 255, 255}, 0.78f);
    const Color tick = alpha({255, 160, 90, 255}, 0.95f);
    const float pole = radius * 1.55f;
    // Pole axis (tilt) — lines only, no floating “moonlet” spheres.
    DrawLine3D({0.0f, -pole, 0.0f}, {0.0f, pole, 0.0f}, axis);
   
    //  the equator line
   // DrawLine3D({radius * 0.82f, 0.0f, 0.0f}, {radius * 1.18f, 0.0f, 0.0f}, tick);
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
    const bool studyMotion = simulation.soloStudy || simulation.earthMoonFocus;
    const double orientClock = presentationClock(simulation);
    const bool showStudyAxis = (simulation.soloStudy || simulation.earthMoonFocus) &&
        simulation.selected >= 0 &&
        simulation.selected < static_cast<int>(simulation.bodies.size()) &&
        simulation.bodies[static_cast<std::size_t>(simulation.selected)].id == body.id;
    if (body.luminous) {
        // Give the Sun a layered corona even when its GLB is available.  The
        // model supplies the surface detail; these transparent shells supply
        // the visible energy and make it unmistakably the light source.
        // Pulse uses presentation clock so it freezes while paused.
        const float pulse = 0.96f + 0.04f * std::sin(static_cast<float>(orientClock) * 1.35f);
        DrawSphere(position, radius * 2.25f * pulse, alpha({255, 110, 24, 255}, 0.045f));
        DrawSphere(position, radius * 1.70f * pulse, alpha({255, 164, 38, 255}, 0.10f));
        DrawSphere(position, radius * 1.28f, alpha({255, 211, 102, 255}, 0.22f));
    }
    if (model != nullptr && presentation != nullptr) {
        const float localRadius = planetAssets.modelRadius(body.id);
        const float scale = radius / localRadius;
        const Vector3 center = planetAssets.modelCenter(body.id);
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, orientClock, studyMotion);
        if (showStudyAxis) drawStudySpinCues(body, radius);
        if (planetLightingEnabled_ && !body.luminous && sunLightPositionLocation >= 0) {
            if (const Body* sun = findBodyById(simulation, "sun")) {
                const Vector3 sunPosition = bodyWorldPosition(*sun, simulation);
                SetShaderValue(planetLightShader, sunLightPositionLocation, &sunPosition, SHADER_UNIFORM_VEC3);
            }
        }
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
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, orientClock, studyMotion);
        DrawSphereEx({0.0f, 0.0f, 0.0f}, radius, 24, 32, bodyColor);
        // Surface tint only — no detached sphere that reads as another body.
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 0.98f, {0.0f, 1.0f, 0.0f}, 90.0f, alpha(accentColor, 0.35f));
        if (showStudyAxis) drawStudySpinCues(body, radius);
        rlPopMatrix();
    } else {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, orientClock, studyMotion);
        DrawSphereEx({0.0f, 0.0f, 0.0f}, radius, 16, 24, bodyColor);
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.02f, {1.0f, 0.0f, 0.0f}, 90.0f, alpha(accentColor, 0.55f));
        if (showStudyAxis) drawStudySpinCues(body, radius);
        rlPopMatrix();
    }
    if (body.ringed && (presentation == nullptr || !presentation->modelIncludesRings)) {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, orientClock, studyMotion);
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.75f, {1.0f, 0.0f, 0.0f}, 90.0f, alpha(accentColor, 0.72f));
        DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.35f, {1.0f, 0.0f, 0.0f}, 90.0f, alpha(bodyColor, 0.55f));
        rlPopMatrix();
    }
}

void Renderer::scene(const Simulation& simulation) {
    const bool trace = [] {
        const char* value = std::getenv("BAGS_LAB_STARTUP_TRACE");
        return value != nullptr && value[0] != '\0' && value[0] != '0';
    }();
    static int sceneFrames = 0;
    using Clock = std::chrono::steady_clock;
    const auto sceneStart = Clock::now();

    const Body* selectedBody = nullptr;
    // Close near-plane while studying so large inspection globes are not clipped.
    if (simulation.soloStudy) rlSetClipPlanes(0.001, 14000.0);
    else rlSetClipPlanes(0.05, 14000.0);
    BeginMode3D(camera);
    drawMilkyWay();
    const auto starsStart = Clock::now();
    drawStars(simulation);
    const auto afterStars = Clock::now();
    if (simulation.showGrid) DrawGrid(40, 10.0f);
    if (simulation.showGrid) drawAxes();
    if (!simulation.bodies.empty()) {
        if (simulation.showVectors || simulation.gravityMode) {
            drawGravityField(simulation);
        }
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
                drawOrbit(body, *orbitParent, simulation);
            }
            drawMoonOrbit(simulation);
        }
        if (simulation.showTrails) {
            for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                if (!simulation.bodyVisibleInView(index)) continue;
                const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
                if (body.active) drawTrail(body);
            }
        }
        if (simulation.showVectors || simulation.gravityMode) {
            // V / G: selected body gets detailed vectors; V also draws others lightly.
            if (simulation.selected >= 0 &&
                simulation.selected < static_cast<int>(simulation.bodies.size())) {
                const Body& selected = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
                if (selected.active && selected.type != "Star") {
                    drawForceMotionVectors(selected, simulation);
                }
            }
            if (simulation.showVectors) {
                for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                    if (index == simulation.selected) continue;
                    if (!simulation.bodyVisibleInView(index)) continue;
                    const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
                    if (body.active && body.type != "Star") drawForceMotionVectors(body, simulation);
                }
            }
        }
        const auto bodiesStart = Clock::now();
        for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
            if (!simulation.bodyVisibleInView(index)) continue;
            const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
            if (!body.active) continue;
            drawBody(body, simulation);
        }
        if (trace && sceneFrames < 3) {
            const double bodiesMs =
                std::chrono::duration<double, std::milli>(Clock::now() - bodiesStart).count();
            const double starsMs =
                std::chrono::duration<double, std::milli>(afterStars - starsStart).count();
            logInfo("[FRAME] scene detail #" + std::to_string(sceneFrames) + " stars=" +
                    std::to_string(starsMs) + " ms bodies=" + std::to_string(bodiesMs) + " ms starCount=" +
                    std::to_string(simulation.stars.size()));
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
    if (simulation.showClassification) drawBodyLabels(simulation);
    if (trace && sceneFrames < 3) {
        const double totalMs = std::chrono::duration<double, std::milli>(Clock::now() - sceneStart).count();
        logInfo("[FRAME] scene total #" + std::to_string(sceneFrames) + " " + std::to_string(totalMs) + " ms");
    }
    ++sceneFrames;
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

void Renderer::drawBodyLabels(const Simulation& simulation) const {
    // Display body names and classifications as 2D labels on screen
    for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
        if (!simulation.bodyVisibleInView(index)) continue;
        const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
        if (!body.active || body.name.empty()) continue;
        
        const Vector3 position = bodyWorldPosition(body, simulation);
        const Vector2 screenPos = GetWorldToScreen(position, camera);
        
        // Only draw if on screen
        if (screenPos.x < 0 || screenPos.x > GetScreenWidth() || 
            screenPos.y < 0 || screenPos.y > GetScreenHeight()) continue;
        
        // Offset label above body
        const float labelY = screenPos.y - presentationRadius(body, simulation) - 20.0f;
        const int textWidth = MeasureText(body.name.c_str(), 12);
        const float labelX = screenPos.x - textWidth * 0.5f;
        
        // Draw semi-transparent background
        DrawRectangle(static_cast<int>(labelX) - 4, static_cast<int>(labelY) - 2, 
                     textWidth + 8, 16, {0, 0, 0, 128});
        
        // Draw name
        DrawText(body.name.c_str(), static_cast<int>(labelX), static_cast<int>(labelY), 12, RAYWHITE);
        
        // Draw classification below name
        if (!body.type.empty()) {
            const int typeWidth = MeasureText(body.type.c_str(), 10);
            const float typeX = screenPos.x - typeWidth * 0.5f;
            DrawText(body.type.c_str(), static_cast<int>(typeX), static_cast<int>(labelY + 14), 10, {180, 180, 180, 255});
        }
    }
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
