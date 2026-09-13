#include "HUD.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "../education/EducationContent.hpp"
#include "../education/EducationChallenges.hpp"
#include "../physics/PhysicsEngine.hpp"

namespace bag {
namespace {

Color alpha(Color color, float amount) {
    color.a = static_cast<unsigned char>(std::clamp(amount * 255.0f, 0.0f, 255.0f));
    return color;
}

std::string format(double value, int digits = 2) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(digits) << value;
    return output.str();
}

std::string scientific(double value, int digits = 3) {
    std::ostringstream output;
    output << std::scientific << std::setprecision(digits) << value;
    return output.str();
}

std::string distance(double meters) {
    const double au = std::abs(meters) / PhysicsEngine::AU;
    if (au >= 0.01) return format(meters / PhysicsEngine::AU, 3) + " AU";
    if (std::abs(meters) >= 1e9) return format(meters / 1e9, 2) + " billion km";
    return format(meters / 1000.0, 0) + " km";
}

std::string time(double seconds) {
    const double absolute = std::abs(seconds);
    if (absolute < 60) return format(absolute, 1) + " s";
    if (absolute < PhysicsEngine::DAY) return format(absolute / 3600.0, 1) + " h";
    if (absolute < PhysicsEngine::YEAR) return format(absolute / PhysicsEngine::DAY, 1) + " days";
    return format(absolute / PhysicsEngine::YEAR, 2) + " years";
}

} // namespace

Rectangle HUD::panel(float x, float y, float width, float height) const {
    DrawRectangleRounded({x, y, width, height}, 0.12f, 12, alpha({10, 18, 32, 255}, 0.94f));
    DrawRectangleRoundedLines({x, y, width, height}, 0.12f, 12, alpha({90, 160, 220, 255}, 0.28f));
    return {x, y, width, height};
}

void HUD::text(const char* value, float x, float y, float size, Color colorValue) const {
    DrawText(value, static_cast<int>(x), static_cast<int>(y), static_cast<int>(size), colorValue);
}

void HUD::line(const char* label, const char* value, float x, float y) const {
    text(label, x, y, 13, alpha({165, 190, 215, 255}, 0.85f));
    text(value, x + 112, y, 13);
}

void HUD::top(const Simulation& simulation) const {
    DrawRectangle(0, 0, GetScreenWidth(), 58, alpha({4, 10, 20, 255}, 0.96f));
    text("BAGSOLAR", 22, 10, 26, {110, 220, 255, 255});
    text("ASTRONOMICAL LABORATORY", 154, 16, 13, alpha(RAYWHITE, 0.65f));
    text(simulation.paused ? "● PAUSED" : "● LIVE", GetScreenWidth() - 160, 17, 13,
         simulation.paused ? RED : Color{80, 235, 150, 255});
}

void HUD::selectedInfo(const Simulation& simulation) const {
    if (simulation.selected < 0 || simulation.selected >= static_cast<int>(simulation.bodies.size())) return;
    const Body& body = simulation.bodies[simulation.selected];
    const Rectangle box = panel(GetScreenWidth() - 340, 76, 320, 330);
    text(body.name.c_str(), box.x + 18, box.y + 16, 23, {body.accent.r, body.accent.g, body.accent.b, body.accent.a});
    text(body.type.c_str(), box.x + 18, box.y + 45, 12, alpha(RAYWHITE, 0.55f));
    const std::string mass = scientific(body.mass) + " kg";
    const std::string radius = distance(body.realRadius);
    const std::string bodyDistance = distance(simulation.distanceFromSun(body));
    const std::string velocity = format(length(body.velocity) / 1000.0, 2) + " km/s";
    const std::string escape = format(simulation.escapeVelocity(body) / 1000.0, 2) + " km/s";
    const std::string gravity = format(simulation.surfaceGravity(body), 2) + " m/s²";
    const std::string eccentricity = format(body.eccentricity, 4);
    const std::string energy = scientific(simulation.specificEnergy(body)) + " J/kg";
    const double kineticEnergy = 0.5 * length(body.velocity) * length(body.velocity);
    const double potentialEnergy = -PhysicsEngine::G * PhysicsEngine::SOLAR_MASS /
                                   std::max(simulation.distanceFromSun(body), 1.0);
    line("Mass", mass.c_str(), box.x + 18, box.y + 75);
    line("Radius", radius.c_str(), box.x + 18, box.y + 98);
    line("Distance", bodyDistance.c_str(), box.x + 18, box.y + 121);
    line("Velocity", velocity.c_str(), box.x + 18, box.y + 144);
    line("Escape", escape.c_str(), box.x + 18, box.y + 167);
    line("Gravity", gravity.c_str(), box.x + 18, box.y + 190);
    line("Eccentricity", eccentricity.c_str(), box.x + 18, box.y + 213);
    line("Energy", energy.c_str(), box.x + 18, box.y + 236);
    text("ORBIT STATE", box.x + 18, box.y + 267, 11, alpha({160, 190, 220, 255}, 0.8f));
    text(simulation.specificEnergy(body) < 0 ? "BOUND ORBIT" : "ESCAPE TRAJECTORY", box.x + 18, box.y + 286, 15,
         simulation.specificEnergy(body) < 0 ? Color{80, 235, 150, 255} : ORANGE);
    text(("KE/PE: " + format(kineticEnergy / std::max(std::abs(potentialEnergy), 1.0), 3)).c_str(),
         box.x + 18, box.y + 308, 12, alpha(RAYWHITE, 0.7f));
}

