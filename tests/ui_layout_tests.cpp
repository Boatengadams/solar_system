// Guards the shell layout invariants that the click-accuracy fix depends on.
//
// Regression history: the view-presets and compass rectangles were silently
// zeroed for a while. Nothing failed, and the earlier version of this test
// reported success, because a zero rect at the origin still passes
// ChromeLayout::ownsPointer() -- it falls inside the "top bar claims the whole
// strip" rule. The degenerate-rect check below is the part that catches it.

#include <cmath>
#include <cstdio>

#include <raylib.h>

#include "ui/UiLayout.hpp"

// No window is created. buildChromeLayout() is a pure function of its inputs
// and the rail width it is handed, and CheckCollisionPointRec() is pure
// geometry, so this test needs neither a GPU nor a display. That keeps it
// runnable in headless CI, which is the whole point of having it.

using namespace bag;

namespace {

int g_failures = 0;

void expect(bool condition, const char* what) {
    if (!condition) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// raylib's CheckCollisionPointRec is edge-INCLUSIVE on all four sides, so a
// probe on a border reads as "inside". This helper mirrors that so the test
// agrees with the code it is testing.
bool inside(const Rectangle& r, Vector2 p) {
    return r.width > 0.0f && r.height > 0.0f &&
           p.x >= r.x && p.x <= r.x + r.width &&
           p.y >= r.y && p.y <= r.y + r.height;
}

// Standard separating-axis test. Note this is deliberately STRICT inequalities
// on the touching edges, so two rects that merely share a border do not count
// as overlapping.
bool overlaps(const Rectangle& a, const Rectangle& b) {
    return a.x < b.x + b.width && b.x < a.x + a.width &&
           a.y < b.y + b.height && b.y < a.y + a.height;
}

Vector2 center(const Rectangle& r) {
    return {r.x + r.width * 0.5f, r.y + r.height * 0.5f};
}

void checkAt(float w, float h, float progress) {
    ui::LayoutContext ctx;
    ctx.screen = AppScreen::Simulation;
    ctx.hasSelection = true;
    const ui::ChromeLayout L = ui::buildChromeLayout(w, h, ctx, progress);

    char msg[256];

    // 1. Every shell region must have real extent. A zero rect is the failure
    //    mode that previously went unnoticed.
    // Floating panels. These are siblings: none may overlap another.
    const Rectangle panels[] = {L.rail, L.simCard, L.viewControls, L.compass, L.inspector};
    const char* panelNames[] = {"rail", "simCard", "viewControls", "compass", "inspector"};
    const int panelCount = 5;

    // Top bar contents. These are CHILDREN of the bar, so containment is
    // expected; what must hold is that each one is fully inside the bar.
    const Rectangle barParts[] = {L.brand, L.tabStrip, L.simClock,
                                  L.fullscreenButton, L.settingsButton};
    const char* barNames[] = {"brand", "tabStrip", "simClock",
                              "fullscreenButton", "settingsButton"};

    const Rectangle all[] = {L.rail, L.simCard, L.viewControls, L.compass,
                             L.inspector, L.topBar, L.brand, L.tabStrip, L.simClock,
                             L.fullscreenButton, L.settingsButton};
    const char* allNames[] = {"rail", "simCard", "viewControls", "compass", "inspector",
                              "topBar", "brand", "tabStrip", "simClock",
                              "fullscreenButton", "settingsButton"};
    constexpr int allCount = 11;

    for (int i = 0; i < allCount; ++i) {
        std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: %s must not be degenerate",
                      w, h, progress, allNames[i]);
        expect(all[i].width > 0.0f && all[i].height > 0.0f, msg);
    }

    // 2. Sibling panels must not overlap.
    for (int i = 0; i < panelCount; ++i) {
        for (int j = i + 1; j < panelCount; ++j) {
            std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: %s overlaps %s",
                          w, h, progress, panelNames[i], panelNames[j]);
            expect(!overlaps(panels[i], panels[j]), msg);
        }
    }

