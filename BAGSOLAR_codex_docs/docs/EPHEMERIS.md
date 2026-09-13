# Ephemeris Integration

Phase 6 provides a raylib-free boundary between astronomical state providers
and the BAGSOLAR simulation. Physics receives only validated SI state vectors;
it never calls a provider, HTTP endpoint, JSON parser, or network library.

## State contract

`EphemerisState` contains a stable body ID, a Julian Date epoch, an explicit
orientation/reference-frame and origin pair, position in metres, velocity in
metres/second, provider and source provenance, units, and an explicit status.
The only epoch type currently accepted is Julian Date. For the SPICE adapter,
that Julian Date is interpreted on the TDB-compatible scale required by the
SPICE ET conversion; it is never silently interpreted as UTC or simulation
time.

Supported origin semantics are `heliocentric`/`sun`, `geocentric`/`earth`, and
`barycentric`/`solar_system_barycenter`. All currently supported states use
the explicit J2000 orientation. Origin semantics and orientation are separate:
J2000 identifies the orientation, while sun, earth, or the solar-system
barycenter identifies the observer/origin. Changing the observer does not
claim a change of orientation, and no arbitrary frame rotation is performed.

## Providers

`EphemerisProvider` exposes a synchronous request/result interface. The local
provider is deterministic and offline-only, with clearly labelled J2000
example states. `JsonEphemerisProvider` accepts schema version 1 documents only
when body, epoch, frame, origin, units, position, and velocity are explicit.
Kilometres and km/s are converted once at that boundary.

`HorizonsProvider` uses an injected `HttpClient`, deterministic request builder,
response parser, and a request-keyed in-memory cache. The official endpoint is
`https://ssd.jpl.nasa.gov/api/horizons.api`; requests use JSON output, VECTORS,
one explicit TLIST Julian Date, an explicit CENTER, and `KM-S` output units.
The parser validates the JSON result, `$$SOE`/`$$EOE` vector section, returned
epoch, finite values, and converts km/km/s to SI. Horizons errors and HTTP or
network failures are returned explicitly. There is no automatic fallback to
the local provider.

The `CurlHttpClient` is a small isolated command transport with a bounded
timeout. It is used only when explicitly selected, including by the
`bagsolar_ephemeris --horizons ...` CLI path. Standard builds and tests do not
perform network requests.

## Snapshots and simulation initialization

`EphemerisSnapshot` accepts multiple states only when they have the same epoch,
orientation, frame/origin, SI units, and unique body IDs. No interpolation is performed. The opt-in
`Simulation::initializeFromEphemeris` operation updates already-known body
positions and velocities, preserves source/provider/epoch/frame metadata, and
does not invent masses or silently create bodies.

## CLI

```sh
./bagsolar_ephemeris --local earth 2451545.0 heliocentric
./bagsolar_ephemeris --json data/ephemeris/earth.json earth 2451545.0 heliocentric
./bagsolar_ephemeris --horizons earth 2460000.5 heliocentric
```

The last command is an explicit network request and reports failure rather
than substituting local data. Retrieval time, if later added, will remain
separate from the astronomical epoch.

## Limitations

Horizons and the optional SPICE provider supply external reference states; they
do not make BAGSOLAR's Newtonian model NASA-accurate. Simplified perturbations,
frame limitations, and numerical integration error remain. Real SPICE
execution still requires a local CSPICE installation and compatible user-owned
kernels. TLE/SGP4, interpolation, spacecraft dynamics, and
prediction-vs-reality analysis are future work.

## Optional CSPICE integration

Configure CSPICE explicitly:

```sh
cmake -S . -B build-spice -DBAGSOLAR_ENABLE_SPICE=ON \
  -DCSPICE_INCLUDE_DIR=/path/to/cspice/include \
  -DCSPICE_LIBRARY=/path/to/cspice/lib/cspice.a
cmake --build build-spice --parallel
```

The project does not download CSPICE or kernels. A manifest uses schema
version 1 and explicit kernel types/load order:

```json
{
  "schema_version": 1,
  "kernels": [
    {"path": "naif0012.tls", "type": "LSK", "load_order": 10},
    {"path": "de440.bsp", "type": "SPK", "load_order": 20}
  ]
}
```

Relative paths resolve against the manifest directory. Supported types are
SPK, LSK, PCK, and FK. Paths are validated, duplicates rejected, and entries
loaded deterministically. Kernel files are not included in this repository.

When enabled, CSPICE `spkezr_c` queries use the explicit `J2000` frame and
mapped observer origin. Julian Date converts deterministically to SPICE ET as
`(JD - 2451545.0) * 86400`; returned km and km/s values are converted to m and
m/s. The real kernel-loading path is implemented conditionally. The installed NAIF archive
and header have now been verified and a CSPICE-linked build succeeds. The
the external `cook_01.tls` LSK and `de440.bsp` planetary SPK load
successfully. A real Earth query at JD 2451545.0 returned finite position and
velocity values through `spkezr_c`; the live test also verified SI-unit state
scale, J2000 orientation, Sun origin, and SPICE telemetry provenance.

The optional `bagsolar_spice_live_tests` target is compiled only in a
CSPICE-enabled build. Set `BAGSOLAR_SPICE_TEST_MANIFEST` to a user-owned
manifest before running it; without that variable the test is skipped.
