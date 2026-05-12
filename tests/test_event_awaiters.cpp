#include "test_helpers.hpp"

#include <cassert>
#include <chrono>
#include <stdexcept>
#include <thread>

using namespace event_hub_test;

int main() {
    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int awaited = 0;

        endpoint.await_once<Ping>(
            [](const Ping& ping) { return ping.value == 8; },
            [&awaited](const Ping& ping) { awaited += ping.value; });

        endpoint.post<Ping>(7);
        endpoint.post<Ping>(8);
        endpoint.post<Ping>(9);

        assert(bus.process() == 3);
        assert(awaited == 8);

        endpoint.emit<Ping>(8);
        assert(awaited == 8);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int even_total = 0;

        auto awaiter = endpoint.await_each<Ping>(
            [](const Ping& ping) { return ping.value % 2 == 0; },
            [&even_total](const Ping& ping) {
                even_total += ping.value;
            });

        endpoint.emit<Ping>(1);
        endpoint.emit<Ping>(2);
        endpoint.emit<Ping>(4);
        awaiter->cancel();
        endpoint.emit<Ping>(6);

        assert(even_total == 6);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        bool cancelled_called = false;
        event_hub::CancellationSource source;
        event_hub::AwaitOptions options;
        options.token = source.token();

        auto awaiter = endpoint.await_once<Ping>(
            [&cancelled_called](const Ping&) {
                cancelled_called = true;
            },
            options);

        source.cancel();
        assert(options.token.is_cancelled());

        source.reset();
        assert(!source.token().is_cancelled());
        assert(options.token.is_cancelled());

        endpoint.emit<Ping>(1);

        assert(!cancelled_called);
        assert(!awaiter->is_active());
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        bool timed_out = false;
        event_hub::AwaitOptions options;
        options.timeout = std::chrono::milliseconds(1);
        options.on_timeout = [&timed_out] {
            timed_out = true;
        };

        auto awaiter = endpoint.await_once<Message>(
            [](const Message&) {
                assert(false && "timeout awaiter should not receive messages");
            },
            options);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        assert(bus.process() == 0);
        assert(timed_out);
        assert(!awaiter->is_active());
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int calls = 0;

        auto awaiter = endpoint.await_each<Ping>([&calls](const Ping&) {
            ++calls;
        });

        endpoint.close();
        endpoint.close();

        assert(endpoint.is_closed());
        assert(!awaiter->is_active());

        bus.emit<Ping>(1);
        assert(calls == 0);
    }

    {
        event_hub::EventBus bus;
        int direct_total = 0;

        auto awaiter = event_hub::EventAwaiter<Ping>::create(
            bus,
            [](const Ping& ping) { return ping.value > 0; },
            [&direct_total](const Ping& ping) {
                direct_total += ping.value;
            },
            event_hub::AwaitOptions{},
            true);

        bus.emit<Ping>(5);
        bus.emit<Ping>(6);

        assert(direct_total == 5);
        assert(!awaiter->is_active());
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int exception_count = 0;

        bus.set_exception_handler([&exception_count](std::exception_ptr exception) {
            try {
                if (exception) {
                    std::rethrow_exception(exception);
                }
            } catch (const std::runtime_error&) {
                ++exception_count;
            }
        });

        event_hub::AwaitOptions options;
        options.timeout = std::chrono::milliseconds(1);
        options.on_timeout = [] {
            throw std::runtime_error("timeout failed");
        };

        endpoint.await_once<Message>(
            [](const Message&) {
                assert(false && "timeout awaiter should not receive messages");
            },
            options);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        assert(bus.process() == 0);
        assert(exception_count == 1);
    }

    return 0;
}
