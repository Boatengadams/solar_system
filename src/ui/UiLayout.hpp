#pragma once

// ChromeLayout is the single source of truth for every interactive region in
// the application shell.
//
// It is computed exactly once per frame (UIManager::update, before the
// renderer and before any drawing) and is then read by BOTH the drawing code
// and the pointer-ownership test. Previously the two sides derived their
// rectangles independently -- the rail width was animated twice per frame, once
// in pointerOverUi() and once in draw() -- so the hit region drifted away from
// the pixels the user could see. Sharing one struct removes that entire class
// of bug by construction.
//
// The layout deliberately records PANEL-level rectangles only. "Does the shell
// own this pointer?" is answered here so the 3D camera never starts a gesture
// over a panel. "Which control was pressed?" is answered by the individual
// raym3 component that owns the click, inside the panel. Keeping those two
// questions at different granularities is what makes the hit-testing robust:
// a control can change size or label without the pointer-ownership test
// drifting out of sync.

#include <algorithm>
#include <array>
#include <cmath>

#include <raylib.h>

#include "UiTheme.hpp"

namespace bag {
namespace ui {

// ---------------------------------------------------------------------------
// Apple-flavoured metrics
// ---------------------------------------------------------------------------
// Spacing follows a 4pt grid, control heights follow Apple's 28pt control
// height, and radii are continuous-style (7-14pt) rather than the very round
// 16-18pt the previous shell used. Hairlines are 1px at low alpha; focus is
// carried by fill and weight rather than by a saturated ring.

inline constexpr float kHairline = 1.0f;
inline constexpr float kControlHeight = 28.0f;    // Apple control height
inline constexpr float kTargetMinHeight = 44.0f;  // comfortable pointer target
inline constexpr float kRadiusSmall = 7.0f;
inline constexpr float kRadiusMedium = 10.0f;
inline constexpr float kRadiusLarge = 14.0f;
inline constexpr float kGapTight = 6.0f;
inline constexpr float kGapRegular = 12.0f;
inline constexpr float kGapLoose = 20.0f;
inline constexpr float kPadPanel = 16.0f;

inline float clampf(float value, float low, float high) {
    return value < low ? low : (value > high ? high : value);
}

// Horizontal pointer slop. Apple's controls forgive a few points of imprecision
// at the edge; without this a press that lands 2px outside a hairline is
// dropped, which reads to the user as "the click missed".
inline Rectangle withSlop(Rectangle rect, float slop = 2.0f) {
    return {rect.x - slop, rect.y - slop, rect.width + slop * 2.0f, rect.height + slop * 2.0f};
}

enum class RailItem : std::size_t {
    Home = 0,
    Labs,
    SolarSystem,
    Motion,
    Forces,
    Energy,
    Waves,
    Optics,
    Electricity,
    Settings,
};
inline constexpr std::size_t kRailItemCount = 10;

struct RailEntry {
    const char* label;
    const char* icon;
    AppScreen screen;
    bool enabled;
};

// The rail is the primary navigation. Disabled entries are architectural
// placeholders for modules with no implementation yet; they render dimmed and
// are not selectable, so the rail never offers a dead end.
inline constexpr std::array<RailEntry, kRailItemCount> kRailItems{{
    {"Home", "home", AppScreen::Simulation, true},
    {"Labs", "science", AppScreen::Education, true},
    {"Solar System", "language", AppScreen::Simulation, true},
    {"Motion", "motion_photos_on", AppScreen::Simulation, false},
    {"Forces", "blur_on", AppScreen::Simulation, false},
    {"Energy", "bolt", AppScreen::Simulation, false},
    {"Waves", "waves", AppScreen::Simulation, false},
    {"Optics", "visibility", AppScreen::Simulation, false},
    {"Electricity", "electric_bolt", AppScreen::Simulation, false},
    {"Settings", "settings", AppScreen::Settings, true},
}};

// Screen tabs shown in the top bar. Order is the reading order of the product,
// not the enum order.
struct ScreenTab {
    const char* label;
    AppScreen screen;
};

inline constexpr std::array<ScreenTab, 7> kScreenTabs{{
    {"Simulation", AppScreen::Simulation},
    {"Learn", AppScreen::Education},
    {"Scenarios", AppScreen::ScenarioBrowser},
    {"Telemetry", AppScreen::Telemetry},
    {"Mission", AppScreen::MissionDesigner},
    {"Settings", AppScreen::Settings},
    {"Help", AppScreen::Help},
}};

// Rail selection derived from the active screen. The rail is the primary
// navigation, so its selection must be a pure function of state rather than
// something the draw pass tracks separately -- otherwise a screen change made
// from the tab strip leaves the rail highlighting the previous destination.
inline int selectedRailIndex(const Simulation& simulation) {
    if (simulation.screen == AppScreen::Settings) return static_cast<int>(RailItem::Settings);
    if (simulation.screen == AppScreen::Education || simulation.screen == AppScreen::LearningLab)
        return static_cast<int>(RailItem::Labs);
    return static_cast<int>(RailItem::SolarSystem);
}

// Index of the tab representing the active screen. Falls back to the first tab
// so the segmented control always has a valid selection.
inline int screenTabIndex(const Simulation& simulation) {
    for (std::size_t i = 0; i < kScreenTabs.size(); ++i) {
        if (kScreenTabs[i].screen == simulation.screen) return static_cast<int>(i);
    }
    return 0;
}

// ---------------------------------------------------------------------------
// ChromeLayout
// ---------------------------------------------------------------------------

struct ChromeLayout {
    float width = 0.0f;
    float height = 0.0f;

