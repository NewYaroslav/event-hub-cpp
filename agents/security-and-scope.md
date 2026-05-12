# Security And Scope

`event-hub-cpp` is a general-purpose C++ event bus for decoupled modules inside
applications.

## Allowed Work

- Library features for typed events, subscriptions, queued dispatch, task
  queues, passive run loops, awaiters, and cancellation.
- Examples, tests, CMake integration, and documentation.
- Compatibility fixes for supported C++ toolchains.
- Refactors that preserve the small public API and header-first design.

## Disallowed Work

Do not add examples, tests, or documentation that encourage:

- unauthorized access;
- credential abuse;
- destructive actions;
- denial of service;
- stealth, evasion, or exfiltration workflows.

## Ambiguous Requests

When a request could be used offensively, narrow it to a benign library,
testing, or documentation workflow before proceeding.

## Data Handling

- Do not commit real secrets, credentials, or machine-local paths.
- Prefer small synthetic examples in docs and tests.
- Keep generated build artifacts out of source changes unless a task explicitly
  asks for them.
