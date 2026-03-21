#pragma once

#include <glass/entity.hpp>

namespace glass {

class World;

template<typename... Ts>
class View {
public:
    explicit View(World& world) : world_(world) {}

    void each(auto&& fn);

private:
    World& world_;
};

} // namespace glass
