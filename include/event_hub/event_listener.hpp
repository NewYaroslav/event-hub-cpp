#pragma once
#ifndef EVENT_HUB_EVENT_LISTENER_HPP_INCLUDED
#define EVENT_HUB_EVENT_LISTENER_HPP_INCLUDED

/// \file event_listener.hpp
/// \brief Defines the optional generic listener interface.

#include "event.hpp"

namespace event_hub {

/// \class EventListener
/// \brief Optional listener interface for Event-derived event types.
///
/// Use this interface when a module wants one generic callback and performs
/// Event::is(), Event::as(), or Event::as_ref() dispatch internally. Plain
/// value events can use typed callbacks directly without deriving from Event.
class EventListener {
public:
    /// \brief Destroy listener.
    virtual ~EventListener() = default;

    /// \brief Handle an Event-derived notification.
    /// \param event Event instance delivered by the bus.
    ///
    /// The default implementation intentionally ignores the event so derived
    /// classes can override only when they need listener-style dispatch.
    virtual void on_event(const Event& event) {
        (void)event;
    }
};

} // namespace event_hub

#endif // EVENT_HUB_EVENT_LISTENER_HPP_INCLUDED
