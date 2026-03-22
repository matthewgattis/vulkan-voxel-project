#pragma once

#include <glm/vec3.hpp>

namespace voxel {

enum class VoxelType : uint8_t {
    Air = 0,
    Grass,
    Dirt,
    Stone,
};

inline bool is_solid(VoxelType type) {
    return type != VoxelType::Air;
}

inline glm::vec3 voxel_color(VoxelType type) {
    switch (type) {
        case VoxelType::Grass: return {0.35f, 0.70f, 0.25f};
        case VoxelType::Dirt:  return {0.55f, 0.36f, 0.20f};
        case VoxelType::Stone: return {0.55f, 0.55f, 0.55f};
        default:               return {0.0f, 0.0f, 0.0f};
    }
}

} // namespace voxel
