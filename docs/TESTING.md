# Testing Strategy

BAGS_LAB keeps the existing scientific CTest suite. Target names may still use
the historical `bagsolar_*` prefix; that is intentional compatibility, not a
product rename gap.

## Layers

- **Unit** — physics, data, selection, planet presentation, resource root
- **Integration** — telemetry, education, curriculum, ephemeris, install/package
- **Scientific validation** — `bagsolar_validation` and related executables
- **Portability** — resource smoke from foreign working directories and USB copies

## Run

```sh
cmake --preset debug
cmake --build --preset debug --parallel 2
ctest --preset debug --output-on-failure
```

Selected smokes:

```sh
ctest --test-dir build -R 'bagsolar_resource_(root_tests|smoke)' --output-on-failure
ctest --test-dir build -R 'bagsolar_(install|package)_smoke' --output-on-failure
```

## Principles

- Do not weaken scientific assertions to pass portability work
- Prefer offline fixtures for default CI
- Optional SPICE live tests require an explicit local manifest
- GUI verification is separate from CTest and needs a display

See `docs/scientific/TESTING.md` and `docs/scientific/VALIDATION.md` for domain
detail.
