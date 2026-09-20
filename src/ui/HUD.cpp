#include "HUD.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "../education/EducationContent.hpp"
#include "../education/EducationChallenges.hpp"
#include "../education/EducationCatalog.hpp"
#include "../education/LearnerReport.hpp"
#include "../astronomy/CelestialBodyCatalog.hpp"
#include "../physics/PhysicsEngine.hpp"
#include "UiTheme.hpp"

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

std::string time(double seconds) {
    const double absolute = std::abs(seconds);
    if (absolute < 60) return format(absolute, 1) + " s";
    if (absolute < PhysicsEngine::DAY) return format(absolute / 3600.0, 1) + " h";
    if (absolute < PhysicsEngine::YEAR) return format(absolute / PhysicsEngine::DAY, 1) + " d";
    return format(absolute / PhysicsEngine::YEAR, 2) + " y";
}

std::string dayLengthLabel(double periodSeconds) {
    if (!std::isfinite(periodSeconds) || periodSeconds == 0.0) return "—";
    const double absolute = std::abs(periodSeconds);
    if (absolute < PhysicsEngine::DAY * 2.5) return format(absolute / 3600.0, 1) + " h";
    return format(absolute / PhysicsEngine::DAY, 1) + " Earth days";
}

std::string spinDirectionLabel(const Body& body) {
    if (!std::isfinite(body.rotationPeriod) || body.rotationPeriod == 0.0) return "—";
    if (std::abs(body.axialTilt) >= 80.0) return "Sideways";
    if (body.rotationPeriod < 0.0) return "Retrograde";
    return "Prograde";
}

std::string formatSpeed(double speed) {
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

void HUD::pageChrome(const Simulation& simulation, const char* title, const char* subtitle) const {
    const auto& t = theme();
    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), t.bg);
    DrawRectangleGradientV(0, 0, GetScreenWidth(), GetScreenHeight(),
                           withAlpha(ui::rgba(14, 28, 44), 0.6f), t.bg);
    topBar(simulation);
    text(title, ui::pagePad(), ui::contentTop(), 26, t.text);
    text(subtitle, ui::pagePad(), ui::contentTop() + 32.0f, 13, t.textMuted);
}

