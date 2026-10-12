#include "HUD.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <cstring>

#include <raym3/raym3.h>

#include "../education/EducationContent.hpp"
#include "../education/EducationChallenges.hpp"
#include "../education/EducationCatalog.hpp"
#include "../education/LearnerReport.hpp"
#include "../astronomy/CelestialBodyCatalog.hpp"
#include "../physics/PhysicsEngine.hpp"
#include "UiTheme.hpp"
#include "../rendering/Renderer.hpp"

namespace bag {
namespace {

using ui::theme;
using ui::withAlpha;

constexpr double TAU = 6.28318530717958647692; // 2π

std::string format(double value, int digits = 2) {
    if (!std::isfinite(value)) return "—";
    std::ostringstream output;
    output << std::fixed << std::setprecision(digits) << value;
    return output.str();
}

std::string scientific(double value, int digits = 3) {
    if (!std::isfinite(value)) return "—";
    std::ostringstream output;
    output << std::scientific << std::setprecision(digits) << value;
    return output.str();
}

std::string distance(double meters) {
    if (!std::isfinite(meters)) return "—";
    const double au = std::abs(meters) / PhysicsEngine::AU;
    if (au >= 0.01) return format(meters / PhysicsEngine::AU, 3) + " AU";
    if (std::abs(meters) >= 1e9) return format(meters / 1e9, 2) + " ×10⁹ km";
    return format(meters / 1000.0, 0) + " km";
}

std::string dayLengthLabel(double periodSeconds) {
    if (!std::isfinite(periodSeconds) || periodSeconds == 0.0) return "—";
    const double absolute = std::abs(periodSeconds);
    if (absolute < PhysicsEngine::DAY * 2.5) return format(absolute / 3600.0, 1) + " h";
    return format(absolute / PhysicsEngine::DAY, 1) + " Earth days";
}

std::string formatSpeed(double speed) {
    if (speed >= 1000000.0) return "×" + format(speed / 1000000.0, 0) + "M";
    if (speed >= 1000.0) return "×" + format(speed, 0);
    if (speed < 1.0) return "×" + format(speed, 2);
    if (speed < 10.0) return "×" + format(speed, 1);
    return "×" + format(speed, 0);
}

void drawWrapped(const std::string& content, float x, float y, float maxWidth, int size, Color color, int maxLines = 4) {
    if (content.empty() || maxLines <= 0) return;
    std::string line;
    int linesDrawn = 0;
    auto flush = [&]() {
        DrawText(line.c_str(), static_cast<int>(x), static_cast<int>(y + linesDrawn * (size + 4)), size, color);
        ++linesDrawn;
        line.clear();
    };
    std::string word;
    for (std::size_t i = 0; i <= content.size(); ++i) {
        const bool end = i == content.size();
        const char ch = end ? ' ' : content[i];
        if (ch == ' ' || ch == '\n' || end) {
            if (!word.empty()) {
                const std::string candidate = line.empty() ? word : line + " " + word;
                if (MeasureText(candidate.c_str(), size) > static_cast<int>(maxWidth) && !line.empty()) {
                    flush();
                    if (linesDrawn >= maxLines) return;
                    line = word;
                } else {
                    line = candidate;
                }
                word.clear();
            }
            if (ch == '\n') {
                flush();
                if (linesDrawn >= maxLines) return;
            }
        } else {
            word.push_back(ch);
        }
    }
    if (!line.empty() && linesDrawn < maxLines) flush();
}

} // namespace

void HUD::text(const char* value, float x, float y, float size, Color colorValue) const {
    DrawText(value, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), colorValue);
}

static double g_screenshotFeedbackTime = 0.0;
static int g_lastInspectorSelection = -2;
static float g_inspectorReveal = 0.0f;

void HUD::triggerScreenshotFeedback() {
    g_screenshotFeedbackTime = GetTime();
}

void HUD::screenshotFeedback() const {
    const double elapsed = GetTime() - g_screenshotFeedbackTime;
    if (elapsed < 0.0 || elapsed > 2.6) return;
    const auto& t = theme();
    const float alphaVal = elapsed > 2.0 ? static_cast<float>((2.6 - elapsed) / 0.6) : 1.0f;
    const float width = 280.0f;
    const float height = 36.0f;
    const float x = (static_cast<float>(GetScreenWidth()) - width) * 0.5f;
    const float y = ui::topBarHeight() + 14.0f;
    const Rectangle box = {x, y, width, height};
    DrawRectangleRounded(box, 0.4f, 12, withAlpha(t.surfaceAlt, 0.95f * alphaVal));
    DrawRectangleRoundedLines(box, 0.4f, 12, withAlpha(t.accent, 0.9f * alphaVal));
    DrawCircle(static_cast<int>(x + 22.0f), static_cast<int>(y + 18.0f), 5.0f, withAlpha(t.success, alphaVal));
    DrawText("CAPTURE SAVED (PNG)", static_cast<int>(x + 36.0f), static_cast<int>(y + 11.0f), 12,
             withAlpha(t.text, alphaVal));
}

// The application bar is drawn by UIManager above this page content, so the
// page only supplies its own heading. No Simulation state is needed here: the
// title and subtitle fully describe the page.
void HUD::pageChrome(const char* title, const char* subtitle) const {
    const auto& t = theme();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), t.bg);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           withAlpha(ui::rgba(14, 28, 44), 0.6f), t.bg);
    // The application bar is drawn by UIManager above this page content, so the
    // page only supplies its own heading. Title and subtitle are inset past the
    // navigation rail so the rail never crowds the text.
    const float headingX = ui::contentGutter() + ui::kGapLoose;
    text(title, headingX, ui::contentTop(), 26, t.text);
    text(subtitle, headingX, ui::contentTop() + 32.0f, 13, t.textMuted);
}

