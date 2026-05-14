#pragma once
#ifndef EVENT_HUB_CALENDAR_SCHEDULER_TYPES_HPP_INCLUDED
#define EVENT_HUB_CALENDAR_SCHEDULER_TYPES_HPP_INCLUDED

/// \file calendar_scheduler/types.hpp
/// \brief Public DTOs and policies for CalendarScheduler.

#ifndef EVENT_HUB_CPP_USE_TIME_SHIELD
#define EVENT_HUB_CPP_USE_TIME_SHIELD 0
#endif

#if !EVENT_HUB_CPP_USE_TIME_SHIELD
#error "event_hub/calendar_scheduler.hpp requires EVENT_HUB_CPP_USE_TIME_SHIELD=ON"
#endif

#include "../task.hpp"

#include <time_shield.hpp>

// time-shield-cpp may include Windows.h for clock helpers on Windows.
#ifdef ReportEvent
#undef ReportEvent
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace event_hub {

/// \brief Stable identifier for a calendar rule owned by CalendarScheduler.
using CalendarTaskId = std::uint64_t;

/// \brief Local time of day used by calendar rules.
using LocalTime = time_shield::TimeStruct;

/// \brief Sunday weekday alias for calendar schedules.
inline constexpr time_shield::Weekday SUN = time_shield::SUN;

/// \brief Monday weekday alias for calendar schedules.
inline constexpr time_shield::Weekday MON = time_shield::MON;

/// \brief Tuesday weekday alias for calendar schedules.
inline constexpr time_shield::Weekday TUE = time_shield::TUE;

/// \brief Wednesday weekday alias for calendar schedules.
inline constexpr time_shield::Weekday WED = time_shield::WED;

/// \brief Thursday weekday alias for calendar schedules.
inline constexpr time_shield::Weekday THU = time_shield::THU;

/// \brief Friday weekday alias for calendar schedules.
inline constexpr time_shield::Weekday FRI = time_shield::FRI;

/// \brief Saturday weekday alias for calendar schedules.
inline constexpr time_shield::Weekday SAT = time_shield::SAT;

/// \brief Build a time-of-day value for calendar rules.
///
/// Daily, weekly, and monthly rules use second granularity and reject nonzero
/// millisecond values.
inline constexpr LocalTime time_of_day(int hour,
                                       int minute,
                                       int second = 0,
                                       int millisecond = 0) noexcept {
    return LocalTime{static_cast<std::int16_t>(hour),
                     static_cast<std::int16_t>(minute),
                     static_cast<std::int16_t>(second),
                     static_cast<std::int16_t>(millisecond)};
}

/// \brief Return true when a time of day can be represented by second granularity.
inline bool is_valid_time_of_day(const LocalTime& time) noexcept {
    return time.ms == 0 && time_shield::is_valid_time(time);
}

/// \brief Convert a time of day to seconds since local midnight.
inline std::int32_t second_of_day(const LocalTime& time) noexcept {
    return time_shield::sec_of_day<std::int32_t>(time.hour,
                                                 time.min,
                                                 time.sec);
}

/// \brief Convert a time of day to milliseconds since local midnight.
inline std::int32_t ms_of_day(const LocalTime& time) noexcept {
    return second_of_day(time) *
               static_cast<std::int32_t>(time_shield::MS_PER_SEC) +
           static_cast<std::int32_t>(time.ms);
}

/// \brief Policy for runs missed before a calendar rule is created.
enum class CalendarMissedRunPolicy : std::uint8_t {
    /// \brief Skip missed work and schedule the next future occurrence.
    skip = 0,

    /// \brief Queue one missed occurrence immediately.
    run_once_immediately = 1
};

/// \brief Policy for occurrences that become overdue while a callback runs.
enum class CalendarOverlapPolicy : std::uint8_t {
    /// \brief Skip overdue occurrences and schedule the next future one.
    skip = 0,

    /// \brief Queue one overdue occurrence for the next process() pass.
    queue_one_after_current = 1
};

/// \brief Observer event emitted by CalendarScheduler.
enum class CalendarObserverEvent : std::uint8_t {
    created,
    scheduled,
    due,
    before_callback,
    after_callback,
    missed,
    cancelled
};

/// \brief Observer payload for calendar task lifecycle events.
struct CalendarObserverInfo {
    /// \brief Stable calendar rule id.
    CalendarTaskId id = 0;

    /// \brief Current one-shot TaskManager id, or zero when none exists.
    TaskId scheduled_task_id = 0;

    /// \brief Planned UTC timestamp for the event, in Unix epoch milliseconds.
    time_shield::ts_ms_t planned_utc_ms = 0;

    /// \brief Observed UTC timestamp from the scheduler clock/provider.
    time_shield::ts_ms_t observed_utc_ms = 0;

    /// \brief Event kind.
    CalendarObserverEvent event = CalendarObserverEvent::created;

    /// \brief Number of callbacks that have completed normally.
    std::uint64_t run_count = 0;
};

/// \brief Options for calendar scheduled tasks.
struct CalendarTaskOptions {
    /// \brief Build options that interpret calendar rules in a named zone.
    static CalendarTaskOptions in_zone(time_shield::TimeZone zone) noexcept {
        CalendarTaskOptions options;
        options.clock.set_zone(zone);
        return options;
    }

    /// \brief Build options with an already configured time-shield clock.
    ///
    /// Pass time_shield::ZonedClock(zone, true) when NTP-backed UTC time should
    /// be used in builds configured with time-shield NTP support.
    static CalendarTaskOptions with_clock(time_shield::ZonedClock clock) {
        CalendarTaskOptions options;
        options.clock = std::move(clock);
        return options;
    }

    /// \brief Build options that interpret rules in a fixed UTC offset.
    ///
    /// \param utc_offset_seconds Offset in seconds, e.g. +3 hours is 10800.
    /// \throws std::invalid_argument when the offset is outside time-shield's
    /// supported range.
    static CalendarTaskOptions fixed_utc_offset(
        time_shield::tz_t utc_offset_seconds) {
        CalendarTaskOptions options;
        options.clock = time_shield::ZonedClock(utc_offset_seconds);
        return options;
    }

    /// \brief Priority used for each queued one-shot callback.
    TaskPriority priority{TaskPriority::normal};

    /// \brief How to handle runs missed before rule creation.
    CalendarMissedRunPolicy missed_policy{CalendarMissedRunPolicy::skip};

    /// \brief How to handle runs overdue after the previous callback.
    CalendarOverlapPolicy overlap_policy{CalendarOverlapPolicy::skip};

    /// \brief Time-zone context for local calendar rules. Defaults to UTC.
    ///
    /// time_shield::ZonedClock owns both timezone interpretation and the UTC
    /// time source. Use time_shield::ZonedClock(zone, true) to request
    /// time-shield NTP-backed UTC time in NTP-enabled builds.
    time_shield::ZonedClock clock{};

    /// \brief Optional UTC now provider returning Unix epoch milliseconds.
    ///
    /// When empty, the scheduler uses clock.utc_time_ms(). The provider should
    /// return UTC time; the clock still defines how local daily/weekly/monthly
    /// rules are interpreted.
    std::function<time_shield::ts_ms_t()> now_provider{};

    /// \brief Optional persisted last planned UTC run in milliseconds.
    ///
    /// Used during creation to detect missed work after application downtime.
    std::optional<time_shield::ts_ms_t> last_due_utc_ms{};

    /// \brief Optional lifecycle observer.
    ///
    /// The observer may be called from add, cancel(), cancel_all(), and the
    /// CalendarScheduler destructor. Exceptions thrown by the observer are
    /// ignored. User code owns the lifetime of objects captured by observer.
    std::function<void(const CalendarObserverInfo&)> observer{};

    /// \brief Set the named zone used to interpret local calendar rules.
    CalendarTaskOptions& set_zone(time_shield::TimeZone zone) noexcept {
        clock.set_zone(zone);
        return *this;
    }

    /// \brief Replace the time-shield clock used by this calendar rule.
    CalendarTaskOptions& set_clock(time_shield::ZonedClock value) {
        clock = std::move(value);
        return *this;
    }

    /// \brief Set a fixed UTC offset used to interpret local calendar rules.
    ///
    /// \param utc_offset_seconds Offset in seconds, e.g. +3 hours is 10800.
    /// \throws std::invalid_argument when the offset is outside time-shield's
    /// supported range.
    CalendarTaskOptions& set_fixed_utc_offset(
        time_shield::tz_t utc_offset_seconds) {
        clock = time_shield::ZonedClock(utc_offset_seconds);
        return *this;
    }

    /// \brief Set a custom UTC millisecond time provider.
    CalendarTaskOptions& use_now_provider(
        std::function<time_shield::ts_ms_t()> provider) {
        now_provider = std::move(provider);
        return *this;
    }
};

/// \brief Weekly local-time schedule, indexed by time_shield::Weekday.
struct WeeklySchedule {
    /// \brief Add a second-of-day to a weekday and return this schedule.
    WeeklySchedule& add(time_shield::Weekday day, int second_of_day) {
        if (day >= time_shield::SUN && day <= time_shield::SAT &&
            second_of_day >= 0 &&
            second_of_day <= time_shield::MAX_SEC_PER_DAY) {
            seconds_by_weekday[static_cast<std::size_t>(day)]
                .push_back(second_of_day);
        }
        return *this;
    }

    /// \brief Add a local time to a weekday and return this schedule.
    WeeklySchedule& add(time_shield::Weekday day, LocalTime time) {
        if (is_valid_time_of_day(time)) {
            add(day, second_of_day(time));
        }
        return *this;
    }

    /// \brief Seconds of day for each weekday, where SUN is index 0.
    std::array<std::vector<int>, 7> seconds_by_weekday{};
};

/// \brief Monthly local-time schedule, indexed by day of month.
struct MonthlySchedule {
    /// \brief Add a second-of-day to a month day and return this schedule.
    MonthlySchedule& add(int month_day, int second_of_day) {
        if (month_day >= 1 &&
            month_day < static_cast<int>(seconds_by_month_day.size()) &&
            second_of_day >= 0 &&
            second_of_day <= time_shield::MAX_SEC_PER_DAY) {
            seconds_by_month_day[static_cast<std::size_t>(month_day)]
                .push_back(second_of_day);
        }
        return *this;
    }

    /// \brief Add a local time to a month day and return this schedule.
    MonthlySchedule& add(int month_day, LocalTime time) {
        if (is_valid_time_of_day(time)) {
            add(month_day, second_of_day(time));
        }
        return *this;
    }

    /// \brief Seconds of day for days 1..31. Index 0 is ignored.
    std::array<std::vector<int>, 32> seconds_by_month_day{};
};

} // namespace event_hub

#endif // EVENT_HUB_CALENDAR_SCHEDULER_TYPES_HPP_INCLUDED