void HUD::topBar(const Simulation& simulation) const {
    const auto& t = theme();
    const float barW = static_cast<float>(GetScreenWidth());
    const float barH = ui::topBarHeight();

    // Dark translucent background with thin bottom border
    DrawRectangle(0, 0, static_cast<int>(barW), static_cast<int>(barH), withAlpha(t.bgElevated, 0.96f));
    DrawRectangleGradientV(0, 0, static_cast<int>(barW), 22, withAlpha(t.accent, 0.04f), withAlpha(t.accent, 0.0f));
    DrawRectangle(0, static_cast<int>(barH) - 1, static_cast<int>(barW), 1, withAlpha(t.border, 0.85f));

    // Left: Scientific BAGS_LAB Logo & wordmark
    constexpr float logoCx = 26.0f;
    constexpr float logoCy = 27.0f;
    DrawCircleLines(static_cast<int>(logoCx), static_cast<int>(logoCy), 12.0f, withAlpha(t.accent, 0.5f));
    DrawCircleLines(static_cast<int>(logoCx), static_cast<int>(logoCy), 7.0f, withAlpha(t.accentStrong, 0.8f));
    DrawCircle(static_cast<int>(logoCx), static_cast<int>(logoCy), 3.0f, t.accentStrong);
    // Orbiting electron/satellite pip
    const float orbitAngle = static_cast<float>(GetTime() * 1.5);
    DrawCircle(static_cast<int>(logoCx + 12.0f * std::cos(orbitAngle)),
               static_cast<int>(logoCy + 12.0f * std::sin(orbitAngle)), 1.8f, t.accent);

    text("BAGS_LAB", 48.0f, 12.0f, 18.0f, t.text);
    text("C++17 ORBITAL MECHANICS LABORATORY", 48.0f, 33.0f, 9.0f, t.textDim);

    // Center Navigation tabs
    const ui::NavLayout nav = ui::buildNavLayout(barW);
    const Vector2 mouse = GetMousePosition();
    for (std::size_t index = 0; index < ui::kNavTabs.size(); ++index) {
        const Rectangle hit = nav.hits[index];
        const bool active = simulation.screen == ui::kNavTabs[index].screen;
        const bool hover = CheckCollisionPointRec(mouse, hit);

        if (active) {
            DrawRectangleRounded({hit.x, hit.y + 4.0f, hit.width, hit.height - 6.0f}, 0.15f, 8,
                                 withAlpha(t.accent, 0.14f));
            DrawRectangle(static_cast<int>(hit.x + 4.0f), static_cast<int>(barH) - 3,
                          static_cast<int>(hit.width - 8.0f), 2, t.accent);
        } else if (hover) {
            DrawRectangleRounded({hit.x, hit.y + 4.0f, hit.width, hit.height - 6.0f}, 0.15f, 8,
                                 withAlpha(t.surfaceAlt, 0.5f));
        }

        const int fontSize = 11;
        const float textWidth = static_cast<float>(MeasureText(ui::kNavTabs[index].label, fontSize));
        DrawText(ui::kNavTabs[index].label,
                 static_cast<int>(hit.x + (hit.width - textWidth) * 0.5f),
                 static_cast<int>(hit.y + (hit.height - fontSize) * 0.5f - 1.0f),
                 fontSize, active ? t.text : (hover ? t.accentStrong : t.textMuted));
    }

    // Right Side: SIM TIME, Speed Pill, Status, Settings, Exit
    const float right = barW;

    // Exit / Power Button
    const Rectangle powerBtn = {right - 34.0f, 12.0f, 28.0f, 30.0f};
    const bool powerHover = CheckCollisionPointRec(mouse, powerBtn);
    if (powerHover) DrawRectangleRounded(powerBtn, 0.3f, 8, withAlpha(t.danger, 0.2f));
    DrawCircleLines(static_cast<int>(powerBtn.x + 14.0f), static_cast<int>(powerBtn.y + 15.0f), 7.0f,
                    powerHover ? t.danger : t.textMuted);
    DrawLine(static_cast<int>(powerBtn.x + 14.0f), static_cast<int>(powerBtn.y + 9.0f),
             static_cast<int>(powerBtn.x + 14.0f), static_cast<int>(powerBtn.y + 16.0f),
             powerHover ? t.danger : t.textMuted);

    // Settings Gear Icon
    const Rectangle settingsBtn = {right - 66.0f, 12.0f, 28.0f, 30.0f};
    const bool settingsHover = CheckCollisionPointRec(mouse, settingsBtn);
    if (settingsHover) DrawRectangleRounded(settingsBtn, 0.3f, 8, withAlpha(t.surfaceAlt, 0.6f));
    const float gearCx = settingsBtn.x + 14.0f;
    const float gearCy = settingsBtn.y + 15.0f;
    DrawCircleLines(static_cast<int>(gearCx), static_cast<int>(gearCy), 5.0f,
                    settingsHover ? t.accentStrong : t.textMuted);
    for (int i = 0; i < 6; ++i) {
        const float a = static_cast<float>(i * PI / 3.0);
        DrawLine(static_cast<int>(gearCx + 5.0f * std::cos(a)), static_cast<int>(gearCy + 5.0f * std::sin(a)),
                 static_cast<int>(gearCx + 8.0f * std::cos(a)), static_cast<int>(gearCy + 8.0f * std::sin(a)),
                 settingsHover ? t.accentStrong : t.textMuted);
    }

    // Wireless / Antenna Telemetry Status Icon
    const float antX = right - 96.0f;
    const float antY = 27.0f;
    DrawCircleLines(static_cast<int>(antX), static_cast<int>(antY + 4.0f), 8.0f, withAlpha(t.accent, 0.7f));
    DrawCircleLines(static_cast<int>(antX), static_cast<int>(antY + 4.0f), 4.0f, withAlpha(t.accent, 0.9f));
    DrawCircle(static_cast<int>(antX), static_cast<int>(antY + 4.0f), 1.5f, t.accentStrong);

    // Speed Control Pill [ ◀ | ⏵/⏸ x1.0 | ▶ ]
    const Rectangle speedPill = {right - 216.0f, 11.0f, 108.0f, 32.0f};
    DrawRectangleRounded(speedPill, 0.45f, 12, withAlpha(t.surface, 0.9f));
    DrawRectangleRoundedLines(speedPill, 0.45f, 12, withAlpha(t.border, 0.8f));

    // Play/pause symbol
    if (simulation.paused) {
        DrawTriangle({speedPill.x + 14.0f, speedPill.y + 10.0f},
                     {speedPill.x + 14.0f, speedPill.y + 22.0f},
                     {speedPill.x + 23.0f, speedPill.y + 16.0f}, t.warn);
    } else {
        DrawRectangle(static_cast<int>(speedPill.x + 14.0f), static_cast<int>(speedPill.y + 10.0f), 3, 12, t.success);
        DrawRectangle(static_cast<int>(speedPill.x + 20.0f), static_cast<int>(speedPill.y + 10.0f), 3, 12, t.success);
    }
    const std::string spdStr = formatSpeed(simulation.speed);
    text(spdStr.c_str(), speedPill.x + 30.0f, speedPill.y + 9.0f, 12, t.text);
    // Chevrons on sides
    text("‹", speedPill.x + 76.0f, speedPill.y + 6.0f, 14, t.textDim);
    text("›", speedPill.x + 92.0f, speedPill.y + 6.0f, 14, t.textDim);

    // SIM TIME Display: "SIM TIME" and "0.00 days"
    const double days = simulation.simTime / PhysicsEngine::DAY;
    std::string daysStr = format(days, 2) + " days";
    text("SIM TIME", right - 336.0f, 12.0f, 9.0f, t.textDim);
    text(daysStr.c_str(), right - 336.0f, 24.0f, 16.0f, t.accentStrong);

    // Live status dot
    DrawCircle(static_cast<int>(right - 350.0f), 27, 4.0f, simulation.paused ? t.warn : t.success);
}

