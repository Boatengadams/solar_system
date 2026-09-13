# Third-party dependencies

## nlohmann/json

The project uses the MIT-licensed nlohmann/json package for Phase 2 JSON
parsing and serialization. The extracted package is kept under
`third_party/nlohmann-json3-dev/usr/include` so builds do not require a
system-wide package installation. The package copyright and license notice are
available under its extracted `usr/share/doc/nlohmann-json3-dev/` directory.

The project-level summary of bundled software, optional CSPICE, external
SPICE kernels, raylib, and the optional Horizons `curl` transport is in
`BAGSOLAR_codex_docs/THIRD_PARTY_LICENSES.md`.

JSON types are confined to the data layer.
