#pragma once
#ifndef EVENT_HUB_CANCELLATION_HPP_INCLUDED
#define EVENT_HUB_CANCELLATION_HPP_INCLUDED

/// \file cancellation.hpp
/// \brief Lightweight cancellation primitives for awaiters.

#include <atomic>
#include <memory>

namespace event_hub {

/// \class CancellationToken
/// \brief Passive handle queried by awaiters for cancellation status.
///
/// Tokens share state with the CancellationSource that created them. An empty
/// token is valid and simply reports that cancellation has not been requested.
class CancellationToken {
public:
    /// \brief Check whether cancellation was requested.
    /// \return True when this token is linked to a cancelled source.
    bool is_cancelled() const noexcept {
        return m_state && m_state->cancelled.load(std::memory_order_relaxed);
    }

    /// \brief Check whether this token is linked to a source.
    /// \return True when the token has shared cancellation state.
    explicit operator bool() const noexcept {
        return static_cast<bool>(m_state);
    }

private:
    struct State {
        std::atomic_bool cancelled{false};
    };

    std::shared_ptr<State> m_state;

    friend class CancellationSource;
};

/// \class CancellationSource
/// \brief Owner object that can request cancellation on associated tokens.
///
/// Sources are lightweight owners for shared cancellation state. Tokens already
/// handed out keep referring to their original state when the source is reset.
class CancellationSource {
public:
    /// \brief Create a fresh cancellation source.
    CancellationSource()
        : m_state(std::make_shared<CancellationToken::State>()) {}

    /// \brief Return a token linked to this source.
    /// \return Token sharing this source's current cancellation state.
    CancellationToken token() const noexcept {
        CancellationToken token;
        token.m_state = m_state;
        return token;
    }

    /// \brief Request cancellation for all linked tokens.
    void cancel() noexcept {
        if (m_state) {
            m_state->cancelled.store(true, std::memory_order_relaxed);
        }
    }

    /// \brief Reset this source to a non-cancelled state.
    ///
    /// Existing tokens keep their previous shared state. Tokens created after
    /// reset() observe the new non-cancelled state.
    void reset() {
        m_state = std::make_shared<CancellationToken::State>();
    }

private:
    std::shared_ptr<CancellationToken::State> m_state;
};

} // namespace event_hub

#endif // EVENT_HUB_CANCELLATION_HPP_INCLUDED
