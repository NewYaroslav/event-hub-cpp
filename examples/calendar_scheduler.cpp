#include <event_hub.hpp>

#include <chrono>
#include <iostream>
#include <string>
#include <thread>
#include <utility>
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

    event_hub::CalendarTaskOptions observed;
    observed.observer = [&log](const event_hub::CalendarObserverInfo& info) {
        if (info.event == event_hub::CalendarObserverEvent::scheduled) {
            log.push_back("scheduled calendar task " +
                          std::to_string(info.id));
        }
    };

    const auto now_ms = time_shield::ZonedClock{}.utc_time_ms();
    const auto now_dt =
        time_shield::DateTime::from_unix_ms(now_ms).to_date_time_struct_utc();
    const auto current_time =
        event_hub::time_of_day(now_dt.hour, now_dt.min, now_dt.sec);

    event_hub::CalendarTaskOptions daily_options = observed;
    daily_options.last_due_utc_ms = now_ms - time_shield::MS_PER_DAY * 2;
    daily_options.missed_policy =
        event_hub::CalendarMissedRunPolicy::run_once_immediately;

    calendar.add_daily_task(
        current_time,
        [&log](event_hub::TaskContext& self) {
            log.push_back("daily maintenance catch-up");
            self.cancel();
        },
        daily_options);

    auto weekday_reports =
        event_hub::WeeklySchedule{}
            .add(event_hub::MON, event_hub::time_of_day(9, 0))
            .add(event_hub::FRI, event_hub::time_of_day(18, 0));
    calendar.add_weekly_task(std::move(weekday_reports),
                             [&log] {
                                 log.push_back("weekly report");
                             },
                             observed);

    auto monthly_cleanup =
        event_hub::MonthlySchedule{}.add(15,
                                         event_hub::time_of_day(12, 0));
    calendar.add_monthly_task(std::move(monthly_cleanup),
                              [&log] {
                                  log.push_back("monthly cleanup");
                              },
                              observed);

    int retry_count = 0;
    calendar.add_custom_calendar_task(
        [](time_shield::ts_ms_t,
           time_shield::ts_ms_t observed_now_utc_ms)
            -> std::optional<time_shield::ts_ms_t> {
            return observed_now_utc_ms + 15;
        },
        [&log, &retry_count](event_hub::TaskContext& self) {
            ++retry_count;
            log.push_back("custom retry " + std::to_string(retry_count));
            if (retry_count == 2) {
                self.cancel();
            }
        },
        observed);

    const auto stop_at =
        event_hub::TaskManager::Clock::now() + std::chrono::milliseconds(250);
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
    std::cout << "calendar_scheduler example requires "
                 "EVENT_HUB_CPP_USE_TIME_SHIELD=ON\n";
#endif

    return 0;
}
