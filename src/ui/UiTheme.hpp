#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <string>

#include <raylib.h>

#include "../simulation/Simulation.hpp"

namespace bag {
namespace ui {

inline Color rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a = 255) {
    return {r, g, b, a};
}

inline Color withAlpha(Color color, float amount) {
    color.a = static_cast<unsigned char>(amount <= 0.0f ? 0 : (amount >= 1.0f ? 255 : amount * 255.0f));
    return color;
}

// Scientific display palette: dark deep-space, restrained electric cyan, subtle glass panels.
struct Theme {
    Color bg = rgba(5, 8, 17);
    Color bgElevated = rgba(9, 15, 27);
    Color surface = rgba(10, 17, 30);
    Color surfaceAlt = rgba(16, 26, 44);
    Color panelGlass = rgba(8, 14, 25, 218);
    Color panelGlassElevated = rgba(12, 20, 36, 230);
    Color border = rgba(36, 58, 86);
    Color borderSoft = rgba(26, 42, 64);
    Color borderGlow = rgba(40, 184, 238, 75);
    Color text = rgba(235, 242, 250);
    Color textMuted = rgba(138, 158, 182);
    Color textDim = rgba(90, 110, 132);
    Color accent = rgba(40, 184, 238);
    Color accentSoft = rgba(40, 184, 238, 40);
    Color accentStrong = rgba(116, 221, 255);
    Color warn = rgba(251, 191, 36);
    Color danger = rgba(248, 113, 113);
    Color success = rgba(52, 211, 153);
    Color overlay = rgba(5, 8, 17, 235);
};

inline const Theme& theme() {
    static const Theme value;
    return value;
}

inline float topBarHeight() { return 54.0f; }
inline float bottomBarHeight() { return 28.0f; }
inline float contentTop() { return topBarHeight() + 18.0f; }
inline float pagePad() { return 28.0f; }

struct NavTab {
    const char* label;
    AppScreen screen;
};

inline constexpr std::array<NavTab, 7> kNavTabs{{
    {"SIMULATION", AppScreen::Simulation},
    {"EDUCATION", AppScreen::Education},
    {"SCENARIOS", AppScreen::ScenarioBrowser},
    {"TELEMETRY", AppScreen::Telemetry},
    {"MISSION", AppScreen::MissionDesigner},
    {"SETTINGS", AppScreen::Settings},
    {"HELP", AppScreen::Help},
}};

struct NavLayout {
    std::array<Rectangle, 7> hits{};
    float brandWidth = 240.0f;
};

inline NavLayout buildNavLayout(float screenWidth) {
    NavLayout layout;
    const float startX = 260.0f;
    const float y = 8.0f;
    const float height = 38.0f;
    const float gap = 4.0f;
    float x = startX;
    const float maxX = screenWidth - 360.0f;
    for (std::size_t index = 0; index < kNavTabs.size(); ++index) {
        const float textWidth = static_cast<float>(MeasureText(kNavTabs[index].label, 12));
        float width = textWidth + 24.0f;
        if (x + width > maxX && maxX > x) width = std::max(48.0f, maxX - x);
        layout.hits[index] = {x, y, width, height};
        x += width + gap;
    }
    return layout;
}

// Glass panel drawing with thin border and subtle top specular edge.
inline void drawGlassPanel(Rectangle box, bool highlighted = false, float roundness = 0.08f) {
    const Theme& t = theme();
    // Glass background fill
    DrawRectangleRounded(box, roundness, 16, highlighted ? t.panelGlassElevated : t.panelGlass);
    // Subtle vertical gradient highlight at the top of the panel
    DrawRectangleGradientV(static_cast<int>(box.x + 1), static_cast<int>(box.y + 1),
                           static_cast<int>(box.width - 2), static_cast<int>(std::min(box.height * 0.35f, 40.0f)),
                           withAlpha(t.accent, highlighted ? 0.08f : 0.035f),
                           withAlpha(t.accent, 0.0f));
    // Precise thin border
    DrawRectangleRoundedLines(box, roundness, 16, highlighted ? t.borderGlow : withAlpha(t.border, 0.85f));
}

inline void drawPanel(Rectangle box, bool elevated = false) {
    drawGlassPanel(box, elevated);
}

inline void drawChip(Rectangle box, const char* label, bool active, Color accent = theme().accent) {
    const Theme& t = theme();
    const Color fill = active ? withAlpha(accent, 0.22f) : withAlpha(t.surfaceAlt, 0.85f);
    const Color stroke = active ? withAlpha(accent, 0.9f) : withAlpha(t.borderSoft, 0.85f);
    DrawRectangleRounded(box, 0.45f, 12, fill);
    DrawRectangleRoundedLines(box, 0.45f, 12, stroke);
    const int size = 12;
    const float textWidth = static_cast<float>(MeasureText(label, size));
    DrawText(label, static_cast<int>(box.x + (box.width - textWidth) * 0.5f),
             static_cast<int>(box.y + (box.height - size) * 0.5f), size,
             active ? t.text : t.textMuted);
}

inline void drawLabelValue(const char* label, const char* value, float x, float y, float valOffset = 118.0f) {
    const Theme& t = theme();
    DrawText(label, static_cast<int>(x), static_cast<int>(y), 12, t.textMuted);
    DrawText(value, static_cast<int>(x + valOffset), static_cast<int>(y), 12, t.text);
}

inline std::string shorten(const std::string& value, std::size_t maxChars) {
    if (value.size() <= maxChars) return value;
    return value.substr(0, maxChars - 1) + "…";
}

// Layout metrics for simulation screen HUD
struct HudLayout {
    Rectangle toolRail{};
    std::array<Rectangle, 7> toolButtons{};
    Rectangle inspector{};
    Rectangle objectList{};
    Rectangle viewControls{};
    Rectangle visualizationToggles{};
    Rectangle orientationCompass{};
};

inline HudLayout buildHudLayout(float screenWidth, float screenHeight, bool hasSelection, bool soloStudy) {
    HudLayout layout;

    // Tool rail: narrow vertical rail at left
    const float railX = 14.0f;
    const float railY = topBarHeight() + 14.0f;
    const float railW = 44.0f;
    const float btnH = 40.0f;
    const float btnGap = 4.0f;
    layout.toolRail = {railX, railY, railW, 7 * (btnH + btnGap)};
    for (int i = 0; i < 7; ++i) {
        layout.toolButtons[static_cast<std::size_t>(i)] = {
            railX + 2.0f,
            railY + 2.0f + i * (btnH + btnGap),
            railW - 4.0f,
            btnH
        };
    }

    // Solar system object list: top right
    const float objW = 236.0f;
    const float objH = 312.0f;
    const float objX = screenWidth - objW - 16.0f;
    const float objY = topBarHeight() + 14.0f;
    layout.objectList = {objX, objY, objW, objH};

    // View controls: bottom left
    const float vcW = 280.0f;
    const float vcH = 80.0f;
    const float vcX = railX + railW + 14.0f;
    const float vcY = screenHeight - vcH - 16.0f;
    layout.viewControls = {vcX, vcY, vcW, vcH};

    // Selected body inspector: above view controls on left
    if (hasSelection) {
        const float inspW = 320.0f;
        const float inspH = soloStudy ? 380.0f : 310.0f;
        const float inspX = railX + railW + 14.0f;
        const float inspY = vcY - inspH - 10.0f;
        layout.inspector = {inspX, inspY, inspW, inspH};
    }

    // Visualization toggles: bottom right
    const float togW = 296.0f;
    const float togH = 36.0f;
    const float togX = screenWidth - 92.0f - togW - 14.0f;
    const float togY = screenHeight - togH - 18.0f;
    layout.visualizationToggles = {togX, togY, togW, togH};

    // Orientation compass: bottom right corner
    layout.orientationCompass = {screenWidth - 78.0f, screenHeight - 78.0f, 64.0f, 64.0f};

    return layout;
}

inline bool isPointInAnyPanel(Vector2 point, float screenWidth, float screenHeight, const Simulation& simulation) {
    if (point.y <= topBarHeight()) return true;
    if (simulation.screen != AppScreen::Simulation) return false;

    const bool hasSelection = simulation.selected >= 0 &&
                              simulation.selected < static_cast<int>(simulation.bodies.size());
    const HudLayout layout = buildHudLayout(screenWidth, screenHeight, hasSelection, simulation.soloStudy);

    if (CheckCollisionPointRec(point, layout.toolRail)) return true;
    if (simulation.showClassification && CheckCollisionPointRec(point, layout.objectList)) return true;
    if (CheckCollisionPointRec(point, layout.viewControls)) return true;
    if (hasSelection && CheckCollisionPointRec(point, layout.inspector)) return true;
    if (CheckCollisionPointRec(point, layout.visualizationToggles)) return true;
    if (CheckCollisionPointRec(point, layout.orientationCompass)) return true;

    return false;
}

} // namespace ui
} // namespace bag
