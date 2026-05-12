#include "test_helpers.hpp"

#include <atomic>
#include <thread>
#include <vector>

using namespace event_hub_test;

int main() {
    {
        event_hub::EventBus bus;
        int sync_total = 0;

        {
            event_hub::EventEndpoint endpoint(bus);
            endpoint.subscribe<Ping>([&sync_total](const Ping& ping) {
                sync_total += ping.value;
            });

            endpoint.emit<Ping>(2);
            EVENT_HUB_TEST_CHECK(sync_total == 2);
        }

        bus.emit<Ping>(3);
        EVENT_HUB_TEST_CHECK(sync_total == 2);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int ping_calls = 0;
        int message_calls = 0;

        endpoint.subscribe<Ping>([&ping_calls](const Ping&) {
            ++ping_calls;
        });
        endpoint.subscribe<Message>([&message_calls](const Message&) {
            ++message_calls;
        });

        endpoint.unsubscribe<Ping>();
        endpoint.emit<Ping>(1);
        endpoint.emit<Message>("still subscribed");

        EVENT_HUB_TEST_CHECK(ping_calls == 0);
        EVENT_HUB_TEST_CHECK(message_calls == 1);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int calls = 0;

        const auto id = endpoint.subscribe<Ping>([&calls](const Ping&) {
            ++calls;
        });

        endpoint.unsubscribe(id);
        endpoint.emit<Ping>(1);
        EVENT_HUB_TEST_CHECK(calls == 0);

        endpoint.subscribe<Ping>([&calls](const Ping&) {
            ++calls;
        });
        endpoint.subscribe<Message>([&calls](const Message&) {
            ++calls;
        });

        endpoint.unsubscribe_all();
        endpoint.emit<Ping>(1);
        endpoint.emit<Message>("ignored");
        EVENT_HUB_TEST_CHECK(calls == 0);
    }

    {
        event_hub::EventBus bus;
        int total = 0;
        int owner = 0;

        const auto id = bus.subscribe<Ping>(&owner, [&total](const Ping& ping) {
            total += ping.value;
        });

        bus.emit<Ping>(3);
        EVENT_HUB_TEST_CHECK(total == 3);

        bus.unsubscribe(id);
        bus.emit<Ping>(4);
        EVENT_HUB_TEST_CHECK(total == 3);

        bus.subscribe<Ping>(&owner, [&total](const Ping& ping) {
            total += ping.value;
        });
        bus.unsubscribe_all(&owner);
        bus.emit<Ping>(5);
        EVENT_HUB_TEST_CHECK(total == 3);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int move_only_total = 0;

        endpoint.subscribe<MoveOnly>(
            [&move_only_total](const MoveOnly& event) {
                move_only_total += *event.value;
            });

        MoveOnly event(11);
        endpoint.emit<MoveOnly>(event);
        EVENT_HUB_TEST_CHECK(move_only_total == 11);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int async_total = 0;

        endpoint.subscribe<Ping>([&async_total](const Ping& ping) {
            async_total += ping.value;
        });

        endpoint.post<Ping>(4);
        endpoint.post<Ping>(6);

        EVENT_HUB_TEST_CHECK(bus.pending_count() == 2);
        EVENT_HUB_TEST_CHECK(bus.process() == 2);
        EVENT_HUB_TEST_CHECK(async_total == 10);
        EVENT_HUB_TEST_CHECK(!bus.has_pending());
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        std::vector<std::string> messages;

        endpoint.subscribe<Message>([&messages](const Message& message) {
            messages.push_back(message.text);
        });

        const Message copied{"copy"};
        endpoint.post<Message>(copied);

        Message moved{"move"};
        endpoint.post<Message>(std::move(moved));

        endpoint.post<Message>("aggregate");

        EVENT_HUB_TEST_CHECK(bus.process() == 3);
        EVENT_HUB_TEST_CHECK((messages == std::vector<std::string>{"copy", "move", "aggregate"}));
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        std::vector<int> seen;

        endpoint.subscribe<Ping>([&endpoint, &seen](const Ping& ping) {
            seen.push_back(ping.value);
            if (ping.value == 1) {
                endpoint.post<Ping>(2);
            }
        });

        endpoint.post<Ping>(1);

        EVENT_HUB_TEST_CHECK(bus.process() == 1);
        EVENT_HUB_TEST_CHECK((seen == std::vector<int>{1}));
        EVENT_HUB_TEST_CHECK(bus.pending_count() == 1);

        EVENT_HUB_TEST_CHECK(bus.process() == 1);
        EVENT_HUB_TEST_CHECK((seen == std::vector<int>{1, 2}));
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int calls = 0;

        endpoint.subscribe<Ping>([&calls](const Ping&) {
            ++calls;
        });

        endpoint.post<Ping>(1);
        endpoint.post<Ping>(2);
        EVENT_HUB_TEST_CHECK(bus.pending_count() == 2);

        bus.clear_pending();
        EVENT_HUB_TEST_CHECK(!bus.has_pending());
        EVENT_HUB_TEST_CHECK(bus.process() == 0);
        EVENT_HUB_TEST_CHECK(calls == 0);

        endpoint.post<Ping>(3);
        bus.clear();
        EVENT_HUB_TEST_CHECK(!bus.has_pending());
        EVENT_HUB_TEST_CHECK(bus.process() == 0);
        EVENT_HUB_TEST_CHECK(calls == 0);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        std::atomic_int total{0};

        endpoint.subscribe<Ping>([&total](const Ping& ping) {
            total.fetch_add(ping.value, std::memory_order_relaxed);
        });

        std::thread producer([&endpoint] {
            endpoint.post<Ping>(7);
        });

        producer.join();
        EVENT_HUB_TEST_CHECK(bus.process() == 1);
        EVENT_HUB_TEST_CHECK(total.load(std::memory_order_relaxed) == 7);
    }

    return 0;
}
