# BAGS_LAB Roadmap

Completed BAGSOLAR phase history is archived in `docs/history/ROADMAP_PHASES.md`.
Multi-platform build foundation: `docs/MULTIPLATFORM_BUILD_ARCHITECTURE.md`.

## Near term

- **Phase 3:** Produce and verify a Windows x86_64 portable package on a Windows host (MSVC)
- **Phase 4:** Produce and verify macOS x64 and ARM64 `.app` packages on macOS hosts
- **Phase 5:** Produce and verify Linux ARM64 portable package on aarch64 (or verified cross)
- Keep Linux x64 static-raylib + bundled-GLFW portable package as the reference (second-machine clean-host check passed)
- Optional Git LFS guidance if planet GLBs exceed host repository limits

## Medium term

- Stronger offline Horizons fixture packs for classroom demos
- Expanded curriculum coverage without changing physics contracts
- Mission Tools editor surface (keeping analytical APIs honest)
- CI portable matrix (build / test / package / audit / artifacts)

## Explicit non-goals

- Bundling CSPICE or proprietary kernels by default
- Claiming observational truth for Prediction vs Reference
- Requiring network access for the default laboratory
- Claiming Windows/macOS/ARM64 support before native verification
