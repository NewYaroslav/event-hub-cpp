#include "test_helpers.hpp"

#include <cassert>
#include <chrono>
#include <future>
#include <memory>
#include <stdexcept>
#include <vector>

using namespace event_hub_test;

int main() {
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
        assert(!tasks.next_deadline());

        tasks.post_after(std::chrono::milliseconds(30), [] {});
        const auto delayed_wait =
            tasks.recommend_wait_for(std::chrono::milliseconds(100));

        assert(tasks.next_deadline());
        assert(delayed_wait > event_hub::TaskManager::Duration::zero());
        assert(delayed_wait <=
               event_hub::TaskManager::Duration(std::chrono::milliseconds(100)));

        tasks.post([] {});
        assert(tasks.recommend_wait_for(std::chrono::milliseconds(10)) ==
               event_hub::TaskManager::Duration::zero());

        assert(tasks.clear_pending() == 2);
        assert(!tasks.has_pending());
    }

    return 0;
}
