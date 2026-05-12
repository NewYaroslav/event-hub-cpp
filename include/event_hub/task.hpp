#pragma once
#ifndef EVENT_HUB_TASK_HPP_INCLUDED
#define EVENT_HUB_TASK_HPP_INCLUDED

/// \file task.hpp
/// \brief Defines lightweight move-only task primitives.

#include <cstdint>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

namespace event_hub {

/// \brief Unique task identifier.
///
/// The value zero is reserved for invalid or rejected submissions.
using TaskId = std::uint64_t;

/// \brief Fixed task priority levels.
enum class TaskPriority : std::uint8_t {
    high = 0,
    normal = 1,
    low = 2
};

/// \brief Options used when submitting a task.
struct TaskOptions {
    TaskPriority priority{TaskPriority::normal};
};

/// \class Task
/// \brief Move-only type-erased wrapper for a void() callable.
///
/// Unlike std::function<void()>, Task can hold move-only callables in C++17,
/// including lambdas that capture std::unique_ptr or std::packaged_task.
class Task {
public:
    /// \brief Construct an empty task.
    Task() noexcept = default;

    /// \brief Construct an empty task.
    Task(std::nullptr_t) noexcept {}

    Task(Task&&) noexcept = default;
    Task& operator=(Task&&) noexcept = default;

    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    /// \brief Construct from std::function for convenience.
    explicit Task(std::function<void()> fn) {
        if (fn) {
            assign(std::move(fn));
        }
    }

    /// \brief Construct from any callable invocable as void().
    template <
        typename F,
        typename std::enable_if<
            !std::is_same<typename std::decay<F>::type, Task>::value &&
                !std::is_same<typename std::decay<F>::type,
                              std::nullptr_t>::value &&
                !std::is_same<typename std::decay<F>::type,
                              std::function<void()>>::value &&
                std::is_invocable_r<void, typename std::decay<F>::type&>::value,
            int>::type = 0>
    Task(F&& fn) {
        assign(std::forward<F>(fn));
    }

    /// \brief Return true when this task stores a callable.
    explicit operator bool() const noexcept {
        return static_cast<bool>(m_self);
    }

    /// \brief Invoke the stored callable.
    /// \throws std::bad_function_call when the task is empty.
    void operator()() {
        if (!m_self) {
            throw std::bad_function_call();
        }

        m_self->call();
    }

    /// \brief Reset this task to empty.
    void reset() noexcept {
        m_self.reset();
    }

private:
    struct Concept {
        virtual ~Concept() = default;
        virtual void call() = 0;
    };

    template <typename F>
    struct Model final : Concept {
        template <typename U>
        explicit Model(U&& fn_)
            : fn(std::forward<U>(fn_)) {}

        void call() override {
            fn();
        }

        F fn;
    };

    template <typename F>
    void assign(F&& fn) {
        using Fn = typename std::decay<F>::type;
        m_self.reset(new Model<Fn>(std::forward<F>(fn)));
    }

private:
    std::unique_ptr<Concept> m_self;
};

} // namespace event_hub

#endif // EVENT_HUB_TASK_HPP_INCLUDED
