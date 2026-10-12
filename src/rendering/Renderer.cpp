#include "Renderer.hpp"

#include <raymath.h>
#include <rlgl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <string>
#include <sstream>
#include <vector>

#include <raym3/raym3.h>

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
    // The Sun's real day is ~25 Earth days; keep study spin slow so it reads as a star,
    // not a whirling ball (planets stay in the faster readable band).
    if (body.luminous || body.id == "sun") return static_cast<float>(sign * 140.0);
    const double earthDay = 86400.0;
    const double ratio = std::abs(body.rotationPeriod) / earthDay;
    const double visualAbs = std::clamp(10.0 * std::pow(std::max(ratio, 1.0e-3), 0.32), 3.5, 48.0);
    return static_cast<float>(sign * visualAbs);
}

float spinDegrees(const Body& body, double clockSeconds, bool studyMotion) {
    if (!std::isfinite(body.rotationPeriod) || body.rotationPeriod == 0.0) return 0.0f;
    if (!std::isfinite(clockSeconds)) return 0.0f;
    double period = studyMotion ? studyVisualPeriodSeconds(body) : body.rotationPeriod;
    // System view: timewarp makes the Sun appear to spin wildly; stretch its visual
    // period so texture motion stays calm and star-like.
    if (!studyMotion && (body.luminous || body.id == "sun")) {
        period *= 12.0;
    }
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
    std::mt19937 particleRng(20260920);
    std::uniform_real_distribution<float> radius(2.1f, 3.3f);
    std::uniform_real_distribution<float> phase(0.0f, 2.0f * PI);
    std::uniform_real_distribution<float> height(-0.045f, 0.045f);
    std::uniform_real_distribution<float> size(0.55f, 1.35f);
    std::uniform_int_distribution<int> shade(72, 142);
    asteroidParticles.reserve(180);
    for (int index = 0; index < 180; ++index) {
        const int cluster = index % 9;
        AsteroidParticle particle;
        particle.semiMajorAxisAU = radius(particleRng) + (cluster % 3) * 0.035f;
        particle.phase = phase(particleRng) + (cluster % 2 == 0 ? 0.12f : -0.08f);
        const double radiusMetres = static_cast<double>(particle.semiMajorAxisAU) * PhysicsEngine::AU;
        particle.angularSpeed = static_cast<float>(
            std::sqrt(PhysicsEngine::G * PhysicsEngine::SOLAR_MASS /
                       (radiusMetres * radiusMetres * radiusMetres)));
        particle.height = height(particleRng);
        particle.size = size(particleRng);
        const int value = std::max(48, shade(particleRng) - 28);
        particle.color = {static_cast<unsigned char>(value),
                          static_cast<unsigned char>(std::max(0, value - 8)),
                          static_cast<unsigned char>(std::max(0, value - 18)), 210};
        asteroidParticles.push_back(particle);
    }
    camera.position = cameraPosition();
    const char* traceEnv = std::getenv("BAGS_LAB_STARTUP_TRACE");
    const bool trace = traceEnv != nullptr && traceEnv[0] != '\0' && traceEnv[0] != '0';
    const auto loadStart = std::chrono::steady_clock::now();
    // Load only the major presentation models before the first frame. Smaller
    // moons and asteroids are uploaded one-at-a-time from the main loop so a
    // large asset set cannot freeze the window during construction.
    planetAssets.load(true);
    deferredAssetsPending_ = true;
    loadPlanetLighting();
    {
        loadMilkyWayAsset(planetAssetDirectory.parent_path() / "milkyway.glb");
    }
    // Load the optional detail models before the first interactive frame.
    {
    // Load the Earth city model for close-up surface view.
    std::filesystem::path villaPath = planetAssetDirectory / "prepared" / "earth" / "bags.glb";
    const bool loadDetailAssets = std::getenv("BAGS_LAB_LOAD_DETAIL_ASSETS") != nullptr;
    if (loadDetailAssets && std::filesystem::exists(villaPath)) {
        villaModel = LoadModel(villaPath.string().c_str());
        // The source export is in millimetres. Normalize it to the simulator's
        // metre-scale presentation coordinates before computing bounds/drawing.
        villaModel.transform = MatrixScale(0.001f, 0.001f, 0.001f);
        if (villaModel.meshCount > 0) {
            logInfo("Earth city model: meshCount=" + std::to_string(villaModel.meshCount) +
                    " materialCount=" + std::to_string(villaModel.materialCount));
            for (int materialIndex = 0; materialIndex < villaModel.materialCount; ++materialIndex) {
                const Material& material = villaModel.materials[materialIndex];
                const MaterialMap& albedo = material.maps[MATERIAL_MAP_ALBEDO];
                logInfo("Earth city material " + std::to_string(materialIndex) +
                        ": albedoTexture=" + std::to_string(albedo.texture.id) +
                        " alpha=" + std::to_string(albedo.color.a));
            }
            // The source GLB includes a very large skybox as its final mesh.
            // It is not part of the villa scene and would dominate the bounds,
            // causing the actual village to render tiny inside a white disc.
            if (villaModel.meshCount > 1) --villaModel.meshCount;
            for (int meshIndex = 0; meshIndex < villaModel.meshCount; ++meshIndex) {
                const BoundingBox meshBounds = GetMeshBoundingBox(villaModel.meshes[meshIndex]);
                logInfo("Earth city mesh " + std::to_string(meshIndex) +
                        ": vertices=" + std::to_string(villaModel.meshes[meshIndex].vertexCount) +
                        " material=" + std::to_string(villaModel.meshMaterial[meshIndex]) +
                        " materialAlbedoTexture=" +
                        std::to_string(villaModel.materials[villaModel.meshMaterial[meshIndex]]
                                           .maps[MATERIAL_MAP_ALBEDO].texture.id) +
                        " bounds=(" + std::to_string(meshBounds.min.x) + "," +
                        std::to_string(meshBounds.min.y) + "," + std::to_string(meshBounds.min.z) +
                        ")-(" + std::to_string(meshBounds.max.x) + "," +
                        std::to_string(meshBounds.max.y) + "," + std::to_string(meshBounds.max.z) + ")");
            }
            villaBounds = GetModelBoundingBox(villaModel);
        }
        else logError("Failed to load Earth city model: " + villaPath.string());
    } else {
        logInfo("Earth city detail asset deferred to avoid blocking startup");
    }
    std::filesystem::path forestPath = planetAssetDirectory / "prepared" / "earth" / "forest.glb";
    if (loadDetailAssets && std::filesystem::exists(forestPath)) {
        forestModel = LoadModel(forestPath.string().c_str());
        forestModel.transform = MatrixScale(0.001f, 0.001f, 0.001f);
        if (forestModel.meshCount > 0) {
            logInfo("Earth forest model: meshCount=" + std::to_string(forestModel.meshCount) +
                    " materialCount=" + std::to_string(forestModel.materialCount));
            for (int materialIndex = 0; materialIndex < forestModel.materialCount; ++materialIndex) {
                const Material& material = forestModel.materials[materialIndex];
                logInfo("Earth forest material " + std::to_string(materialIndex) +
                        ": albedoTexture=" +
                        std::to_string(material.maps[MATERIAL_MAP_ALBEDO].texture.id) +
                        " alpha=" +
                        std::to_string(material.maps[MATERIAL_MAP_ALBEDO].color.a));
            }
            for (int meshIndex = 0; meshIndex < forestModel.meshCount; ++meshIndex) {
                logInfo("Earth forest mesh " + std::to_string(meshIndex) +
                        ": material=" + std::to_string(forestModel.meshMaterial[meshIndex]));
            }
            if (forestModel.materialCount > 2) {
                // Keep the forest readable even when its source textures are
                // exported with a white diffuse factor.
                forestModel.materials[1].maps[MATERIAL_MAP_ALBEDO].color = {104, 150, 82, 255};
                forestModel.materials[2].maps[MATERIAL_MAP_ALBEDO].color = {126, 104, 68, 255};
            }
            // The final forest mesh is an enclosing environment sphere, not
            // part of the location geometry. It can occlude both locations.
            if (forestModel.meshCount > 1) --forestModel.meshCount;
            forestBounds = GetModelBoundingBox(forestModel);
        } else {
            logError("Failed to load Earth forest model: " + forestPath.string());
        }
    } else {
        logInfo("Earth forest detail asset deferred to avoid blocking startup");
    }
    // Load Einstein spacetime curvature model for gravity visualization
    std::filesystem::path spacetimePath = planetAssetDirectory.parent_path() / "warped_spacetime.glb";
    if (std::filesystem::exists(spacetimePath)) {
        spacetimeModel = LoadModel(spacetimePath.string().c_str());
        if (spacetimeModel.meshCount > 0) {
            // Compute the model's bounding box to determine actual dimensions
            spacetimeBounds = GetModelBoundingBox(spacetimeModel);

            // Calculate the model's height (Y-axis extent) and width (X-axis extent)
            const float modelHeight = spacetimeBounds.max.y - spacetimeBounds.min.y;
            const float modelWidth = spacetimeBounds.max.x - spacetimeBounds.min.x;
            const float modelDepth = spacetimeBounds.max.z - spacetimeBounds.min.z;

            logInfo("Spacetime model bounds: min=(" + std::to_string(spacetimeBounds.min.x) + "," +
                    std::to_string(spacetimeBounds.min.y) + "," + std::to_string(spacetimeBounds.min.z) + ") max=(" +
                    std::to_string(spacetimeBounds.max.x) + "," + std::to_string(spacetimeBounds.max.y) + "," +
                    std::to_string(spacetimeBounds.max.z) + ")");
            logInfo("Spacetime model dimensions: height=" + std::to_string(modelHeight) + " width=" +
                    std::to_string(modelWidth) + " depth=" + std::to_string(modelDepth));

            // The well opening width should match the Sun's diameter
            // Use the maximum horizontal extent (width or depth) to determine opening size
            const float wellOpeningSize = std::max(modelWidth, modelDepth);
            if (wellOpeningSize > 0.0f) {
                // We'll scale the model at render time based on Sun radius
                // Store 1.0 as base scale; actual scale will be: sunRadius * 2.0f / wellOpeningSize
                spacetimeModelScale = 1.0f;
                logInfo("Loaded spacetime model: meshCount=" + std::to_string(spacetimeModel.meshCount) +
                        " wellOpeningSize=" + std::to_string(wellOpeningSize));
            } else {
                logError("Failed to compute spacetime model scale - invalid well opening size");
            }
        } else {
            logError("Failed to load spacetime model - invalid mesh count");
        }
    } else {
        logError("Spacetime model file not found: " + spacetimePath.string());
    }
    }
    if (trace) {
        const double ms =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - loadStart).count();
        logInfo("[STARTUP] Planet models loaded (" + std::to_string(ms) + " ms)");
    }
}