void HUD::lessonPanel(const Simulation& simulation) const {
    if (!simulation.education) return;
    const Rectangle box = panel(20, 76, 360, 250);
    const Lesson& lesson = lessonAt(simulation.lesson);
    text("LEARN", box.x + 18, box.y + 16, 11, {100, 220, 255, 255});
    text(lesson.title, box.x + 18, box.y + 39, 19);
    DrawTextEx(GetFontDefault(), lesson.body, {box.x + 18, box.y + 73}, 15, 2, alpha(RAYWHITE, 0.82f));
    text("A / D  change lesson", box.x + 18, box.y + 208, 11, alpha(RAYWHITE, 0.48f));
}

void HUD::bottom(const Simulation& simulation) const {
    DrawRectangle(0, GetScreenHeight() - 54, GetScreenWidth(), 54, alpha({4, 10, 20, 255}, 0.97f));
    text("SPACE", 20, GetScreenHeight() - 36, 12, alpha(RAYWHITE, 0.65f));
    text(simulation.paused ? "Resume" : "Pause", 75, GetScreenHeight() - 36, 12);
    text("+ / -  Time", 155, GetScreenHeight() - 36, 12);
    text("L Lessons", 270, GetScreenHeight() - 36, 12);
    text("E Experiment", 380, GetScreenHeight() - 36, 12);
    text("V Vectors", 520, GetScreenHeight() - 36, 12);
    text("O Orbits", 620, GetScreenHeight() - 36, 12);
    text("T Trails", 715, GetScreenHeight() - 36, 12);
    text("F Fullscreen", 800, GetScreenHeight() - 36, 12);
    text("Right-drag Pan • Wheel Zoom • Click Select • H Home", 920, GetScreenHeight() - 36, 12, alpha(RAYWHITE, 0.55f));
    text("F1 Solar • F2 Earth • F3 Empty", 20, GetScreenHeight() - 16, 11, alpha({150, 210, 240, 255}, 0.7f));
    text("SIM TIME", GetScreenWidth() - 220, 72, 10, alpha(RAYWHITE, 0.45f));
    text(time(simulation.simTime).c_str(), GetScreenWidth() - 220, 87, 17, {100, 220, 255, 255});
    text(("×" + format(simulation.speed, 1)).c_str(), GetScreenWidth() - 120, 87, 15);
}

void HUD::experimentPanel(const Simulation& simulation) const {
    if (!simulation.education) return;
    const Rectangle box = panel(20, 345, 360, 245);
    const Experiment& experiment = experimentAt(simulation.experiment);
    text("EXPERIMENT", box.x + 18, box.y + 16, 11, {100, 220, 255, 255});
    text(experiment.title, box.x + 18, box.y + 39, 19);
    DrawTextEx(GetFontDefault(), experiment.prompt, {box.x + 18, box.y + 74}, 15, 2, alpha(RAYWHITE, 0.82f));
    text(experiment.equation, box.x + 18, box.y + 137, 21, {150, 225, 255, 255});
    text("↑ / ↓  change experiment", box.x + 18, box.y + 184, 11, alpha(RAYWHITE, 0.48f));
    text("P  launch probe at escape velocity", box.x + 18, box.y + 204, 11, alpha(RAYWHITE, 0.65f));
}

void HUD::challengePanel(const Simulation& simulation) const {
    if (!simulation.education) return;
    const ChallengeDefinition& challenge = challengeAt(simulation.challenge);
    const Rectangle box = panel(400, 76, 420, 250);
    text("CHALLENGE", box.x + 18, box.y + 16, 11, {255, 205, 105, 255});
    text(challenge.title.c_str(), box.x + 18, box.y + 39, 19);
    DrawTextEx(GetFontDefault(), challenge.description.c_str(), {box.x + 18, box.y + 73}, 14, 2, alpha(RAYWHITE, 0.82f));
    text(("Objective: " + challenge.learningObjective).c_str(), box.x + 18, box.y + 125, 11, alpha({190, 215, 235, 255}, 0.82f));
    if (challenge.kind == ChallengeKind::IntegratorComparison) {
        text(("Method: " + std::string(integratorName(simulation.challengeIntegrator))).c_str(), box.x + 18, box.y + 150, 14, {150, 225, 255, 255});
    } else {
        text(("Answer: " + format(simulation.challengeAnswer, 2) + " SI").c_str(), box.x + 18, box.y + 150, 14, {150, 225, 255, 255});
    }
    if (simulation.lastChallengeResult) {
        const ChallengeResult& result = *simulation.lastChallengeResult;
        text((result.grade + "  " + format(result.score, 1) + "/100").c_str(), box.x + 18, box.y + 177, 15,
             result.passed ? Color{80, 235, 150, 255} : ORANGE);
        DrawTextEx(GetFontDefault(), result.feedback.c_str(), {box.x + 18, box.y + 201}, 12, 2, alpha(RAYWHITE, 0.75f));
    } else {
        text("Z / X challenge   [ / ] adjust   I method   C submit", box.x + 18, box.y + 188, 11, alpha(RAYWHITE, 0.58f));
    }
}

void HUD::draw(const Simulation& simulation) const {
    top(simulation);
    lessonPanel(simulation);
    experimentPanel(simulation);
    challengePanel(simulation);
    selectedInfo(simulation);
    bottom(simulation);
}

} // namespace bag
