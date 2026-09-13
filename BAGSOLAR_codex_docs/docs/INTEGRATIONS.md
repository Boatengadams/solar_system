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
Phase 6 provides an isolated `HorizonsProvider` using the official API's JSON
VECTORS contract. HTTP is injected through `HttpClient`; normal tests use a
fake client and never access NASA. The command-line curl transport is explicit
and timeout-bounded. Optional SPICE support is provided by the astronomy
adapter; spacecraft dynamics remain an internal domain subsystem rather than
an ephemeris provider.

## Advanced
### NASA SPICE
The Phase 8 boundary includes `SpiceEphemerisProvider` and
`SpiceKernelManifest`, but CSPICE is not bundled. Without an explicit CSPICE
integration build and kernels, requests return `PROVIDER_UNAVAILABLE`; no
fake SPICE values are produced. With CSPICE enabled, `SpiceKernelManager`
loads validated SPK/LSK/PCK/FK entries in deterministic order and the provider
uses `spkezr_c` behind the astronomy boundary.

### Tudat
Use as an optional reference/validation backend, not as a reason to replace the core engine.

### SGP4
Potential future satellite-orbit module for TLE-based objects.

## Integration Rule
External libraries must sit behind adapters/interfaces where practical.

External systems must never leak vendor-specific types throughout the physics core.

## Phase 6 ephemeris boundary

```text
EphemerisProvider → EphemerisState (epoch/frame/origin/SI) → Simulation
       ├→ LocalEphemerisProvider
       ├→ JsonEphemerisProvider
       └→ HorizonsProvider → HttpClient → HorizonsParser
```

Horizons failure is never silently converted into local data. The provider
cache is in-memory and keyed by the complete generated request URL.

## Licensing
Before adding any dependency:
- identify license
- verify compatibility with BAGSOLAR's chosen license
- document the dependency
- preserve required notices