void HUD::bottomBar(const Simulation& simulation) const {
    (void)simulation;
}

void HUD::toolRail(const Simulation& simulation) const {
    const auto& t = theme();
    const ui::HudLayout layout = ui::buildHudLayout(static_cast<float>(GetScreenWidth()),
                                               static_cast<float>(GetScreenHeight()),
                                               simulation.selected >= 0, simulation.soloStudy);
    const Vector2 mouse = GetMousePosition();

    // Tool rail glass container
    DrawRectangleRounded(layout.toolRail, 0.2f, 12, withAlpha(t.panelGlass, 0.92f));
    DrawRectangleRoundedLines(layout.toolRail, 0.2f, 12, withAlpha(t.border, 0.8f));

    // 7 technical tools:
    // 0: Select (default active)
    // 1: Focus
    // 2: Screenshot
    // 3: Coordinate/Grid
    // 4: Orbit Analysis
    // 5: Layers / Trails
    // 6: Data / Objects
    for (int i = 0; i < 7; ++i) {
        const Rectangle btn = layout.toolButtons[static_cast<std::size_t>(i)];
        bool isActive = false;
        if (i == 0) isActive = (simulation.selected < 0);
        else if (i == 1) isActive = (simulation.selected >= 0);
        else if (i == 3) isActive = simulation.showGrid;
        else if (i == 4) isActive = simulation.showOrbits;
        else if (i == 5) isActive = simulation.showTrails || simulation.showVectors;

        const bool hover = CheckCollisionPointRec(mouse, btn);

        if (isActive) {
            DrawRectangleRounded(btn, 0.25f, 8, withAlpha(t.accent, 0.24f));
            DrawRectangleRoundedLines(btn, 0.25f, 8, t.accent);
        } else if (hover) {
            DrawRectangleRounded(btn, 0.25f, 8, withAlpha(t.surfaceAlt, 0.6f));
            DrawRectangleRoundedLines(btn, 0.25f, 8, withAlpha(t.borderSoft, 0.9f));
        }

        const Color iconColor = isActive ? t.accentStrong : (hover ? t.text : t.textMuted);
        const float cx = btn.x + btn.width * 0.5f;
        const float cy = btn.y + btn.height * 0.5f;

        switch (i) {
        case 0: { // Cursor / Select arrow
            DrawTriangle({cx - 5.0f, cy - 7.0f}, {cx - 5.0f, cy + 7.0f}, {cx + 6.0f, cy + 1.0f}, iconColor);
            break;
        }
        case 1: { // Focus target / Crosshairs
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 7.0f, iconColor);
            DrawCircle(static_cast<int>(cx), static_cast<int>(cy), 2.0f, iconColor);
            DrawLine(static_cast<int>(cx - 10.0f), static_cast<int>(cy), static_cast<int>(cx - 7.0f), static_cast<int>(cy), iconColor);
            DrawLine(static_cast<int>(cx + 7.0f), static_cast<int>(cy), static_cast<int>(cx + 10.0f), static_cast<int>(cy), iconColor);
            DrawLine(static_cast<int>(cx), static_cast<int>(cy - 10.0f), static_cast<int>(cx), static_cast<int>(cy - 7.0f), iconColor);
            DrawLine(static_cast<int>(cx), static_cast<int>(cy + 7.0f), static_cast<int>(cx), static_cast<int>(cy + 10.0f), iconColor);
            break;
        }
        case 2: { // Camera / Capture
            DrawRectangleRoundedLines({cx - 9.0f, cy - 5.0f, 18.0f, 13.0f}, 0.2f, 4, iconColor);
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy + 1.5f), 4.0f, iconColor);
            DrawRectangle(static_cast<int>(cx - 3.0f), static_cast<int>(cy - 8.0f), 6, 3, iconColor);
            break;
        }
        case 3: { // Globe / Grid
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 8.0f, iconColor);
            DrawLine(static_cast<int>(cx - 8.0f), static_cast<int>(cy), static_cast<int>(cx + 8.0f), static_cast<int>(cy), iconColor);
            DrawLine(static_cast<int>(cx), static_cast<int>(cy - 8.0f), static_cast<int>(cx), static_cast<int>(cy + 8.0f), iconColor);
            break;
        }
        case 4: { // Orbit analysis
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), 8.0f, iconColor);
            DrawCircle(static_cast<int>(cx + 6.0f), static_cast<int>(cy - 3.0f), 2.5f, iconColor);
            break;
        }
        case 5: { // Layers stack
            DrawLine(static_cast<int>(cx - 8.0f), static_cast<int>(cy - 4.0f), static_cast<int>(cx), static_cast<int>(cy - 8.0f), iconColor);
            DrawLine(static_cast<int>(cx), static_cast<int>(cy - 8.0f), static_cast<int>(cx + 8.0f), static_cast<int>(cy - 4.0f), iconColor);
            DrawLine(static_cast<int>(cx + 8.0f), static_cast<int>(cy - 4.0f), static_cast<int>(cx), static_cast<int>(cy), iconColor);
            DrawLine(static_cast<int>(cx), static_cast<int>(cy), static_cast<int>(cx - 8.0f), static_cast<int>(cy - 4.0f), iconColor);

            DrawLine(static_cast<int>(cx - 8.0f), static_cast<int>(cy + 2.0f), static_cast<int>(cx), static_cast<int>(cy + 6.0f), iconColor);
            DrawLine(static_cast<int>(cx), static_cast<int>(cy + 6.0f), static_cast<int>(cx + 8.0f), static_cast<int>(cy + 2.0f), iconColor);
            break;
        }
        case 6: { // Data / Objects cylinders
            DrawRectangleRoundedLines({cx - 8.0f, cy - 7.0f, 16.0f, 14.0f}, 0.3f, 4, iconColor);
            DrawLine(static_cast<int>(cx - 8.0f), static_cast<int>(cy - 2.0f), static_cast<int>(cx + 8.0f), static_cast<int>(cy - 2.0f), iconColor);
            DrawLine(static_cast<int>(cx - 8.0f), static_cast<int>(cy + 3.0f), static_cast<int>(cx + 8.0f), static_cast<int>(cy + 3.0f), iconColor);
            break;
        }
        }
    }
}