    // Top bar -------------------------------------------------------------
    float topBarHeight = 56.0f;
    Rectangle topBar{};
    Rectangle brand{};
    Rectangle tabStrip{};   // raym3 SegmentedButton hosting kScreenTabs
    Rectangle simClock{};
    Rectangle fullscreenButton{};
    Rectangle settingsButton{};
    bool compactTopBar = false;

    // Navigation rail ----------------------------------------------------
    float railTop = 72.0f;
    float railWidth = 0.0f;   // animated; resolved once per frame
    Rectangle rail{};
    Rectangle railToggle{};
    Rectangle railItems{};    // passed to raym3::NavigationRail
    bool railExpanded = false;

    // Simulation control card --------------------------------------------
    Rectangle simCard{};
    Rectangle playButton{};
    Rectangle resetButton{};
    Rectangle speedDown{};
    Rectangle speedUp{};
    std::array<Rectangle, 4> simToggles{};  // Orbits, Trails, Forces, Gravity

    // Scene HUD ----------------------------------------------------------
    Rectangle viewControls{};   // F1 / F2 / F3 segmented view presets
    Rectangle compass{};
    Rectangle inspector{};
    Rectangle hintBar{};
    bool hasSelection = false;

    // Non-simulation pages -----------------------------------------------
    Rectangle pageBody{};

    // ---------------------------------------------------------------------
    // Pointer ownership
    // ---------------------------------------------------------------------
    // True when the pointer is over any shell-owned region. Regions are tested
    // with a small slop so presses near a hairline still land, and the whole
    // top bar is claimed so the camera can never orbit behind the chrome.
    bool ownsPointer(Vector2 point) const {
        if (point.y <= topBarHeight) return true;
        if (CheckCollisionPointRec(point, withSlop(rail, 1.0f))) return true;
        if (CheckCollisionPointRec(point, withSlop(simCard, 1.0f))) return true;
        if (CheckCollisionPointRec(point, withSlop(viewControls, 2.0f))) return true;
        if (CheckCollisionPointRec(point, withSlop(compass, 2.0f))) return true;
        if (hasSelection && CheckCollisionPointRec(point, withSlop(inspector, 1.0f))) return true;
        if (CheckCollisionPointRec(point, withSlop(hintBar, 1.0f))) return true;
        return false;
    }

