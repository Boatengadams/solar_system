# Third-Party Software and External Data

This document distinguishes the BAGSOLAR project license from software and
data supplied by other parties. BAGSOLAR source code is distributed under the
MIT License in the repository-level `LICENSE` file.

| Component | Purpose | Source / distribution | License or terms | Requirement |
|---|---|---|---|---|
| raylib | Windowing, input, and rendering | System package or user-provided installation | Verify the license for the installed raylib release from the raylib distribution before redistribution; raylib is not bundled here | Required for the application target |
| nlohmann/json | JSON parsing and serialization | Bundled under `third_party/nlohmann-json3-dev` | MIT, with the bundled copyright notice retained under the extracted package | Required by the data/education serialization code |
| NAIF CSPICE | Optional SPICE ephemeris adapter | External user-provided CSPICE installation | Verify and follow the terms distributed by NAIF with the selected CSPICE release; CSPICE is not bundled | Optional |
| SPICE kernels, including DE440 | External ephemeris data | User-owned kernel download and manifest | Follow the terms and attribution supplied with each kernel/data distribution; kernels are not included or redistributed by BAGSOLAR | Optional external data |
| `curl` | Command-line transport for the Horizons adapter | System executable | Follow the license/terms of the installed curl distribution; curl is not bundled or linked into BAGSOLAR | Optional, only for the CLI Horizons path |
| Downloaded planet GLB models | Optional renderer presentation | Local `assets/planets/` (+ `prepared/`) development files | Each GLB's Sketchfab `asset.extras` claims a license (mostly CC-BY-4.0; Venus CC-BY-SA-4.0) with author/source URLs; treat as claimed metadata pending explicit redistribution approval and attribution packaging | Optional; package/install still excludes GLB binaries and installs only `manifest.json` |

The local provider, JSON provider, and ordinary tests do not require network
access, CSPICE, or external kernels. BAGSOLAR does not download or silently
substitute external providers or data.

The raylib, CSPICE, kernel, and curl entries intentionally call for review of
the installed/vendor distribution terms rather than asserting terms that are
not present in this repository.
