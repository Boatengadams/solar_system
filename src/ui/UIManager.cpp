#include <algorithm>
#include <cmath>
#include <string>

#include "UIManager.hpp"

#include "raym3/raym3.h"

#include "../rendering/Renderer.hpp"
#include "../ui/UiTheme.hpp"

namespace bag {

void UIManager::initialize() {
    if (initialized_) return;
    raym3::Initialize();
    raym3::SetTheme(true);
    initialized_ = true;
}

void UIManager::shutdown() {
    if (!initialized_) return;
    raym3::Shutdown();
    initialized_ = false;
}

void UIManager::beginFrame() {
    if (initialized_) raym3::BeginFrame();
}

void UIManager::endFrame() {
    if (initialized_) raym3::EndFrame();
}

namespace {

// Shared with the HUD so both sides agree on how a scientific quantity reads.
std::string formatTimeScale(double scale) {
    if (scale >= 1000000.0) return std::to_string(static_cast<long long>(scale / 1000000.0)) + "M\u00d7";
    if (scale >= 1000.0) return std::to_string(static_cast<long long>(scale / 1000.0)) + "K\u00d7";
    if (scale >= 1.0) return std::to_string(static_cast<long long>(scale)) + "\u00d7";
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f\u00d7", scale);
    return buffer;
}

std::string formatSimulationDays(double seconds) {
    constexpr double day = 86400.0;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.2f d", seconds / day);
    return buffer;
}

} // namespace

void UIManager::update(const Simulation& simulation) {
    if (!initialized_) return;

    // Advance the rail animation here and only here. Both the drawing code and
    // the pointer-ownership test read the resulting layout, so a control can
    // never be hit at a width that differs from the width it was drawn at.
    const float target = sidebarOpen_ ? 1.0f : 0.0f;
    const float blend = ui::approachBlend(14.0f, GetFrameTime());
    sidebarProgress_ += (target - sidebarProgress_) * blend;
    if (std::abs(target - sidebarProgress_) < 0.001f) sidebarProgress_ = target;

    // Clicking outside an open rail collapses it. Tested against the layout
    // built last frame, which is the same one the user is looking at.
    if (sidebarOpen_ && IsMouseButtonPressed(MOUSE_LEFT_BUTTON) &&
        !CheckCollisionPointRec(GetMousePosition(), layout_.rail)) {
        sidebarOpen_ = false;
    }

    layout_ = ui::buildChromeLayout(static_cast<float>(GetScreenWidth()),
                                    static_cast<float>(GetScreenHeight()),
                                    ui::layoutContextFor(simulation), sidebarProgress_);
}

bool UIManager::pointerOverUi() const {
    if (!initialized_) return false;
    return layout_.ownsPointer(GetMousePosition());
}

bool UIManager::pointerPressStartedOverUi() const {
    if (!initialized_) return false;
    return layout_.pressStartedOverUi();
}

void UIManager::draw(Simulation& simulation, Renderer& renderer) {
    if (!initialized_) return;
    drawTopBar(simulation, renderer);
    drawRail(simulation);
    drawSimulationCard(simulation, renderer);
}

// ---------------------------------------------------------------------------
// Top bar
// ---------------------------------------------------------------------------
// Apple-style: a 56pt translucent bar separated by a single hairline, a fixed
// brand block, a segmented control for screen tabs, and a quiet status cluster.
// The tab strip is a real raym3 SegmentedButton, so its segments are both what
// the user sees and what the click test measures -- the previous implementation
// measured tab widths with raylib's default font while rendering the labels in
// Roboto, so the hit regions never lined up with the text.
void UIManager::drawTopBar(Simulation& simulation, Renderer& renderer) {
    const auto& t = ui::theme();
    const ui::ChromeLayout& L = layout_;

    // Translucent material with a 1pt hairline rather than a hard border.
    DrawRectangleRec(L.topBar, ui::withAlpha(t.bgElevated, 0.86f));
    DrawRectangleRec({0.0f, L.topBarHeight - ui::kHairline, L.width, ui::kHairline},
                     ui::withAlpha(t.borderSoft, 0.85f));

    // Brand block: mark + wordmark + descriptor.
    const float markX = L.brand.x + 11.0f;
    const float markY = L.topBarHeight * 0.5f;
    DrawCircleLines(static_cast<int>(markX), static_cast<int>(markY), 10.0f, ui::withAlpha(t.accent, 0.45f));
    DrawCircle(static_cast<int>(markX), static_cast<int>(markY), 3.0f, t.accentStrong);
    const double phase = GetTime() * 0.9;
    DrawCircle(static_cast<int>(markX + 10.0f * std::cos(phase)),
               static_cast<int>(markY + 10.0f * std::sin(phase)), 1.6f, t.accent);

    raym3::Text("BAGS STEM LAB", {markX + 20.0f, 8.0f, 200.0f, 18.0f}, 15.0f, t.text,
                raym3::FontWeight::Medium);
    raym3::Text("Virtual Science Laboratory", {markX + 20.0f, 27.0f, 200.0f, 14.0f}, 9.0f,
                t.textDim, raym3::FontWeight::Medium);

    // Screen tabs as a segmented control.
    if (L.tabStrip.width >= 120.0f) {
        raym3::SegmentedButtonItem items[std::size(ui::kScreenTabs)];
        for (std::size_t i = 0; i < std::size(ui::kScreenTabs); ++i) {
            items[i] = {ui::kScreenTabs[i].label, nullptr, false};
        }
        int selected = ui::screenTabIndex(simulation);
        raym3::SegmentedButton(L.tabStrip, items, static_cast<int>(std::size(items)), &selected);
        if (selected != ui::screenTabIndex(simulation)) {
            simulation.setScreen(ui::kScreenTabs[static_cast<std::size_t>(selected)].screen);
        }
    }

    // Status cluster: elapsed time, run state, time scale.
    const bool compact = L.compactTopBar;
    raym3::Text(formatSimulationDays(simulation.simTime).c_str(),
                {L.simClock.x, compact ? 8.0f : 6.0f, L.simClock.width, 20.0f}, 14.0f,
                t.text, raym3::FontWeight::Medium, raym3::TextAlignment::Right);
    raym3::Text(simulation.paused ? "Paused" : "Running",
                {L.simClock.x, compact ? 30.0f : 28.0f, L.simClock.width, 14.0f}, 9.0f,
                simulation.paused ? t.warn : t.success, raym3::FontWeight::Medium,
                raym3::TextAlignment::Right);
    if (!compact) {
        raym3::Text(formatTimeScale(simulation.speed).c_str(),
                    {L.simClock.x, 42.0f, L.simClock.width, 12.0f}, 9.0f, t.textDim,
                    raym3::FontWeight::Medium, raym3::TextAlignment::Right);
    }

    // Trailing icon buttons: 28pt, tonal, so they read as secondary.
    if (raym3::IconButton("fullscreen", L.fullscreenButton, raym3::ButtonVariant::Text)) {
        if (!IsWindowFullscreen()) ToggleFullscreen();
    }
    if (raym3::IconButton("settings", L.settingsButton, raym3::ButtonVariant::Text)) {
        simulation.setScreen(AppScreen::Settings);
    }
    (void)renderer;
}

// ---------------------------------------------------------------------------
// Navigation rail
// ---------------------------------------------------------------------------
// A single material with a hairline, a disclosure toggle, and the raym3 rail.
// Collapsed it shows icons only. The whole rail is claimed by the layout's
// pointer-ownership test, so the camera can never begin an orbit over it.
void UIManager::drawRail(Simulation& simulation) {
    const auto& t = ui::theme();
    const ui::ChromeLayout& L = layout_;

    // One surface, not two. raym3's NavigationRail paints its own elevated
    // background, so drawing a filled container behind it as well produced a
    // lighter panel nested inside a darker one. Only the hairline is drawn here
    // and the rail component owns the fill.
    DrawRectangleRoundedLines(L.rail, 0.14f, 16, ui::withAlpha(t.borderSoft, 0.70f));

    if (raym3::Button(L.railExpanded ? "Collapse" : "Menu", L.railToggle,
                      raym3::ButtonVariant::Text)) {
        sidebarOpen_ = !sidebarOpen_;
    }

    raym3::NavigationItem navItems[std::size(ui::kRailItems)];
    for (std::size_t i = 0; i < std::size(ui::kRailItems); ++i) {
        navItems[i] = {ui::kRailItems[i].label, ui::kRailItems[i].icon,
                       ui::kRailItems[i].icon, nullptr, false, !ui::kRailItems[i].enabled};
    }

    int selected = ui::selectedRailIndex(simulation);
    // expressiveRail is off: the expressive variant adds a 3-level elevation
    // shadow to its background, which reads as a second, floating panel.
    if (raym3::NavigationRail(L.railItems, navItems,
                              static_cast<int>(std::size(navItems)), &selected,
                              {L.railExpanded, false, nullptr})) {
        const ui::RailEntry& item = ui::kRailItems[static_cast<std::size_t>(selected)];
        if (item.enabled) simulation.setScreen(item.screen);
    }
}

// Apple-style toggle. The whole row is the target, the MD3 track is centred in
// it, and the state glyph comes from raym3's SVG icon set (see the note on
// ui::ToggleMetrics for why the raym3 SwitchComponent is not used here).
//
// Motion: the thumb grows and slides over 180ms, which is Apple's own switch
// timing. The animation is driven from GetTime() rather than an accumulated
// delta so it is correct regardless of frame rate.
bool UIManager::drawToggle(Rectangle row, const char* label, bool* value) {
    using namespace ui;
    const auto& t = theme();
    const Vector2 mouse = GetMousePosition();

    const bool overTrack = CheckCollisionPointRec(mouse, toggleTrackIn(row));
    const bool overRow = CheckCollisionPointRec(mouse, row);
    if (overTrack) raym3::RequestCursor(MOUSE_CURSOR_POINTING_HAND);

    const bool down = overRow && IsMouseButtonDown(MOUSE_LEFT_BUTTON);
    const bool changed = overRow && IsMouseButtonReleased(MOUSE_LEFT_BUTTON);
    if (changed) *value = !*value;

    // Per-label animation state, keyed by the label so each row tracks its own
    // progress without the controls having to carry state.
    static float progress[4] = {0.0f, 0.0f, 0.0f, 0.0f};
    int slot = 0;
    for (int i = 0; i < 4; ++i) {
        if (row.y == layout_.simToggles[static_cast<std::size_t>(i)].y) slot = i;
    }
    const float target = *value ? 1.0f : 0.0f;
    progress[slot] += (target - progress[slot]) * approachBlend(16.0f, GetFrameTime());
    if (std::abs(target - progress[slot]) < 0.002f) progress[slot] = target;
    const float a = progress[slot];

    const Rectangle track = toggleTrackIn(row);
    const float trackRadius = ToggleMetrics::kTrackHeight * 0.5f;
    DrawRectangleRounded(track, trackRadius, 24,
                        ColorAlpha(t.accent, 0.28f + 0.55f * a));
    // Hairline only while off, matching how a macOS switch reads at rest.
    if (a < 0.5f) {
        DrawRectangleRoundedLines(track, trackRadius, 24, ColorAlpha(t.borderSoft, 0.9f));
    }

    const float thumbSize = ToggleMetrics::kThumbOff +
                            (ToggleMetrics::kThumbOn - ToggleMetrics::kThumbOff) * a;
    const float travel = ToggleMetrics::kTrackWidth - ToggleMetrics::kTrackHeight;
    const float cx = track.x + ToggleMetrics::kTrackHeight * 0.5f + travel * a;
    const float cy = track.y + ToggleMetrics::kTrackHeight * 0.5f;
    DrawCircle(static_cast<int>(cx), static_cast<int>(cy), thumbSize * 0.5f,
               a > 0.5f ? t.text : t.textDim);
    if (a > 0.02f) {
        // Check mark, drawn through the SVG icon path that raym3 resolves
        // correctly, and faded in with the thumb.
        const float iconSize = thumbSize * 0.62f;
        raym3::Icon("check",
                    {cx - iconSize * 0.5f, cy - iconSize * 0.5f, iconSize, iconSize},
                    raym3::IconVariation::Filled, withAlpha(t.accent, a));
    }

    // State layer under the press, matching the rest of the shell.
    if (down) {
        DrawRectangleRounded(track, trackRadius, 24, withAlpha(t.text, 0.10f));
    }

    raym3::Text(label, {row.x + 2.0f, row.y + (row.height - 14.0f) * 0.5f,
                        track.x - row.x - 8.0f, 14.0f}, 14.0f, t.text,
                raym3::FontWeight::Regular);
    return changed;
}

// ---------------------------------------------------------------------------
// Simulation control card
// ---------------------------------------------------------------------------
// Transport, time scale, and the four display switches. Every control is a
// raym3 component driven directly from Simulation/Renderer state, and every
// rectangle comes from ChromeLayout, so the clickable area is by construction
// the area that was drawn.
void UIManager::drawSimulationCard(Simulation& simulation, Renderer& renderer) {
    if (simulation.screen != AppScreen::Simulation) return;
    const auto& t = ui::theme();
    const ui::ChromeLayout& L = layout_;

    raym3::Card(L.simCard, raym3::CardVariant::Elevated);

    // Section header. A label plus the shortcut it maps to, so the keyboard
    // contract is discoverable instead of folklore.
    raym3::Text("SIMULATION", {L.simCard.x + ui::kPadPanel, L.simCard.y + 16.0f,
                               L.simCard.width - 32.0f, 16.0f}, 12.0f, t.accent,
                raym3::FontWeight::Bold);
    raym3::Text("Space", {L.simCard.x + ui::kPadPanel, L.simCard.y + 34.0f,
                          L.simCard.width - 32.0f, 14.0f}, 10.0f, t.textDim,
                raym3::FontWeight::Medium);

    // Transport. Filled for the primary action, tonal for the destructive one.
    if (raym3::Button(simulation.paused ? "Play" : "Pause", L.playButton,
                      simulation.paused ? raym3::ButtonVariant::Filled
                                       : raym3::ButtonVariant::Tonal)) {
        simulation.paused = !simulation.paused;
    }
    if (raym3::Button("Reset", L.resetButton, raym3::ButtonVariant::Outlined)) {
        renderer.resetSystemView();
    }

    // Time scale stepper.
    raym3::Text("TIME SCALE", {L.simCard.x + ui::kPadPanel, L.simCard.y + 112.0f,
                               L.simCard.width - 32.0f, 12.0f}, 9.0f, t.textDim,
                raym3::FontWeight::Bold);
    if (raym3::Button("-", L.speedDown, raym3::ButtonVariant::Outlined)) {
        simulation.adjustSpeed(-1);
    }
    if (raym3::Button("+", L.speedUp, raym3::ButtonVariant::Outlined)) {
        simulation.adjustSpeed(1);
    }
    raym3::Text(formatTimeScale(simulation.speed).c_str(),
                {L.speedDown.x + L.speedDown.width, L.speedDown.y,
                 L.speedUp.x - (L.speedDown.x + L.speedDown.width), L.speedDown.height},
                12.0f, t.text, raym3::FontWeight::Medium, raym3::TextAlignment::Center);

    // Display switches. Each row is a full-width 28pt target, so the click works
    // anywhere across the row rather than only on the small track.
    raym3::Text("DISPLAY", {L.simCard.x + ui::kPadPanel, L.simCard.y + 184.0f,
                            L.simCard.width - 32.0f, 12.0f}, 9.0f, t.textDim,
                raym3::FontWeight::Bold);

    bool orbits = simulation.showOrbits;
    bool trails = simulation.showTrails;
    bool forces = simulation.showVectors;
    bool gravity = simulation.gravityMode;

    if (drawToggle(L.simToggles[0], "Orbits", &orbits)) {
        simulation.showOrbits = orbits;
        if (!orbits) simulation.showMoons = false;
    }
    if (drawToggle(L.simToggles[1], "Trails", &trails)) {
        simulation.showTrails = trails;
        simulation.settings.trailsEnabled = trails;
    }
    if (drawToggle(L.simToggles[2], "Forces", &forces)) {
        simulation.showVectors = forces;
        simulation.settings.vectorsEnabled = forces;
    }
    if (drawToggle(L.simToggles[3], "Gravity", &gravity)) {
        simulation.gravityMode = gravity;
        // Leaving gravity mode restores base masses so experiments stay
        // reversible, matching what the G shortcut does.
        if (!gravity) simulation.resetMassMultipliers();
    }
}

} // namespace bag