void Renderer::loadDeferredAssets() {
    if (!deferredAssetsPending_) return;
    // One GLB per frame keeps input/rendering responsive while the GPU-side
    // raylib resources are populated on the owning main thread.
    if (!planetAssets.loadNext()) deferredAssetsPending_ = false;
}

void Renderer::drawDeferredLoadingOverlay() const {
    if (!deferredAssetsPending_) return;
    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    const int panelWidth = 360;
    const int panelHeight = 92;
    const Rectangle panel = {static_cast<float>((width - panelWidth) / 2), static_cast<float>((height - panelHeight) / 2),
                             static_cast<float>(panelWidth), static_cast<float>(panelHeight)};
    raym3::Card(panel, raym3::CardVariant::Elevated);
    const std::size_t total = planetAssets.totalEnabledCount();
    const std::size_t loaded = std::min(planetAssets.loadedCount(), total);
    const float progress = total == 0 ? 1.0f : static_cast<float>(loaded) / static_cast<float>(total);
    const int percent = static_cast<int>(std::round(progress * 100.0f));
    const auto& loadingTheme = ui::theme();
    raym3::Text("Loading...", {panel.x + 22.0f, panel.y + 16.0f, 180.0f, 22.0f}, 14.0f,
                loadingTheme.text, raym3::FontWeight::Bold);
    raym3::Text((std::to_string(percent) + "%").c_str(), {panel.x + panel.width - 86.0f, panel.y + 16.0f, 64.0f, 22.0f}, 13.0f,
                loadingTheme.textMuted, raym3::FontWeight::Bold, raym3::TextAlignment::Right);
    raym3::LinearProgressIndicator({panel.x + 22.0f, panel.y + 54.0f, panel.width - 44.0f, 8.0f}, progress, false);
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
    if (villaModel.meshCount > 0) {
        UnloadModel(villaModel);
        villaModel = {};
        villaBounds = {};
    }
    if (forestModel.meshCount > 0) {
        UnloadModel(forestModel);
        forestModel = {};
        forestBounds = {};
    }
    if (spacetimeModel.meshCount > 0) {
        UnloadModel(spacetimeModel);
        spacetimeModel = {};
    }
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
    villaFocus = false;
    forestFocus = false;
    villaTransition = false;
    villaExiting = false;
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

void Renderer::frameGravityInteraction(const Simulation& simulation, int index, bool snap) {
    if (index < 0 || index >= static_cast<int>(simulation.bodies.size())) return;
    const Body& subject = simulation.bodies[static_cast<std::size_t>(index)];
    if (!subject.active) return;

    const Body* source = nullptr;
    double strongest = 0.0;
    for (const Body& candidate : simulation.bodies) {
        if (!candidate.active || &candidate == &subject || candidate.mass <= 0.0) continue;
        const Vec3 delta = candidate.position - subject.position;
        const double distanceSquared = dot(delta, delta);
        if (distanceSquared <= 1.0) continue;
        const double pull = PhysicsEngine::G * candidate.mass / distanceSquared;
        if (pull > strongest) { strongest = pull; source = &candidate; }
    }
    if (source == nullptr) { focusBodyIndex(simulation, index, snap); return; }

    villaFocus = forestFocus = villaTransition = villaExiting = false;
    focusBody = -1; // Framing is intentional, but manual camera movement remains authoritative.
    const Vector3 subjectPosition = bodyWorldPosition(subject, simulation);
    const Vector3 sourcePosition = bodyWorldPosition(*source, simulation);
    desiredTarget = Vector3Scale(Vector3Add(subjectPosition, sourcePosition), 0.5f);
    const float separation = Vector3Distance(subjectPosition, sourcePosition);
    targetDistance = std::clamp(separation * 1.65f + presentationRadius(*source, simulation) * 3.0f,
                                12.0f, 14000.0f);
    if (snap) {
        target = desiredTarget;
        cameraDistance = targetDistance;
        camera.target = target;
        camera.position = cameraPosition();
    }
}

void Renderer::focusEarthVilla(const Simulation& simulation) {
    int earthIndex = -1;
    for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
        if (simulation.bodies[static_cast<std::size_t>(index)].id == "earth" &&
            simulation.bodies[static_cast<std::size_t>(index)].active) {
            earthIndex = index;
            break;
        }
    }
    if (earthIndex < 0 || villaModel.meshCount <= 0) return;

    focusBody = earthIndex;
    forestFocus = false;
    villaFocus = false;
    villaExiting = false;
    desiredTarget = bodyWorldPosition(simulation.bodies[static_cast<std::size_t>(earthIndex)], simulation);
    villaLookOffset = {};
    villaTransition = true;
    villaTransitionTime = 0.0f;
    targetDistance = std::max(cameraDistance * 1.2f, 120.0f);
    pitch = 0.82f;
    yawVelocity = 0.0f;
    pitchVelocity = 0.0f;
    zoomVelocity = 0.0f;
}

