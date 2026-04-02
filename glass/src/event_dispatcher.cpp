#include <glass/event_dispatcher.hpp>
#include <steel/engine.hpp>

#include <algorithm>

namespace glass {

// --- Subscription ---

Subscription::Subscription(EventDispatcher* dispatcher, uint32_t id)
    : dispatcher_{dispatcher}, id_{id} {}

Subscription::~Subscription() {
    if (dispatcher_) {
        dispatcher_->remove(id_);
    }
}

Subscription::Subscription(Subscription&& other) noexcept
    : dispatcher_{other.dispatcher_}, id_{other.id_} {
    other.dispatcher_ = nullptr;
}

Subscription& Subscription::operator=(Subscription&& other) noexcept {
    if (this != &other) {
        if (dispatcher_) {
            dispatcher_->remove(id_);
        }
        dispatcher_ = other.dispatcher_;
        id_ = other.id_;
        other.dispatcher_ = nullptr;
    }
    return *this;
}

// --- EventDispatcher ---

EventDispatcher::EventDispatcher(steel::Engine& engine) {
    engine.set_event_callback([this](const SDL_Event& event) { dispatch(event); });
}

Subscription EventDispatcher::subscribe(Callback callback) {
    uint32_t id = next_id_++;
    subscribers_.push_back({id, std::move(callback)});
    return {this, id};
}

void EventDispatcher::dispatch(const SDL_Event& event) {
    bool handled = false;
    for (auto& entry : subscribers_) {
        entry.callback(event, handled);
    }
}

void EventDispatcher::remove(uint32_t id) {
    std::erase_if(subscribers_, [id](const Entry& e) { return e.id == id; });
}

} // namespace glass
