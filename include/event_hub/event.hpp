#pragma once
#ifndef EVENT_HUB_EVENT_HPP_INCLUDED
#define EVENT_HUB_EVENT_HPP_INCLUDED

/// \file event.hpp
/// \brief Defines the optional base event contract.

#include <memory>
#include <typeindex>
#include <typeinfo>

namespace event_hub {

/// \class Event
/// \brief Optional base class for events that need runtime metadata.
///
/// The bus can dispatch plain C++ value types. Derive from Event only when
/// runtime type information, a stable debug name, or cloning is useful.
///
/// \note Derived event types should remain small value-like contracts. Use
/// EVENT_HUB_EVENT in each concrete derived type when default metadata and
/// clone behavior are sufficient.
class Event {
public:
    /// \brief Destroy event.
    virtual ~Event() = default;

    /// \brief Return the concrete runtime type.
    /// \return Type index of the most-derived event type.
    virtual std::type_index type() const = 0;

    /// \brief Return a stable event name for diagnostics.
    /// \return Null-terminated event name string.
    virtual const char* name() const = 0;

    /// \brief Create a copy of this event.
    /// \return Owning pointer to a copy of this event.
    virtual std::unique_ptr<Event> clone() const = 0;

    /// \brief Check whether this event has the requested concrete type.
    /// \tparam EventType Concrete event type to check.
    /// \return True when this event's runtime type exactly matches EventType.
    template <typename EventType>
    bool is() const {
        return type() == std::type_index(typeid(EventType));
    }

    /// \brief Cast this event to the requested concrete type or return null.
    /// \tparam EventType Concrete event type to cast to.
    /// \return Pointer to EventType when the runtime type matches, otherwise
    /// null.
    template <typename EventType>
    const EventType* as() const {
        return is<EventType>() ? static_cast<const EventType*>(this) : nullptr;
    }

    /// \brief Cast this event to the requested concrete type.
    /// \tparam EventType Concrete event type to cast to.
    /// \return Reference to this event as EventType.
    /// \throws std::bad_cast when the type does not match.
    template <typename EventType>
    const EventType& as_ref() const {
        if (!is<EventType>()) {
            throw std::bad_cast{};
        }
        return static_cast<const EventType&>(*this);
    }
};

} // namespace event_hub

/// \brief Implement Event metadata and clone support for a concrete type.
/// \param TypeName Concrete event class that derives from event_hub::Event.
///
/// The macro assumes TypeName is copy-constructible because clone() returns a
/// newly allocated copy of the concrete event.
#define EVENT_HUB_EVENT(TypeName)                                             \
    std::type_index type() const override {                                   \
        return std::type_index(typeid(TypeName));                             \
    }                                                                         \
    const char* name() const override {                                       \
        return #TypeName;                                                     \
    }                                                                         \
    std::unique_ptr<::event_hub::Event> clone() const override {              \
        return std::make_unique<TypeName>(*this);                             \
    }

#endif // EVENT_HUB_EVENT_HPP_INCLUDED
