#include "test_helpers.hpp"

#include <cassert>
#include <chrono>
#include <functional>
#include <future>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace event_hub_test;

int main() {
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
        int calls = 0;
        event_hub::Task task([&calls] {
            ++calls;
        });

        assert(task);
        task();
        assert(calls == 1);

        task.reset();
        assert(!task);
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

        tasks.post([&order] { order.push_back(1); });
        tasks.post([&order] { order.push_back(2); });
        tasks.post([&order] { order.push_back(3); });

        assert(tasks.process(2) == 2);
        assert((order == std::vector<int>{1, 2}));
        assert(tasks.pending_count() == 1);
        assert(tasks.ready_count() == 1);

        assert(tasks.process(2) == 1);
        assert((order == std::vector<int>{1, 2, 3}));
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

        assert_ready(entered);
        assert(!tasks.cancel(id));
        release_promise.set_value();
        worker.join();
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        std::thread producer([&tasks, &value] {
            tasks.post([&value] {
                value = 5;
            });
        });

        producer.join();
        assert(tasks.process() == 1);
        assert(value == 5);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto queued = tasks.post([&value] {
            ++value;
        });
        assert(queued != 0);

        tasks.close();
        assert(tasks.is_closed());
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

    return 0;
}