void HUD::selectedInfo(const Simulation& simulation) const {
    // Reveal the live inspector with a short, non-blocking transition when the
    // selected object changes. This state is presentation-only.
    const bool hasSelection = simulation.selected >= 0 &&
                              simulation.selected < static_cast<int>(simulation.bodies.size()) &&
                              !simulation.gravityMode;
    if (simulation.selected != g_lastInspectorSelection) {
        g_lastInspectorSelection = simulation.selected;
        g_inspectorReveal = hasSelection ? 0.0f : 1.0f;
    }
    const float target = hasSelection ? 1.0f : 0.0f;
    const float blend = 1.0f - std::exp(-14.0f * std::max(0.001f, GetFrameTime()));
    g_inspectorReveal += (target - g_inspectorReveal) * blend;
    if (std::abs(target - g_inspectorReveal) < 0.001f) g_inspectorReveal = target;
    if (!hasSelection || g_inspectorReveal < 0.01f) return;
    const float reveal = g_inspectorReveal;
    const Body& body = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    const CelestialBodyInfo info = lookupCelestialInfo(body.id, body.type, body.luminous);
    const auto& t = theme();
    // The reveal offset is presentation-only and is applied to a copy, so the
    // pointer-ownership rect in ChromeLayout always covers the settled panel.
    const Rectangle box = shellLayout_.inspector;
    Rectangle animatedBox = box;
    animatedBox.x += (1.0f - reveal) * 18.0f;
    animatedBox.y += (1.0f - reveal) * 8.0f;
    const Rectangle drawBox = animatedBox;
    raym3::Card(drawBox, raym3::CardVariant::Elevated);

    raym3::Text(body.name.c_str(), {drawBox.x + 30.0f, drawBox.y + 10.0f, drawBox.width - 42.0f, 22.0f}, 16.0f, t.text, raym3::FontWeight::Bold);
    raym3::Text(classificationLabel(info.classification), {drawBox.x + 30.0f, drawBox.y + 30.0f, drawBox.width - 42.0f, 16.0f}, 10.0f, t.textDim, raym3::FontWeight::Medium);

    const float thumbCx = drawBox.x + 48.0f;
    const float thumbCy = drawBox.y + 88.0f;
    const float thumbR = 26.0f;
    DrawCircleGradient({thumbCx, thumbCy}, thumbR + 8.0f,
                       withAlpha({body.accent.r, body.accent.g, body.accent.b, 255}, 0.35f),
                       withAlpha({body.accent.r, body.accent.g, body.accent.b, 255}, 0.0f));
    DrawCircle(static_cast<int>(thumbCx), static_cast<int>(thumbCy), thumbR,
               {body.color.r, body.color.g, body.color.b, 255});
    DrawCircleGradient({thumbCx - 8.0f, thumbCy - 8.0f}, thumbR * 0.95f,
                       withAlpha(WHITE, 0.45f), withAlpha(BLACK, 0.7f));
    DrawCircleLines(static_cast<int>(thumbCx), static_cast<int>(thumbCy), thumbR,
                    withAlpha({body.accent.r, body.accent.g, body.accent.b, 255}, 0.8f));
    if (body.ringed) {
        DrawLineEx({thumbCx - thumbR * 1.5f, thumbCy + 6.0f}, {thumbCx + thumbR * 1.5f, thumbCy - 6.0f}, 2.5f,
                   withAlpha({body.accent.r, body.accent.g, body.accent.b, 255}, 0.9f));
    }

    const float valX = drawBox.x + 92.0f;
    const float offset = 120.0f;
    const double displayMass = body.mass > 0.0 ? body.mass : body.baseMass;
    ui::drawLabelValue("Mass", (scientific(displayMass) + " kg").c_str(), valX, drawBox.y + 54.0f, offset);
    ui::drawLabelValue("Radius", distance(body.realRadius).c_str(), valX, drawBox.y + 74.0f, offset);

    if (info.classification == BodyClassification::Star) {
        ui::drawLabelValue("Role", info.role ? info.role : "Central star", valX, drawBox.y + 94.0f, offset);
    } else if (info.classification == BodyClassification::NaturalSatellite) {
        ui::drawLabelValue("Parent", info.parentBodyId ? info.parentBodyId : body.parentId.c_str(),
                           valX, drawBox.y + 94.0f, offset);
    } else if (info.orderFromSun > 0) {
        ui::drawLabelValue("Order", (std::string(ordinalFromSun(info.orderFromSun)) + " from Sun").c_str(),
                           valX, drawBox.y + 94.0f, offset);
    } else {
        ui::drawLabelValue("Position", distance(simulation.distanceFromSun(body)).c_str(),
                           valX, drawBox.y + 94.0f, offset);
    }

    ui::drawLabelValue("Velocity", (format(length(body.velocity) / 1000.0, 2) + " km/s").c_str(),
                       drawBox.x + 18.0f, drawBox.y + 124.0f, 140.0f);

    bool accelerationValid = false;
    const Vec3 acceleration = PhysicsEngine::acceleration(simulation.bodies,
        static_cast<std::size_t>(simulation.selected), {}, &accelerationValid);
    if (simulation.educationalVisualization() && accelerationValid) {
        ui::drawLabelValue("Acceleration", (scientific(length(acceleration), 2) + " m/s²").c_str(),
                           drawBox.x + 18.0f, drawBox.y + 144.0f, 140.0f);
        ui::drawLabelValue("Distance to Sun", distance(simulation.distanceFromSun(body)).c_str(),
                           drawBox.x + 18.0f, drawBox.y + 164.0f, 140.0f);
        text("AMBER ARROW = NET GRAVITY", drawBox.x + 18.0f, drawBox.y + 190.0f, 10.0f, t.warn);
        return;
    }

    if (info.classification == BodyClassification::Star) {
        ui::drawLabelValue("Spectral", info.spectralClass ? info.spectralClass : "—",
                           drawBox.x + 18.0f, drawBox.y + 144.0f, 140.0f);
        ui::drawLabelValue("Energy", info.energySource ? info.energySource : "Fusion",
                           drawBox.x + 18.0f, drawBox.y + 164.0f, 140.0f);
        ui::drawLabelValue("Surface g", (format(simulation.surfaceGravity(body), 2) + " m/s²").c_str(),
                           drawBox.x + 18.0f, drawBox.y + 184.0f, 140.0f);
    } else {
        const double periodDays = body.orbitalPeriod > 0.0 ? (body.orbitalPeriod / PhysicsEngine::DAY) : 0.0;
        ui::drawLabelValue("Orbital Period", (periodDays > 0.0 ? format(periodDays, 2) + " days" : "—").c_str(),
                           drawBox.x + 18.0f, drawBox.y + 144.0f, 140.0f);
        ui::drawLabelValue("Rotation", dayLengthLabel(body.rotationPeriod).c_str(),
                           drawBox.x + 18.0f, drawBox.y + 164.0f, 140.0f);
        ui::drawLabelValue("Surface g", (format(simulation.surfaceGravity(body), 2) + " m/s²").c_str(),
                           drawBox.x + 18.0f, drawBox.y + 184.0f, 140.0f);
    }

    raym3::Divider({drawBox.x + 16.0f, drawBox.y + 208.0f, drawBox.width - 32.0f, 1.0f});

    if (info.naturalSatelliteCount > 0 && info.classification != BodyClassification::NaturalSatellite) {
        text("NATURAL SATELLITES", drawBox.x + 18.0f, drawBox.y + 218.0f, 11.0f, t.accent);
        ui::drawLabelValue("Count", std::to_string(info.naturalSatelliteCount).c_str(),
                           drawBox.x + 18.0f, drawBox.y + 238.0f, 140.0f);
        if (info.largestPlanet) {
            text("Largest planet in the Solar System", drawBox.x + 18.0f, drawBox.y + 258.0f, 11.0f, t.textMuted);
        }
    } else {
        text("EDUCATIONAL NOTE", drawBox.x + 18.0f, drawBox.y + 218.0f, 11.0f, t.accent);
        const std::string note = info.description ? info.description : simulation.selectedBodyLesson();
        drawWrapped(note, drawBox.x + 18.0f, drawBox.y + 238.0f, drawBox.width - 36.0f, 11, t.textMuted, 3);
    }

    if (simulation.soloStudy || simulation.earthMoonFocus) {
        const char* modeNote = simulation.earthMoonFocus
            ? "Earth–Moon Focus · Press Q to return"
            : "Study Mode · Press Q to return";
        text(modeNote, drawBox.x + 18.0f, drawBox.y + drawBox.height - 28.0f, 11.0f, t.accentStrong);
    }
}

