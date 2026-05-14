# Dependencies

## Policy

- Keep `event-hub-cpp` dependency-light.
- Do not introduce external dependencies for core event dispatch, awaiters, or
  cancellation.
- If an external dependency becomes necessary, place it under `external/` as a
  flat submodule and document why it is required.
- CMake should consume local dependencies from `external/` during normal builds,
  not download them on demand.
- Do not edit submodules unless the task explicitly asks for dependency work.
- `time-shield-cpp` is the approved optional dependency for calendar-time
  scheduling utilities. Keep it under `external/time-shield-cpp`, consume it
  only when `EVENT_HUB_CPP_USE_TIME_SHIELD=ON`, and leave NTP disabled unless
  `EVENT_HUB_CPP_USE_TIME_SHIELD_NTP=ON`. The only public layer that should use
  it directly is `CalendarScheduler`; `TaskManager` must remain a steady-clock
  executor.

## Standard Library Baseline

The public library currently targets C++17 and uses only the standard library:

- `<functional>` for callbacks;
- `<typeindex>` for type-keyed subscriber maps;
- `<mutex>` and `<queue>` for queued dispatch;
- `<memory>` for awaiter lifetime and optional event cloning;
- `<chrono>` and `<atomic>` for timeout and cancellation support.

## Compatibility

Preserve Windows MinGW compatibility when changing build scripts or public
headers. Avoid compiler-specific extensions in the library API.