void Renderer::selectVillaArea(int area) {
    if (!villaFocus && !villaTransition) return;
    if (area == 0) {
        if (!forestFocus) villaLookOffset = {};
        yaw = 0.72f;
        pitch = 0.62f;
        targetDistance = 1.4f;
        return;
    }
    if (area < 1 || area > 9) return;

    // Number-pad style layout: 7 8 9 are the far/upper row,
    // 4 5 6 the middle row, and 1 2 3 the near/lower row.
    const int column = (area - 1) % 3;
    const int row = (area - 1) / 3;
    yaw = (static_cast<float>(column) - 1.0f) * 0.95f;
    pitch = std::clamp(0.35f + static_cast<float>(row) * 0.45f, -1.45f, 1.45f);
    if (!forestFocus) villaLookOffset = {};
    targetDistance = 0.9f;
}

void Renderer::focusEarthForest(const Simulation& simulation) {
    if (!villaFocus && !villaTransition) return;
    const Body* earth = findBodyById(simulation, "earth");
    if (earth == nullptr || !earth->active || forestModel.meshCount <= 0) return;
    forestFocus = true;
    villaLookOffset = {};
    desiredTarget = bodyWorldPosition(*earth, simulation);
    targetDistance = 1.4f;
    villaSwapTime = 0.0f;
}

void Renderer::exitEarthVilla() {
    if (!villaFocus && !villaTransition) return;
    villaFocus = false;
    forestFocus = false;
    villaLookOffset = {};
    villaExiting = true;
    villaTransition = true;
    villaTransitionTime = 0.0f;
    targetDistance = std::max(cameraDistance * 1.15f, 140.0f);
    yawVelocity = 0.0f;
    pitchVelocity = 0.0f;
    zoomVelocity = 0.0f;
}

void Renderer::focusEarthMoonSystem(const Simulation& simulation, bool snap) {
    villaFocus = false;
    forestFocus = false;
    villaTransition = false;
    villaExiting = false;
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
    const Vector3 earthPos = bodyWorldPosition(*earth, simulation);
    if (moon == nullptr || !moon->active) {
        focusBodyIndex(simulation, earthIndex, snap);
        return;
    }
    // Track the pair's midpoint rather than Earth alone. This keeps the mesh
    // visually anchored while the Earth–Moon system remains centered.
    focusBody = -1;

    // Bounding sphere around Sun + Earth + Moon presentation positions.
    const Body* sun = findBodyById(simulation, "sun");
    const Vector3 moonPos = bodyWorldPosition(*moon, simulation);
    const Vector3 sunPos = (sun != nullptr && sun->active) ? bodyWorldPosition(*sun, simulation) : earthPos;
    const Vector3 mid = Vector3Scale(Vector3Add(Vector3Add(earthPos, moonPos), sunPos), 1.0f / 3.0f);
    const float earthR = presentationRadius(*earth, simulation);
    const float moonR = presentationRadius(*moon, simulation);
    const float sunR = (sun != nullptr && sun->active) ? presentationRadius(*sun, simulation) : 0.0f;
    const float span = std::max({Vector3Distance(earthPos, moonPos) + earthR + moonR,
                                 Vector3Distance(earthPos, sunPos) + earthR + sunR,
                                 Vector3Distance(moonPos, sunPos) + moonR + sunR});
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
    villaFocus = false;
    forestFocus = false;
    villaTransition = false;
    villaExiting = false;
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
    villaFocus = false;
    forestFocus = false;
    villaTransition = false;
    villaExiting = false;
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
    // F2 keeps a fixed teaching viewpoint. The simulation and mesh continue
    // moving, but the camera must not chase the changing Sun–Earth–Moon
    // centroid or the whole screen will appear to drift.
    if (focusBody >= 0 && (focusBody >= static_cast<int>(simulation.bodies.size()) ||
                           !simulation.bodies[static_cast<std::size_t>(focusBody)].active)) {
        focusBody = -1;
    }
    if (focusBody >= 0 && focusBody < static_cast<int>(simulation.bodies.size()) &&
        simulation.bodies[static_cast<std::size_t>(focusBody)].active) {
        desiredTarget = bodyWorldPosition(simulation.bodies[static_cast<std::size_t>(focusBody)], simulation);
        if (villaFocus || villaTransition) desiredTarget = Vector3Add(desiredTarget, villaLookOffset);
    }

    if (villaFocus) villaSwapTime = std::min(villaSwapTime + GetFrameTime(), 0.3f);
    if (villaTransition) {
        villaTransitionTime += GetFrameTime();
        if (!villaExiting && villaTransitionTime >= 0.9f) {
            targetDistance = 1.4f;
        }
        if (!villaExiting && villaTransitionTime >= 1.2f) {
            villaTransition = false;
            villaFocus = true;
            villaSwapTime = 0.0f;
            targetDistance = 1.4f;
        } else if (villaExiting && villaTransitionTime >= 0.9f) {
            villaTransition = false;
            villaExiting = false;
            targetDistance = 90.0f;
        }
    }

    // Smooth follow for focus / pan target.
    target = Vector3Lerp(target, desiredTarget, 0.14f);
    cameraDistance += (targetDistance - cameraDistance) * 0.16f;

    // Inertia on orbit after release.
    if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
        yaw += yawVelocity;
        pitch = std::clamp(pitch + pitchVelocity, -1.50f, 1.50f);
        yawVelocity *= 0.90f;
        pitchVelocity *= 0.90f;
        if (std::abs(yawVelocity) < 1.0e-5f) yawVelocity = 0.0f;
        if (std::abs(pitchVelocity) < 1.0e-5f) pitchVelocity = 0.0f;
    }

    // Zoom inertia. Allow a closer approach while Z-studying a single body.
    if (zoomVelocity != 0.0f) {
        const float minZoom = (simulation.soloStudy || villaFocus) ? 0.6f : 5.0f;
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
    const bool overScene = pointerInputEnabled_ && mouse.y > topGuard && mouse.y < bottomGuard;

    cameraManipulating = false;

    // Left-drag orbit with click-vs-drag threshold for body picking.
    if (overScene && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        leftPressValid = true;
        leftPressOverScene = true;
        leftDragActive = false;
        leftPressPos = mouse;
    } else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
        // The press began over the shell. Latch that fact so the release cannot
        // be reinterpreted as a scene click, and drop any stale gesture state.
        leftPressValid = false;
        leftPressOverScene = false;
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
            pitch = std::clamp(pitch + dpitch, -1.50f, 1.50f);
            yawVelocity = dyaw;
            pitchVelocity = dpitch;
        }
    }
    if (IsMouseButtonReleased(MOUSE_LEFT_BUTTON)) {
        // Decided entirely from the latched press origin, never from where the
        // pointer happens to be on the release frame. Two failures came from
        // judging this live: a click that began on a body but ended over a panel
        // was dropped, and a click that began on a button could be converted
        // into a camera gesture when the rail animated under the cursor.
        //
        // The pick uses the PRESS position. That is where the user aimed, and it
        // is immune to the release landing a few pixels inside a panel edge.
        if (leftPressValid && !leftDragActive && leftPressOverScene) {
            pendingBodyPick = true;
            pendingPickMouse = leftPressPos;
        }
        leftPressValid = false;
        leftPressOverScene = false;
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
        const float panScale = targetDistance * (villaFocus ? 0.0008f : 0.0016f);
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
        zoomVelocity += wheel * boost * (villaFocus ? 0.55f : 1.0f);
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
    if (body.type == "Asteroid") return static_cast<float>(std::clamp(body.visualRadius, 0.08, 0.65));
    const double earthRatio = std::max(body.realRadius / PhysicsEngine::EARTH_RADIUS, 1.0e-6);

    if (simulation.visualScaleMode == VisualScaleMode::Realistic) {
        // Physically proportional radii in presentation units (Earth ≈ 1).
        // At true AU distances planets are tiny — that is intentional.
        if (body.luminous) return static_cast<float>(earthRatio * 0.35);
        return static_cast<float>(earthRatio * 0.35);
    }

    // Educational display scale: maintain recognizable relative sizes while
    // enlarging smaller bodies enough to remain readable at system scale.
    // Keep Mercury's innermost educational orbit visible: its presentation
    // radius is about 7.7 world units, so the Sun must remain smaller than that.
    // Sun is clearly the largest body; giants stay below it.
    if (body.luminous || body.id == "sun") return 7.2f;
    if (body.id == "jupiter") return 5.6f;
    if (body.id == "saturn") return 5.0f;
    if (body.id == "uranus" || body.id == "neptune") return 4.4f;
    if (body.id == "earth") return 2.2f;
    if (body.id == "venus") return 2.1f;
    if (body.id == "mars") return 1.7f;
    if (body.id == "mercury") return 1.5f;
    if (body.id == "moon") return 1.05f;
    return std::max(0.85f, static_cast<float>(2.2 * std::pow(earthRatio, 0.42)));
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
    if (simulation.soloStudy || simulation.earthMoonFocus || villaFocus) {
        if (!simulation.paused) studyPresentationClock = GetTime();
        return studyPresentationClock;
    }
    return simulation.simTime;
}

