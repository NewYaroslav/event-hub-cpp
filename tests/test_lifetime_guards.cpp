#include "test_helpers.hpp"

#include <cassert>
#include <atomic>
#include <future>
#include <memory>
#include <thread>

using namespace event_hub_test;

int main() {
    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        int calls = 0;

        endpoint.subscribe<Ping>([&calls](const Ping&) {
            ++calls;
        });

        endpoint.close();
        endpoint.close();
        bus.emit<Ping>(1);

        assert(endpoint.is_closed());
        assert(calls == 0);
    }

    {
        event_hub::EventBus bus;
        auto blocker = std::make_unique<event_hub::EventEndpoint>(bus);
        auto victim = std::make_unique<event_hub::EventEndpoint>(bus);

        std::atomic_int victim_calls{0};
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        blocker->subscribe<Ping>(
            [&entered_promise, release](const Ping&) {
                entered_promise.set_value();
                release.wait();
            });

        victim->subscribe<Ping>([&victim_calls](const Ping&) {
            victim_calls.fetch_add(1, std::memory_order_relaxed);
        });

        std::thread dispatcher([&bus] {
            bus.emit<Ping>(1);
        });

        assert_ready(entered);
        victim.reset();
        release_promise.set_value();
        dispatcher.join();

        assert(victim_calls.load(std::memory_order_relaxed) == 0);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        auto user_guard = std::make_shared<int>(0);

        std::atomic_int guarded_calls{0};
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        endpoint.subscribe<Ping>(
            [&entered_promise, release](const Ping&) {
                entered_promise.set_value();
                release.wait();
            });

        endpoint.subscribe<Ping>(
            std::weak_ptr<int>(user_guard),
            [&guarded_calls](const Ping&) {
                guarded_calls.fetch_add(1, std::memory_order_relaxed);
            });

        std::thread dispatcher([&bus] {
            bus.emit<Ping>(1);
        });

        assert_ready(entered);
        user_guard.reset();
        release_promise.set_value();
        dispatcher.join();

        assert(guarded_calls.load(std::memory_order_relaxed) == 0);
    }

    {
        event_hub::EventBus bus;
        event_hub::EventEndpoint endpoint(bus);
        std::atomic_bool guard_destroyed{false};
        auto user_guard = std::make_shared<LifetimeProbe>(guard_destroyed);

        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        endpoint.subscribe<Ping>(
            std::weak_ptr<LifetimeProbe>(user_guard),
            [&entered_promise, release, &guard_destroyed](const Ping&) {
                entered_promise.set_value();
                release.wait();
                assert(!guard_destroyed.load(std::memory_order_relaxed));
            });

        std::thread dispatcher([&bus] {
            bus.emit<Ping>(1);
        });

        assert_ready(entered);
        user_guard.reset();
        assert(!guard_destroyed.load(std::memory_order_relaxed));
        release_promise.set_value();
        dispatcher.join();

        assert(guard_destroyed.load(std::memory_order_relaxed));
    }

    {
        event_hub::EventBus bus;
        std::atomic_bool module_destroyed{false};
        std::atomic_bool handled{false};
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        auto module = std::make_shared<StrictLifetimeModule>(
            bus,
            module_destroyed,
            handled);
        module->start(entered_promise, release);

        std::thread dispatcher([&bus] {
            bus.emit<Ping>(1);
        });

        assert_ready(entered);
        module.reset();
        assert(!module_destroyed.load(std::memory_order_relaxed));

        release_promise.set_value();
        dispatcher.join();

        assert(handled.load(std::memory_order_relaxed));
        assert(module_destroyed.load(std::memory_order_relaxed));
    }

    return 0;
}
