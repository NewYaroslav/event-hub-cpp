#include "test_helpers.hpp"

#include <cassert>
#include <stdexcept>
#include <vector>

using namespace event_hub_test;

int main() {
    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int callback_total = 0;

        endpoint.subscribe<Ping>([](const Ping&) {
            throw std::runtime_error("callback failed");
        });
        endpoint.subscribe<Ping>([&callback_total](const Ping& ping) {
            callback_total += ping.value;
        });

        bool threw = false;
        try {
            endpoint.emit<Ping>(1);
        } catch (const std::runtime_error&) {
            threw = true;
        }

        assert(threw);
        assert(callback_total == 0);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);

        int exception_count = 0;
        int callback_total = 0;
        bus.set_exception_handler([&exception_count](std::exception_ptr exception) {
            try {
                if (exception) {
                    std::rethrow_exception(exception);
                }
            } catch (const std::runtime_error&) {
                ++exception_count;
            }
        });

        endpoint.subscribe<Ping>([](const Ping&) {
            throw std::runtime_error("callback failed");
        });
        endpoint.subscribe<Ping>([&callback_total](const Ping& ping) {
            callback_total += ping.value;
        });

        endpoint.post<Ping>(1);
        endpoint.post<Ping>(2);

        assert(bus.process() == 2);
        assert(exception_count == 2);
        assert(callback_total == 3);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        std::vector<int> seen;

        endpoint.subscribe<Ping>([&bus, &seen](const Ping& ping) {
            if (ping.value == 1) {
                bus.post<Ping>(4);
                throw std::runtime_error("callback failed");
            }
            seen.push_back(ping.value);
        });

        endpoint.post<Ping>(1);
        endpoint.post<Ping>(2);
        endpoint.post<Ping>(3);

        bool threw = false;
        try {
            (void)bus.process();
        } catch (const std::runtime_error&) {
            threw = true;
        }

        assert(threw);
        assert(bus.pending_count() == 3);

        assert(bus.process() == 3);
        assert((seen == std::vector<int>{2, 3, 4}));
    }

    return 0;
}
