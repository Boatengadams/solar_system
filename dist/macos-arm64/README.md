# BAGS_LAB — macOS ARM64 portable runtime

**Status: PLANNED / NOT YET TESTED**

No Apple Silicon binary or `.app` bundle is shipped here yet.

Produce on a macOS host with `CMAKE_OSX_ARCHITECTURES=arm64`:

```sh
cmake --preset macos-arm64
cmake --build --preset macos-arm64
```

Intended package: `BAGS_LAB.app` with resources under `Contents/Resources/`.

See `platform/macos/README.md` and `docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md`.
