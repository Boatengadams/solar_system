# Ephemeris Integration

## Goal
Support real astronomical states without coupling the physics engine directly to a web API.

## Provider Interface

Conceptually:

```cpp
class EphemerisProvider {
public:
    virtual ~EphemerisProvider() = default;
    virtual StateVector getState(
        const BodyId& body,
        const Epoch& epoch,
        const ReferenceFrame& frame
    ) = 0;
};
```

## Providers
- Static/local provider
- JSON provider
- JPL Horizons provider
- Optional SPICE provider

## Rules
- Explicitly represent epoch.
- Explicitly represent reference frame.
- Explicitly represent units.
- Cache remote data.
- Provide offline fallback.
- Validate downloaded data before use.
- Do not mix coordinate systems silently.

## JPL Horizons
Use Horizons as a real-data source, not as a replacement for BAGSOLAR's educational physics engine.

Preferred flow:

```text
Horizons
   ↓
State Vector
   ↓
Validation / Conversion
   ↓
Simulation Initial State
   ↓
BAGSOLAR Physics
```
