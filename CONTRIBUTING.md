# Contributing

Thank you for helping improve BAGS_LAB.

## Development workflow

1. Create a feature branch from the current branch.
2. Keep changes focused and add or update tests for behavioral changes.
3. Run the project test suite with `cmake --build build --parallel` and `ctest --test-dir build --output-on-failure`.
4. Open a pull request with a concise summary of the change and validation performed.

## Coding expectations

- Prefer small, well-scoped changes.
- Keep simulation data assets and package metadata in sync when updating the default scenario or install layout.
- Preserve the project's scientific and educational validation checks.
