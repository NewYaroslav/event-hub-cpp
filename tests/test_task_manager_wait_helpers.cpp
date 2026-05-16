#include "test_helpers.hpp"

#include <chrono>

using namespace event_hub_test;

int main() {
    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post_after_ms(5, [&calls] {
            ++calls;
        });

        EVENT_HUB_TEST_CHECK(id != 0);
        wait_until_processed(tasks, 1);
        EVENT_HUB_TEST_CHECK(calls == 1);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
    }

    {
        event_hub::TaskManager tasks;
        int calls = 0;

        const auto id = tasks.post_every_after_ms(
            5,
            5,
            [&calls](event_hub::TaskContext& self) {
                ++calls;
                if (calls == 2) {
                    EVENT_HUB_TEST_CHECK(self.cancel());
                }
            });

        EVENT_HUB_TEST_CHECK(id != 0);
        wait_until_processed(tasks, 2);
        EVENT_HUB_TEST_CHECK(calls == 2);
        EVENT_HUB_TEST_CHECK(!tasks.has_pending());
    }

    return 0;
}