float moonPhaseOffset(const Body& moon) {
    if (moon.id == "europa") return 0.5f * PI;
    if (moon.id == "ganymede") return PI;
    if (moon.id == "callisto") return 1.5f * PI;
    if (moon.id == "deimos" || moon.id == "enceladus" || moon.id == "dione") return PI;
    if (moon.id == "tethys" || moon.id == "titan" || moon.id == "iapetus") return 0.5f * PI;
    if (moon.id == "rhea" || moon.id == "hyperion") return 1.5f * PI;
    if (moon.id == "ariel" || moon.id == "titania" || moon.id == "triton") return 0.5f * PI;
    if (moon.id == "miranda" || moon.id == "oberon" || moon.id == "umbriel") return PI;
    return 0.0f;
}

float moonPresentationAngle(const Body& moon, const Body& parent, const Simulation& simulation,
                            double studyClock) {
    const Vec3 relative = moon.position - parent.position;
    const float physicsAngle = static_cast<float>(std::atan2(relative.y, relative.x));
    const float phaseOffset = moonPhaseOffset(moon);
    if (!simulation.soloStudy && !simulation.earthMoonFocus &&
        std::isfinite(moon.orbitalPeriod) && moon.orbitalPeriod > 0.0) {
        double period = moon.orbitalPeriod;
        // Preserve the Io-Europa-Ganymede 1:2:4 Laplace resonance visually.
        if (moon.id == "europa") period = 2.0 * 152853.8;
        else if (moon.id == "ganymede") period = 4.0 * 152853.8;
        const double direction = moon.id == "triton" ? -1.0 : 1.0;
        return static_cast<float>(phaseOffset + direction * 2.0 * PI * simulation.simTime / period);
    }
    if (!simulation.soloStudy && !simulation.earthMoonFocus) return physicsAngle + phaseOffset;

    // Study / F2: readable satellite motion — frozen when paused (studyClock held).
    constexpr double studyOrbitSeconds = 22.0;
    if (!std::isfinite(studyClock)) return physicsAngle + phaseOffset;
    const double direction = moon.id == "triton" ? -1.0 : 1.0;
    return static_cast<float>(phaseOffset + direction * 2.0 * PI * (studyClock / studyOrbitSeconds));
}

Vector3 Renderer::bodyWorldPosition(const Body& body, const Simulation& simulation) const {
    const Vector3 physicsPos = worldPosition(body.position);
    if (!(body.id == "moon" || body.type == "Moon")) return physicsPos;

    const Body* parent = nullptr;
    if (!body.parentId.empty()) parent = findBodyById(simulation, body.parentId);
    if (parent == nullptr) {
        parent = findBodyById(simulation, body.id == "moon" ? "earth" :
                              (body.id == "phobos" || body.id == "deimos" ? "mars" : "jupiter"));
    }
    if (parent == nullptr || !parent->active) return physicsPos;

    // AU presentation scale buries the Moon inside Earth's display globe. Place it on
    // a readable ring outside Earth; angle follows physics (system) or study clock (Z/F2).
    const Vector3 parentPos = worldPosition(parent->position);
    const float angle = moonPresentationAngle(body, *parent, simulation, presentationClock(simulation));
    const float inclination = body.id == "moon" ? 5.1f * DEG2RAD :
        (parent->id == "uranus" ? 98.0f * DEG2RAD : 0.0f);
    const Vector3 dir = {
        std::cos(angle),
        std::sin(angle) * std::sin(inclination),
        std::sin(angle) * std::cos(inclination)
    };
    const float parentR = presentationRadius(*parent, simulation);
    const float moonR = presentationRadius(body, simulation);
    float moonOrbitOffset = body.parentId == "mars" ? 2.5f : 3.0f;
    if (body.id == "europa") moonOrbitOffset = 5.0f;
    else if (body.id == "ganymede") moonOrbitOffset = 7.0f;
    else if (body.id == "callisto") moonOrbitOffset = 9.0f;
    else if (body.id == "phobos") moonOrbitOffset = 2.5f;
    else if (body.id == "deimos") moonOrbitOffset = 4.0f;
    else if (body.id == "mimas") moonOrbitOffset = 2.5f;
    else if (body.id == "enceladus") moonOrbitOffset = 3.5f;
    else if (body.id == "tethys") moonOrbitOffset = 4.5f;
    else if (body.id == "dione") moonOrbitOffset = 5.5f;
    else if (body.id == "rhea") moonOrbitOffset = 6.5f;
    else if (body.id == "titan") moonOrbitOffset = 8.0f;
    else if (body.id == "hyperion") moonOrbitOffset = 9.5f;
    else if (body.id == "iapetus") moonOrbitOffset = 11.0f;
    const float eccentricity = static_cast<float>(std::clamp(body.eccentricity, 0.0, 0.85));
    const float orbitalScale = (1.0f - eccentricity * eccentricity) /
        (1.0f + eccentricity * std::cos(angle));
    const float separation = body.id == "moon"
        ? parentR + moonR + std::max(parentR * 0.55f, 1.2f)
        : (parentR + moonOrbitOffset + moonR * 0.2f) * orbitalScale;
    return Vector3Add(parentPos, Vector3Scale(dir, separation));
}

