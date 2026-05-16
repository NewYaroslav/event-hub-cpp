#include <event_hub.hpp>
#include <event_hub/calendar_scheduler.hpp>
#include <time_shield.hpp>

#include <vector>

int main() {
    event_hub::TaskManager tasks;
    std::vector<event_hub::CalendarObserverInfo> events;
    event_hub::CalendarScheduler calendar(tasks);

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

    if (events[1].planned_utc_ms != expected) {
        return 1;
    }

    return calendar.cancel(id) ? 0 : 1;
}
