#include "SpacetimeGrid.hpp"
#include "rlgl.h"
#include <cmath>
#include <algorithm>

SpacetimeGrid::SpacetimeGrid(float extent, int resolution)
    : m_extent(extent), m_res(resolution) {
    m_heights.assign(static_cast<size_t>(m_res + 1) * (m_res + 1), 0.0f);
}

void SpacetimeGrid::SetView(float centerX, float centerZ, float extent) {
    m_centerX = centerX;
    m_centerZ = centerZ;
    m_extent = std::max(extent, 1.0f);
}

static float WellDepth(float dx, float dz, double mass, double G) {
    // gravity_sim reference: softened inverse-distance gravity wells.
    const float softening = 1.4f;
    float r = std::sqrt(dx * dx + dz * dz + softening * softening);
    float strength = static_cast<float>(std::cbrt(std::max(mass, 0.0)) * std::sqrt(std::max(G, 0.0))) * 1.6f;
    return -strength * (1.0f / r) * 6.0f;
}

float SpacetimeGrid::HeightAt(float x, float z, const std::vector<Vector3>& positions,
                               const std::vector<double>& masses, double G) const {
    float h = 0.0f;
    size_t n = std::min(positions.size(), masses.size());
    for (size_t i = 0; i < n; ++i) {
        h += WellDepth(x - positions[i].x, z - positions[i].z, masses[i], G);
    }
    return std::max(h, -12.0f); // gravity_sim visual depth clamp
}

void SpacetimeGrid::Update(const std::vector<Vector3>& positions, const std::vector<double>& masses, double G) {
    float step = (2.0f * m_extent) / static_cast<float>(m_res);
    for (int iz = 0; iz <= m_res; ++iz) {
        float z = m_centerZ - m_extent + iz * step;
        for (int ix = 0; ix <= m_res; ++ix) {
            float x = m_centerX - m_extent + ix * step;
            m_heights[Idx(ix, iz)] = HeightAt(x, z, positions, masses, G);
        }
    }
}

void SpacetimeGrid::Draw() const {
    float step = (2.0f * m_extent) / static_cast<float>(m_res);
    rlBegin(RL_LINES);
    for (int iz = 0; iz <= m_res; ++iz) {
        float z = m_centerZ - m_extent + iz * step;
        for (int ix = 0; ix < m_res; ++ix) {
            float x0 = m_centerX - m_extent + ix * step;
            float x1 = x0 + step;
            float y0 = m_heights[Idx(ix, iz)];
            float y1 = m_heights[Idx(ix + 1, iz)];
            float depth = std::clamp((-(y0 + y1) * 0.5f) / 10.0f, 0.0f, 1.0f);
            rlColor4ub(static_cast<unsigned char>(210 - 140 * depth), static_cast<unsigned char>(225 - 120 * depth),
                       static_cast<unsigned char>(245 - 40 * depth), static_cast<unsigned char>(90 + 140 * depth));
            rlVertex3f(x0, y0, z);
            rlVertex3f(x1, y1, z);
        }
    }
    for (int ix = 0; ix <= m_res; ++ix) {
        float x = m_centerX - m_extent + ix * step;
        for (int iz = 0; iz < m_res; ++iz) {
            float z0 = m_centerZ - m_extent + iz * step;
            float z1 = z0 + step;
            float y0 = m_heights[Idx(ix, iz)];
            float y1 = m_heights[Idx(ix, iz + 1)];
            float depth = std::clamp((-(y0 + y1) * 0.5f) / 10.0f, 0.0f, 1.0f);
            rlColor4ub(static_cast<unsigned char>(210 - 140 * depth), static_cast<unsigned char>(225 - 120 * depth),
                       static_cast<unsigned char>(245 - 40 * depth), static_cast<unsigned char>(90 + 140 * depth));
            rlVertex3f(x, y0, z0);
            rlVertex3f(x, y1, z1);
        }
    }
    rlEnd();
}
