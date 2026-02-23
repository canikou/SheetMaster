# Contributing

## Development Workflow

1. Create a feature branch from `main`.
2. Run `cmake --preset debug`.
3. Run `cmake --build --preset debug --parallel`.
4. Run `ctest --preset debug`.
5. Open a pull request with a clear summary and scope.

## CI Parity Check

If your change touches build logic, presets, CI, or dependencies, mirror CI locally:

1. Run `cmake --preset ci-debug`.
2. Run `cmake --build --preset ci-debug --parallel`.
3. Run `ctest --preset ci-debug`.
4. Run `cmake --preset ci-release`.
5. Run `cmake --build --preset ci-release --parallel`.

## Lint and Format

Run before opening a PR when possible:

1. Run `cmake --build --preset debug --target format-check`.
2. Run `cmake --build --preset debug --target lint`.

## Pull Request Checklist

- [ ] Build passes locally (`debug` or `release`).
- [ ] Tests pass locally.
- [ ] If build/CI files changed, CI presets (`ci-debug`/`ci-release`) were validated.
- [ ] If code style or static-analysis touched, `format-check` and `lint` were run.
- [ ] Changes are scoped and documented.
- [ ] New behavior is covered by tests where practical.

## Commit Guidance

- Use small, focused commits.
- Prefer imperative commit messages, for example: `Add release test preset`.