void HUD::learningExplanation(const Simulation& simulation) const {
    if (!simulation.educationalVisualization() || simulation.selected < 0 ||
        simulation.selected >= static_cast<int>(simulation.bodies.size())) return;

    const auto& t = theme();
    const Body& body = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    const Body* source = nullptr;
    double strongestPull = 0.0;
    for (const Body& candidate : simulation.bodies) {
        if (!candidate.active || &candidate == &body || candidate.mass <= 0.0) continue;
        const Vec3 delta = candidate.position - body.position;
        const double distanceSquared = dot(delta, delta);
        if (distanceSquared <= 1.0) continue;
        const double pull = PhysicsEngine::G * candidate.mass / distanceSquared;
        if (pull > strongestPull) { strongestPull = pull; source = &candidate; }
    }
    if (source == nullptr) return;

    // Amber arrow = net N-body acceleration. The caption names the strongest
    // single attractor so the lesson stays readable for multi-body systems.
    const Vec3 towardSource = source->position - body.position;
    const double radius = length(towardSource);
    const double radialVelocity = radius > 0.0 ? dot(body.velocity, towardSource) / radius : 0.0;
    const double speed = length(body.velocity);
    std::string message;
    if (simulation.visualizationMode == VisualizationMode::ExploreKids) {
        message = "Gravity is pulling " + body.name + " toward " + source->name + ".";
        if (speed > 1.0 && std::abs(radialVelocity) < speed * 0.65) {
            message += " It is also moving sideways, so its path curves.";
        }
    } else if (radialVelocity < -speed * 0.65) {
        message = body.name + " is falling toward " + source->name +
            "; gravity is changing its velocity. (Amber arrow = net pull from all bodies.)";
    } else if (std::abs(radialVelocity) < speed * 0.65) {
        message = body.name + " moves forward while gravity pulls inward toward " +
            source->name + ". Together, these make a curved orbit.";
    } else {
        message = "Gravity pulls " + body.name + " toward " + source->name +
            ", even as it moves away. Gravity always acts — not only when falling.";
    }

    const float width = std::min(520.0f, static_cast<float>(GetScreenWidth()) - 120.0f);
    // Sit above the scale/mode badge so both remain readable.
    const Rectangle panel = {(static_cast<float>(GetScreenWidth()) - width) * 0.5f,
                             static_cast<float>(GetScreenHeight()) - 132.0f, width, 56.0f};
    raym3::Card(panel, raym3::CardVariant::Elevated);

    text(visualizationModeName(simulation.visualizationMode), panel.x + 14.0f, panel.y + 8.0f, 10.0f, t.accentStrong);
    drawWrapped(message, panel.x + 14.0f, panel.y + 22.0f, panel.width - 28.0f, 12, t.text, 2);
}

void HUD::forcesMotionOverlay(const Simulation& simulation) const {
    if (!simulation.showVectors) return;
    const auto& t = theme();
    const float screenW = static_cast<float>(GetScreenWidth());
    const Rectangle box = {screenW - 320.0f - 16.0f, ui::topBarHeight() + 14.0f, 320.0f, 340.0f};
    // Shift down if classification is also open
    const float y0 = simulation.showClassification ? box.y + 290.0f : box.y;
    const Rectangle panel = {box.x, y0, box.width, 300.0f};
    raym3::Card(panel, raym3::CardVariant::Elevated);

    text("FORCES & MOTION", panel.x + 14.0f, panel.y + 12.0f, 12.0f, t.accent);
    text("VECTOR LEGEND", panel.x + 14.0f, panel.y + 34.0f, 10.0f, t.textDim);
    DrawLine(static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 54.0f),
             static_cast<int>(panel.x + 40.0f), static_cast<int>(panel.y + 54.0f), {80, 220, 255, 255});
    text("Velocity — direction of motion", panel.x + 48.0f, panel.y + 46.0f, 11.0f, t.text);
    DrawLine(static_cast<int>(panel.x + 14.0f), static_cast<int>(panel.y + 74.0f),
             static_cast<int>(panel.x + 40.0f), static_cast<int>(panel.y + 74.0f), {255, 170, 70, 255});
    text("Gravity — net gravitational accel.", panel.x + 48.0f, panel.y + 66.0f, 11.0f, t.text);

    text("Velocity", panel.x + 14.0f, panel.y + 96.0f, 11.0f, {80, 220, 255, 255});
    text("Instantaneous rate of change of position", panel.x + 14.0f, panel.y + 112.0f, 10.0f, t.textMuted);
    text("(magnitude and direction).", panel.x + 14.0f, panel.y + 126.0f, 10.0f, t.textMuted);

    text("Gravity", panel.x + 14.0f, panel.y + 148.0f, 11.0f, {255, 170, 70, 255});
    text("Attractive force:  F = G M m / r²", panel.x + 14.0f, panel.y + 164.0f, 10.0f, t.textMuted);
    text("Vector from live N-body accelerations.", panel.x + 14.0f, panel.y + 178.0f, 10.0f, t.textMuted);

    text("Centripetal acceleration", panel.x + 14.0f, panel.y + 200.0f, 11.0f, t.accentStrong);
    text("a_c = v²/r  — inward accel. for curves", panel.x + 14.0f, panel.y + 216.0f, 10.0f, t.textMuted);
    text("Gravity can supply this in an orbit.", panel.x + 14.0f, panel.y + 230.0f, 10.0f, t.textMuted);

    text("Centrifugal (rotating frame only)", panel.x + 14.0f, panel.y + 252.0f, 11.0f, t.warn);
    text("Apparent outward pseudo-force — not an", panel.x + 14.0f, panel.y + 268.0f, 10.0f, t.textMuted);
    text("extra gravitational force in inertial frame.", panel.x + 14.0f, panel.y + 282.0f, 10.0f, t.textMuted);
}

