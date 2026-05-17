# Agent Instructions

This repository contains `event-hub-cpp`, a lightweight C++ event bus for
modular applications.

## Reading Order

1. `agents/security-and-scope.md` for project boundaries and safe handling.
2. `agents/codebase-orientation.md` for the library model and layout.
3. `agents/task-manager-and-run-loop.md` for task scheduling or event-loop
   work.
4. `agents/modules-and-module-hub.md` for module orchestration work.
5. `agents/request-response-and-reply.md` for request/result workflows and
   callback-in-event shortcuts.
6. `agents/project-structure.md` for recommended layouts of modules,
   standalone repositories, DTOs, events, providers, adapters, and `module.hpp`
   entry points.
7. The task-specific playbook for C++, headers, dependencies, build/test, or
   commits.

## Project Rules

- Keep the public library under `include/event_hub/`.
- Keep examples in `examples/` and tests in `tests/`.
- Preserve the library as dependency-light C++ unless a task explicitly asks
  for integration with another package.
- Prefer clear value-type event contracts and RAII subscription ownership.
- Use Doxygen comments for public APIs and non-obvious contracts.
- Use Conventional Commits for commit messages.

## Build And Test

Use CMake for verification:

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build
ctest --test-dir build --output-on-failure
```

If the requested generator is unavailable, use another local CMake generator and
state that in the task result.

## Reference Material

`temp/agents/` and `temp/pubsub/` are reference inputs for this repository.
Do not treat them as the canonical source after the adapted files exist under
`agents/` and `include/event_hub/`.
`temp/deep-research-report.md` and `temp/BaseTradingPlatform.hpp` may explain
the module-layer motivation, but `agents/modules-and-module-hub.md` is the
canonical instruction file for current module architecture.
