#include <event_hub.hpp>

#include <chrono>
#include <iostream>
#include <string>

struct LogEvent {
    std::string text;
};

int main() {
    using namespace std::chrono_literals;

    event_hub::SyncNotifier notifier;

    event_hub::EventBus bus;
    bus.set_notifier(&notifier);

    event_hub::TaskManager tasks;
    tasks.set_notifier(&notifier);

    event_hub::EventEndpoint endpoint(bus);
    bool running = true;

    endpoint.subscribe<LogEvent>([&running](const LogEvent& event) {
        std::cout << "event: " << event.text << '\n';
        if (event.text == "quit") {
            running = false;
        }
    });

    tasks.post([] {
        std::cout << "task: immediate work\n";
    });
    tasks.post_after(2ms, [&endpoint] {
        endpoint.post<LogEvent>("published by delayed task");
    });
    tasks.post_after(4ms, [&endpoint] {
        endpoint.post<LogEvent>("quit");
    });

    while (running) {
        const auto generation = notifier.generation();

        std::size_t work_done = 0;
        work_done += bus.process();
        work_done += tasks.process(128);

        if (work_done != 0) {
            continue;
        }

        const auto timeout = tasks.recommend_wait_for(1ms);

        if (!bus.has_pending() && !tasks.has_ready()) {
            (void)notifier.wait_for(generation, timeout);
        }
    }

    tasks.reset_notifier();
    bus.reset_notifier();
}
