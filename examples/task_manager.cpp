#include <event_hub.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace std::chrono_literals;

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
        },
        {event_hub::TaskPriority::high});
    tasks.post_after(2ms, [&log] {
        log.push_back("delayed task");
    });
    tasks.post_after(4ms, [&running] {
        running = false;
    });

    while (running || tasks.has_pending()) {
        const auto generation = notifier.generation();

        const auto work_done = tasks.process(128);
        if (work_done != 0) {
            continue;
        }

        const auto timeout = tasks.recommend_wait_for(1ms);
        if (!tasks.has_ready()) {
            (void)notifier.wait_for(generation, timeout);
        }
    }

    std::cout << "future result: " << answer.get() << '\n';
    for (const auto& item : log) {
        std::cout << item << '\n';
    }

    tasks.reset_notifier();
}
