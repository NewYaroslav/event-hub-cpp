#include <event_hub.hpp>

#include <iostream>
#include <string>
#include <vector>

int main() {
    event_hub::SyncNotifier notifier;
    event_hub::TaskManager tasks;
    tasks.set_notifier(&notifier);

    bool running = true;
    std::vector<std::string> log;

    auto answer = tasks.submit([] {
        return 42;
    });

    tasks.post([&log] {
        log.push_back("normal task");
    });

    tasks.post(
        [&log] {
            log.push_back("high priority task");
        }, {
            event_hub::TaskPriority::high
    });

    tasks.post_after_ms(2, [&log] {
        log.push_back("delayed task");
    });

    tasks.post_after_ms(4, [&running] {
        running = false;
    });

    while (running || tasks.has_pending()) {
        // Capture the wake-up generation before processing. A producer may
        // notify while this loop is busy; wait_for() uses this old value so
        // that notification is not lost before the loop goes to sleep.
        const auto generation = notifier.generation();

        // Run at most 128 ready callbacks this iteration. Delayed tasks become
        // ready only after their deadline, and tasks posted by callbacks are
        // left for a later process() call.
        const auto work_done = tasks.process(128);
        if (work_done != 0) {
            continue;
        }

        // Use 1 ms as the idle cap, but wake sooner when the next delayed task
        // is due.
        const auto timeout = tasks.recommend_wait_for_ms(1);
        if (!tasks.has_ready()) {
            // Sleep until a notifier generation change or the computed timeout.
            // If notify() already changed the generation, this returns
            // immediately instead of sleeping through pending work.
            (void)notifier.wait_for(generation, timeout);
        }
    }

    std::cout << "future result: " << answer.get() << '\n';

    for (const auto& item : log) {
        std::cout << item << '\n';
    }

    tasks.reset_notifier();
}