void HUD::gravityModeOverlay(const Simulation& simulation) const {
    if (!simulation.gravityMode) return;
    const auto& t = theme();
    // Expanded panel (350x380) to fit centripetal/centrifugal calculations
    const float screenH = static_cast<float>(GetScreenHeight());
    const Rectangle panel = {ui::contentGutter() + 58.0f, screenH - 380.0f - 14.0f, 350.0f, 380.0f};
    raym3::Card(panel, raym3::CardVariant::Elevated);

    text("GRAVITY - INTERACTIVE", panel.x + 14.0f, panel.y + 8.0f, 11.0f, t.accent);
    text("↑/↓ adjust  TAB cycle  R reset  Q exit", panel.x + 14.0f, panel.y + 22.0f, 9.0f, t.textDim);

    if (simulation.selected < 0 || simulation.selected >= static_cast<int>(simulation.bodies.size())) {
        text("Select a body", panel.x + 14.0f, panel.y + 40.0f, 11.0f, t.textMuted);
        return;
    }

    const Body& body = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    const double base = body.baseMass > 0.0 ? body.baseMass : body.mass;
    const double effective = body.mass;

    ui::drawLabelValue("Body", body.name.c_str(), panel.x + 14.0f, panel.y + 40.0f, 120.0f);
    
    // Mass field (editable)
    const Color massBg = simulation.gravityModeEditField == 0 ? t.accentStrong : t.surface;
    DrawRectangle(static_cast<int>(panel.x + 14.0f) - 4, static_cast<int>(panel.y + 58.0f) - 4, 280, 16, massBg);
    ui::drawLabelValue("Mass", (scientific(base) + " kg").c_str(), panel.x + 14.0f, panel.y + 58.0f, 130.0f);
    
    // Radius field (editable)
    const Color radiusBg = simulation.gravityModeEditField == 1 ? t.accentStrong : t.surface;
    DrawRectangle(static_cast<int>(panel.x + 14.0f) - 4, static_cast<int>(panel.y + 74.0f) - 4, 280, 16, radiusBg);
    ui::drawLabelValue("Radius", (format(body.radius / 1000.0, 1) + " Mkm").c_str(), panel.x + 14.0f, panel.y + 74.0f, 130.0f);

    // Dominant gravitational source: parent, else Sun, else nearest massive body.
    const Body* source = nullptr;
    if (!body.parentId.empty()) {
        for (const Body& candidate : simulation.bodies) {
            if (candidate.id == body.parentId && candidate.active) {
                source = &candidate;
                break;
            }
        }
    }
    if (source == nullptr) {
        for (const Body& candidate : simulation.bodies) {
            if (candidate.active && (candidate.id == "sun" || candidate.luminous) && candidate.id != body.id) {
                source = &candidate;
                break;
            }
        }
    }

    if (source != nullptr) {
        const double dist = length(source->position - body.position);
        const double sourceMu = PhysicsEngine::G * source->mass;
        const double force = PhysicsEngine::G * effective * source->mass / std::max(dist * dist, 1.0);
        const double accel = PhysicsEngine::G * source->mass / std::max(dist * dist, 1.0);
        const double weight = effective * accel;
        const double orbitalVelocity = std::sqrt(sourceMu / std::max(dist, 1.0));
        const double orbitalPeriod = TAU * std::sqrt((dist * dist * dist) / std::max(sourceMu, 1.0));
        const double centripetal = effective * orbitalVelocity * orbitalVelocity / std::max(dist, 1.0);
        
        ui::drawLabelValue("Source", source->name.c_str(), panel.x + 14.0f, panel.y + 92.0f, 120.0f);
        ui::drawLabelValue("Dist", distance(dist).c_str(), panel.x + 180.0f, panel.y + 92.0f, 120.0f);
        
        // Row 1: Gravitational force
        ui::drawLabelValue("Force", (scientific(force, 2) + "N").c_str(), panel.x + 14.0f, panel.y + 108.0f, 120.0f);
        ui::drawLabelValue("Accel", (scientific(accel, 2) + "m/s²").c_str(), panel.x + 180.0f, panel.y + 108.0f, 120.0f);
        
        // Row 2: Weight & Orbital velocity
        ui::drawLabelValue("Weight", (scientific(weight, 2) + "N").c_str(), panel.x + 14.0f, panel.y + 124.0f, 120.0f);
        ui::drawLabelValue("Vel", (format(orbitalVelocity, 0) + "m/s").c_str(), panel.x + 180.0f, panel.y + 124.0f, 120.0f);
        
        // Row 3: Orbital period
        const int days = static_cast<int>(orbitalPeriod / 86400.0);
        const int hours = static_cast<int>((orbitalPeriod - days * 86400.0) / 3600.0);
        std::string periodStr = std::to_string(days) + "d " + std::to_string(hours) + "h";
        ui::drawLabelValue("Period", periodStr.c_str(), panel.x + 14.0f, panel.y + 140.0f, 120.0f);
        
        // Row 4: Centripetal & Centrifugal forces
        ui::drawLabelValue("Centripetal", (scientific(centripetal, 2) + "N").c_str(), panel.x + 14.0f, panel.y + 156.0f, 120.0f);
        ui::drawLabelValue("Centrifugal", (scientific(centripetal, 2) + "N").c_str(), panel.x + 180.0f, panel.y + 156.0f, 120.0f);
        
        // Formulas section
        text("Formulas:", panel.x + 14.0f, panel.y + 180.0f, 9.0f, t.accentStrong);
        text("F=GMm/r²    g=GM/R²    W=mg", panel.x + 14.0f, panel.y + 192.0f, 7.5f, t.textMuted);
        text("v=√(GM/r)    T=2π√(r³/GM)", panel.x + 14.0f, panel.y + 203.0f, 7.5f, t.textMuted);
        text("Fc=mv²/r    Fcf=-mv²/r", panel.x + 14.0f, panel.y + 214.0f, 7.5f, t.textMuted);
     }
}

void HUD::earthMoonOverlay(const Simulation& simulation) const {
    if (!simulation.earthMoonFocus) return;
    const auto& t = theme();
    const float screenW = static_cast<float>(GetScreenWidth());
    const Rectangle panel = {screenW * 0.5f - 200.0f, ui::topBarHeight() + 14.0f, 400.0f, 200.0f};
    raym3::Card(panel, raym3::CardVariant::Elevated);

    text("EARTH–MOON SYSTEM", panel.x + 16.0f, panel.y + 12.0f, 13.0f, t.accent);
    text("Earth", panel.x + 16.0f, panel.y + 40.0f, 12.0f, t.text);
    text("• 3rd planet from Sun  • Terrestrial  • Satellite: Moon",
         panel.x + 16.0f, panel.y + 58.0f, 11.0f, t.textMuted);
    text("Moon", panel.x + 16.0f, panel.y + 84.0f, 12.0f, t.text);
    text("• Earth's natural satellite  • Orbits Earth",
         panel.x + 16.0f, panel.y + 102.0f, 11.0f, t.textMuted);
    text("Orbital relationship", panel.x + 16.0f, panel.y + 128.0f, 12.0f, t.accentStrong);
    text("Gravity provides the attractive interaction.", panel.x + 16.0f, panel.y + 146.0f, 11.0f, t.textMuted);
    text("Both bodies orbit their common centre of mass.", panel.x + 16.0f, panel.y + 164.0f, 11.0f, t.textMuted);
}

void HUD::scaleModeBadge(const Simulation& simulation) const {
    const auto& t = theme();
    const std::string mode = std::string(visualScaleModeLabel(simulation.visualScaleMode)) +
        "   ·   " + visualizationModeName(simulation.visualizationMode) + "   F10";
    const char* label = mode.c_str();

    // Rendered through raym3 rather than DrawText. raylib's default font is
    // ASCII-only, so the "•" separator used here came out as a literal "?", and
    // MeasureText could not size it either -- the badge was both wrong and
    // mis-sized. Roboto covers the separator.
    //
    // Width is estimated from the character count at Roboto's average advance
    // rather than measured, because raym3 exposes no text-measurement API. The
    // estimate is padded, so the pill is never too narrow for its label.
    const float fontSize = 11.0f;
    const float textW = static_cast<float>(std::strlen(label)) * fontSize * 0.54f;
    const float screenW = static_cast<float>(GetScreenWidth());
    const Rectangle badge = {screenW * 0.5f - (textW + 32.0f) * 0.5f,
                             static_cast<float>(GetScreenHeight()) - 54.0f,
                             textW + 32.0f, 24.0f};
    DrawRectangleRounded(badge, 0.42f, 8, withAlpha(t.surfaceAlt, 0.86f));
    DrawRectangleRoundedLines(badge, 0.42f, 8, withAlpha(t.borderSoft, 0.9f));
    raym3::Text(label, badge, fontSize,
                simulation.visualScaleMode == VisualScaleMode::Realistic ? t.warn : t.textDim,
                raym3::FontWeight::Medium, raym3::TextAlignment::Center);
}

