#include "test_helpers.hpp"

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

        EVENT_HUB_TEST_CHECK(threw);
    }

    {
        int calls = 0;
        event_hub::Task task([&calls] {
            ++calls;
        });

        EVENT_HUB_TEST_CHECK(task);
        task();
        EVENT_HUB_TEST_CHECK(calls == 1);

        task.reset();
        EVENT_HUB_TEST_CHECK(!task);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;
        std::function<void()> empty_function;

        EVENT_HUB_TEST_CHECK(tasks.post(empty_function) == 0);

        const auto id = tasks.post([&value] {
            ++value;
        });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.ready_count() == 1);
        EVENT_HUB_TEST_CHECK(tasks.pending_count() == 1);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(value == 1);
        EVENT_HUB_TEST_CHECK(!tasks.has_ready());
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        tasks.post([&tasks, &value] {
            tasks.post([&value] {
                ++value;
            });
        });

        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(value == 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(value == 1);
    }

    {
        event_hub::TaskManager tasks;
        std::vector<int> order;

        tasks.post([&order] { order.push_back(1); });
        tasks.post([&order] { order.push_back(2); });
        tasks.post([&order] { order.push_back(3); });

        EVENT_HUB_TEST_CHECK(tasks.process(2) == 2);
        EVENT_HUB_TEST_CHECK((order == std::vector<int>{1, 2}));
        EVENT_HUB_TEST_CHECK(tasks.pending_count() == 1);
        EVENT_HUB_TEST_CHECK(tasks.ready_count() == 1);

        EVENT_HUB_TEST_CHECK(tasks.process(2) == 1);
        EVENT_HUB_TEST_CHECK((order == std::vector<int>{1, 2, 3}));
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

        EVENT_HUB_TEST_CHECK(tasks.process() == 5);
        EVENT_HUB_TEST_CHECK((order == std::vector<int>{0, 1, 2, 3, 4}));
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto id = tasks.post_after(
            std::chrono::milliseconds(30),
            [&value] {
                ++value;
            });
        EVENT_HUB_TEST_CHECK(id != 0);

        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        EVENT_HUB_TEST_CHECK(value == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(value == 1);
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

        EVENT_HUB_TEST_CHECK(id1 != 0);
        EVENT_HUB_TEST_CHECK(id2 != 0);
        EVENT_HUB_TEST_CHECK(id3 != 0);
        EVENT_HUB_TEST_CHECK(tasks.ready_count() == 3);
        EVENT_HUB_TEST_CHECK(tasks.process() == 3);
        EVENT_HUB_TEST_CHECK(value == 7);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto id = tasks.post([&value] {
            ++value;
        });

        EVENT_HUB_TEST_CHECK(tasks.cancel(id));
        EVENT_HUB_TEST_CHECK(!tasks.cancel(id));
        EVENT_HUB_TEST_CHECK(!tasks.cancel(999999));
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        EVENT_HUB_TEST_CHECK(value == 0);
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
            EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        });

        require_ready(entered);
        EVENT_HUB_TEST_CHECK(!tasks.cancel(id));
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
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(value == 5);
    }

    {
        event_hub::TaskManager tasks;
        int value = 0;

        const auto queued = tasks.post([&value] {
            ++value;
        });
        EVENT_HUB_TEST_CHECK(queued != 0);

        tasks.close();
        EVENT_HUB_TEST_CHECK(tasks.is_closed());
        EVENT_HUB_TEST_CHECK(tasks.post([] {}) == 0);

        bool submit_threw = false;
        try {
            (void)tasks.submit([] {
                return 1;
            });
        } catch (const std::runtime_error&) {
            submit_threw = true;
        }

        EVENT_HUB_TEST_CHECK(submit_threw);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(value == 1);
    }

    return 0;
}
