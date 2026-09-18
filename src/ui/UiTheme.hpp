#pragma once

#include <algorithm>
#include <array>
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

// Modern scientific lab palette: deep ink + soft teal. Avoids neon/gamer chrome.
struct Theme {
    Color bg = rgba(7, 11, 18);
    Color bgElevated = rgba(12, 18, 28);
    Color surface = rgba(16, 24, 36);
    Color surfaceAlt = rgba(22, 32, 48);
    Color border = rgba(42, 58, 78);
    Color borderSoft = rgba(32, 44, 62);
    Color text = rgba(232, 238, 244);
    Color textMuted = rgba(138, 154, 171);
    Color textDim = rgba(96, 112, 130);
    Color accent = rgba(61, 184, 160);
    Color accentSoft = rgba(61, 184, 160, 40);
    Color accentStrong = rgba(90, 210, 188);
    Color warn = rgba(240, 180, 41);
    Color danger = rgba(240, 96, 96);
    Color success = rgba(74, 222, 128);
    Color overlay = rgba(7, 11, 18, 235);
};

inline const Theme& theme() {
    static const Theme value;
    return value;
}

inline float topBarHeight() { return 64.0f; }
inline float bottomBarHeight() { return 44.0f; }
inline float contentTop() { return topBarHeight() + 20.0f; }
inline float pagePad() { return 28.0f; }

struct NavTab {
    const char* label;
    AppScreen screen;
};

inline constexpr std::array<NavTab, 8> kNavTabs{{
    {"Sim", AppScreen::Simulation},
    {"Learn", AppScreen::Education},
    {"Lab", AppScreen::LearningLab},
    {"Scenes", AppScreen::ScenarioBrowser},
    {"Telemetry", AppScreen::Telemetry},
    {"Mission", AppScreen::MissionDesigner},
    {"Settings", AppScreen::Settings},
    {"Help", AppScreen::Help},
}};

struct NavLayout {
    std::array<Rectangle, 8> hits{};
    float brandWidth = 150.0f;
};

inline NavLayout buildNavLayout(float screenWidth) {
    NavLayout layout;
    const float startX = 168.0f;
    const float y = 14.0f;
    const float height = 36.0f;
    const float gap = 6.0f;
    float x = startX;
    const float maxX = screenWidth - 280.0f;
    for (std::size_t index = 0; index < kNavTabs.size(); ++index) {
        const float textWidth = static_cast<float>(MeasureText(kNavTabs[index].label, 16));
        float width = textWidth + 28.0f;
        if (x + width > maxX) width = std::max(56.0f, maxX - x);
        layout.hits[index] = {x, y, width, height};
        x += width + gap;
    }
    return layout;
}

inline void drawPanel(Rectangle box, bool elevated = false) {
    const Theme& t = theme();
    DrawRectangleRounded(box, 0.08f, 16, elevated ? t.surfaceAlt : t.surface);
    DrawRectangleRoundedLines(box, 0.08f, 16, withAlpha(t.border, 0.85f));
}

inline void drawChip(Rectangle box, const char* label, bool active, Color accent = theme().accent) {
    const Theme& t = theme();
    const Color fill = active ? withAlpha(accent, 0.22f) : withAlpha(t.surfaceAlt, 0.9f);
    const Color stroke = active ? withAlpha(accent, 0.9f) : withAlpha(t.borderSoft, 0.9f);
    DrawRectangleRounded(box, 0.45f, 12, fill);
    DrawRectangleRoundedLines(box, 0.45f, 12, stroke);
    const int size = 15;
    const float textWidth = static_cast<float>(MeasureText(label, size));
    DrawText(label, static_cast<int>(box.x + (box.width - textWidth) * 0.5f),
             static_cast<int>(box.y + (box.height - size) * 0.5f), size,
             active ? t.text : t.textMuted);
}

inline void drawLabelValue(const char* label, const char* value, float x, float y) {
    const Theme& t = theme();
    DrawText(label, static_cast<int>(x), static_cast<int>(y), 13, t.textMuted);
    DrawText(value, static_cast<int>(x + 118), static_cast<int>(y), 13, t.text);
}

inline std::string shorten(const std::string& value, std::size_t maxChars) {
    if (value.size() <= maxChars) return value;
    return value.substr(0, maxChars - 1) + "…";
}

} // namespace ui
} // namespace bag
