# Contributing to BAGS_LAB

## Principles

1. Inspect existing code before changing it.
2. Preserve scientific units, equations, and validation strength.
3. Prefer small, testable changes.
4. Keep physics independent of raylib.
5. Do not store credentials or secrets in the repository.

## Workflow

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```

Commit locally with clear messages. Configure a remote only when you intend to
publish. Authentication stays outside the project.

## Documentation

- User/developer entry: `README.md`
- Architecture: `ARCHITECTURE.md`
- Scientific docs: `docs/scientific/`
- Historical phase notes: `docs/history/`
