#include <event_hub.hpp>

#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

int main() {
    event_hub::SyncNotifier notifier;
    event_hub::TaskManager tasks(&notifier);

    std::vector<std::string> log;
    bool running = true;

    tasks.set_exception_handler([&log](std::exception_ptr error) {
        try {
            if (error) {
                std::rethrow_exception(error);
            }
        } catch (const std::exception& ex) {
            log.push_back(std::string("handled exception: ") + ex.what());
        }
    });

    int heartbeat_count = 0;
    const auto heartbeat = tasks.post_every_ms(
        5,
        [&log, &heartbeat_count](event_hub::TaskContext& self) {
            ++heartbeat_count;
            log.push_back("fixed-delay heartbeat " +
                          std::to_string(heartbeat_count));

            if (heartbeat_count == 3) {
                self.cancel();
                log.push_back("fixed-delay heartbeat cancelled");
            }
        });

    event_hub::PeriodicTaskOptions fixed_rate;
    fixed_rate.priority = event_hub::TaskPriority::high;
    fixed_rate.schedule = event_hub::PeriodicSchedule::fixed_rate;

    int metrics_count = 0;
    const auto metrics = tasks.post_every_after_ms(
        2,
        4,
        [&log, &metrics_count](event_hub::TaskContext& self) {
            ++metrics_count;
            log.push_back("fixed-rate metrics " +
                          std::to_string(metrics_count));

            // This is intentionally longer than the period. Fixed-rate
            // scheduling notices the missed deadline and makes the next cycle
            // ready for the following process() pass.
            std::this_thread::sleep_for(std::chrono::milliseconds(6));

            if (metrics_count == 3) {
                self.cancel();
                log.push_back("fixed-rate metrics cancelled");
            }
        },
        fixed_rate);

    int backoff_count = 0;
    const auto backoff = tasks.post(
        [&log, &backoff_count](event_hub::TaskContext& self) {
            ++backoff_count;
            log.push_back("self-rescheduled one-shot " +
                          std::to_string(backoff_count));

            if (backoff_count == 1) {
                self.reschedule_after(std::chrono::milliseconds(3));
                log.push_back("self-rescheduled one-shot delayed");
            }
        });

    int retry_count = 0;
    tasks.post_every_after_ms(1, 6, [&log, &retry_count] {
        ++retry_count;
        log.push_back("retry-like periodic attempt " +
                      std::to_string(retry_count));

        if (retry_count == 2) {
            throw std::runtime_error("retry source failed");
        }
    });

    tasks.post_after_ms(40, [&running] {
        running = false;
    });

    while (running || tasks.has_pending()) {
        const auto generation = notifier.generation();

        const auto work_done = tasks.process(8);
        if (work_done != 0) {
            continue;
        }

        const auto timeout = tasks.recommend_wait_for_ms(1);
        if (!tasks.has_ready()) {
            (void)notifier.wait_for(generation, timeout);
        }
    }

    std::cout << "scheduled ids: heartbeat=" << heartbeat
              << ", metrics=" << metrics
              << ", backoff=" << backoff << '\n';

    for (const auto& item : log) {
        std::cout << item << '\n';
    }

    tasks.reset_notifier();
}
