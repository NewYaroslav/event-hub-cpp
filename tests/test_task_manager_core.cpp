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
        event_hub::TaskId seen_id = 0;
        event_hub::TaskContext saved_context;

        const auto id = tasks.post(
            [&seen_id, &saved_context](event_hub::TaskContext& self) {
                seen_id = self.id();
                saved_context = self;
            });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(seen_id == id);
        EVENT_HUB_TEST_CHECK(!saved_context.cancel());
        EVENT_HUB_TEST_CHECK(!saved_context.reschedule_after(
            std::chrono::milliseconds(1)));
        EVENT_HUB_TEST_CHECK(!saved_context.is_cancelled());
    }

    {
        event_hub::TaskManager tasks;
        std::vector<event_hub::Task> batch;
        std::vector<event_hub::TaskId> seen;

        batch.emplace_back([&seen](event_hub::TaskContext& self) {
            seen.push_back(self.id());
        });
        batch.emplace_back([&seen] {
            seen.push_back(0);
        });

        const auto ids = tasks.post_batch(std::move(batch));
        EVENT_HUB_TEST_CHECK(ids.size() == 2);
        EVENT_HUB_TEST_CHECK(ids[0] != 0);
        EVENT_HUB_TEST_CHECK(ids[1] != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 2);
        EVENT_HUB_TEST_CHECK((seen == std::vector<event_hub::TaskId>{
            ids[0], 0}));
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

        const auto id = tasks.post_after_ms(
            30,
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

        const auto id1 = tasks.post_after_ms(
            -1,
            [&value] {
                value += 1;
            });
        const auto id2 = tasks.post_at(
            event_hub::TaskManager::Clock::now() -
                std::chrono::milliseconds(1),
            [&value] {
                value += 2;
            });
        const auto id3 = tasks.post_after_ms(
            -1,
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
        std::vector<event_hub::TaskId> seen;

        const auto id1 = tasks.post_after(
            std::chrono::milliseconds(-1),
            [&seen](event_hub::TaskContext& self) {
                seen.push_back(self.id());
            });
        const auto id2 = tasks.post_after_ms(
            -1,
            [&seen](event_hub::TaskContext& self) {
                seen.push_back(self.id());
            });
        const auto id3 = tasks.post_at(
            event_hub::TaskManager::Clock::now() -
                std::chrono::milliseconds(1),
            [&seen](event_hub::TaskContext& self) {
                seen.push_back(self.id());
            });
        const auto id4 = tasks.post_every(
            std::chrono::milliseconds(100),
            [&seen](event_hub::TaskContext& self) {
                seen.push_back(self.id());
                EVENT_HUB_TEST_CHECK(self.cancel());
            });
        const auto id5 = tasks.post_every_after(
            std::chrono::milliseconds(-1),
            std::chrono::milliseconds(100),
            [&seen](event_hub::TaskContext& self) {
                seen.push_back(self.id());
                EVENT_HUB_TEST_CHECK(self.cancel());
            });
        const auto id6 = tasks.post_every_after_ms(
            -1,
            100,
            [&seen](event_hub::TaskContext& self) {
                seen.push_back(self.id());
                EVENT_HUB_TEST_CHECK(self.cancel());
            });

        EVENT_HUB_TEST_CHECK(id1 != 0);
        EVENT_HUB_TEST_CHECK(id2 != 0);
        EVENT_HUB_TEST_CHECK(id3 != 0);
        EVENT_HUB_TEST_CHECK(id4 != 0);
        EVENT_HUB_TEST_CHECK(id5 != 0);
        EVENT_HUB_TEST_CHECK(id6 != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 6);
        EVENT_HUB_TEST_CHECK((seen == std::vector<event_hub::TaskId>{
            id1, id2, id3, id4, id5, id6}));
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;
        event_hub::TaskId seen_id = 0;

        const auto id = tasks.post_every_ms(20, [&calls, &seen_id](
                                                  event_hub::TaskContext& self) {
            ++calls;
            seen_id = self.id();
        });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.ready_count() == 1);
        EVENT_HUB_TEST_CHECK(tasks.pending_count() == 1);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(seen_id == id);
        EVENT_HUB_TEST_CHECK(tasks.pending_count() == 1);
        EVENT_HUB_TEST_CHECK(tasks.next_deadline());
        EVENT_HUB_TEST_CHECK(tasks.recommend_wait_for_ms(100) >
               event_hub::TaskManager::Duration::zero());
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 2);
        EVENT_HUB_TEST_CHECK(tasks.cancel(id));
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post_every_after_ms(
            30,
            10,
            [&calls] {
                ++calls;
            });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(!tasks.has_ready());
        EVENT_HUB_TEST_CHECK(tasks.pending_count() == 1);
        EVENT_HUB_TEST_CHECK(tasks.next_deadline());
        EVENT_HUB_TEST_CHECK(tasks.recommend_wait_for_ms(100) >
               event_hub::TaskManager::Duration::zero());
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(40));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(tasks.cancel(id));
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post_every_ms(20, [&calls] {
            ++calls;
            std::this_thread::sleep_for(std::chrono::milliseconds(30));
        });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        EVENT_HUB_TEST_CHECK(tasks.cancel(id));
    }

    {
        event_hub::TaskManager tasks;
        event_hub::PeriodicTaskOptions options;
        options.schedule = event_hub::PeriodicSchedule::fixed_rate;
        int calls = 0;

        const auto id = tasks.post_every_ms(
            10,
            [&calls] {
                ++calls;
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            },
            options);

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(tasks.has_ready());
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 2);
        EVENT_HUB_TEST_CHECK(tasks.cancel(id));
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto cancel_before_start = tasks.post_every_after_ms(
            20,
            10,
            [&calls] {
                ++calls;
            });
        EVENT_HUB_TEST_CHECK(cancel_before_start != 0);
        EVENT_HUB_TEST_CHECK(tasks.cancel(cancel_before_start));
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        EVENT_HUB_TEST_CHECK(calls == 0);

        const auto cancel_between_runs = tasks.post_every_ms(10, [&calls] {
            ++calls;
        });
        EVENT_HUB_TEST_CHECK(cancel_between_runs != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(tasks.cancel(cancel_between_runs));
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        EVENT_HUB_TEST_CHECK(calls == 1);
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post_every_ms(1, [&calls](
                                                 event_hub::TaskContext& self) {
            ++calls;
            EVENT_HUB_TEST_CHECK(self.cancel());
            EVENT_HUB_TEST_CHECK(self.is_cancelled());
        });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;
        event_hub::TaskId seen_id = 0;

        const auto id = tasks.post([&calls, &seen_id](
                                       event_hub::TaskContext& self) {
            ++calls;
            seen_id = self.id();
            if (calls == 1) {
                EVENT_HUB_TEST_CHECK(self.reschedule_after(
                    std::chrono::milliseconds(5)));
            }
        });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(seen_id == id);
        EVENT_HUB_TEST_CHECK(tasks.has_pending());
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 2);
        EVENT_HUB_TEST_CHECK(seen_id == id);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
    }

    {
        event_hub::TaskManager tasks;
        std::vector<int> order;
        int rescheduled_runs = 0;

        tasks.post([&order, &rescheduled_runs](
                       event_hub::TaskContext& self) {
            order.push_back(1);
            ++rescheduled_runs;
            if (rescheduled_runs == 1) {
                EVENT_HUB_TEST_CHECK(self.reschedule_at(
                    event_hub::TaskContext::Clock::now() -
                    std::chrono::milliseconds(1)));
            }
        });
        tasks.post([&order] {
            order.push_back(2);
        });

        EVENT_HUB_TEST_CHECK(tasks.process() == 2);
        EVENT_HUB_TEST_CHECK((order == std::vector<int>{1, 2}));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK((order == std::vector<int>{1, 2, 1}));
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post_every_ms(
            20,
            [&calls](event_hub::TaskContext& self) {
                ++calls;
                if (calls == 1) {
                    EVENT_HUB_TEST_CHECK(self.reschedule_after(
                        std::chrono::milliseconds(1)));
                } else if (calls == 3) {
                    EVENT_HUB_TEST_CHECK(self.cancel());
                }
            });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);

        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 2);

        EVENT_HUB_TEST_CHECK(tasks.process() == 0);

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 3);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post([&calls](event_hub::TaskContext& self) {
            ++calls;
            EVENT_HUB_TEST_CHECK(self.reschedule_after(
                std::chrono::milliseconds(1)));
            EVENT_HUB_TEST_CHECK(self.cancel());
        });

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());

        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        EVENT_HUB_TEST_CHECK(calls == 1);
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post_every_after_ms(
            10,
            10,
            [&calls] {
                ++calls;
            });
        EVENT_HUB_TEST_CHECK(id != 0);

        tasks.close();
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EVENT_HUB_TEST_CHECK(tasks.process() == 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
        EVENT_HUB_TEST_CHECK(!tasks.cancel(id));
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto ready_id = tasks.post_every_ms(10, [&calls] {
            ++calls;
        });
        const auto delayed_id = tasks.post_every_after_ms(20, 10, [&calls] {
            ++calls;
        });

        EVENT_HUB_TEST_CHECK(ready_id != 0);
        EVENT_HUB_TEST_CHECK(delayed_id != 0);
        EVENT_HUB_TEST_CHECK(tasks.pending_count() == 2);
        EVENT_HUB_TEST_CHECK(tasks.clear_pending() == 2);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
        EVENT_HUB_TEST_CHECK(!tasks.has_ready());

        std::this_thread::sleep_for(std::chrono::milliseconds(30));
        EVENT_HUB_TEST_CHECK(tasks.process() == 0);
        EVENT_HUB_TEST_CHECK(calls == 0);
        EVENT_HUB_TEST_CHECK(!tasks.cancel(ready_id));
        EVENT_HUB_TEST_CHECK(!tasks.cancel(delayed_id));
    }

    {
        event_hub::TaskManager tasks;
        event_hub::PeriodicTaskOptions high;
        high.priority = event_hub::TaskPriority::high;
        std::vector<int> order;

        tasks.post([&order] {
            order.push_back(2);
        });
        const auto id = tasks.post_every_ms(
            10,
            [&order] {
                order.push_back(1);
            },
            high);

        EVENT_HUB_TEST_CHECK(id != 0);
        EVENT_HUB_TEST_CHECK(tasks.process(2) == 2);
        EVENT_HUB_TEST_CHECK((order == std::vector<int>{1, 2}));
        EVENT_HUB_TEST_CHECK(tasks.cancel(id));
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
