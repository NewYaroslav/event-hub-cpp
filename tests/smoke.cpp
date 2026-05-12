#include <event_hub.hpp>

#include <cassert>
#include <atomic>
#include <chrono>
#include <functional>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

struct Ping {
    int value = 0;
};

struct Message {
    std::string text;
};

class CountingNotifier final : public event_hub::INotifier {
public:
    void notify() noexcept override {
        notifications.fetch_add(1, std::memory_order_relaxed);
    }

    std::atomic_int notifications{0};
};

struct MoveOnly {
    explicit MoveOnly(int value)
        : value(std::make_unique<int>(value)) {}

    MoveOnly(const MoveOnly&) = delete;
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&&) noexcept = default;
    MoveOnly& operator=(MoveOnly&&) noexcept = default;

    std::unique_ptr<int> value;
};

struct LifetimeProbe {
    explicit LifetimeProbe(std::atomic_bool& destroyed)
        : destroyed(&destroyed) {}

    ~LifetimeProbe() {
        destroyed->store(true, std::memory_order_relaxed);
    }

    std::atomic_bool* destroyed = nullptr;
};

class DerivedEvent final : public event_hub::Event {
public:
    explicit DerivedEvent(int value)
        : value(value) {}

    EVENT_HUB_EVENT(DerivedEvent)

    int value = 0;
};

class CountingListener final : public event_hub::EventListener {
public:
    void on_event(const event_hub::Event& event) override {
        const auto& derived = event.as_ref<DerivedEvent>();
        total += derived.value;
    }

    int total = 0;
};

struct NodeTestEvent final : event_hub::Event {
    explicit NodeTestEvent(int value_)
        : value(value_) {}

    EVENT_HUB_EVENT(NodeTestEvent)

    int value = 0;
};

class TestNode final : public event_hub::EventNode {
public:
    explicit TestNode(event_hub::EventBus& bus)
        : EventNode(bus) {}

    void start() {
        listen<NodeTestEvent>();
    }

    void on_event(const event_hub::Event& event) override {
        if (const auto* ev = event.as<NodeTestEvent>()) {
            received += ev->value;
        }
    }

    int received = 0;
};

class StrictLifetimeModule final
    : public std::enable_shared_from_this<StrictLifetimeModule> {
public:
    StrictLifetimeModule(event_hub::EventBus& bus,
                         std::atomic_bool& destroyed,
                         std::atomic_bool& handled)
        : m_endpoint(bus),
          m_destroyed(&destroyed),
          m_handled(&handled) {}

    ~StrictLifetimeModule() {
        m_destroyed->store(true, std::memory_order_relaxed);
    }

    void start(std::promise<void>& entered,
               std::shared_future<void> release) {
        auto weak = weak_from_this();

        m_endpoint.subscribe<Ping>(
            weak,
            [weak, &entered, release](const Ping&) mutable {
                auto self = weak.lock();
                if (!self) {
                    return;
                }

                entered.set_value();
                release.wait();
                assert(!self->is_destroyed());
                self->handle();
            });
    }

private:
    bool is_destroyed() const {
        return m_destroyed->load(std::memory_order_relaxed);
    }

    void handle() {
        m_handled->store(true, std::memory_order_relaxed);
    }

private:
    event_hub::EventEndpoint m_endpoint;
    std::atomic_bool* m_destroyed = nullptr;
    std::atomic_bool* m_handled = nullptr;
};

} // namespace

