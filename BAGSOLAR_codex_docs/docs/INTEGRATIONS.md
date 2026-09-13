# Integration Strategy

## High Priority
### nlohmann/json
Use for scenario/body configuration.

Phase 2 uses the MIT-licensed nlohmann/json package locally under
`third_party/nlohmann-json3-dev`. JSON types are confined to
`src/data/ScenarioLoader.cpp` and `src/data/ScenarioSerializer.cpp`; they do
not cross into physics or rendering.

### Dear ImGui
Use for scientific/debug/control panels if it improves UX.

### JPL Horizons
Use for real planetary/spacecraft ephemeris.

## Advanced
### NASA SPICE
Use for high-fidelity spacecraft and planetary geometry.

### Tudat
Use as an optional reference/validation backend, not as a reason to replace the core engine.

### SGP4
Potential future satellite-orbit module for TLE-based objects.

## Integration Rule
External libraries must sit behind adapters/interfaces where practical.

External systems must never leak vendor-specific types throughout the physics core.

## Licensing
Before adding any dependency:
- identify license
- verify compatibility with BAGSOLAR's chosen license
- document the dependency
- preserve required notices
