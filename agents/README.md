# agents/

Split AI-agent instruction files for `event-hub-cpp`.

## Reading Order

1. `security-and-scope.md` for project boundaries.
2. `codebase-orientation.md` for the library model and expected layout.
3. `task-manager-and-run-loop.md` for task queues and passive loops.
4. `modules-and-module-hub.md` for module orchestration and lifecycle.
5. The task-specific playbook for build, C++, headers, dependencies, or
   commits.

## Files

- `security-and-scope.md` - repository scope, data handling, and safe outputs.
- `codebase-orientation.md` - event-hub model, public API, and source layout.
- `task-manager-and-run-loop.md` - passive task queues, notifiers, and loops.
- `modules-and-module-hub.md` - module lifecycle, execution modes, and
  ModuleHub architecture.
- `dependencies.md` - dependency policy for a small header-first library.
- `build-and-test.md` - CMake configure/build/test guidance.
- `cpp-development-guidelines.md` - C++ style, comments, and API boundaries.
- `header-implementation-guidelines.md` - ownership rules for `.hpp` files.
- `commit-conventions.md` - Conventional Commit format and grouping rules.

## Maintenance

- Keep each file focused on one concern.
- Update the canonical topic file instead of copying rules into `AGENTS.md`.
- Do not add tool-specific rules unless they are genuinely different for that
  tool.
