#include <event_hub.hpp>
#include <event_hub/calendar_scheduler.hpp>
#include <time_shield.hpp>

#include <vector>

int main() {
    event_hub::TaskManager tasks;
    event_hub::CalendarScheduler calendar(tasks);
    std::vector<event_hub::CalendarObserverInfo> events;

    event_hub::CalendarTaskOptions options;
    options.now_provider = [] {
        return time_shield::to_timestamp_ms(2024, 6, 1, 10, 0, 0, 0);
    };
    options.observer = [&](const event_hub::CalendarObserverInfo& info) {
        events.push_back(info);
    };

    const auto id = calendar.add_daily_task(
        event_hub::time_of_day(11, 0),
        [] {},
        options);

    if (id == 0 || events.size() < 2) {
        return 1;
    }

    const auto expected = time_shield::to_timestamp_ms(
        2024,
        6,
        1,
        11,
        0,
        0,
        0);

    return events[1].planned_utc_ms == expected ? 0 : 1;
}