    // True when the press being resolved this frame began over the shell. The
    // renderer consults this to avoid converting a UI click into a camera
    // gesture, which is what previously ate the first click of every gesture.
    bool pressStartedOverUi() const {
        if (!IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) return false;
        return ownsPointer(GetMousePosition());
    }
};
inline float easeInOutCubic(float value) {
    value = clampf(value, 0.0f, 1.0f);
    return value * value * (3.0f - 2.0f * value);
}

// Rail width endpoints. The content gutter always reserves the EXPANDED width,
// so expanding the rail can never sweep over the inspector, the view presets,
// or the simulation card. Reserving the settled width rather than the animated
// one is what keeps those panels from jumping sideways during the transition.
inline constexpr float kRailCollapsedWidth = 64.0f;
inline constexpr float kRailExpandedWidth = 200.0f;

// Rail width curve. Collapsed shows icons only; expanded reveals labels.
inline float railWidthFor(float progress) {
    return kRailCollapsedWidth +
           (kRailExpandedWidth - kRailCollapsedWidth) * easeInOutCubic(progress);
}

// Left edge for shell-anchored content: past the widest the rail can ever get.
inline float contentGutter() {
    return appEdge() + kRailExpandedWidth + kGapRegular;
}

// Frame-rate independent exponential approach, so the motion is identical at
// 30fps and 144fps.
inline float approachBlend(float rate, float dt) {
    return 1.0f - std::exp(-rate * std::max(0.0001f, dt));
}

// The only facts about application state that the layout depends on.
//
// Passing this instead of the whole Simulation keeps the layout a pure
// function, which is what lets the layout invariants be unit-tested without
// linking the physics, data, and curriculum engines. When the layout grew
// coupled to Simulation, its test needed the entire simulation stack, and a
// regression that zeroed two rectangles shipped undetected.
struct LayoutContext {
    AppScreen screen = AppScreen::Simulation;
    bool hasSelection = false;
    // Solo study and the Earth-Moon focus both want a taller inspector.
    bool deepInspector = false;
};

inline LayoutContext layoutContextFor(const Simulation& simulation) {
    LayoutContext ctx;
    ctx.screen = simulation.screen;
    ctx.hasSelection = simulation.selected >= 0 &&
                       simulation.selected < static_cast<int>(simulation.bodies.size());
    ctx.deepInspector = simulation.soloStudy || simulation.earthMoonFocus;
    return ctx;
}

// Builds the layout for one frame. Call exactly once per frame, before the
// renderer reads pointer input and before anything draws.
inline ChromeLayout buildChromeLayout(float width, float height,
                                     const LayoutContext& ctx,
                                     float railProgress) {
    ChromeLayout L;
    L.width = width;
    L.height = height;

    // -- Top bar ---------------------------------------------------------
    // 56pt bar with a 1pt hairline. 28pt controls, centred vertically.
    L.topBarHeight = 56.0f;
    L.topBar = {0.0f, 0.0f, width, L.topBarHeight};

    // Below 1180pt the clock and the tab strip cannot both breathe, so the
    // strip keeps priority and the clock collapses to a compact readout.
    L.compactTopBar = width < 1180.0f;

    const float pad = appEdge();
    const float controlY = (L.topBarHeight - kControlHeight) * 0.5f;
    const float iconGap = 4.0f;

    L.fullscreenButton = {width - pad - kControlHeight * 2.0f - iconGap, controlY,
                          kControlHeight, kControlHeight};
    L.settingsButton = {width - pad - kControlHeight, controlY, kControlHeight, kControlHeight};

    const float clockW = L.compactTopBar ? 116.0f : 150.0f;
    L.simClock = {L.fullscreenButton.x - 14.0f - clockW, 0.0f, clockW, L.topBarHeight};

    // Brand is fixed-width; the tab strip takes the remaining space so the two
    // never overlap at any window size.
    const float brandW = L.compactTopBar ? 208.0f : 236.0f;
    L.brand = {pad, 0.0f, brandW, L.topBarHeight};

    const float stripX = L.brand.x + L.brand.width + kGapRegular;
    const float stripRight = L.simClock.x - kGapRegular;
    const float stripW = std::max(0.0f, stripRight - stripX);
    const float stripH = 32.0f;
    L.tabStrip = {stripX, (L.topBarHeight - stripH) * 0.5f, stripW, stripH};

    // -- Navigation rail --------------------------------------------------
    L.railTop = L.topBarHeight + kGapRegular;
    L.railWidth = railWidthFor(railProgress);
    L.railExpanded = railProgress > 0.5f;
    const float railH = std::max(0.0f, height - L.railTop - pad);
    L.rail = {pad, L.railTop, L.railWidth, railH};

    L.railToggle = {L.rail.x + 8.0f, L.rail.y + 8.0f, L.rail.width - 16.0f, kControlHeight};
    // raym3's rail lays items out from the top of this rect at a fixed 64pt
    // pitch; 76pt of leading space clears the toggle without overlapping.
    L.railItems = {L.rail.x + 6.0f, L.rail.y + 76.0f, L.rail.width - 12.0f,
                   std::max(0.0f, railH - 84.0f)};

    // -- Scene HUD --------------------------------------------------------
    // Anchored to the same left gutter as the rail so the composition reads as
    // one column instead of two unrelated stacks.
    const float leftGutter = contentGutter();

    const float viewW = 268.0f;
    // Tall enough for caption + 32pt segment + shortcut legend without the
    // legend landing on top of the control. At 74pt the legend at y+56
    // overlapped the segment that ends at y+60.
    const float viewH = 96.0f;
    L.viewControls = {leftGutter, height - viewH - pad, viewW, viewH};

    const float compassSize = 64.0f;
    L.compass = {width - pad - compassSize, height - pad - compassSize,
                  compassSize, compassSize};

    L.hasSelection = ctx.hasSelection;
    if (L.hasSelection) {
        const float inspW = 320.0f;
        const float inspH = ctx.deepInspector ? 372.0f : 306.0f;
        L.inspector = {leftGutter, L.viewControls.y - inspH - 10.0f, inspW, inspH};
    }

    // -- Simulation control card -----------------------------------------
    // Only meaningful on the simulation screen; other pages leave it empty so
    // ownsPointer() reports false there.
    if (ctx.screen == AppScreen::Simulation) {
        const float cardW = std::min(320.0f, std::max(240.0f, width - 2.0f * leftGutter - 60.0f));
        const float cardX = width - pad - cardW;
        const float cardY = L.railTop;
        // Card height follows the vertical rhythm below. The previous 268pt
        // clipped the second switch row and let the DISPLAY caption collide
        // with it, because the MD3 track is 32pt tall inside a 28pt row and
        // overflows by 2pt on each side.
        const float cardH = 286.0f;
        L.simCard = {cardX, cardY, cardW, cardH};

        const float innerX = cardX + kPadPanel;
        const float innerW = cardW - kPadPanel * 2.0f;

        // Transport row: play/reset side by side, 36pt tall.
        const float transportH = 36.0f;
        const float transportY = cardY + 60.0f;
        const float transportW = (innerW - kGapTight) * 0.5f;
        L.playButton = {innerX, transportY, transportW, transportH};
        L.resetButton = {innerX + transportW + kGapTight, transportY, transportW, transportH};

        // Speed stepper: paired square buttons flanking a centred readout.
        const float stepY = cardY + 140.0f;
        L.speedDown = {innerX, stepY, kControlHeight, kControlHeight};
        L.speedUp = {cardX + cardW - kPadPanel - kControlHeight, stepY,
                     kControlHeight, kControlHeight};

        // Four display switches in a 2x2 grid. Each row is 28pt tall -- the
        // Apple control height -- and spans the full inner width so the whole
        // row is a valid target rather than only the 52pt track.
        const float colW = (innerW - kGapRegular) * 0.5f;
        // Row 0 clears the DISPLAY caption by 8pt, which absorbs the 2pt the
        // track overhangs its row.
        const float rowY0 = cardY + 206.0f;
        const float rowY1 = rowY0 + kControlHeight + kGapTight;
        L.simToggles[0] = {innerX, rowY0, colW, kControlHeight};
        L.simToggles[1] = {innerX + colW + kGapRegular, rowY0, colW, kControlHeight};
        L.simToggles[2] = {innerX, rowY1, colW, kControlHeight};
        L.simToggles[3] = {innerX + colW + kGapRegular, rowY1, colW, kControlHeight};
    }

    // -- Non-simulation pages ---------------------------------------------
    if (ctx.screen != AppScreen::Simulation) {
        const float p = contentGutter();
        L.pageBody = {p,
                      contentTop() + 70.0f,
                      std::max(0.0f, width - 2.0f * p - 300.0f),
                      std::max(0.0f, height - L.topBarHeight - 120.0f)};
    }

    return L;
}

// ---------------------------------------------------------------------------
// Toggle switch
// ---------------------------------------------------------------------------
// raym3 ships SwitchComponent, but that component draws its state glyph with a
// Material Icons TTF that is not part of the raym3 resource bundle. The glyph
// therefore falls back to raylib's default ASCII font and renders as a literal
// "?" inside the thumb. raym3's SVG icon path is unaffected -- it is what draws
// the navigation rail and the top bar buttons correctly -- so this switch is
// built on raym3::Icon instead and keeps the MD3 metrics and motion.
//
// The other reason this exists is hit area. SwitchComponent treats `bounds` as
// the track rect, which forces the 52pt track and the 14pt label to share the
// row, so the clickable region is not what the user perceives as the control.
// Here the whole row is the target and the track is centred inside it.
struct ToggleMetrics {
    static constexpr float kTrackWidth = 52.0f;
    static constexpr float kTrackHeight = 32.0f;
    static constexpr float kThumbOn = 24.0f;
    static constexpr float kThumbOff = 16.0f;
};

inline Rectangle toggleTrackIn(Rectangle row) {
    const float x = row.x + row.width - ToggleMetrics::kTrackWidth;
    const float y = row.y + (row.height - ToggleMetrics::kTrackHeight) * 0.5f;
    return {x, y, ToggleMetrics::kTrackWidth, ToggleMetrics::kTrackHeight};
}

} // namespace ui
} // namespace bag
