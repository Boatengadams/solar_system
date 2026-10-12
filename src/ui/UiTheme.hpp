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

// Shared scientific desktop palette: deep navy surfaces, neutral glass,
// restrained system blue, and cool muted text for readable measurements.
struct Theme {
    Color bg = rgba(5, 8, 17);
    Color bgElevated = rgba(12, 15, 21);
    Color surface = rgba(20, 23, 30);
    Color surfaceAlt = rgba(31, 35, 44);
    Color panelGlass = rgba(20, 23, 30, 236);
    Color panelGlassElevated = rgba(31, 35, 44, 244);
    Color border = rgba(88, 96, 112);
    Color borderSoft = rgba(62, 70, 84);
    Color borderGlow = rgba(10, 132, 255, 110);
    Color text = rgba(245, 245, 247);
    Color textMuted = rgba(152, 160, 172);
    Color textDim = rgba(112, 122, 138);
    Color accent = rgba(10, 132, 255);
    Color accentSoft = rgba(10, 132, 255, 38);
    Color accentStrong = rgba(92, 176, 255);
    Color warn = rgba(251, 191, 36);
    Color danger = rgba(248, 113, 113);
    Color success = rgba(52, 211, 153);
    Color overlay = rgba(5, 8, 17, 235);
};

inline const Theme& theme() {
    static const Theme value;
    return value;
}

inline float topBarHeight() { return 62.0f; }
inline float bottomBarHeight() { return 28.0f; }
inline float contentTop() { return topBarHeight() + 18.0f; }
inline float pagePad() { return 28.0f; }
inline float appEdge() { return 18.0f; }
inline float railTop() { return 76.0f; }

// Screen tab metadata now lives in UiLayout.hpp, together with the rest of the
// shell layout. See ChromeLayout for the single source of truth.

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

} // namespace ui
} // namespace bag
