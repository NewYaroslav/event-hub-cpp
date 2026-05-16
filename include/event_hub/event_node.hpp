#pragma once
#ifndef EVENT_HUB_EVENT_NODE_HPP_INCLUDED
#define EVENT_HUB_EVENT_NODE_HPP_INCLUDED

/// \file event_node.hpp
/// \brief Defines the EventNode convenience base class.

#include "event.hpp"
#include "event_bus.hpp"
#include "event_endpoint.hpp"
#include "event_listener.hpp"

#include <future>
#include <memory>
#include <type_traits>
#include <utility>

namespace event_hub {

/// \class EventNode
/// \brief Convenience base class for modules that own event subscriptions.
///
/// \details
/// EventNode combines EventListener-style polymorphic event handling with
/// EventEndpoint-based subscription ownership.
///
/// The endpoint-like API is protected on purpose. External code should expose
/// and call module business methods instead of treating the module object as a
/// public bus facade.
///
/// It is intended for classes that prefer a single
/// `on_event(const Event&)` method and `Event::as<T>()` dispatch.
///
/// For plain C++ events that do not derive from `event_hub::Event`, prefer
/// `subscribe<T>(callback)` or use EventEndpoint directly.
///
/// Modules owned by `std::shared_ptr` can pass an external `std::weak_ptr`
/// guard to `subscribe<T>(guard, callback)` when callbacks capture module
/// state and need strict lifetime control.
///
/// \note EventBus must outlive EventNode.
/// \note Destroying EventNode closes the internal EventEndpoint.
/// \note Destruction unsubscribes owned subscriptions, but does not wait for
/// callbacks that have already started.
class EventNode : public EventListener {
public:
    /// \brief Subscription id type returned by EventBus.
    using SubscriptionId = EventBus::SubscriptionId;

    /// \brief Construct a node connected to a bus.
    /// \param bus Event bus that must outlive this node.
    explicit EventNode(EventBus& bus)
        : m_endpoint(bus) {}

    /// \brief Destroy the node and close the internal endpoint.
    ~EventNode() override {
        close();
    }

    EventNode(const EventNode&) = delete;
    EventNode& operator=(const EventNode&) = delete;

    EventNode(EventNode&&) = delete;
    EventNode& operator=(EventNode&&) = delete;

    /// \brief Close the internal endpoint.
    ///
    /// Closing cancels awaiters and unsubscribes the subscriptions owned by
    /// this node. The operation is idempotent and does not wait for callbacks
    /// that have already started.
    void close() {
        m_endpoint.close();
    }

    /// \brief Return true after the internal endpoint has been closed.
    /// \return True when the node endpoint is closed.
    bool is_closed() const noexcept {
        return m_endpoint.is_closed();
    }

    /// \brief Return the bus referenced by this node.
    /// \return Event bus reference.
    EventBus& bus() noexcept {
        return m_endpoint.bus();
    }

    /// \brief Return the bus referenced by this node.
    /// \return Event bus reference.
    const EventBus& bus() const noexcept {
        return m_endpoint.bus();
    }

protected:
    /// \brief Subscribe this node as EventListener to EventType.
    /// \tparam EventType Event type derived from event_hub::Event.
    /// \return Subscription id that can be used for targeted unsubscription.
    ///
    /// Use listen() for Event-derived types handled by on_event(). Plain value
    /// events should use subscribe<T>(callback) instead.
    template <typename EventType>
    SubscriptionId listen() {
        static_assert(std::is_base_of<Event, EventType>::value,
                      "EventType must derive from event_hub::Event");

        return m_endpoint.subscribe<EventType>(*this);
    }

    /// \brief Subscribe with a callback owned by this node.
    /// \tparam EventType Concrete event type.
    /// \tparam Callback Callback type invocable with `const EventType&`.
    /// \param callback Callback invoked when EventType is dispatched.
    /// \return Subscription id that can be used for targeted unsubscription.
    template <typename EventType, typename Callback>
    SubscriptionId subscribe(Callback&& callback) {
        return m_endpoint.subscribe<EventType>(
            std::forward<Callback>(callback));
    }

    /// \brief Subscribe with a callback and an external lifetime guard.
    /// \tparam EventType Concrete event type.
    /// \tparam Guard Type stored by the external weak guard.
    /// \tparam Callback Callback type invocable with `const EventType&`.
    /// \param guard Weak guard that must lock before callback invocation.
    /// \param callback Callback invoked when EventType is dispatched.
    /// \return Subscription id that can be used for targeted unsubscription.
    ///
    /// The internal endpoint guard still applies, so both the node endpoint
    /// and the external guard must be alive before user code runs.
    template <typename EventType, typename Guard, typename Callback>
    SubscriptionId subscribe(std::weak_ptr<Guard> guard, Callback&& callback) {
        return m_endpoint.subscribe<EventType>(
            std::move(guard),
            std::forward<Callback>(callback));
    }

