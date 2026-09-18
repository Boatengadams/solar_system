# Third-party dependencies

## nlohmann/json

The project uses the MIT-licensed nlohmann/json package for JSON parsing and
serialization. The extracted package is kept under
`third_party/nlohmann-json3-dev/usr/include` so builds do not require a
system-wide package installation. Copyright and license notices are under its
extracted `usr/share/doc/nlohmann-json3-dev/` directory.

Project-level summaries for bundled software, optional CSPICE, external SPICE
kernels, raylib, and the optional Horizons `curl` transport are in
`docs/THIRD_PARTY_LICENSES.md`.

JSON types are confined to the data layer.
