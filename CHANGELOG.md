# Changelog

All notable changes to this project will be documented in this file.

## [v0.1.0] - 2026-05-12
- Added the initial header-only C++17 event bus API with typed subscriptions,
  synchronous `emit<T>()`, queued `post<T>()`, explicit `process()`, pending
  queue inspection, and queue clearing.
- Added RAII subscription ownership through `EventEndpoint`, including endpoint
  lifetime guards, targeted unsubscribe helpers, and guarded callback dispatch.
- Added optional `Event`/`EventListener` contracts for runtime event metadata,
  safe downcasts, cloning, and listener-style dispatch.
- Added `EventNode` as a convenience base for modules that combine
  `EventListener` dispatch with an owned endpoint.
- Added cancelable event awaiters with `await_once()`, `await_each()`,
  `CancellationToken`, `CancellationSource`, timeout polling, and timeout
  callback error reporting.
- Added `Task`, `TaskManager`, task priorities, delayed one-shot tasks,
  `submit()` futures, cancellation, batch posting, and exception handling for
  explicit application-owned task processing.
- Added `INotifier`, `SyncNotifier`, and `RunLoop` for integrating event buses
  and task managers with current-thread application run loops.
- Added focused examples for basic events, event/listener usage, `EventNode`,
  awaiters and cancellation, exception handling, strict lifetime guards,
  external notifiers, task queues, task/event-bus integration, and `RunLoop`.
- Added smoke coverage for event dispatch, lifetime guards, awaiters,
  cancellation, task management, notifier behavior, run-loop processing, and
  exception policies.
- Added CMake interface target `event_hub::event_hub`, install/export metadata,
  `find_package(event-hub-cpp CONFIG REQUIRED)` support, package-name alias
  `event-hub-cpp::event-hub-cpp`, and install-consumer coverage.
- Added pkg-config generation, a vcpkg overlay port, and CI coverage for
  Windows, Linux, macOS, C++17, C++20, sanitizer checks, installed-package
  consumption, and vcpkg overlay validation.
- Added README and README-RU documentation with package-manager integration
  notes, platform/status badges, tested-platform tables, examples, threading
  model notes, and integration guidance.