// View presets as a segmented control. These are mutually exclusive framings, so
// a segmented control states that exclusivity directly: exactly one is selected
// at all times, and the whole segment is the target rather than a small glyph.
//
// This replaces a hand-rolled row of rectangles that measured its own labels
// with raylib's default font while the shell rendered the same labels in Roboto,
// and that computed a drag threshold against a moving pointer. Clicking it felt
// unreliable because the hit region and the painted region were computed
// independently.
void HUD::viewControls(Simulation& simulation, Renderer& renderer) const {
    const auto& t = theme();
    const Rectangle box = shellLayout_.viewControls;
    if (box.width <= 0.0f || box.height <= 0.0f) return;

    raym3::Card(box, raym3::CardVariant::Filled);

    raym3::Text("VIEW PRESETS", {box.x + 14.0f, box.y + 9.0f, box.width - 28.0f, 14.0f},
                9.0f, t.textDim, raym3::FontWeight::Bold);

    enum class Preset { Solar, EarthMoon, Empty };
    Preset active = Preset::Solar;
    if (simulation.earthMoonFocus) {
        active = Preset::EarthMoon;
    } else if (simulation.scenarioId == "empty_space") {
        active = Preset::Empty;
    }

    raym3::SegmentedButtonItem items[] = {
        {"Solar", nullptr, false},
        {"Earth–Moon", nullptr, false},
        {"Empty", nullptr, false},
    };

    int selected = static_cast<int>(active);
    const Rectangle strip = {box.x + 12.0f, box.y + 28.0f, box.width - 24.0f, 32.0f};
    if (raym3::SegmentedButton(strip, items, 3, &selected)) {
        switch (static_cast<Preset>(selected)) {
        case Preset::Solar:
            simulation.earthMoonFocus = false;
            if (simulation.loadScenario("default_solar_system")) renderer.resetSystemView();
            break;
        case Preset::EarthMoon:
            if (simulation.loadScenario("earth_orbit")) {
                for (int index = 0; index < static_cast<int>(simulation.bodies.size()); ++index) {
                    const auto& body = simulation.bodies[static_cast<std::size_t>(index)];
                    if (body.id == "earth" && body.active) {
                        simulation.selectBody(index);
                        simulation.isolateSelected();
                        simulation.earthMoonFocus = true;
                        break;
                    }
                }
                if (!simulation.earthMoonFocus) {
                    simulation.earthMoonFocus = true;
                    renderer.focusEarthMoonSystem(simulation, true);
                }
            }
            break;
        case Preset::Empty:
            simulation.earthMoonFocus = false;
            if (simulation.loadScenario("empty_space")) renderer.resetSystemView();
            break;
        }
    }

    // Shortcut legend, so the keyboard contract is discoverable. Placed below
    // the segment, inside the card's bottom padding.
    raym3::Text("F1  ·  F2  ·  F3", {box.x + 14.0f, box.y + 66.0f,
                                     box.width - 28.0f, 12.0f}, 9.0f, t.textDim,
                raym3::FontWeight::Medium);
}

void HUD::orientationIndicator(float cameraYaw) const {
    const auto& t = theme();
    const float cx = shellLayout_.compass.x + shellLayout_.compass.width * 0.5f;
    const float cy = shellLayout_.compass.y + shellLayout_.compass.height * 0.5f;
    const float r = 24.0f;

    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r, withAlpha(t.accent, 0.42f));
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r * 0.5f, withAlpha(t.border, 0.5f));

    // Compass crosshair ticks
    DrawLine(static_cast<int>(cx - r), static_cast<int>(cy), static_cast<int>(cx - r + 5.0f), static_cast<int>(cy), t.accent);
    DrawLine(static_cast<int>(cx + r - 5.0f), static_cast<int>(cy), static_cast<int>(cx + r), static_cast<int>(cy), t.accent);
    DrawLine(static_cast<int>(cx), static_cast<int>(cy - r), static_cast<int>(cx), static_cast<int>(cy - r + 5.0f), t.accent);
    DrawLine(static_cast<int>(cx), static_cast<int>(cy + r - 5.0f), static_cast<int>(cx), static_cast<int>(cy + r), t.accent);

    // Cardinal directions
    DrawText("N", static_cast<int>(cx - 3.0f), static_cast<int>(cy - r - 12.0f), 10, t.accentStrong);
    DrawText("E", static_cast<int>(cx + r + 4.0f), static_cast<int>(cy - 5.0f), 10, t.textDim);
    DrawText("S", static_cast<int>(cx - 3.0f), static_cast<int>(cy + r + 3.0f), 10, t.textDim);
    DrawText("W", static_cast<int>(cx - r - 12.0f), static_cast<int>(cy - 5.0f), 10, t.textDim);

    // Rotating pointer based on cameraYaw
    const float needleAngle = -cameraYaw - PI * 0.5f;
    const float tipX = cx + (r - 6.0f) * std::cos(needleAngle);
    const float tipY = cy + (r - 6.0f) * std::sin(needleAngle);
    DrawLineEx({cx, cy}, {tipX, tipY}, 1.5f, t.accentStrong);
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 2.0f, t.accentStrong);
}

void HUD::draw(Simulation& simulation, Renderer& renderer, float cameraYaw) const {
    switch (simulation.screen) {
    case AppScreen::Simulation:
        // The raym3 shell owns the application bar, navigation rail, tab strip,
        // and primary display controls. Keep the scientific HUD's contextual
        // panels and educational overlays below.
        viewControls(simulation, renderer);
        selectedInfo(simulation);
        learningExplanation(simulation);
        orientationIndicator(cameraYaw);
        scaleModeBadge(simulation);
        forcesMotionOverlay(simulation);
        gravityModeOverlay(simulation);
        earthMoonOverlay(simulation);
        screenshotFeedback();
        break;
    case AppScreen::Education: educationScreenPanel(simulation); break;
    case AppScreen::LearningLab: learningLabScreenPanel(simulation); break;
    case AppScreen::ScenarioBrowser: scenarioScreen(simulation); break;
    case AppScreen::Telemetry: telemetryScreen(simulation); break;
    case AppScreen::MissionDesigner: missionScreen(simulation); break;
    case AppScreen::Settings: settingsScreen(simulation); break;
    case AppScreen::Help: helpScreen(simulation); break;
    }
}

