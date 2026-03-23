#include <glass/world.hpp>

namespace glass {

Entity World::create() {
    uint32_t index;
    uint32_t generation;

    if (!free_list_.empty()) {
        index = free_list_.back();
        free_list_.pop_back();
        generation = generations_[index];
    } else {
        index = next_index_++;
        generations_.push_back(0);
        generation = 0;
    }

    return Entity{index, generation};
}

void World::destroy(Entity e) {
    if (!alive(e)) {
        return;
    }

    // Fire callback before removing components (lets listeners capture GPU resources)
    if (on_destroy_) {
        on_destroy_(*this, e);
    }

    // Remove from all pools
    for (auto& [type, pool] : pools_) {
        pool->remove(e);
    }

    // Bump generation and add to free list
    generations_[e.index]++;
    free_list_.push_back(e.index);
}

bool World::alive(Entity e) const {
    if (e == null_entity) {
        return false;
    }
    if (e.index >= generations_.size()) {
        return false;
    }
    return generations_[e.index] == e.generation;
}

} // namespace glass
