# Contributing

## Development Workflow

1. Create a feature branch from `main`.
2. Run `cmake --preset debug`.
3. Run `cmake --build --preset debug --parallel`.
4. Run `ctest --preset debug`.
5. Open a pull request with a clear summary and scope.

## Pull Request Checklist

- [ ] Build passes locally (`debug` or `release`).
- [ ] Tests pass locally.
- [ ] Changes are scoped and documented.
- [ ] New behavior is covered by tests where practical.

## Commit Guidance

- Use small, focused commits.
- Prefer imperative commit messages, for example: `Add release test preset`.