void HUD::selectedInfo(const Simulation& simulation) const {
    // Hide selection panel when in G (gravity) mode - gravity overlay shows the info
    if (simulation.gravityMode) return;
    if (simulation.selected < 0 || simulation.selected >= static_cast<int>(simulation.bodies.size())) return;
    const Body& body = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    const CelestialBodyInfo info = lookupCelestialInfo(body.id, body.type, body.luminous);
    const auto& t = theme();
    const ui::HudLayout layout = ui::buildHudLayout(static_cast<float>(GetScreenWidth()),
                                               static_cast<float>(GetScreenHeight()),
                                               true, simulation.soloStudy || simulation.earthMoonFocus);
    const Rectangle box = layout.inspector;
    ui::drawGlassPanel(box, true, 0.08f);

    DrawCircle(static_cast<int>(box.x + 18.0f), static_cast<int>(box.y + 20.0f), 4.5f, t.accentStrong);
    text(body.name.c_str(), box.x + 30.0f, box.y + 11.0f, 16.0f, t.text);

    text(classificationLabel(info.classification), box.x + 30.0f, box.y + 30.0f, 10.0f, t.textDim);

    const float thumbCx = box.x + 48.0f;
    const float thumbCy = box.y + 88.0f;
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

    const float valX = box.x + 92.0f;
    const float offset = 120.0f;
    const double displayMass = body.mass > 0.0 ? body.mass : body.baseMass;
    ui::drawLabelValue("Mass", (scientific(displayMass) + " kg").c_str(), valX, box.y + 54.0f, offset);
    ui::drawLabelValue("Radius", distance(body.realRadius).c_str(), valX, box.y + 74.0f, offset);

    if (info.classification == BodyClassification::Star) {
        ui::drawLabelValue("Role", info.role ? info.role : "Central star", valX, box.y + 94.0f, offset);
    } else if (info.classification == BodyClassification::NaturalSatellite) {
        ui::drawLabelValue("Parent", info.parentBodyId ? info.parentBodyId : body.parentId.c_str(),
                           valX, box.y + 94.0f, offset);
    } else if (info.orderFromSun > 0) {
        ui::drawLabelValue("Order", (std::string(ordinalFromSun(info.orderFromSun)) + " from Sun").c_str(),
                           valX, box.y + 94.0f, offset);
    } else {
        ui::drawLabelValue("Position", distance(simulation.distanceFromSun(body)).c_str(),
                           valX, box.y + 94.0f, offset);
    }

    ui::drawLabelValue("Velocity", (format(length(body.velocity) / 1000.0, 2) + " km/s").c_str(),
                       box.x + 18.0f, box.y + 124.0f, 140.0f);

    if (info.classification == BodyClassification::Star) {
        ui::drawLabelValue("Spectral", info.spectralClass ? info.spectralClass : "—",
                           box.x + 18.0f, box.y + 144.0f, 140.0f);
        ui::drawLabelValue("Energy", info.energySource ? info.energySource : "Fusion",
                           box.x + 18.0f, box.y + 164.0f, 140.0f);
        ui::drawLabelValue("Surface g", (format(simulation.surfaceGravity(body), 2) + " m/s²").c_str(),
                           box.x + 18.0f, box.y + 184.0f, 140.0f);
    } else {
        const double periodDays = body.orbitalPeriod > 0.0 ? (body.orbitalPeriod / PhysicsEngine::DAY) : 0.0;
        ui::drawLabelValue("Orbital Period", (periodDays > 0.0 ? format(periodDays, 2) + " days" : "—").c_str(),
                           box.x + 18.0f, box.y + 144.0f, 140.0f);
        ui::drawLabelValue("Rotation", dayLengthLabel(body.rotationPeriod).c_str(),
                           box.x + 18.0f, box.y + 164.0f, 140.0f);
        ui::drawLabelValue("Surface g", (format(simulation.surfaceGravity(body), 2) + " m/s²").c_str(),
                           box.x + 18.0f, box.y + 184.0f, 140.0f);
    }

    DrawLine(static_cast<int>(box.x + 16.0f), static_cast<int>(box.y + 208.0f),
             static_cast<int>(box.x + box.width - 16.0f), static_cast<int>(box.y + 208.0f),
             withAlpha(t.border, 0.7f));

    if (info.naturalSatelliteCount > 0 && info.classification != BodyClassification::NaturalSatellite) {
        text("NATURAL SATELLITES", box.x + 18.0f, box.y + 218.0f, 11.0f, t.accent);
        ui::drawLabelValue("Count", std::to_string(info.naturalSatelliteCount).c_str(),
                           box.x + 18.0f, box.y + 238.0f, 140.0f);
        if (info.largestPlanet) {
            text("Largest planet in the Solar System", box.x + 18.0f, box.y + 258.0f, 11.0f, t.textMuted);
        }
    } else {
        text("EDUCATIONAL NOTE", box.x + 18.0f, box.y + 218.0f, 11.0f, t.accent);
        const std::string note = info.description ? info.description : simulation.selectedBodyLesson();
        drawWrapped(note, box.x + 18.0f, box.y + 238.0f, box.width - 36.0f, 11, t.textMuted, 3);
    }

    if (simulation.soloStudy || simulation.earthMoonFocus) {
        const char* modeNote = simulation.earthMoonFocus
            ? "Earth–Moon Focus · Press Q to return"
            : "Study Mode · Press Q to return";
        text(modeNote, box.x + 18.0f, box.y + box.height - 28.0f, 11.0f, t.accentStrong);
    }
}

