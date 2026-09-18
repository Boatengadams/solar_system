#ifdef NDEBUG
#undef NDEBUG
#endif
#include <cassert>
#include <vector>

#include "rendering/RenderTransform.hpp"
#include "rendering/Selection.hpp"

int main() {
    using namespace bag;
    const std::vector<SelectionCandidate> candidates = {
        {0, {100.0f, 100.0f}, 10.0f},
        {1, {150.0f, 100.0f}, 14.0f},
    };
    assert(selectNearestBody({100.0f, 100.0f}, candidates) == 0);
    assert(selectNearestBody({109.0f, 100.0f}, candidates) == 0);
    assert(selectNearestBody({162.0f, 100.0f}, candidates) == 1);
    assert(selectNearestBody({165.0f, 100.0f}, candidates) == -1);
    assert(selectNearestBody({200.0f, 100.0f}, candidates) == -1);

    // A moved body is represented by its new rendered screen position. Its
    // former position must not remain selectable.
    const std::vector<SelectionCandidate> moved = {{0, {200.0f, 200.0f}, 10.0f}};
    assert(selectNearestBody({200.0f, 200.0f}, moved) == 0);
    assert(selectNearestBody({100.0f, 100.0f}, moved) == -1);

    // Camera changes are reflected in the transformed candidate position.
    const std::vector<SelectionCandidate> pannedAndZoomed = {{0, {320.0f, 280.0f}, 12.0f}};
    assert(selectNearestBody({320.0f, 280.0f}, pannedAndZoomed) == 0);
    assert(selectNearestBody({200.0f, 200.0f}, pannedAndZoomed) == -1);

    // Empty-space clicks have no candidate and therefore clear selection.
    assert(selectNearestBody({500.0f, 500.0f}, {}) == -1);
    assert(selectionIndicatorVisible(true, 2, true));
    assert(!selectionIndicatorVisible(false, 2, true));
    assert(!selectionIndicatorVisible(true, -1, true));
    assert(!selectionIndicatorVisible(true, 2, false));
    const std::vector<SelectionCandidate> overlap = {{4, {0.0f, 0.0f}, 10.0f}, {2, {0.0f, 0.0f}, 10.0f}};
    assert(selectNearestBody({0.0f, 0.0f}, overlap) == 2);

    const std::vector<int> activeBodies = {0, 2, 5};
    assert(cycleActiveBody(-1, activeBodies, 1) == 0);
    assert(cycleActiveBody(0, activeBodies, 1) == 2);
    assert(cycleActiveBody(2, activeBodies, -1) == 0);
    assert(cycleActiveBody(5, activeBodies, 1) == 0);
    assert(cycleActiveBody(0, activeBodies, -1) == 5);
    assert(cycleActiveBody(3, activeBodies, 1) == 0);

    const RenderTransform transform{20.0f};
    const Vector3 renderOrigin = transform.position({0.0, 0.0, 0.0});
    const Vector3 renderPosition = transform.position({PhysicsEngine::AU, 2.0 * PhysicsEngine::AU, 3.0 * PhysicsEngine::AU});
    assert(renderOrigin.x == 0.0f && renderOrigin.y == 0.0f && renderOrigin.z == 0.0f);
    assert(renderPosition.x == 20.0f && renderPosition.y == 60.0f && renderPosition.z == 40.0f);

    const RenderRay ray{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}};
    const std::vector<RaySphereCandidate> rayCandidates = {
        {5, {10.0, 0.0, 0.0}, 2.0},
        {2, {4.0, 0.0, 0.0}, 1.0},
        {9, {0.0, 5.0, 0.0}, 1.0},
    };
    assert(selectNearestRaySphere(ray, rayCandidates) == 2);
    assert(selectNearestRaySphere(ray, {{1, {0.0, 5.0, 0.0}, 1.0}}) == -1);
    assert(selectNearestRaySphere({{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
                                  {{1, {20.0, 0.0, 0.0}, 2.0}}) == 1);
    assert(selectNearestRaySphere({{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}},
                                  {{1, {30.0, 5.0, 0.0}, 2.0}}) == -1);
    return 0;
}
