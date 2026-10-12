#pragma once
#include "raylib.h"
#include <vector>

// Renders the classic "rubber sheet" visualization of spacetime curvature:
// a horizontal grid whose vertices are pulled down near each mass,
// proportional to mass and inversely to distance. Recomputed every frame
// from live positions so the grid warps in real time as masses move.
class SpacetimeGrid {
public:
    SpacetimeGrid(float extent = 26.0f, int resolution = 70);

    // Places the grid in world XZ (typically on the Sun) and sets how far the
    // sheet extends. Zoom may change extent; pan must not drag the well away.
    void SetView(float centerX, float centerZ, float extent);

    // positions[i] / masses[i] must be the same length, one entry per body.
    // Only .x and .z of each position are used (grid lies in the XZ plane).
    void Update(const std::vector<Vector3>& positions, const std::vector<double>& masses, double G);

    // Draws the warped grid as a batched wireframe, colored by depth.
    void Draw() const;

    // Height (well depth) at an arbitrary world (x,z) — used to visually
    // "seat" bodies into their own gravity well when drawing them.
    float HeightAt(float x, float z, const std::vector<Vector3>& positions,
                   const std::vector<double>& masses, double G) const;

private:
    float m_extent;
    float m_centerX = 0.0f;
    float m_centerZ = 0.0f;
    int   m_res;
    std::vector<float> m_heights;
    int Idx(int ix, int iz) const { return iz * (m_res + 1) + ix; }
};