    /// \brief Remove one subscription by id.
    /// \param id Subscription id returned by subscribe() or listen().
    void unsubscribe(SubscriptionId id) {
        m_endpoint.unsubscribe(id);
    }

    /// \brief Remove this node's subscriptions for a concrete event type.
    /// \tparam EventType Concrete event type to unsubscribe.
    template <typename EventType>
    void unsubscribe() {
        m_endpoint.unsubscribe<EventType>();
    }

    /// \brief Remove all subscriptions owned by this node.
    ///
    /// This removes future bus records but does not wait for callbacks that
    /// already started.
    void unsubscribe_all() {
        m_endpoint.unsubscribe_all();
    }

    /// \brief Dispatch an already constructed event synchronously.
    /// \tparam EventType Concrete event type.
    /// \param event Event object to dispatch.
    /// \throws Any callback exception when no exception handler is configured
    /// on the bus.
    template <typename EventType>
    void emit(const EventType& event) {
        m_endpoint.emit<EventType>(event);
    }

    /// \brief Dispatch an already constructed event synchronously.
    /// \tparam EventType Concrete event type.
    /// \param event Event object to dispatch.
    /// \throws Any callback exception when no exception handler is configured
    /// on the bus.
    template <typename EventType>
    void emit(EventType&& event) {
        m_endpoint.emit<EventType>(std::move(event));
    }

    /// \brief Construct and dispatch an event synchronously.
    /// \tparam EventType Concrete event type.
    /// \tparam Args Constructor argument types.
    /// \param args Arguments used to construct the event.
    /// \throws Any callback exception when no exception handler is configured
    /// on the bus.
    template <typename EventType,
              typename... Args,
              typename std::enable_if<
                  !detail::IsSingleEventArgument<EventType, Args...>::value,
                  int>::type = 0>
    void emit(Args&&... args) {
        m_endpoint.emit<EventType>(std::forward<Args>(args)...);
    }

    /// \brief Queue an already constructed event for later processing.
    /// \tparam EventType Concrete event type.
    /// \param event Event object to queue.
    template <typename EventType>
    void post(const EventType& event) {
        m_endpoint.post<EventType>(event);
    }

    /// \brief Queue an already constructed event for later processing.
    /// \tparam EventType Concrete event type.
    /// \param event Event object to queue.
    template <typename EventType>
    void post(EventType&& event) {
        m_endpoint.post<EventType>(std::move(event));
    }

    /// \brief Construct and queue an event for later processing.
    /// \tparam EventType Concrete event type.
    /// \tparam Args Constructor argument types.
    /// \param args Arguments used to construct the event.
    template <typename EventType,
              typename... Args,
              typename std::enable_if<
                  !detail::IsSingleEventArgument<EventType, Args...>::value,
                  int>::type = 0>
    void post(Args&&... args) {
        m_endpoint.post<EventType>(std::forward<Args>(args)...);
    }

    /// \brief Post a request and await the paired result event.
    /// \tparam RequestEvent Request event type.
    /// \tparam ResultEvent Result event type.
    /// \tparam Callback Callback type invocable with `const ResultEvent&`.
    /// \param request Request event. Its request id is overwritten.
    /// \param callback Callback invoked for the first result with the same id.
    /// \param options Timeout and cancellation options for the awaiter.
    /// \return Generated request id stored in the request event.
    template <typename RequestEvent, typename ResultEvent, typename Callback>
    RequestId request(RequestEvent request,
                      Callback&& callback,
                      AwaitOptions options = {}) {
        return m_endpoint.template request<RequestEvent, ResultEvent>(
            std::move(request),
            std::forward<Callback>(callback),
            std::move(options));
    }

    /// \brief Post a request and return a future for the paired result event.
    /// \tparam RequestEvent Request event type.
    /// \tparam ResultEvent Result event type.
    /// \param request Request event. Its request id is overwritten.
    /// \param options Timeout and cancellation options for the awaiter.
    /// \return Future completed by the first matching result event.
    template <typename RequestEvent, typename ResultEvent>
    std::future<ResultEvent> request_future(RequestEvent request,
                                            AwaitOptions options = {}) {
        return m_endpoint.template request_future<RequestEvent, ResultEvent>(
            std::move(request),
            std::move(options));
    }

    /// \brief Return the next bus-wide request id.
    /// \return Generated request id.
    RequestId next_request_id() noexcept {
        return m_endpoint.next_request_id();
    }

    /// \brief Return the internal endpoint.
    /// \return Mutable endpoint reference.
    ///
    /// Prefer the protected EventNode forwarding methods unless derived code
    /// needs direct endpoint access for an advanced integration point.
    EventEndpoint& endpoint() noexcept {
        return m_endpoint;
    }

    /// \brief Return the internal endpoint.
    /// \return Const endpoint reference.
    const EventEndpoint& endpoint() const noexcept {
        return m_endpoint;
    }

private:
    EventEndpoint m_endpoint;
};

} // namespace event_hub

#endif // EVENT_HUB_EVENT_NODE_HPP_INCLUDED
