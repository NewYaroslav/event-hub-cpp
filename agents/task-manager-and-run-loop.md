# TaskManager And RunLoop Guidelines

This project includes a lightweight task layer beside the typed event bus. Keep
it passive, explicit, and dependency-light.

## Why TaskManager Exists

`EventBus` is for typed module communication. `TaskManager` is for ordinary
`void()` work that should run on the same application/event-loop thread without
turning the bus into a task scheduler.

The design intentionally mirrors `EventBus`:

- producers can submit work from other threads;
- user callbacks run only when the application calls `process()`;
- no library-owned worker thread, `run()`, `join()`, or blocking destructor;
- one shared `INotifier`/`SyncNotifier` can wake an external loop for both
  events and tasks.

This keeps the library usable in UI loops, game loops, service loops, tests, and
embedded-style polling code without forcing a thread ownership model.

## TaskManager Implementation Model

Core files:

- `include/event_hub/task.hpp` defines `TaskId`, `TaskPriority`,
  `TaskOptions`, and move-only `Task`.
- `include/event_hub/task_manager.hpp` defines the passive task queue.

Important choices:

- `Task` is a local move-only type erasure wrapper because C++17
  `std::function<void()>` cannot store move-only callables such as lambdas with
  `std::unique_ptr` captures or `std::packaged_task`.
- Ready tasks use three FIFO `std::deque` queues: high, normal, low.
- Delayed tasks use a min-heap ordered by `std::chrono::steady_clock`
  deadlines and a sequence number for stable ordering.
- Cancellation is lazy: `TaskId` maps to an atomic task state, and containers
  are pruned or skipped later. Do not add eager O(n) removal as the main path.
- `TaskId` values are monotonic within one manager lifetime; `0` means invalid
  or rejected.
- `process()` takes a snapshot of ready work. Tasks posted by callbacks run on a
  later `process()` call, matching `EventBus`.
- User callbacks must never run while the queue mutex is held.
- Without an exception handler, `process()` restores unstarted work from the
  current batch and rethrows. With a handler, it reports and continues.

Do not add periodic tasks to the core. Fixed-rate, fixed-delay, catch-up, and
exception policies are application decisions. Build repeating work with
self-rescheduling delayed tasks.

## Correct Manual Loop

When manually combining buses and task managers, capture the notifier generation
before processing, then process every source, then wait on that old generation:

```cpp
while (running) {
    const auto generation = notifier.generation();

    std::size_t work_done = 0;
    work_done += bus.process();
    work_done += tasks.process(128);

    if (work_done != 0) {
        continue;
    }

    const auto timeout =
        tasks.recommend_wait_for(std::chrono::milliseconds(1));
    if (!bus.has_pending() && !tasks.has_ready()) {
        notifier.wait_for(generation, timeout);
    }
}
```

Do not read `generation()` after processing. That can lose a wakeup that arrives
between the processing phase and the actual wait.

## RunLoop Utility

`include/event_hub/run_loop.hpp` provides `RunLoop`, a small blocking helper for
applications that want a ready-made loop.

Use it when an app wants one current-thread loop over many passive sources:

```cpp
event_hub::EventBus bus;
event_hub::TaskManager tasks;
event_hub::RunLoop loop;

loop.add(bus);
loop.add(tasks);
loop.run();
```

RunLoop owns a `SyncNotifier`, registers it on every added `EventBus` and
`TaskManager`, and blocks the calling thread until `request_stop()` is called.
It does not create a thread. Registered sources are non-owning pointers; add
them before `run()`, keep them alive longer than the loop, and do not mutate the
source list concurrently with processing.

Prefer manual loops in examples that teach the underlying model. Use `RunLoop`
examples only when demonstrating the convenience helper itself.

## Usage Rules

- Attach the same notifier to all passive sources that share one loop.
- Call `reset_notifier()` before destroying a notifier manually. `RunLoop`
  handles this for sources it registered, assuming those sources still exist.
- Use `submit()` when a task result is needed through `std::future`.
- Use `post_batch()` when a producer has several immediate tasks and should
  wake the loop once.
- Keep task callbacks short; publish events or enqueue follow-up tasks for
  larger workflows.
- Treat `process()`, `clear_pending()`, `close()`, and destruction as
  single-consumer/lifetime operations.

## Testing Expectations

Task-related changes should cover:

- immediate and delayed execution;
- priority ordering and FIFO within one priority;
- snapshot semantics for tasks posted by callbacks;
- cancellation before start and rejection after start;
- exception handler and fail-fast restore behavior;
- notifier wakeups and `recommend_wait_for(...)`;
- `submit()` futures and move-only task captures;
- `RunLoop` with multiple task managers and at least one `EventBus`.