void HUD::educationScreenPanel(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome("Learn", "Guided orbital-mechanics lessons, experiments, and challenges");
    const LearnerReport report = buildLearnerReport(simulation.educationProgress);
    const AuthoredLesson& lesson = authoredLessonAt(simulation.lesson);

    const Rectangle left = {ui::contentGutter(), ui::contentTop() + 70.0f, 340.0f, 560.0f};
    const Rectangle mid = {ui::contentGutter() + 360.0f, ui::contentTop() + 70.0f, 420.0f, 560.0f};
    const Rectangle right = {ui::contentGutter() + 800.0f, ui::contentTop() + 70.0f, 340.0f, 560.0f};
    ui::drawPanel(left);
    ui::drawPanel(mid);
    ui::drawPanel(right);

    text("Progress", left.x + 20, left.y + 18, 12, t.accent);
    text(("Lessons  " + std::to_string(report.completedLessons) + " / " + std::to_string(report.totalLessons)).c_str(),
         left.x + 20, left.y + 44, 14, t.text);
    text(("Experiments  " + std::to_string(report.completedExperiments) + " / " + std::to_string(report.totalExperiments)).c_str(),
         left.x + 20, left.y + 68, 14, t.text);
    text(("Challenges  " + std::to_string(report.completedChallenges) + " / " + std::to_string(report.totalChallenges)).c_str(),
         left.x + 20, left.y + 92, 14, t.text);
    DrawRectangleRounded({left.x + 20, left.y + 126, 300, 8}, 0.5f, 8, t.surfaceAlt);
    const float fill = report.totalLessons == 0 ? 0.0f
        : 300.0f * static_cast<float>(report.completedLessons) / static_cast<float>(report.totalLessons);
    DrawRectangleRounded({left.x + 20, left.y + 126, fill, 8}, 0.5f, 8, t.accent);

    text("Lessons", left.x + 20, left.y + 160, 12, t.textDim);
    for (int index = 0; index < authoredLessonCount(); ++index) {
        const AuthoredLesson& item = authoredLessonAt(index);
        const bool selected = index == simulation.lesson;
        const bool complete = simulation.educationProgress.lessonComplete(index);
        const float y = left.y + 186.0f + index * 34.0f;
        if (selected) DrawRectangleRounded({left.x + 12, y - 6, 316, 30}, 0.2f, 8, withAlpha(t.accent, 0.18f));
        text((std::string(complete ? "● " : "○ ") + item.title).c_str(), left.x + 22, y, 14,
             selected ? t.text : t.textMuted);
    }

    text(lesson.title.c_str(), mid.x + 22, mid.y + 22, 24, t.text);
    text(lesson.shortDescription.c_str(), mid.x + 22, mid.y + 56, 14, t.textMuted);
    text("Objectives", mid.x + 22, mid.y + 100, 12, t.accent);
    for (std::size_t index = 0; index < lesson.objectives.size() && index < 4; ++index) {
        text(("• " + lesson.objectives[index]).c_str(), mid.x + 22, mid.y + 126.0f + index * 24.0f, 14, t.text);
    }
    text(("Difficulty  " + std::string(educationDifficultyName(lesson.difficulty))).c_str(),
         mid.x + 22, mid.y + 250, 13, t.textMuted);
    text(("Duration  " + std::to_string(lesson.estimatedMinutes) + " min").c_str(),
         mid.x + 22, mid.y + 274, 13, t.textMuted);
    text(simulation.educationProgress.lessonComplete(simulation.lesson) ? "Completed" : "In progress",
         mid.x + 22, mid.y + 310, 16,
         simulation.educationProgress.lessonComplete(simulation.lesson) ? t.success : t.warn);

    text("Learner report", right.x + 20, right.y + 18, 12, t.accent);
    text(report.hasAverageScore ? ("Avg score  " + format(report.averageScore, 1)).c_str() : "Avg score  —",
         right.x + 20, right.y + 48, 14, t.text);
    text("Strongest", right.x + 20, right.y + 90, 12, t.textDim);
    if (report.strongestAreas.empty()) text("None yet", right.x + 20, right.y + 114, 13, t.textMuted);
    for (std::size_t i = 0; i < report.strongestAreas.size() && i < 3; ++i)
        text(("• " + report.strongestAreas[i]).c_str(), right.x + 20, right.y + 114.0f + i * 22.0f, 13, t.text);
    text("Practice next", right.x + 20, right.y + 200, 12, t.textDim);
    if (report.areasNeedingImprovement.empty()) text("None identified", right.x + 20, right.y + 224, 13, t.textMuted);
    for (std::size_t i = 0; i < report.areasNeedingImprovement.size() && i < 3; ++i)
        text(("• " + report.areasNeedingImprovement[i]).c_str(), right.x + 20, right.y + 224.0f + i * 22.0f, 13, t.text);
    text("Recommended", right.x + 20, right.y + 320, 12, t.accent);
    text(report.recommendation.available ? report.recommendation.title.c_str() : "None",
         right.x + 20, right.y + 348, 15, t.accentStrong);

    text("A / D browse   Enter start   B observe   Y evaluate   N next   Esc back",
         ui::contentGutter(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::learningLabScreenPanel(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome("Learning Lab", "Ghana curriculum activities on the shared simulation engine");

    if (!simulation.curriculumReady) {
        const Rectangle box = {ui::contentGutter(), ui::contentTop() + 70.0f, 900.0f, 140.0f};
        ui::drawPanel(box);
        text("Curriculum pack failed to load", box.x + 24, box.y + 36, 20, t.warn);
        DrawTextEx(GetFontDefault(), simulation.curriculumLoadError.c_str(),
                   {box.x + 24, box.y + 74}, 14, 1, t.textMuted);
        return;
    }

    const std::string gradeKey = std::string("grade.") + gradeIdString(simulation.learnerGrade);
    const std::string gradeLabel = simulation.curriculumI18n.translate(gradeKey, gradeIdString(simulation.learnerGrade));
    text((gradeLabel + "  ·  " + presentationLayerName(simulation.learnerPresentationLayer()) +
          "  ·  " + simulation.curriculumCatalog.manifest.curriculumVersion).c_str(),
         ui::contentGutter(), ui::contentTop() + 58.0f, 14, t.textMuted);

    const auto activities = simulation.activitiesForLearnerGrade();
    const Rectangle left = {ui::contentGutter(), ui::contentTop() + 90.0f, 360.0f, 520.0f};
    const Rectangle right = {ui::contentGutter() + 380.0f, ui::contentTop() + 90.0f, 760.0f, 520.0f};
    ui::drawPanel(left);
    ui::drawPanel(right);

    text("Activities", left.x + 20, left.y + 18, 12, t.accent);
    if (activities.empty()) text("No activities for this grade.", left.x + 20, left.y + 54, 14, t.textMuted);
    const int visible = std::min(static_cast<int>(activities.size()), 14);
    for (int index = 0; index < visible; ++index) {
        const CurriculumActivity* activity = activities[static_cast<std::size_t>(index)];
        const bool selected = index == simulation.curriculumActivityIndex;
        const float y = left.y + 52.0f + index * 30.0f;
        if (selected) DrawRectangleRounded({left.x + 12, y - 5, 336, 28}, 0.2f, 8, withAlpha(t.accent, 0.18f));
        const std::string title = simulation.curriculumI18n.translate(activity->titleKey, activity->id);
        text(ui::shorten(title, 34).c_str(), left.x + 22, y, 14, selected ? t.text : t.textMuted);
    }

    const CurriculumActivity* activity = simulation.selectedCurriculumActivity();
    if (!activity) {
        text("Select a grade activity to begin.", right.x + 24, right.y + 28, 16, t.textMuted);
        return;
    }

    const std::string title = simulation.curriculumI18n.translate(activity->titleKey, activity->id);
    text(title.c_str(), right.x + 24, right.y + 22, 22, t.text);
    text((std::string(curriculumAlignmentName(activity->alignment)) + "  ·  " + activity->curriculumReference).c_str(),
         right.x + 24, right.y + 52, 13,
         activity->alignment == CurriculumAlignment::Official ? t.success : t.warn);
    text((activity->subject + " / " + activity->strand + " / " + activity->subStrand).c_str(),
         right.x + 24, right.y + 74, 13, t.textMuted);
    DrawTextEx(GetFontDefault(), activity->learningObjective.c_str(),
               {right.x + 24, right.y + 104}, 14, 1, t.text);

    const LearningLabState& lab = simulation.learningLabSession.state();
    text(("Session  " + std::string(LearningLabSession::stepName(lab.step))).c_str(),
         right.x + 24, right.y + 170, 12, t.accent);

    if (lab.step == LearningLabStep::Selecting) {
        text("Press Enter to start the inquiry sequence.", right.x + 24, right.y + 198, 14, t.textMuted);
        DrawTextEx(GetFontDefault(),
                   simulation.curriculumI18n.translate(activity->instructionsKey, activity->instructionsKey).c_str(),
                   {right.x + 24, right.y + 228}, 14, 1, t.text);
    } else if (const CurriculumActivity* active = simulation.learningLabSession.activity()) {
        if (lab.stepIndex >= 0 && lab.stepIndex < static_cast<int>(active->steps.size())) {
            const ActivityStep& step = active->steps[static_cast<std::size_t>(lab.stepIndex)];
            text(simulation.curriculumI18n.translate(step.titleKey, LearningLabSession::stepName(lab.step)).c_str(),
                 right.x + 24, right.y + 198, 16, t.text);
            if (!step.bodyKey.empty()) {
                DrawTextEx(GetFontDefault(),
                           simulation.curriculumI18n.translate(step.bodyKey, step.bodyKey).c_str(),
                           {right.x + 24, right.y + 228}, 14, 1, t.textMuted);
            }
        }

        if (lab.step == LearningLabStep::Assess) {
            const CurriculumQuestion* question = simulation.selectedCurriculumQuestion();
            if (question) {
                DrawTextEx(GetFontDefault(),
                           simulation.curriculumI18n.translate(question->questionKey, question->id).c_str(),
                           {right.x + 24, right.y + 280}, 14, 1, t.text);
                for (std::size_t index = 0; index < question->options.size() && index < 6; ++index) {
                    const QuestionOption& option = question->options[index];
                    const bool chosen = std::find(simulation.curriculumAnswerSelection.begin(),
                                                  simulation.curriculumAnswerSelection.end(),
                                                  option.id) != simulation.curriculumAnswerSelection.end();
                    const float y = right.y + 316.0f + static_cast<float>(index) * 24.0f;
                    text((std::to_string(index + 1) + "  " + (chosen ? "● " : "○ ") +
                          simulation.curriculumI18n.translate(option.labelKey, option.id)).c_str(),
                         right.x + 24, y, 14, chosen ? t.accentStrong : t.textMuted);
                }
            }
        }

        if ((lab.step == LearningLabStep::Feedback || lab.step == LearningLabStep::Complete) &&
            simulation.lastCurriculumAssessment) {
            const AssessmentResult& result = *simulation.lastCurriculumAssessment;
            text((std::string(result.correct ? "Passed" : "Try again") + "  " + format(result.score, 0) + "/100").c_str(),
                 right.x + 24, right.y + 280, 18, result.correct ? t.success : t.warn);
            text(simulation.curriculumI18n.translate(result.feedbackKey, result.feedbackKey).c_str(),
                 right.x + 24, right.y + 312, 14, t.text);
            if (simulation.lastCurriculumMisconception) {
                text(simulation.curriculumI18n.translate(simulation.lastCurriculumMisconception->responseKey,
                                                         simulation.lastCurriculumMisconception->responseKey).c_str(),
                     right.x + 24, right.y + 348, 14, t.warn);
            }
        }
    }

    text("[ ] grade   A/D activity   Enter start   N next   1–6 answers   C submit   Esc back",
         ui::contentGutter(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::scenarioScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome("Scenes", "Choose a starting system for the laboratory");

    const struct Scenario { const char* id; const char* title; const char* description; const char* bodies; } scenarios[] = {
        {"default_solar_system", "Solar System", "Sun, planets, Moon, and expanded major moons.", "30 bodies"},
        {"earth_orbit", "Earth Orbit", "Focused Sun–Earth–Moon study.", "3 bodies"},
        {"empty_space", "Empty Space", "Blank scene for custom systems.", "0 bodies"},
    };

    for (int index = 0; index < 3; ++index) {
        const float y = ui::contentTop() + 80.0f + index * 110.0f;
        const Rectangle card = {ui::contentGutter(), y, 720.0f, 92.0f};
        const bool selected = index == simulation.scenarioBrowserSelection;
        ui::drawPanel(card, selected);
        if (selected) DrawRectangleRoundedLines(card, 0.08f, 16, t.accent);
        text(scenarios[index].title, card.x + 24, card.y + 22, 22, selected ? t.text : t.textMuted);
        text(scenarios[index].description, card.x + 24, card.y + 54, 14, t.textMuted);
        text(scenarios[index].bodies, card.x + 560, card.y + 36, 14, t.accentStrong);
    }

    const Rectangle side = {ui::contentGutter() + 760.0f, ui::contentTop() + 80.0f, 380.0f, 320.0f};
    ui::drawPanel(side);
    text("Current", side.x + 22, side.y + 22, 12, t.accent);
    text(simulation.scenarioMetadata.name.c_str(), side.x + 22, side.y + 52, 22, t.text);
    DrawTextEx(GetFontDefault(), simulation.scenarioMetadata.description.c_str(),
               {side.x + 22, side.y + 92}, 14, 1, t.textMuted);
    ui::drawLabelValue("Epoch", simulation.scenarioMetadata.epoch.c_str(), side.x + 22, side.y + 170);
    ui::drawLabelValue("Frame", simulation.scenarioMetadata.referenceFrame.c_str(), side.x + 22, side.y + 196);

    text("↑↓ select   Enter load   Esc back", ui::contentGutter(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::telemetryScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome("Telemetry", "Deterministic sampling of scientific state");

    const Rectangle main = {ui::contentGutter(), ui::contentTop() + 70.0f, 760.0f, 500.0f};
    const Rectangle side = {ui::contentGutter() + 790.0f, ui::contentTop() + 70.0f, 350.0f, 500.0f};
    ui::drawPanel(main);
    ui::drawPanel(side);

    text(simulation.telemetryEnabled() ? "Live session" : "No active session",
         main.x + 24, main.y + 24, 16, simulation.telemetryEnabled() ? t.success : t.warn);

    if (simulation.telemetry.samples.empty()) {
        text("No samples yet.", main.x + 24, main.y + 80, 20, t.text);
        text("Press K to start a 60 s session. Select a body first for relative values.",
             main.x + 24, main.y + 118, 14, t.textMuted);
    } else {
        const TelemetrySample& sample = simulation.telemetry.samples.back();
        text(sample.bodyId.c_str(), main.x + 24, main.y + 70, 24, t.accentStrong);
        ui::drawLabelValue("Time", (format(sample.simulationTimeSeconds, 1) + " s").c_str(), main.x + 24, main.y + 120);
        ui::drawLabelValue("Velocity", (format(length(sample.velocityMps) / 1000.0, 3) + " km/s").c_str(), main.x + 24, main.y + 146);
        ui::drawLabelValue("Acceleration", (scientific(length(sample.accelerationMps2)) + " m/s²").c_str(), main.x + 24, main.y + 172);
        ui::drawLabelValue("Energy", (scientific(sample.specificEnergyJPerKg) + " J/kg").c_str(), main.x + 24, main.y + 198);
        ui::drawLabelValue("Ang. mom.", scientific(sample.angularMomentumMagnitudeM2PerS).c_str(), main.x + 24, main.y + 224);
        ui::drawLabelValue("Eccentricity", format(sample.eccentricity, 5).c_str(), main.x + 24, main.y + 250);
        ui::drawLabelValue("Apoapsis", distance(sample.apoapsisM).c_str(), main.x + 24, main.y + 276);
        ui::drawLabelValue("Method", integratorName(sample.integrator), main.x + 24, main.y + 302);
        ui::drawLabelValue("Status", telemetryStatusName(sample.status), main.x + 24, main.y + 328);
    }

    text("Session", side.x + 22, side.y + 22, 12, t.accent);
    ui::drawLabelValue("Scenario", simulation.telemetry.metadata.scenarioName.c_str(), side.x + 22, side.y + 60);
    ui::drawLabelValue("Samples", std::to_string(simulation.telemetry.samples.size()).c_str(), side.x + 22, side.y + 86);
    ui::drawLabelValue("Interval", (format(simulation.telemetry.metadata.samplingIntervalSeconds, 1) + " s").c_str(),
                       side.x + 22, side.y + 112);
    ui::drawLabelValue("Frame", simulation.telemetry.metadata.referenceFrame.c_str(), side.x + 22, side.y + 138);
    ui::drawLabelValue("Integrator", simulation.telemetry.metadata.integrator.c_str(), side.x + 22, side.y + 164);

    text("K start/stop   J JSON   C CSV   Esc back",
         ui::contentGutter(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::missionScreen(const Simulation& simulation) const {
    (void)simulation;
    const auto& t = theme();
    pageChrome("Mission Tools", "Analytical APIs for maneuvers and trajectory studies");

    const Rectangle main = {ui::contentGutter(), ui::contentTop() + 70.0f, 820.0f, 420.0f};
    const Rectangle side = {ui::contentGutter() + 850.0f, ui::contentTop() + 70.0f, 290.0f, 420.0f};
    ui::drawPanel(main);
    ui::drawPanel(side);

    text("Available capabilities", main.x + 24, main.y + 24, 12, t.accent);
    const char* items[] = {
        "Maneuver nodes and vector delta-v",
        "Hohmann transfer analysis",
        "Spacecraft mass and propellant accounting",
        "Gravity-assist turn-angle analysis",
        "Trajectory prediction and mission metrics",
    };
    for (int i = 0; i < 5; ++i) text(("•  " + std::string(items[i])).c_str(), main.x + 28, main.y + 70.0f + i * 36.0f, 16, t.text);
    text("No full mission editor is exposed yet — this screen stays honest about the API surface.",
         main.x + 24, main.y + 280, 14, t.warn);

    text("Next step", side.x + 20, side.y + 24, 12, t.accent);
    DrawTextEx(GetFontDefault(),
               "Use the mission and spacecraft APIs for deterministic analysis. Return to Sim for visual context.",
               {side.x + 20, side.y + 60}, 14, 1, t.textMuted);

    text("Esc back", ui::contentGutter(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::settingsScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome("Settings", "Live simulation and presentation controls");

    const Rectangle left = {ui::contentGutter(), ui::contentTop() + 70.0f, 560.0f, 460.0f};
    const Rectangle right = {ui::contentGutter() + 590.0f, ui::contentTop() + 70.0f, 550.0f, 460.0f};
    ui::drawPanel(left);
    ui::drawPanel(right);

    text("Simulation", left.x + 24, left.y + 22, 12, t.accent);
    ui::drawLabelValue("Integrator", simulation.settings.integrator.c_str(), left.x + 24, left.y + 60);
    ui::drawLabelValue("Timestep", (format(simulation.settings.timestepSeconds, 1) + " s").c_str(), left.x + 24, left.y + 88);
    ui::drawLabelValue("Speed", formatSpeed(simulation.speed).c_str(), left.x + 24, left.y + 116);
    ui::drawLabelValue("Trails", simulation.showTrails ? "On" : "Off", left.x + 24, left.y + 144);
    ui::drawLabelValue("Forces (V)", simulation.showVectors ? "On" : "Off", left.x + 24, left.y + 172);
    ui::drawLabelValue("Orbits", simulation.showOrbits ? "On" : "Off", left.x + 24, left.y + 200);
    ui::drawLabelValue("Gravity (G)", simulation.gravityMode ? "On" : "Off", left.x + 24, left.y + 228);
    ui::drawLabelValue("Grid (Ctrl+G)", simulation.showGrid ? "On" : "Off", left.x + 24, left.y + 256);
    ui::drawLabelValue("Highlight", simulation.settings.selectionHighlightEnabled ? "On" : "Off", left.x + 24, left.y + 284);
    DrawRectangleLinesEx({left.x + 20, left.y + 246, 500, 30}, 1.0f, withAlpha(t.accent, 0.45f));
    text("Click Highlight row or press H to toggle", left.x + 24, left.y + 300, 13, t.textMuted);
    text(("Lab grade " + std::string(gradeIdString(simulation.learnerGrade)) + " · " +
          presentationLayerName(simulation.learnerPresentationLayer())).c_str(),
         left.x + 24, left.y + 330, 13, t.textMuted);
    text("I integrator   ± timestep", left.x + 24, left.y + 370, 13, t.textDim);

    text("Camera & display", right.x + 24, right.y + 22, 12, t.accent);
    text("Left-drag   orbit", right.x + 24, right.y + 60, 15, t.text);
    text("Right / Middle-drag   pan", right.x + 24, right.y + 90, 15, t.text);
    text("Wheel   zoom   ·   Shift+Wheel faster", right.x + 24, right.y + 120, 15, t.text);
    text("Click body   select   ·   Z zoom alone", right.x + 24, right.y + 150, 15, t.text);
    text("Q back one step   ·   Home system view", right.x + 24, right.y + 180, 15, t.text);
    text("Double-click   focus body under cursor", right.x + 24, right.y + 210, 15, t.text);
    text("F fullscreen", right.x + 24, right.y + 240, 15, t.text);
    text("V O T G   vectors / orbits / trails / grid", right.x + 24, right.y + 290, 14, t.textMuted);

    text("Esc back", ui::contentGutter(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::helpScreen(const Simulation& simulation) const {
    (void)simulation; // The help page is a static legend; it reads no live state.
    const auto& t = theme();
    pageChrome("Help", "Controls that exist in the current build");

    const Rectangle left = {ui::contentGutter(), ui::contentTop() + 70.0f, 560.0f, 520.0f};
    const Rectangle right = {ui::contentGutter() + 590.0f, ui::contentTop() + 70.0f, 550.0f, 520.0f};
    ui::drawPanel(left);
    ui::drawPanel(right);

    text("Navigate", left.x + 24, left.y + 22, 12, t.accent);
    text("Tab cycle screens", left.x + 24, left.y + 56, 15, t.text);
    text("L Learn   F9 Lab   F5 Scenes", left.x + 24, left.y + 86, 15, t.text);
    text("F6 Telemetry   F7 Mission   F8 Settings", left.x + 24, left.y + 116, 15, t.text);
    text("F1–F3 load scenes   H Help   Esc → Sim", left.x + 24, left.y + 146, 15, t.text);

    text("Simulate", left.x + 24, left.y + 200, 12, t.accent);
    text("Space pause   ± speed   R reset", left.x + 24, left.y + 234, 15, t.text);
    text("Ctrl+S select   V Forces & Motion   G Gravity mass mode", left.x + 24, left.y + 264, 13, t.text);
    text("F2 Earth–Moon   / visual scale   Z isolate · Q back", left.x + 24, left.y + 294, 13, t.text);
    text("1–6 speed   Ctrl+G grid   P probe", left.x + 24, left.y + 324, 13, t.textMuted);

    text("Camera", left.x + 24, left.y + 370, 12, t.accent);
    text("Drag orbit · Right/Middle pan · Wheel zoom", left.x + 24, left.y + 404, 14, t.text);
    text("Z isolate · Q back · Home reset · Click select", left.x + 24, left.y + 434, 14, t.text);

    text("Learn / Lab", right.x + 24, right.y + 22, 12, t.accent);
    text("A/D lessons   Enter start   B observe", right.x + 24, right.y + 56, 15, t.text);
    text("Y evaluate   N next   Q retry", right.x + 24, right.y + 86, 15, t.text);
    text("Lab: [ ] grade · 1–6 answers · C submit", right.x + 24, right.y + 116, 15, t.text);

    text("Telemetry", right.x + 24, right.y + 180, 12, t.accent);
    text("K start/stop   J JSON   C CSV", right.x + 24, right.y + 214, 15, t.text);

    text("Physics modes", right.x + 24, right.y + 280, 12, t.accent);
    text("V velocity + gravity vectors (live physics).", right.x + 24, right.y + 314, 13, t.textMuted);
    text("G then ↑/↓ changes mass — orbits respond.", right.x + 24, right.y + 344, 13, t.textMuted);
    text("Paused: zoom/pan never advances physics.", right.x + 24, right.y + 374, 13, t.textMuted);

    text("Esc back", ui::contentGutter(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

} // namespace bag
