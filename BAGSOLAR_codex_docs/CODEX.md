# Codex Development Contract

## Mission
Implement BAGSOLAR incrementally according to PROJECT.md, ARCHITECTURE.md and ROADMAP.md.

## Mandatory Rules
1. Inspect the existing code before changing it.
2. Preserve working behavior unless a roadmap item explicitly changes it.
3. Do not rewrite the whole project unnecessarily.
4. Keep physics independent from raylib rendering.
5. Avoid magic numbers; use named constants/configuration.
6. Prefer small, testable classes and functions.
7. Do not add a dependency without documenting why it is needed.
8. Check the dependency license before integration.
9. Do not copy substantial code from external projects.
10. Prefer interfaces/adapters around external services.
11. Network APIs must have graceful offline behavior.
12. Never silently change scientific units.
13. Document reference frames and epochs.
14. Add tests with every new physics feature.
15. Run the build and tests after meaningful changes.
16. Fix warnings introduced by the current task.
17. Keep public APIs documented where behavior is non-obvious.
18. Do not delete existing functionality merely to make compilation easier.

SPICE-specific rule: CSPICE and kernel files are optional external resources.
The default build must remain offline and must return
`PROVIDER_UNAVAILABLE` rather than fabricate astronomical states when CSPICE
is absent.

SPICE verification rule: a compile-only or disabled-provider test is not real
SPICE verification. Phase 8 can be marked complete only after a CSPICE-linked
build loads user-provided kernels and extracts at least one real state.

## Implementation Order
When beginning work:
1. Inspect repository.
2. Read all root markdown specifications.
3. Read relevant docs.
4. Identify current architecture.
5. Make the smallest safe change.
6. Build.
7. Test.
8. Update documentation.
9. Report files changed and verification performed.

## Definition of Done
A feature is complete only when:
- It compiles.
- Relevant tests pass.
- Existing behavior remains intact unless intentionally changed.
- Errors are handled.
- Documentation is updated.
- No unexplained warnings remain.
- The architecture remains consistent.

## Stop Conditions
Stop and ask for direction when:
- A major API redesign is required that is not covered by the roadmap.
- An external dependency introduces a license conflict.
- Scientific assumptions cannot be determined safely.
- Existing behavior conflicts with the specification in a way that requires a product decision.