void HUD::classificationOverlay(const Simulation& simulation) const {
    // Classification details have been moved into selectedInfo panel
    // This panel is now hidden to consolidate UI
    return;
    const auto& t = theme();
    const float screenW = static_cast<float>(GetScreenWidth());
    const Rectangle box = {screenW - 300.0f - 16.0f, ui::topBarHeight() + 14.0f, 300.0f, 280.0f};
    ui::drawGlassPanel(box, true, 0.1f);

    text("NAMES & CLASSIFICATION", box.x + 14.0f, box.y + 12.0f, 11.0f, t.accent);
    text("Ctrl+N to close", box.x + 14.0f, box.y + 28.0f, 10.0f, t.textDim);

    if (simulation.selected < 0 || simulation.selected >= static_cast<int>(simulation.bodies.size())) {
        text("Select a body to inspect its", box.x + 14.0f, box.y + 60.0f, 12.0f, t.textMuted);
        text("astronomical classification.", box.x + 14.0f, box.y + 78.0f, 12.0f, t.textMuted);
        return;
    }

    const Body& body = simulation.bodies[static_cast<std::size_t>(simulation.selected)];
    const CelestialBodyInfo info = lookupCelestialInfo(body.id, body.type, body.luminous);

    std::string title = body.name;
    for (char& c : title) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    text(title.c_str(), box.x + 14.0f, box.y + 52.0f, 18.0f, t.text);

    float y = box.y + 82.0f;
    ui::drawLabelValue("Type", classificationLabel(info.classification), box.x + 14.0f, y, 140.0f);
    y += 22.0f;
    if (info.orderFromSun > 0) {
        ui::drawLabelValue("Order from Sun", ordinalFromSun(info.orderFromSun), box.x + 14.0f, y, 140.0f);
        y += 22.0f;
    }
    if (info.classification == BodyClassification::NaturalSatellite) {
        ui::drawLabelValue("Parent Body", info.parentBodyId ? info.parentBodyId : "—", box.x + 14.0f, y, 140.0f);
        y += 22.0f;
    }
    if (info.naturalSatelliteCount > 0 && info.classification != BodyClassification::NaturalSatellite) {
        ui::drawLabelValue("Natural Satellites", std::to_string(info.naturalSatelliteCount).c_str(),
                           box.x + 14.0f, y, 140.0f);
        y += 22.0f;
    }
    if (info.largestPlanet) {
        ui::drawLabelValue("Largest Planet", "Yes", box.x + 14.0f, y, 140.0f);
        y += 22.0f;
    }
    if (info.spectralClass) {
        ui::drawLabelValue("Spectral class", info.spectralClass, box.x + 14.0f, y, 140.0f);
        y += 22.0f;
    }
    if (info.role) {
        ui::drawLabelValue("Role", info.role, box.x + 14.0f, y, 140.0f);
        y += 22.0f;
    }
    ui::drawLabelValue("System", "Solar System", box.x + 14.0f, y, 140.0f);
}

