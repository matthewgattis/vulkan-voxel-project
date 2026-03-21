#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>

namespace glass {

struct Entity {
    uint32_t index;
    uint32_t generation;
    bool operator==(const Entity&) const = default;
};

inline constexpr Entity null_entity{0xFFFFFFFF, 0};

} // namespace glass

template<>
struct std::hash<glass::Entity> {
    std::size_t operator()(const glass::Entity& e) const noexcept {
        auto h1 = std::hash<uint32_t>{}(e.index);
        auto h2 = std::hash<uint32_t>{}(e.generation);
        return h1 ^ (h2 << 16);
    }
};
