#pragma once

#include <SDL3/SDL_events.h>

#include <cstdint>
#include <functional>
#include <vector>

namespace steel { class Engine; }

namespace glass {

class EventDispatcher;

/// RAII subscription handle. Unsubscribes automatically on destruction.
class Subscription {
public:
    Subscription() = default;
    ~Subscription();

    Subscription(Subscription&& other) noexcept;
    Subscription& operator=(Subscription&& other) noexcept;

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

private:
    friend class EventDispatcher;
    Subscription(EventDispatcher* dispatcher, uint32_t id);

    EventDispatcher* dispatcher_ = nullptr;
    uint32_t id_ = 0;
};

/// Fans out SDL events from a steel::Engine to multiple subscribers.
/// Each subscriber receives a `bool& handled` flag that earlier subscribers
/// may have set, allowing event consumption (e.g. ImGui blocking mouse input).
class EventDispatcher {
public:
    using Callback = std::function<void(const SDL_Event&, bool& handled)>;

    /// Registers as the engine's sole event callback.
    explicit EventDispatcher(steel::Engine& engine);

    /// Subscribe a callback. Subscribers are called in subscription order.
    /// The returned Subscription unsubscribes automatically when destroyed.
    Subscription subscribe(Callback callback);

private:
    friend class Subscription;

    void dispatch(const SDL_Event& event);
    void remove(uint32_t id);

    struct Entry {
        uint32_t id;
        Callback callback;
    };

    std::vector<Entry> subscribers_;
    uint32_t next_id_ = 0;
};

} // namespace glass