void HUD::forcesMotionOverlay(const Simulation& simulation) const {
    if (!simulation.showVectors) return;
    const auto& t = theme();
    const float screenW = static_cast<float>(GetScreenWidth());
    const Rectangle box = {screenW - 320.0f - 16.0f, ui::topBarHeight() + 14.0f, 320.0f, 340.0f};
    // Shift down if classification is also open
    const float y0 = simulation.showClassification ? box.y + 290.0f : box.y;
    const Rectangle panel = {box.x, y0, box.width, 300.0f};
    ui::drawGlassPanel(panel, true, 0.1f);

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
    const Rectangle panel = {ui::pagePad() + 58.0f, screenH - 380.0f - 14.0f, 350.0f, 380.0f};
    ui::drawGlassPanel(panel, true, 0.1f);

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
    ui::drawGlassPanel(panel, true, 0.08f);

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
    const char* label = visualScaleModeLabel(simulation.visualScaleMode);
    const float screenW = static_cast<float>(GetScreenWidth());
    const float textW = static_cast<float>(MeasureText(label, 11));
    const Rectangle badge = {screenW * 0.5f - (textW + 28.0f) * 0.5f,
                             static_cast<float>(GetScreenHeight()) - 58.0f,
                             textW + 28.0f, 22.0f};
    DrawRectangleRounded(badge, 0.4f, 6, withAlpha(t.surfaceAlt, 0.85f));
    DrawRectangleRoundedLines(badge, 0.4f, 6, withAlpha(t.borderSoft, 0.9f));
    DrawText(label, static_cast<int>(badge.x + 14.0f), static_cast<int>(badge.y + 5.0f), 11,
             simulation.visualScaleMode == VisualScaleMode::Realistic ? t.warn : t.textDim);
}

void HUD::viewControls(const Simulation& simulation) const {
    const auto& t = theme();
    const ui::HudLayout layout = ui::buildHudLayout(static_cast<float>(GetScreenWidth()),
                                               static_cast<float>(GetScreenHeight()),
                                               simulation.selected >= 0, simulation.soloStudy);
    const Rectangle box = layout.viewControls;
    ui::drawGlassPanel(box, false, 0.08f);

    // Header
    text("VIEW CONTROLS", box.x + 14.0f, box.y + 8.0f, 10.0f, t.textDim);
    text("⌃", box.x + box.width - 20.0f, box.y + 8.0f, 11.0f, t.textDim);

    // 3 Buttons: Solar View (F1), Earth Orbit (F2), Empty Space (F3)
    const Vector2 mouse = GetMousePosition();
    const float btnW = (box.width - 28.0f) / 3.0f;
    const float btnH = 46.0f;
    const float btnY = box.y + 24.0f;

    struct ViewBtn { const char* label; const char* badge; const char* scenario; };
    constexpr ViewBtn kViews[] = {
        {"Solar View", "F1", "default_solar_system"},
        {"Earth–Moon", "F2", "earth_orbit"},
        {"Empty Space", "F3", "empty_space"},
    };

    for (int i = 0; i < 3; ++i) {
        const Rectangle btnRect = {box.x + 10.0f + i * (btnW + 4.0f), btnY, btnW, btnH};
        const bool isCur = (i == 1)
            ? simulation.earthMoonFocus
            : (simulation.scenarioId == kViews[i].scenario && !simulation.earthMoonFocus);
        const bool hover = CheckCollisionPointRec(mouse, btnRect);

        DrawRectangleRounded(btnRect, 0.2f, 8, isCur ? withAlpha(t.accent, 0.2f) : withAlpha(t.surfaceAlt, 0.65f));
        DrawRectangleRoundedLines(btnRect, 0.2f, 8, isCur ? t.accent : withAlpha(t.borderSoft, 0.8f));

        // Icon
        const float cx = btnRect.x + btnRect.width * 0.5f;
        if (i == 0) { // Solar icon
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(btnRect.y + 12.0f), 5.0f, isCur ? t.accentStrong : t.textMuted);
            DrawCircle(static_cast<int>(cx), static_cast<int>(btnRect.y + 12.0f), 2.0f, t.warn);
        } else if (i == 1) { // Earth orbit icon
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(btnRect.y + 12.0f), 6.0f, isCur ? t.accentStrong : t.textMuted);
            DrawCircle(static_cast<int>(cx + 4.0f), static_cast<int>(btnRect.y + 10.0f), 2.0f, t.accent);
        } else { // Empty space icon
            DrawCircleLines(static_cast<int>(cx), static_cast<int>(btnRect.y + 12.0f), 5.0f, isCur ? t.accentStrong : t.textMuted);
        }

        const int fontSize = 10;
        const float textW = static_cast<float>(MeasureText(kViews[i].label, fontSize));
        DrawText(kViews[i].label, static_cast<int>(cx - textW * 0.5f), static_cast<int>(btnRect.y + 22.0f), fontSize,
                 isCur ? t.text : (hover ? t.accentStrong : t.textMuted));
        const float badgeW = static_cast<float>(MeasureText(kViews[i].badge, 9));
        DrawText(kViews[i].badge, static_cast<int>(cx - badgeW * 0.5f), static_cast<int>(btnRect.y + 34.0f), 9, t.textDim);
    }
}