    // 2b. Every top-bar control must live inside the bar and must not overlap
    //     its siblings.
    for (int i = 0; i < 5; ++i) {
        std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: %s escapes the top bar",
                      w, h, progress, barNames[i]);
        expect(barParts[i].x >= L.topBar.x - 0.01f &&
               barParts[i].y >= L.topBar.y - 0.01f &&
               barParts[i].x + barParts[i].width <= L.topBar.x + L.topBar.width + 0.01f &&
               barParts[i].y + barParts[i].height <= L.topBar.y + L.topBar.height + 0.01f,
               msg);
        for (int j = i + 1; j < 5; ++j) {
            std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: top-bar %s overlaps %s",
                          w, h, progress, barNames[i], barNames[j]);
            expect(!overlaps(barParts[i], barParts[j]), msg);
        }
    }

    // 3. The rail must never reach into the content gutter, otherwise expanding
    //    it sweeps across the inspector and the view presets.
    std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: rail overruns content gutter",
                  w, h, progress);
    expect(L.rail.x + L.railWidth <= ui::contentGutter() + 0.01f, msg);

    // 4. The visible middle of every panel must be claimed by the pointer-
    //    ownership test, so a click on it can never fall through to the camera.
    for (int i = 0; i < allCount; ++i) {
        std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: %s centre is not clickable",
                      w, h, progress, allNames[i]);
        expect(L.ownsPointer(center(all[i])), msg);
    }

    // 5. Open scene space must stay unclaimed, or the camera stops working.
    for (float fx = 0.30f; fx <= 0.86f; fx += 0.08f) {
        for (float fy = 0.30f; fy <= 0.88f; fy += 0.08f) {
            const Vector2 probe{w * fx, h * fy};
            if (probe.y <= L.topBarHeight) continue;
            bool inPanel = false;
            for (int i = 0; i < allCount; ++i) {
                if (inside(all[i], probe)) { inPanel = true; break; }
            }
            if (inPanel) continue;
            std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: open scene at (%.0f,%.0f) is claimed",
                          w, h, progress, probe.x, probe.y);
            expect(!L.ownsPointer(probe), msg);
        }
    }

    // 6. Every control must sit inside the card that contains it.
    const Rectangle controls[] = {L.playButton, L.resetButton, L.speedDown, L.speedUp,
                                  L.simToggles[0], L.simToggles[1],
                                  L.simToggles[2], L.simToggles[3]};
    for (int i = 0; i < 8; ++i) {
        std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: sim-card control %d escapes the card",
                      w, h, progress, i);
        expect(inside(L.simCard, center(controls[i])), msg);
        std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: sim-card control %d is degenerate",
                      w, h, progress, i);
        expect(controls[i].width > 0.0f && controls[i].height > 0.0f, msg);
    }

    // 7. The view-presets segment and its shortcut legend must not collide.
    const Rectangle strip{L.viewControls.x + 12.0f, L.viewControls.y + 28.0f,
                          L.viewControls.width - 24.0f, 32.0f};
    const Rectangle legend{L.viewControls.x + 14.0f, L.viewControls.y + 66.0f,
                           L.viewControls.width - 28.0f, 12.0f};
    std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: view-preset legend overlaps its segment",
                  w, h, progress);
    expect(!overlaps(strip, legend), msg);

    // 8. The DISPLAY caption must clear the first switch row. The MD3 track is
    //    32pt tall inside a 28pt row and overhangs by 2pt on each side.
    std::snprintf(msg, sizeof(msg), "%.0fx%.0f p=%.2f: DISPLAY caption collides with switch row 1",
                  w, h, progress);
    expect(!overlaps({L.simCard.x + 16.0f, L.simCard.y + 184.0f, L.simCard.width - 32.0f, 12.0f},
                     L.simToggles[0]), msg);
}

} // namespace

int main() {
    const float widths[] = {1100.0f, 1180.0f, 1280.0f, 1440.0f, 1920.0f, 2560.0f};
    const float heights[] = {700.0f, 900.0f, 1200.0f};
    const float progresses[] = {0.0f, 0.25f, 0.5f, 0.75f, 1.0f};

    for (float w : widths)
        for (float h : heights)
            for (float p : progresses)
                checkAt(w, h, p);

    // The sim card only exists on the simulation screen; on every other page it
    // must stay empty so the camera keeps the whole viewport.
    ui::LayoutContext pageCtx;
    pageCtx.screen = AppScreen::Telemetry;
    const ui::ChromeLayout page = ui::buildChromeLayout(1440.0f, 900.0f, pageCtx, 0.0f);
    expect(page.simCard.width == 0.0f && page.simCard.height == 0.0f,
           "sim card must not be laid out on non-simulation pages");

    if (g_failures == 0) {
        std::printf("ui layout invariants hold across all window sizes and rail states\n");
        return 0;
    }
    std::printf("%d layout failure(s)\n", g_failures);
    return 1;
}
