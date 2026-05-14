#include <event_hub.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if EVENT_HUB_CPP_USE_TIME_SHIELD
#include <event_hub/calendar_scheduler.hpp>
#include <time_shield.hpp>
#endif

int main() {
#if EVENT_HUB_CPP_USE_TIME_SHIELD
    event_hub::TaskManager tasks;
    event_hub::CalendarScheduler calendar(tasks);
    std::vector<std::string> log;

    auto fake_now_utc_ms =
        time_shield::to_timestamp_ms(2024, 10, 27, 0, 55, 0);

    auto options =
        event_hub::CalendarTaskOptions::in_zone(time_shield::CET)
            .use_now_provider([&fake_now_utc_ms] {
                return fake_now_utc_ms;
            });

    options.observer =
        [&log, &options](const event_hub::CalendarObserverInfo& info) {
            if (info.event != event_hub::CalendarObserverEvent::scheduled) {
                return;
            }

            const auto local =
                options.clock.from_utc_ms(info.planned_utc_ms).to_iso8601();
            const auto utc =
                time_shield::DateTime::from_unix_ms(info.planned_utc_ms)
                    .to_iso8601_utc();
            log.push_back("CET rule planned local " + local +
                          " / utc " + utc);
        };

    calendar.add_daily_task(
        event_hub::time_of_day(3, 5),
        [&log](event_hub::TaskContext& self) {
            log.push_back("calendar callback");
            self.cancel();
        },
        options);

    int custom_runs = 0;
    calendar.add_custom_calendar_task(
        [](time_shield::ts_ms_t,
           time_shield::ts_ms_t observed_now_utc_ms)
            -> std::optional<time_shield::ts_ms_t> {
            return observed_now_utc_ms + 10;
        },
        [&custom_runs, &fake_now_utc_ms, &log, options](
            event_hub::TaskContext& self) mutable {
            ++custom_runs;
            const auto local =
                options.clock.from_utc_ms(fake_now_utc_ms).to_iso8601();
            log.push_back("custom-time callback at local " + local);

            fake_now_utc_ms += time_shield::MS_PER_DAY;
            self.cancel();
        },
        options);

    const auto stop_at =
        event_hub::TaskManager::Clock::now() + std::chrono::milliseconds(200);
    while (tasks.has_pending() &&
           event_hub::TaskManager::Clock::now() < stop_at) {
        tasks.process();
        std::this_thread::sleep_for(tasks.recommend_wait_for_ms(20));
    }

    calendar.cancel_all();

    for (const auto& line : log) {
        std::cout << line << '\n';
    }
#else
    std::cout << "calendar_scheduler_custom_time example requires "
                 "EVENT_HUB_CPP_USE_TIME_SHIELD=ON\n";
#endif

    return 0;
}