void HUD::visualizationControls(const Simulation& simulation) const {
    const auto& t = theme();
    const ui::HudLayout layout = ui::buildHudLayout(static_cast<float>(GetScreenWidth()),
                                               static_cast<float>(GetScreenHeight()),
                                               simulation.selected >= 0, simulation.soloStudy);
    const Rectangle box = layout.visualizationToggles;
    ui::drawGlassPanel(box, false, 0.45f);

    struct ToggleItem { const char* label; bool active; };
    const ToggleItem items[] = {
        {"Orbits", simulation.showOrbits},
        {"Trails", simulation.showTrails},
        {"Forces", simulation.showVectors},
        {"Gravity", simulation.gravityMode},
    };

    const float itemW = box.width / 4.0f;
    for (int i = 0; i < 4; ++i) {
        const float itemX = box.x + i * itemW;
        text(items[i].label, itemX + 8.0f, box.y + 11.0f, 11.0f, items[i].active ? t.text : t.textMuted);

        // Rounded pill toggle switch
        const float pillX = itemX + itemW - 28.0f;
        const float pillY = box.y + 10.0f;
        const Rectangle pillRect = {pillX, pillY, 24.0f, 14.0f};

        DrawRectangleRounded(pillRect, 0.5f, 8, items[i].active ? withAlpha(t.accent, 0.5f) : withAlpha(t.surfaceAlt, 0.9f));
        DrawRectangleRoundedLines(pillRect, 0.5f, 8, items[i].active ? t.accent : withAlpha(t.borderSoft, 0.8f));

        const float knobX = items[i].active ? (pillX + 17.0f) : (pillX + 7.0f);
        const float knobY = pillY + 7.0f;
        DrawCircle(static_cast<int>(knobX), static_cast<int>(knobY), 4.5f, items[i].active ? t.text : t.textDim);
        if (items[i].active) {
            DrawCircleGradient({knobX, knobY}, 7.0f, withAlpha(t.accentStrong, 0.4f), withAlpha(t.accentStrong, 0.0f));
        }
    }
}

void HUD::orientationIndicator(float cameraYaw) const {
    const auto& t = theme();
    const ui::HudLayout layout = ui::buildHudLayout(static_cast<float>(GetScreenWidth()),
                                               static_cast<float>(GetScreenHeight()),
                                               false, false);
    const float cx = layout.orientationCompass.x + layout.orientationCompass.width * 0.5f;
    const float cy = layout.orientationCompass.y + layout.orientationCompass.height * 0.5f;
    const float r = 24.0f;

    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r, withAlpha(t.accent, 0.55f));
    DrawCircleLines(static_cast<int>(cx), static_cast<int>(cy), r * 0.5f, withAlpha(t.borderSoft, 0.5f));

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

