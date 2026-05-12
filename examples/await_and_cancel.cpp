#include <event_hub.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

struct JobEvent {
    std::string id;
    int progress = 0;
};

struct TimeoutEvent {
    std::string name;
};

int main() {
    event_hub::EventBus bus;
    event_hub::EventEndpoint endpoint(bus);

    endpoint.await_once<JobEvent>(
        [](const JobEvent& event) {
            return event.progress == 100;
        },
        [](const JobEvent& event) {
            std::cout << "job finished: " << event.id << '\n';
        });

    auto progress_stream = endpoint.await_each<JobEvent>(
        [](const JobEvent& event) {
            std::cout << "progress: " << event.id << ' ' << event.progress << '\n';
        });

    endpoint.post<JobEvent>("compile", 10);
    endpoint.post<JobEvent>("compile", 100);
    bus.process();

    progress_stream->cancel();
    endpoint.emit<JobEvent>("compile", 200);

    event_hub::CancellationSource source;
    event_hub::AwaitOptions cancelled_options;
    cancelled_options.token = source.token();
    endpoint.await_once<JobEvent>(
        [](const JobEvent&) {
            std::cout << "this line is cancelled\n";
        },
        cancelled_options);
    source.cancel();
    endpoint.emit<JobEvent>("compile", 300);

    event_hub::AwaitOptions timeout_options;
    timeout_options.timeout = std::chrono::milliseconds(1);
    timeout_options.on_timeout = [] {
        std::cout << "timeout fired\n";
    };
    endpoint.await_once<TimeoutEvent>(
        [](const TimeoutEvent&) {
            std::cout << "this line should not print\n";
        },
        timeout_options);

    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    bus.process();
}