Vector3 Renderer::seatedBodyPosition(const Body& body, const Simulation& simulation) const {
    Vector3 position = bodyWorldPosition(body, simulation);
    if (!simulation.showGrid || !gridSamplesValid) return position;
    const float wellDepth = grid.HeightAt(position.x, position.z, gridSamplePositions, gridSampleMasses, 1.0);
    // Keep the body's lower hemisphere above the rubber-sheet surface. This
    // makes the well visibly bend underneath it instead of passing through it.
    position.y += wellDepth + presentationRadius(body, simulation) * 0.72f;
    return position;
}

float Renderer::presentationRadius(const Body& body, const Simulation& simulation) const {
    float radius = displayRadius(body, simulation);
    // Z study: grow the selected body into a large inspection globe that fills the screen.
    // Earth/Moon companions stay readable but smaller than the focused body.
    // F2 system focus: enlarge the pair without collapsing to a single-body fill.
    if (simulation.earthMoonFocus &&
        (body.id == "earth" || body.id == "moon")) {
        radius = std::max(radius * 2.6f, body.id == "earth" ? 4.5f : 2.2f);
    } else if (simulation.soloStudy && simulation.selected >= 0 &&
               simulation.selected < static_cast<int>(simulation.bodies.size())) {
        const Body& selected = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
        const bool isSelected = body.id == selected.id;
        if (isSelected) {
            radius = std::max(radius * 4.8f, body.luminous ? 18.0f : 8.5f);
        } else if (isolatingEarthMoon(simulation) &&
                   (body.id == "earth" || body.id == "moon")) {
            radius = std::max(radius * 1.8f, 2.4f);
        }
    }
    if (const PlanetPresentation* presentation = planetAssets.presentation(body.visualAssetId.empty() ? body.id : body.visualAssetId)) {
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

void Renderer::drawAsteroidParticles(const Simulation& simulation) const {
    if (asteroidParticles.empty()) return;
    for (const AsteroidParticle& particle : asteroidParticles) {
        const float angle = particle.phase + particle.angularSpeed * static_cast<float>(simulation.simTime);
        const float x = particle.semiMajorAxisAU * std::cos(angle);
        const float orbitalCoordinate = particle.semiMajorAxisAU * std::sin(angle);
        // The physical X/Y plane is the solar-system orbital plane. Physical Z
        // is only a small inclination offset; RenderTransform maps it to
        // screen-up, preventing the belt from becoming a vertical ring.
        const Vector3 position = transform.positionAU({x, orbitalCoordinate, particle.height});
        // Point primitives collapse to a single pixel in the wide solar-system
        // view. Small low-poly rocks preserve the particle-field illusion while
        // staying far cheaper than loading/rendering duplicate asteroid GLBs.
        const float apparentSize = std::clamp(0.22f + cameraDistance * 0.00055f, 0.22f, 3.6f) *
            particle.size;
        DrawSphereEx(position, apparentSize, 4, 6, particle.color);
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

void Renderer::drawTrail(const Body& body, const Simulation& simulation) const {
    if (body.type == "Asteroid") return;
    // Moon uses an educational display offset; its AU trail would draw inside Earth.
    if (body.id == "moon" || body.type == "Moon") return;
    if (body.trail.size() <= 1) return;
    const Body* sun = findBodyById(simulation, "sun");
    const Vector3 sunPosition = sun != nullptr ? bodyWorldPosition(*sun, simulation) : Vector3{};
    const float sunRadius = sun != nullptr ? presentationRadius(*sun, simulation) : 0.0f;
    // Keep trails deliberately short. A long history wraps into a full ellipse
    // at high time scales and competes with the explicit orbit guides.
    const bool innerPlanet = body.id == "mercury" || body.id == "venus" ||
                             body.id == "earth" || body.id == "mars";
    const std::size_t maxTrailPoints = innerPlanet ? 12 : 36;
    const std::size_t first = body.trail.size() > maxTrailPoints
        ? body.trail.size() - maxTrailPoints : 0;
    for (std::size_t index = first + 1; index < body.trail.size(); ++index) {
        const Vector3 a = transform.positionAU(body.trail[index - 1]);
        const Vector3 b = transform.positionAU(body.trail[index]);
        const Vector3 midpoint = Vector3Scale(Vector3Add(a, b), 0.5f);
        if (sun != nullptr && Vector3Distance(midpoint, sunPosition) <= sunRadius * 1.05f) continue;
        const float age = static_cast<float>(index - first) /
                          static_cast<float>(std::max<std::size_t>(1, body.trail.size() - first));
        // A subtle gradient keeps the recent direction readable without turning
        // the history into a bright, closed-looking orbit.
        DrawLine3D(a, b, alpha(color(body.accent), 0.025f + 0.24f * age));
    }
}

void Renderer::drawMoonOrbit(const Simulation& simulation) const {
    constexpr int segments = 96;
    for (int moonIndex = 0; moonIndex < static_cast<int>(simulation.bodies.size()); ++moonIndex) {
        const Body& moon = simulation.bodies[static_cast<std::size_t>(moonIndex)];
        if (!moon.active || moon.type != "Moon" || !simulation.bodyVisibleInView(moonIndex)) continue;
        const Body* parent = !moon.parentId.empty() ? findBodyById(simulation, moon.parentId) : nullptr;
        if (parent == nullptr || !parent->active) continue;
        const Vector3 parentPos = bodyWorldPosition(*parent, simulation);
        const Vector3 moonPos = bodyWorldPosition(moon, simulation);
        const float radius = Vector3Distance(parentPos, moonPos);
        if (radius < 1.0e-4f) continue;
        const Color orbitColor = alpha(color(moon.accent), simulation.soloStudy ? 0.55f : 0.40f);
        const float inclination = moon.id == "moon" ? 5.1f * DEG2RAD :
            (parent->id == "uranus" ? 98.0f * DEG2RAD : 0.0f);
        Vector3 previous{};
        for (int segment = 0; segment <= segments; ++segment) {
            const float angle = 2.0f * PI * static_cast<float>(segment) / static_cast<float>(segments);
            const float orbitalAngle = moonPresentationAngle(moon, *parent, simulation, presentationClock(simulation)) +
                angle;
            const Vector3 point = {
                parentPos.x + radius * std::cos(orbitalAngle),
                parentPos.y + radius * std::sin(orbitalAngle) * std::sin(inclination),
                parentPos.z + radius * std::sin(orbitalAngle) * std::cos(inclination),
            };
            if (segment > 0) DrawLine3D(previous, point, orbitColor);
            previous = point;
        }
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
                        alpha({72, 205, 245, 255}, 0.94f)); // cyan = velocity
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
        // Log compression keeps a real inverse-square range visible without
        // allowing a close approach to fill the scene with an arrow.
        const float arrowLen = std::clamp(static_cast<float>(std::log10(accelMag + 1.0e-30) + 12.0) * 0.72f,
                                          std::max(bodyR * 1.4f, 1.0f),
                                            std::max(bodyR * 6.5f, 5.5f));
        drawArrow3D(start, transform.vector(netAccel, 1.0), arrowLen,
                    alpha({255, 181, 82, 255}, 0.94f)); // amber = gravity
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
        const double physicalRadius = radius * PhysicsEngine::AU / transform.astronomicalUnitScale;
        const double strength = PhysicsEngine::G * sun->mass / (physicalRadius * physicalRadius);
        const float emphasis = std::clamp(static_cast<float>((std::log10(strength + 1.0e-20) + 8.0) / 5.0), 0.05f, 0.28f);
        DrawCircle3D(sunPosition, radius, {1.0f, 0.0f, 0.0f}, 90.0f,
                     alpha({255, 182, 72, 255}, emphasis));
    }

    // Short inward marks beside each planet show the direction of the Sun's
    // gravitational pull without drawing distracting full-length spokes.
    for (const Body& body : simulation.bodies) {
        if (!body.active || body.id == "sun" || body.type == "Spacecraft") continue;
        if (&body == sun) continue;
        const std::size_t index = static_cast<std::size_t>(&body - simulation.bodies.data());
        bool valid = false;
        const Vec3 acceleration = PhysicsEngine::acceleration(simulation.bodies, index, {}, &valid);
        if (!valid || length(acceleration) <= 0.0) continue;
        const float magnitude = static_cast<float>(length(acceleration));
        const float cueLength = std::clamp(0.7f + 0.28f * (std::log10(magnitude + 1.0e-30f) + 12.0f),
                                           0.7f, std::max(1.0f, presentationRadius(body, simulation) * 1.8f));
        drawArrow3D(bodyWorldPosition(body, simulation), transform.vector(acceleration, 1.0), cueLength,
                    alpha({255, 194, 103, 255}, 0.48f));
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
    const Vector3 position = seatedBodyPosition(body, simulation);
    const float radius = presentationRadius(body, simulation);
    const Color bodyColor = color(body.color);
    const Color accentColor = color(body.accent);
    const std::string& assetId = body.visualAssetId.empty() ? body.id : body.visualAssetId;
    const bool selectedAsteroid = body.type == "Asteroid" &&
        simulation.selected >= 0 &&
        simulation.selected < static_cast<int>(simulation.bodies.size()) &&
        simulation.bodies[static_cast<std::size_t>(simulation.selected)].id == body.id;
    const Model* model = (body.type == "Asteroid" && !selectedAsteroid) ? nullptr : planetAssets.model(assetId);
    const PlanetPresentation* presentation = planetAssets.presentation(assetId);
    const bool studyMotion = simulation.soloStudy || simulation.earthMoonFocus || villaFocus;
    const double orientClock = presentationClock(simulation);
    // Check if this is Earth and we're zoomed in very close for surface detail view
    const bool showVilla = body.id == "earth" && villaModel.meshCount > 0 && villaFocus;
    
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
    
    // Render villa when zoomed in close to Earth
    if (showVilla) {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, orientClock, studyMotion);
        const float villaWidth = villaBounds.max.x - villaBounds.min.x;
        const float villaHeight = villaBounds.max.y - villaBounds.min.y;
        const float villaDepth = villaBounds.max.z - villaBounds.min.z;
        const float forestWidth = forestBounds.max.x - forestBounds.min.x;
        const float forestHeight = forestBounds.max.y - forestBounds.min.y;
        const float forestDepth = forestBounds.max.z - forestBounds.min.z;
        const float forestExtent = std::max({forestWidth, forestHeight, forestDepth, 1.0e-3f});
        const float villaExtent = std::max({villaWidth, villaHeight, villaDepth, 1.0e-3f});
        const float modelExtent = forestFocus ? forestExtent : villaExtent;
        const float modelScale = radius * 2.8f / modelExtent;
        rlScalef(modelScale, modelScale, modelScale);
        const Vector3 villaCenter = {
            (villaBounds.min.x + villaBounds.max.x) * 0.5f,
            (villaBounds.min.y + villaBounds.max.y) * 0.5f,
            (villaBounds.min.z + villaBounds.max.z) * 0.5f
        };
        const Vector3 forestCenter = {
            (forestBounds.min.x + forestBounds.max.x) * 0.5f,
            (forestBounds.min.y + forestBounds.max.y) * 0.5f,
            (forestBounds.min.z + forestBounds.max.z) * 0.5f
        };
        const float swapProgress = std::clamp(villaSwapTime / 0.3f, 0.0f, 1.0f);
        const float swapScale = 0.94f + 0.06f * swapProgress;
        rlScalef(swapScale, swapScale, swapScale);
        rlDisableBackfaceCulling();
        if (forestFocus) {
            DrawModel(forestModel, {-forestCenter.x, -forestCenter.y, -forestCenter.z}, 1.0f, WHITE);
        } else {
            DrawModel(villaModel, {-villaCenter.x, -villaCenter.y, -villaCenter.z}, 1.0f, WHITE);
        }
        rlEnableBackfaceCulling();
        rlPopMatrix();
        return;  // Don't draw the regular planet model when villa is shown
    }
    
    if (model != nullptr && presentation != nullptr) {
        const float localRadius = planetAssets.modelRadius(assetId);
        const float scale = radius / localRadius;
        const Vector3 center = planetAssets.modelCenter(assetId);
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, orientClock, studyMotion);
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
        rlPopMatrix();
    } else {
        rlPushMatrix();
        rlTranslatef(position.x, position.y, position.z);
        applyBodyOrientation(body, orientClock, studyMotion);
        const int rings = body.type == "Asteroid" ? 8 : 16;
        const int slices = body.type == "Asteroid" ? 12 : 24;
        DrawSphereEx({0.0f, 0.0f, 0.0f}, radius, rings, slices, bodyColor);
        if (body.id == "earth") {
            DrawCircle3D({0.0f, 0.0f, 0.0f}, radius * 1.02f, {1.0f, 0.0f, 0.0f}, 90.0f, alpha(accentColor, 0.55f));
        }
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
    if (simulation.soloStudy || villaFocus || villaTransition) rlSetClipPlanes(0.001, 14000.0);
    else rlSetClipPlanes(0.05, 14000.0);

    std::vector<Vector3> gridPositions;
    std::vector<double> gridMasses;
    gridPositions.reserve(simulation.bodies.size());
    gridMasses.reserve(simulation.bodies.size());
    for (const Body& body : simulation.bodies) {
        if (!body.active || body.type == "Asteroid") continue;
        // F2 is an Earth–Moon teaching view. Do not let hidden planets bend
        // the visible mesh or pull the sheet away from the focused system.
        if (simulation.earthMoonFocus &&
            body.id != "earth" && body.id != "moon" && body.id != "sun") continue;
        gridPositions.push_back(bodyWorldPosition(body, simulation));
        // Localized masses: Sun deepest but not a system-wide crater; planets sit in
        // smaller cups that read as real indentations under each body.
        if (body.luminous || body.id == "sun") {
            gridMasses.push_back(55.0);
        } else if (body.id == "jupiter" || body.id == "saturn") {
            gridMasses.push_back(9.0);
        } else if (body.id == "uranus" || body.id == "neptune") {
            gridMasses.push_back(6.0);
        } else if (body.type == "Moon" || body.id == "moon") {
            gridMasses.push_back(0.7);
        } else {
            gridMasses.push_back(3.2);
        }
    }
    gridSamplePositions = gridPositions;
    gridSampleMasses = gridMasses;
    gridSamplesValid = simulation.showGrid && !gridPositions.empty();
    if (simulation.showGrid) {
        // Anchor the rubber-sheet to the Sun so orbit/pan never drags the well
        // off the central body. Zoom only changes how much of the sheet we draw.
        const Body* sun = findBodyById(simulation, "sun");
        const Body* earth = findBodyById(simulation, "earth");
        const Body* moon = findBodyById(simulation, "moon");
        Vector3 anchor = (sun != nullptr && sun->active)
            ? bodyWorldPosition(*sun, simulation)
            : Vector3{0.0f, 0.0f, 0.0f};
        float viewExtent = std::clamp(std::max(cameraDistance * 0.42f, 36.0f), 36.0f, 220.0f);
        if (simulation.earthMoonFocus && earth != nullptr && earth->active) {
            const Vector3 earthPosition = bodyWorldPosition(*earth, simulation);
            const Vector3 moonPosition = (moon != nullptr && moon->active)
                ? bodyWorldPosition(*moon, simulation) : earthPosition;
            const Vector3 sunPosition = (sun != nullptr && sun->active)
                ? bodyWorldPosition(*sun, simulation) : earthPosition;
            anchor = Vector3Scale(Vector3Add(Vector3Add(earthPosition, moonPosition), sunPosition), 1.0f / 3.0f);
            viewExtent = std::clamp(std::max(cameraDistance * 0.75f, 12.0f), 12.0f, 80.0f);
        }
        grid.SetView(anchor.x, anchor.z, viewExtent);
        grid.Update(gridPositions, gridMasses, 1.0);
    }

    BeginMode3D(camera);
    if (villaFocus) {
        const Body* earth = findBodyById(simulation, "earth");
        if (earth != nullptr && earth->active) drawBody(*earth, simulation);
        EndMode3D();
        ++sceneFrames;
        return;
    }
    if (simulation.showGrid) grid.Draw();
    drawMilkyWay();
    drawAsteroidParticles(simulation);
    const auto starsStart = Clock::now();
    drawStars(simulation);
    const auto afterStars = Clock::now();
    if (!simulation.bodies.empty()) {
        if ((simulation.showVectors || simulation.educationalVisualization()) && !simulation.gravityMode) {
            drawGravityField(simulation);
        }
        const Body& centralBody = simulation.bodies.front();
        if (simulation.showOrbits) {
            for (std::size_t index = 1; index < simulation.bodies.size(); ++index) {
                if (!simulation.bodyVisibleInView(static_cast<int>(index))) continue;
                const Body& body = simulation.bodies[index];
                if (!body.active || body.type == "Spacecraft" || body.type == "Asteroid") continue;
                // Moon uses the educational ring from drawMoonOrbit instead of AU-scale ellipse.
                if (body.id == "moon" || body.type == "Moon") continue;
                const Body* orbitParent = &centralBody;
                if (!body.parentId.empty()) {
                    if (const Body* parent = findBodyById(simulation, body.parentId)) orbitParent = parent;
                }
                drawOrbit(body, *orbitParent, simulation);
            }
            if (simulation.showMoons) drawMoonOrbit(simulation);
        }
        if (simulation.showTrails && !simulation.earthMoonFocus) {
            for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                if (!simulation.bodyVisibleInView(index)) continue;
                const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
                if (body.active) drawTrail(body, simulation);
            }
        }
        // V mode (showVectors) OR G mode (gravityMode) - show force vectors
        if (simulation.showVectors || simulation.gravityMode || simulation.educationalVisualization()) {
            // When a body is selected: ONLY show that body's forces
            if (simulation.selected >= 0 &&
                simulation.selected < static_cast<int>(simulation.bodies.size())) {
                const Body& selected = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
                if (selected.active && selected.type != "Star") {
                    drawForceMotionVectors(selected, simulation);
                }
                // In G mode or when body selected: only selected body's forces shown
                // Do NOT draw other bodies
            } else if ((simulation.showVectors || simulation.educationalVisualization()) && !simulation.gravityMode) {
                // V mode only (NOT G mode) + no selection: show all bodies' forces
                for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                    if (!simulation.bodyVisibleInView(index)) continue;
                    const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
                    if (body.active && body.type != "Star") drawForceMotionVectors(body, simulation);
                }
            }
        }
        const auto bodiesStart = Clock::now();
        
        // Draw warped spacetime grid when gravity mode (Ctrl+G) is active
        // (Einstein spacetime background showing gravity well from sun)
        if (simulation.gravityMode && spacetimeModel.meshCount > 0) {
            const Body* sun = findBodyById(simulation, "sun");
            if (sun != nullptr) {
                const Vector3 sunPos = bodyWorldPosition(*sun, simulation);
                const float sunRadius = presentationRadius(*sun, simulation);

                // Compute well opening diameter from model's horizontal extents
                const float modelWidth = spacetimeBounds.max.x - spacetimeBounds.min.x;
                const float modelDepth = spacetimeBounds.max.z - spacetimeBounds.min.z;
                const float wellOpeningDiameter = std::max(modelWidth, modelDepth);

                // Calculate scale so well opening matches sun's diameter (2 × sunRadius)
                const float sunDiameter = sunRadius * 2.0f;
                const float scale = (wellOpeningDiameter > 0.0f) ? (sunDiameter / wellOpeningDiameter) : 1.0f;

                // Model bounding box analysis:
                // The model's center is at the bbox center. We need to find which end is the well opening.
                // Typically, the opening is the side with the larger cross-section (flat rim of spacetime grid)
                // and the narrow neck is the end pointing toward the massive object (Sun).
                const float modelHeight = spacetimeBounds.max.y - spacetimeBounds.min.y;
                const float modelCenterX = (spacetimeBounds.min.x + spacetimeBounds.max.x) * 0.5f;
                const float modelCenterY = (spacetimeBounds.min.y + spacetimeBounds.max.y) * 0.5f;
                const float modelCenterZ = (spacetimeBounds.min.z + spacetimeBounds.max.z) * 0.5f;

                // Position the model so the top of the well (max.y in local space) aligns with Sun's Y position
                // After translating center to -modelCenter and scaling, the model's local coordinates are transformed.
                // To place the opening at sunPos.y, we need:
                // sunPos.y = (max.y - modelCenterY) * scale + translateY
                // Therefore: translateY = sunPos.y - (max.y - modelCenterY) * scale
                const float halfHeight = modelHeight * 0.5f;
                const float yOffset = halfHeight * scale;

                rlPushMatrix();

                // Translate to sun's XZ position and offset Y so top of well is at sun
                rlTranslatef(sunPos.x, sunPos.y - yOffset, sunPos.z);

                // Apply uniform scale to match sun diameter with well opening
                rlScalef(scale, scale, scale);

                // Center the model at origin before scale is applied
                rlTranslatef(-modelCenterX, -modelCenterY, -modelCenterZ);

                DrawModel(spacetimeModel, {0.0f, 0.0f, 0.0f}, 1.0f, {255, 255, 255, 255});
                rlPopMatrix();
            }
        }

        // Always render normal bodies (on top of spacetime grid)
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
    if (simulation.showClassification || simulation.educationalVisualization()) drawBodyLabels(simulation);
    if (simulation.showClassification && simulation.showPlanetGroups) drawPlanetGroupLabels(simulation);
    if (simulation.educationalVisualization()) drawMotionLegend(simulation);
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
    const Vector3 center = seatedBodyPosition(body, simulation);
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
        // Learning labels are intentionally sparse: the selected body and the
        // central mass carry the explanation; classification mode remains the
        // explicit all-label view.
        if (!simulation.showClassification && index != simulation.selected && !body.luminous && body.id != "sun") continue;
        
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
        
        // The learning layer uses mass rather than a technical classification.
        const std::string detail = simulation.showClassification ? body.type :
            (std::string("Mass ") + [&] { std::ostringstream value; value << std::scientific << std::setprecision(2) << body.mass; return value.str(); }() + " kg");
        if (!detail.empty()) {
            const int typeWidth = MeasureText(detail.c_str(), 10);
            const float typeX = screenPos.x - typeWidth * 0.5f;
            DrawText(detail.c_str(), static_cast<int>(typeX), static_cast<int>(labelY + 14), 10, {180, 180, 180, 255});
        }
    }
}

void Renderer::drawMotionLegend(const Simulation& simulation) const {
    const float x = static_cast<float>(GetScreenWidth()) - 286.0f;
    const float y = ui::topBarHeight() + 18.0f;
    DrawRectangleRounded({x, y, 270.0f, 64.0f}, 0.12f, 8, {7, 13, 24, 210});
    DrawRectangleRoundedLines({x, y, 270.0f, 64.0f}, 0.12f, 8, {42, 69, 99, 210});
    DrawLineEx({x + 14.0f, y + 22.0f}, {x + 46.0f, y + 22.0f}, 2.0f, {72, 205, 245, 255});
    DrawTriangle({x + 46.0f, y + 22.0f}, {x + 38.0f, y + 18.0f}, {x + 38.0f, y + 26.0f}, {72, 205, 245, 255});
    DrawText("VELOCITY — moving now", static_cast<int>(x + 56.0f), static_cast<int>(y + 15.0f), 11, {207, 235, 247, 255});
    DrawLineEx({x + 14.0f, y + 46.0f}, {x + 46.0f, y + 46.0f}, 2.0f, {255, 181, 82, 255});
    DrawTriangle({x + 46.0f, y + 46.0f}, {x + 38.0f, y + 42.0f}, {x + 38.0f, y + 50.0f}, {255, 181, 82, 255});
    DrawText("GRAVITY — net pull (physics)", static_cast<int>(x + 56.0f), static_cast<int>(y + 39.0f), 11, {255, 221, 170, 255});
    (void)simulation;
}

void Renderer::drawPlanetGroupLabels(const Simulation& simulation) const {
    // Identify terrestrial and Jovian planets on screen
    std::vector<std::pair<Vector2, std::string>> terrestrial; // screen positions
    std::vector<std::pair<Vector2, std::string>> jovian;
    
    for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
        if (!simulation.bodyVisibleInView(index)) continue;
        const Body& body = simulation.bodies[static_cast<std::size_t>(index)];
        if (!body.active || body.type.empty()) continue;
        
        const Vector3 position = bodyWorldPosition(body, simulation);
        const Vector2 screenPos = GetWorldToScreen(position, camera);
        
        // Only draw if on screen
        if (screenPos.x < 0 || screenPos.x > GetScreenWidth() || 
            screenPos.y < 0 || screenPos.y > GetScreenHeight()) continue;
        
        // Categorize planets
        if (body.type == "Terrestrial Planet") {
            terrestrial.push_back({screenPos, body.name});
        } else if (body.type == "Jovian Planet") {
            jovian.push_back({screenPos, body.name});
        }
    }
    
    const Color terrestrialColor = {100, 200, 255, 255}; // Blue
    const Color jovianColor = {255, 180, 100, 255};       // Orange
    
    // Draw terrestrial group
    if (!terrestrial.empty()) {
        float minX = terrestrial[0].first.x;
        float maxX = terrestrial[0].first.x;
        float minY = terrestrial[0].first.y;
        float maxY = terrestrial[0].first.y;
        
        for (const auto& p : terrestrial) {
            minX = std::min(minX, p.first.x);
            maxX = std::max(maxX, p.first.x);
            minY = std::min(minY, p.first.y);
            maxY = std::max(maxY, p.first.y);
        }
        
        // Draw left brace
        const float braceGap = 30.0f;
        const float braceX = minX - braceGap;
        const float braceTopY = minY - 15.0f;
        const float braceBotY = maxY + 15.0f;
        const float braceW = 20.0f;
        
        DrawText("}", static_cast<int>(braceX), static_cast<int>(braceTopY), 60, terrestrialColor);
        DrawText("}", static_cast<int>(braceX), static_cast<int>(braceBotY - 45), 60, terrestrialColor);
        
        // Label: "Terrestrial"
        DrawText("Terrestrial", static_cast<int>(braceX - braceW - 80), 
                static_cast<int>((braceTopY + braceBotY) * 0.5f - 7), 14, terrestrialColor);
    }
    
    // Draw Jovian group
    if (!jovian.empty()) {
        float minX = jovian[0].first.x;
        float maxX = jovian[0].first.x;
        float minY = jovian[0].first.y;
        float maxY = jovian[0].first.y;
        
        for (const auto& p : jovian) {
            minX = std::min(minX, p.first.x);
            maxX = std::max(maxX, p.first.x);
            minY = std::min(minY, p.first.y);
            maxY = std::max(maxY, p.first.y);
        }
        
        // Draw right brace
        const float braceGap = 30.0f;
        const float braceX = maxX + braceGap;
        const float braceTopY = minY - 15.0f;
        const float braceBotY = maxY + 15.0f;
        
        DrawText("{", static_cast<int>(braceX), static_cast<int>(braceTopY), 60, jovianColor);
        DrawText("{", static_cast<int>(braceX), static_cast<int>(braceBotY - 45), 60, jovianColor);
        
        // Label: "Jovian"
        DrawText("Jovian", static_cast<int>(braceX + 20), 
                static_cast<int>((braceTopY + braceBotY) * 0.5f - 7), 14, jovianColor);
    }
    
    // Draw separator line in the middle if both groups present
    if (!terrestrial.empty() && !jovian.empty()) {
        float maxTerrestrial = terrestrial[0].first.x;
        float minJovian = jovian[0].first.x;
        for (const auto& p : terrestrial) maxTerrestrial = std::max(maxTerrestrial, p.first.x);
        for (const auto& p : jovian) minJovian = std::min(minJovian, p.first.x);
        
        const float separatorX = (maxTerrestrial + minJovian) * 0.5f;
        const float screenH = static_cast<float>(GetScreenHeight());
        DrawLineEx({separatorX, 60.0f}, {separatorX, screenH - 60.0f}, 2.0f, {150, 150, 150, 100});
        
        // Draw separator label
        DrawText("Asteroid Belt", static_cast<int>(separatorX - 50), 20, 12, {150, 150, 150, 180});
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
            GetRayCollisionSphere(ray, seatedBodyPosition(body, simulation), selectionRadius(body, simulation));
        if (collision.hit && collision.distance < nearest) {
            nearest = collision.distance;
            selected = index;
        }
    }
    return selected;
}

} // namespace bag
