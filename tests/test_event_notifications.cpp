#include "test_helpers.hpp"

#include <cassert>
#include <chrono>

using namespace event_hub_test;

int main() {
    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        CountingNotifier notifier;
        int total = 0;

        endpoint.subscribe<Ping>([&total](const Ping& ping) {
            total += ping.value;
        });

        bus.set_notifier(&notifier);

        endpoint.emit<Ping>(1);
        assert(notifier.notifications.load(std::memory_order_relaxed) == 0);
        assert(total == 1);

        endpoint.post<Ping>(2);
        endpoint.post<Ping>(3);
        endpoint.post<Ping>(4);
        assert(notifier.notifications.load(std::memory_order_relaxed) == 3);

        bus.reset_notifier();
        endpoint.post<Ping>(5);
        assert(notifier.notifications.load(std::memory_order_relaxed) == 3);

        assert(bus.pending_count() == 4);
        assert(bus.process() == 4);
        assert(total == 15);
    }

    {
        event_hub::SyncNotifier notifier;
        const auto generation = notifier.generation();

        notifier.notify();

        assert(notifier.generation() == generation + 1);
        assert(notifier.wait_for(generation, std::chrono::milliseconds(0)));
        assert(!notifier.wait_for(notifier.generation(),
                                  std::chrono::milliseconds(0)));
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        event_hub::SyncNotifier notifier;
        int total = 0;

        endpoint.subscribe<Ping>([&total](const Ping& ping) {
            total += ping.value;
        });
        bus.set_notifier(&notifier);

        const auto generation = notifier.generation();
        const bool did_work = bus.process() > 0;
        assert(!did_work);

        endpoint.post<Ping>(6);
        assert(notifier.wait_for(generation, std::chrono::milliseconds(0)));
        assert(bus.process() == 1);
        assert(total == 6);
    }

    return 0;
}