void HUD::draw(const Simulation& simulation, float cameraYaw) const {
    switch (simulation.screen) {
    case AppScreen::Simulation:
        topBar(simulation);
        toolRail(simulation);
        viewControls(simulation);
        selectedInfo(simulation);
        visualizationControls(simulation);
        orientationIndicator(cameraYaw);
        scaleModeBadge(simulation);
        classificationOverlay(simulation);
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
    pageChrome(simulation, "Learn", "Guided orbital-mechanics lessons, experiments, and challenges");
    const LearnerReport report = buildLearnerReport(simulation.educationProgress);
    const AuthoredLesson& lesson = authoredLessonAt(simulation.lesson);

    const Rectangle left = {ui::pagePad(), ui::contentTop() + 70.0f, 340.0f, 560.0f};
    const Rectangle mid = {ui::pagePad() + 360.0f, ui::contentTop() + 70.0f, 420.0f, 560.0f};
    const Rectangle right = {ui::pagePad() + 800.0f, ui::contentTop() + 70.0f, 340.0f, 560.0f};
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
         ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::learningLabScreenPanel(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Learning Lab", "Ghana curriculum activities on the shared simulation engine");

    if (!simulation.curriculumReady) {
        const Rectangle box = {ui::pagePad(), ui::contentTop() + 70.0f, 900.0f, 140.0f};
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
         ui::pagePad(), ui::contentTop() + 58.0f, 14, t.textMuted);

    const auto activities = simulation.activitiesForLearnerGrade();
    const Rectangle left = {ui::pagePad(), ui::contentTop() + 90.0f, 360.0f, 520.0f};
    const Rectangle right = {ui::pagePad() + 380.0f, ui::contentTop() + 90.0f, 760.0f, 520.0f};
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
         ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::scenarioScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Scenes", "Choose a starting system for the laboratory");

    const struct Scenario { const char* id; const char* title; const char* description; const char* bodies; } scenarios[] = {
        {"default_solar_system", "Solar System", "Sun, planets, Moon, and BAGSOLAR-1.", "11 bodies"},
        {"earth_orbit", "Earth Orbit", "Focused Sun–Earth–Moon study.", "3 bodies"},
        {"empty_space", "Empty Space", "Blank scene for custom systems.", "0 bodies"},
    };

    for (int index = 0; index < 3; ++index) {
        const float y = ui::contentTop() + 80.0f + index * 110.0f;
        const Rectangle card = {ui::pagePad(), y, 720.0f, 92.0f};
        const bool selected = index == simulation.scenarioBrowserSelection;
        ui::drawPanel(card, selected);
        if (selected) DrawRectangleRoundedLines(card, 0.08f, 16, t.accent);
        text(scenarios[index].title, card.x + 24, card.y + 22, 22, selected ? t.text : t.textMuted);
        text(scenarios[index].description, card.x + 24, card.y + 54, 14, t.textMuted);
        text(scenarios[index].bodies, card.x + 560, card.y + 36, 14, t.accentStrong);
    }

    const Rectangle side = {ui::pagePad() + 760.0f, ui::contentTop() + 80.0f, 380.0f, 320.0f};
    ui::drawPanel(side);
    text("Current", side.x + 22, side.y + 22, 12, t.accent);
    text(simulation.scenarioMetadata.name.c_str(), side.x + 22, side.y + 52, 22, t.text);
    DrawTextEx(GetFontDefault(), simulation.scenarioMetadata.description.c_str(),
               {side.x + 22, side.y + 92}, 14, 1, t.textMuted);
    ui::drawLabelValue("Epoch", simulation.scenarioMetadata.epoch.c_str(), side.x + 22, side.y + 170);
    ui::drawLabelValue("Frame", simulation.scenarioMetadata.referenceFrame.c_str(), side.x + 22, side.y + 196);

    text("↑↓ select   Enter load   Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::telemetryScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Telemetry", "Deterministic sampling of scientific state");

    const Rectangle main = {ui::pagePad(), ui::contentTop() + 70.0f, 760.0f, 500.0f};
    const Rectangle side = {ui::pagePad() + 790.0f, ui::contentTop() + 70.0f, 350.0f, 500.0f};
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
         ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::missionScreen(const Simulation& simulation) const {
    (void)simulation;
    const auto& t = theme();
    pageChrome(simulation, "Mission Tools", "Analytical APIs for maneuvers and trajectory studies");

    const Rectangle main = {ui::pagePad(), ui::contentTop() + 70.0f, 820.0f, 420.0f};
    const Rectangle side = {ui::pagePad() + 850.0f, ui::contentTop() + 70.0f, 290.0f, 420.0f};
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

    text("Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::settingsScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Settings", "Live simulation and presentation controls");

    const Rectangle left = {ui::pagePad(), ui::contentTop() + 70.0f, 560.0f, 460.0f};
    const Rectangle right = {ui::pagePad() + 590.0f, ui::contentTop() + 70.0f, 550.0f, 460.0f};
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

    text("Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

void HUD::helpScreen(const Simulation& simulation) const {
    const auto& t = theme();
    pageChrome(simulation, "Help", "Controls that exist in the current build");

    const Rectangle left = {ui::pagePad(), ui::contentTop() + 70.0f, 560.0f, 520.0f};
    const Rectangle right = {ui::pagePad() + 590.0f, ui::contentTop() + 70.0f, 550.0f, 520.0f};
    ui::drawPanel(left);
    ui::drawPanel(right);

    text("Navigate", left.x + 24, left.y + 22, 12, t.accent);
    text("Tab cycle screens", left.x + 24, left.y + 56, 15, t.text);
    text("L Learn   F9 Lab   F5 Scenes", left.x + 24, left.y + 86, 15, t.text);
    text("F6 Telemetry   F7 Mission   F8 Settings", left.x + 24, left.y + 116, 15, t.text);
    text("F1–F3 load scenes   H Help   Esc → Sim", left.x + 24, left.y + 146, 15, t.text);

    text("Simulate", left.x + 24, left.y + 200, 12, t.accent);
    text("Space pause   ± speed   R reset", left.x + 24, left.y + 234, 15, t.text);
    text("V Forces & Motion   G Gravity mass mode", left.x + 24, left.y + 264, 14, t.text);
    text("F2 Earth–Moon   Ctrl+N Names   / scale", left.x + 24, left.y + 294, 14, t.text);
    text("1–6 speed   Ctrl+G grid   P probe", left.x + 24, left.y + 324, 14, t.textMuted);

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

    text("Esc back", ui::pagePad(), static_cast<float>(GetScreenHeight()) - 28.0f, 13, t.textDim);
}

} // namespace bag
