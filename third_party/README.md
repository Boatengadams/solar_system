# Third-party dependencies

## nlohmann/json

The project uses the MIT-licensed nlohmann/json package for Phase 2 JSON
parsing and serialization. The extracted package is kept under
`third_party/nlohmann-json3-dev/usr/include` so builds do not require a
system-wide package installation. The package copyright and license notice are
available under its extracted `usr/share/doc/nlohmann-json3-dev/` directory.

JSON types are confined to the data layer.