int main() {
    event_hub::EventBus bus;

    int sync_total = 0;
    {
        event_hub::EventEndpoint endpoint(bus);
        endpoint.subscribe<Ping>([&sync_total](const Ping& ping) {
            sync_total += ping.value;
        });

        endpoint.emit<Ping>(2);
        assert(sync_total == 2);
    }

    bus.emit<Ping>(3);
    assert(sync_total == 2);

    int move_only_total = 0;
    {
        event_hub::EventEndpoint endpoint(bus);
        endpoint.subscribe<MoveOnly>([&move_only_total](const MoveOnly& event) {
            move_only_total += *event.value;
        });

        MoveOnly event(11);
        endpoint.emit<MoveOnly>(event);
        assert(move_only_total == 11);
    }

    int async_total = 0;
    {
        event_hub::EventEndpoint endpoint(bus);
        endpoint.subscribe<Ping>([&async_total](const Ping& ping) {
            async_total += ping.value;
        });

        endpoint.post<Ping>(4);
        endpoint.post<Ping>(6);
        assert(bus.pending_count() == 2);
        assert(bus.process() == 2);
        assert(async_total == 10);
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        CountingNotifier notifier;
        int total = 0;

        endpoint.subscribe<Ping>([&total](const Ping& ping) {
            total += ping.value;
        });

        local_bus.set_notifier(&notifier);

        endpoint.emit<Ping>(1);
        assert(notifier.notifications.load(std::memory_order_relaxed) == 0);
        assert(total == 1);

        endpoint.post<Ping>(2);
        endpoint.post<Ping>(3);
        endpoint.post<Ping>(4);
        assert(notifier.notifications.load(std::memory_order_relaxed) == 3);

        local_bus.reset_notifier();
        endpoint.post<Ping>(5);
        assert(notifier.notifications.load(std::memory_order_relaxed) == 3);

        assert(local_bus.pending_count() == 4);
        assert(local_bus.process() == 4);
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
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        event_hub::SyncNotifier notifier;
        int total = 0;

        endpoint.subscribe<Ping>([&total](const Ping& ping) {
            total += ping.value;
        });
        local_bus.set_notifier(&notifier);

        const auto generation = notifier.generation();
        bool did_work = local_bus.process() > 0;
        assert(!did_work);

        endpoint.post<Ping>(6);
        assert(notifier.wait_for(generation, std::chrono::milliseconds(0)));
        assert(local_bus.process() == 1);
        assert(total == 6);
    }

    int awaited = 0;
    {
        event_hub::EventEndpoint endpoint(bus);
        endpoint.await_once<Ping>(
            [](const Ping& ping) { return ping.value == 8; },
            [&awaited](const Ping& ping) { awaited += ping.value; });

        endpoint.post<Ping>(7);
        endpoint.post<Ping>(8);
        endpoint.post<Ping>(9);
        bus.process();
        bus.process();
        assert(awaited == 8);
    }

    int each_total = 0;
    {
        event_hub::EventEndpoint endpoint(bus);
        auto awaiter = endpoint.await_each<Ping>([&each_total](const Ping& ping) {
            each_total += ping.value;
        });

        endpoint.emit<Ping>(1);
        endpoint.emit<Ping>(2);
        awaiter->cancel();
        endpoint.emit<Ping>(3);
        assert(each_total == 3);
    }

    bool cancelled_called = false;
    {
        event_hub::EventEndpoint endpoint(bus);
        event_hub::CancellationSource source;
        event_hub::AwaitOptions options;
        options.token = source.token();

        endpoint.await_once<Ping>(
            [&cancelled_called](const Ping&) { cancelled_called = true; },
            options);

        source.cancel();
        endpoint.emit<Ping>(1);
        assert(!cancelled_called);
    }

    bool timed_out = false;
    {
        event_hub::EventEndpoint endpoint(bus);
        event_hub::AwaitOptions options;
        options.timeout = std::chrono::milliseconds(1);
        options.on_timeout = [&timed_out] { timed_out = true; };

        endpoint.await_once<Message>(
            [](const Message&) {
                assert(false && "timeout awaiter should not receive messages");
            },
            options);

        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        bus.process();
        assert(timed_out);
    }

    {
        event_hub::EventEndpoint endpoint(bus);
        CountingListener listener;
        endpoint.subscribe<DerivedEvent>(listener);
        endpoint.emit<DerivedEvent>(5);
        assert(listener.total == 5);
    }

    {
        event_hub::EventBus local_bus;
        TestNode node(local_bus);

        assert(&node.bus() == &local_bus);
        assert(!node.is_closed());

        node.start();

        local_bus.post<NodeTestEvent>(3);
        local_bus.process();

        assert(node.received == 3);

        node.close();
        assert(node.is_closed());

        local_bus.post<NodeTestEvent>(5);
        local_bus.process();

        assert(node.received == 3);
    }

    {
        event_hub::EventEndpoint endpoint(bus);
        CountingListener listener;
        auto listener_guard = std::make_shared<int>(0);
        endpoint.subscribe<DerivedEvent>(
            std::weak_ptr<int>(listener_guard),
            listener);

        listener_guard.reset();
        endpoint.emit<DerivedEvent>(7);
        assert(listener.total == 0);
    }

    {
        auto blocker = std::make_unique<event_hub::EventEndpoint>(bus);
        auto victim = std::make_unique<event_hub::EventEndpoint>(bus);

        std::atomic_int victim_calls{0};
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        blocker->subscribe<Ping>([&entered_promise, release](const Ping&) mutable {
            entered_promise.set_value();
            release.wait();
        });

        victim->subscribe<Ping>([&victim_calls](const Ping&) {
            victim_calls.fetch_add(1, std::memory_order_relaxed);
        });

        std::thread dispatcher([&bus] {
            bus.emit<Ping>(1);
        });

        entered.wait();
        victim.reset();
        release_promise.set_value();
        dispatcher.join();

        assert(victim_calls.load(std::memory_order_relaxed) == 0);
    }

    {
        event_hub::EventEndpoint endpoint(bus);
        auto user_guard = std::make_shared<int>(0);

        std::atomic_int guarded_calls{0};
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        endpoint.subscribe<Ping>([&entered_promise, release](const Ping&) mutable {
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

        entered.wait();
        user_guard.reset();
        release_promise.set_value();
        dispatcher.join();

        assert(guarded_calls.load(std::memory_order_relaxed) == 0);
    }

    {
        event_hub::EventEndpoint endpoint(bus);
        std::atomic_bool guard_destroyed{false};
        auto user_guard = std::make_shared<LifetimeProbe>(guard_destroyed);

        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        endpoint.subscribe<Ping>(
            std::weak_ptr<LifetimeProbe>(user_guard),
            [&entered_promise, release, &guard_destroyed](const Ping&) mutable {
                entered_promise.set_value();
                release.wait();
                assert(!guard_destroyed.load(std::memory_order_relaxed));
            });

        std::thread dispatcher([&bus] {
            bus.emit<Ping>(1);
        });

        entered.wait();
        user_guard.reset();
        assert(!guard_destroyed.load(std::memory_order_relaxed));
        release_promise.set_value();
        dispatcher.join();

        assert(guard_destroyed.load(std::memory_order_relaxed));
    }

    {
        event_hub::EventBus local_bus;
        std::atomic_bool module_destroyed{false};
        std::atomic_bool handled{false};
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        auto module = std::make_shared<StrictLifetimeModule>(
            local_bus,
            module_destroyed,
            handled);
        module->start(entered_promise, release);

        std::thread dispatcher([&local_bus] {
            local_bus.emit<Ping>(1);
        });

        entered.wait();
        module.reset();
        assert(!module_destroyed.load(std::memory_order_relaxed));

        release_promise.set_value();
        dispatcher.join();

        assert(handled.load(std::memory_order_relaxed));
        assert(module_destroyed.load(std::memory_order_relaxed));
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);

        int exception_count = 0;
        int callback_total = 0;
        local_bus.set_exception_handler([&exception_count](std::exception_ptr exception) {
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
        assert(local_bus.process() == 2);
        assert(exception_count == 2);
        assert(callback_total == 3);
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        std::vector<int> seen;

        endpoint.subscribe<Ping>([&local_bus, &seen](const Ping& ping) {
            if (ping.value == 1) {
                local_bus.post<Ping>(4);
                throw std::runtime_error("callback failed");
            }
            seen.push_back(ping.value);
        });

        endpoint.post<Ping>(1);
        endpoint.post<Ping>(2);
        endpoint.post<Ping>(3);

        bool threw = false;
        try {
            (void)local_bus.process();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        assert(threw);
        assert(local_bus.pending_count() == 3);

        assert(local_bus.process() == 3);
        assert((seen == std::vector<int>{2, 3, 4}));
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        int exception_count = 0;

        local_bus.set_exception_handler([&exception_count](std::exception_ptr exception) {
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
        (void)local_bus.process();
        assert(exception_count == 1);
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        std::vector<std::string> messages;

        endpoint.subscribe<Message>([&messages](const Message& message) {
            messages.push_back(message.text);
        });

        const Message copied{"copy"};
        endpoint.post<Message>(copied);

        Message moved{"move"};
        endpoint.post<Message>(std::move(moved));

        endpoint.post<Message>("aggregate");

        assert(local_bus.process() == 3);
        assert((messages == std::vector<std::string>{"copy", "move", "aggregate"}));
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        int calls = 0;

        const auto id = endpoint.subscribe<Ping>([&calls](const Ping&) {
            ++calls;
        });
        endpoint.unsubscribe(id);
        endpoint.emit<Ping>(1);
        assert(calls == 0);
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        int calls = 0;

        endpoint.subscribe<Ping>([&calls](const Ping&) {
            ++calls;
        });

        endpoint.post<Ping>(1);
        endpoint.post<Ping>(2);
        assert(local_bus.pending_count() == 2);
        local_bus.clear_pending();
        assert(!local_bus.has_pending());
        assert(local_bus.process() == 0);
        assert(calls == 0);
    }

    {
        event_hub::Task empty;
        bool threw = false;
        try {
            empty();
        } catch (const std::bad_function_call&) {
            threw = true;
        }
        assert(threw);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;
        std::function<void()> empty_function;

        assert(tasks.post(empty_function) == 0);
        const auto id = tasks.post([&value] {
            ++value;
        });
        assert(id != 0);
        assert(tasks.ready_count() == 1);
        assert(tasks.pending_count() == 1);
        assert(tasks.process() == 1);
        assert(value == 1);
        assert(!tasks.has_ready());
        assert(!tasks.has_pending());
        assert(tasks.process() == 0);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        tasks.post([&tasks, &value] {
            tasks.post([&value] {
                ++value;
            });
        });

        assert(tasks.process() == 1);
        assert(value == 0);
        assert(tasks.process() == 1);
        assert(value == 1);
    }

    {
        event_hub::TaskManager tasks;
        std::vector<int> order;

        tasks.post([&order] { order.push_back(2); },
                   {event_hub::TaskPriority::normal});
        tasks.post([&order] { order.push_back(4); },
                   {event_hub::TaskPriority::low});
        tasks.post([&order] { order.push_back(0); },
                   {event_hub::TaskPriority::high});
        tasks.post([&order] { order.push_back(3); },
                   {event_hub::TaskPriority::normal});
        tasks.post([&order] { order.push_back(1); },
                   {event_hub::TaskPriority::high});

        assert(tasks.process() == 5);
        assert((order == std::vector<int>{0, 1, 2, 3, 4}));
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto id = tasks.post_after(
            std::chrono::milliseconds(30),
            [&value] {
                ++value;
            });
        assert(id != 0);

        assert(tasks.process() == 0);
        assert(value == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        assert(tasks.process() == 1);
        assert(value == 1);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto id1 = tasks.post_after(
            std::chrono::milliseconds(-1),
            [&value] {
                value += 1;
            });
        const auto id2 = tasks.post_at(
            event_hub::TaskManager::Clock::now() -
                std::chrono::milliseconds(1),
            [&value] {
                value += 2;
            });
        const auto id3 = tasks.post_after(
            std::chrono::milliseconds(-1),
            event_hub::Task([&value] {
                value += 4;
            }));

        assert(id1 != 0);
        assert(id2 != 0);
        assert(id3 != 0);
        assert(tasks.ready_count() == 3);
        assert(tasks.process() == 3);
        assert(value == 7);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto id = tasks.post([&value] {
            ++value;
        });
        assert(tasks.cancel(id));
        assert(!tasks.cancel(id));
        assert(!tasks.cancel(999999));
        assert(tasks.process() == 0);
        assert(value == 0);
    }

    {
        event_hub::TaskManager tasks;
        std::promise<void> entered_promise;
        auto entered = entered_promise.get_future();
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();

        const auto id = tasks.post([&entered_promise, release] {
            entered_promise.set_value();
            release.wait();
        });

        std::thread worker([&tasks] {
            assert(tasks.process() == 1);
        });

        entered.wait();
        assert(!tasks.cancel(id));
        release_promise.set_value();
        worker.join();
    }

    {
        event_hub::TaskManager tasks;
        CountingNotifier notifier;
        tasks.set_notifier(&notifier);

        int value = 0;
        std::vector<event_hub::Task> batch;
        batch.emplace_back([&value] {
            value += 1;
        });
        batch.emplace_back(nullptr);
        batch.emplace_back([&value] {
            value += 2;
        });

        const auto ids = tasks.post_batch(std::move(batch));
        assert(ids.size() == 3);
        assert(ids[0] != 0);
        assert(ids[1] == 0);
        assert(ids[2] != 0);
        assert(notifier.notifications.load(std::memory_order_relaxed) == 1);

        assert(tasks.process() == 2);
        assert(value == 3);
        tasks.reset_notifier();
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        auto owned = std::make_unique<int>(7);
        const auto id = tasks.post([ptr = std::move(owned), &value] {
            value = *ptr;
        });
        assert(id != 0);
        assert(tasks.process() == 1);
        assert(value == 7);

        auto future = tasks.submit([] {
            return 42;
        });
        assert(future.wait_for(std::chrono::milliseconds(0)) ==
               std::future_status::timeout);
        assert(tasks.process() == 1);
        assert(future.get() == 42);

        auto void_future = tasks.submit([&value] {
            value = 9;
        });
        assert(tasks.process() == 1);
        void_future.get();
        assert(value == 9);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto queued = tasks.post([&value] {
            ++value;
        });
        assert(queued != 0);
        tasks.close();

        assert(tasks.post([] {}) == 0);
        bool submit_threw = false;
        try {
            (void)tasks.submit([] {
                return 1;
            });
        } catch (const std::runtime_error&) {
            submit_threw = true;
        }
        assert(submit_threw);

        assert(tasks.process() == 1);
        assert(value == 1);
    }

    {
        event_hub::TaskManager tasks;
        int exception_count = 0;
        int value = 0;

        tasks.set_exception_handler(
            [&exception_count](std::exception_ptr exception) {
                try {
                    if (exception) {
                        std::rethrow_exception(exception);
                    }
                } catch (const std::runtime_error&) {
                    ++exception_count;
                }
            });

        tasks.post([] {
            throw std::runtime_error("task failed");
        });
        tasks.post([&value] {
            value = 5;
        });

        assert(tasks.process() == 2);
        assert(exception_count == 1);
        assert(value == 5);
    }

    {
        event_hub::TaskManager tasks;
        std::vector<int> seen;

        tasks.post([&tasks, &seen] {
            seen.push_back(1);
            tasks.post([&seen] {
                seen.push_back(4);
            });
            throw std::runtime_error("task failed");
        });
        tasks.post([&seen] {
            seen.push_back(2);
        });
        tasks.post([&seen] {
            seen.push_back(3);
        });

        bool threw = false;
        try {
            (void)tasks.process();
        } catch (const std::runtime_error&) {
            threw = true;
        }

        assert(threw);
        assert(tasks.pending_count() == 3);
        assert(tasks.ready_count() == 3);
        assert(tasks.process() == 3);
        assert((seen == std::vector<int>{1, 2, 3, 4}));
    }

    {
        event_hub::SyncNotifier notifier;
        event_hub::TaskManager tasks(&notifier);

        const auto generation = notifier.generation();
        const auto id = tasks.post([] {});
        assert(id != 0);
        assert(notifier.wait_for(generation, std::chrono::milliseconds(0)));
        assert(tasks.process() == 1);
        tasks.reset_notifier();
    }

    {
        event_hub::TaskManager tasks;

        assert(tasks.recommend_wait_for(std::chrono::milliseconds(10)) ==
               event_hub::TaskManager::Duration(std::chrono::milliseconds(10)));

        tasks.post_after(std::chrono::milliseconds(30), [] {});
        const auto delayed_wait =
            tasks.recommend_wait_for(std::chrono::milliseconds(100));
        assert(delayed_wait > event_hub::TaskManager::Duration::zero());
        assert(delayed_wait <=
               event_hub::TaskManager::Duration(std::chrono::milliseconds(100)));

        tasks.post([] {});
        assert(tasks.recommend_wait_for(std::chrono::milliseconds(10)) ==
               event_hub::TaskManager::Duration::zero());

        assert(tasks.clear_pending() == 2);
        assert(!tasks.has_pending());
    }

    {
        event_hub::EventBus local_bus;
        event_hub::EventEndpoint endpoint(local_bus);
        event_hub::TaskManager first_tasks;
        event_hub::TaskManager second_tasks;
        event_hub::RunLoop loop;
        std::vector<std::string> messages;

        endpoint.subscribe<Message>([&messages, &loop](const Message& message) {
            messages.push_back(message.text);
            if (message.text == "second") {
                loop.request_stop();
            }
        });

        loop.add(local_bus);
        loop.add(first_tasks);
        loop.add(second_tasks);

        first_tasks.post([&endpoint] {
            endpoint.post<Message>("first");
        });
        second_tasks.post_after(std::chrono::milliseconds(2), [&endpoint] {
            endpoint.post<Message>("second");
        });

        loop.run();

        assert(loop.stop_requested());
        assert((messages == std::vector<std::string>{"first", "second"}));
    }

    return 0;
}
